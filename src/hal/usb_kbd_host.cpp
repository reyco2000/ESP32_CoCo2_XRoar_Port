/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : usb_kbd_host.cpp
 *  Module : USB HID Host driver — Core 0 event loop with FreeRTOS queue to Core 1
 * ============================================================
 */

/*
 * usb_kbd_host.cpp — USB HID Host using ESP32_USB_Host_HID library
 *
 * Architecture
 * ────────────
 *  Core 0 tasks  →  USB host lib event loop + HID driver background task
 *  Core 1 loop   →  hid_host_process() drains FreeRTOS queue → calls user CB
 *
 * Based on working ESP32-S3 USB keyboard reader code.
 * Uses the esp32beans ESP32_USB_Host_HID library which wraps
 * the Espressif USB Host HID class driver.
 */

#include "usb_kbd_host.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"

// ESP32_USB_Host_HID library headers
#include "hid_host.h"
#include "hid_usage_keyboard.h"

static const char* TAG = "USB_KBD";

// ── Key event passed through the inter-core queue ────────────────────────────
typedef struct {
    uint8_t usage;
    uint8_t modifiers;
    bool    pressed;
} key_event_t;

// ── Module state ──────────────────────────────────────────────────────────────
static hid_key_callback_t s_callback     = nullptr;
static QueueHandle_t      s_key_queue    = nullptr;
static volatile bool      s_connected    = false;

static uint8_t s_prev_keys[6] = {0};  // Previous report keycodes for diff
static uint8_t s_prev_modifier = 0;   // Previous modifier byte

// ═══════════════════════════════════════════════════════════════════════════════
// Boot-protocol report processing — diff consecutive reports
// ═══════════════════════════════════════════════════════════════════════════════

static void process_keyboard_report(const uint8_t* data, unsigned int length)
{
    if (length < 8) return;

    uint8_t modifier = data[0];

    // ── Modifier changes ─────────────────────────────────────────────────────
    if (modifier != s_prev_modifier) {
        key_event_t evt = { 0, modifier, true };
        xQueueSend(s_key_queue, &evt, 0);
        s_prev_modifier = modifier;
    }

    // ── Newly pressed keys ───────────────────────────────────────────────────
    for (int i = 2; i < 8; i++) {
        uint8_t keycode = data[i];
        if (keycode == 0) continue;

        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (s_prev_keys[j] == keycode) { was_pressed = true; break; }
        }
        if (!was_pressed) {
            key_event_t evt = { keycode, modifier, true };
            xQueueSend(s_key_queue, &evt, 0);
        }
    }

    // ── Newly released keys ──────────────────────────────────────────────────
    for (int j = 0; j < 6; j++) {
        if (s_prev_keys[j] == 0) continue;
        bool still_pressed = false;
        for (int i = 2; i < 8; i++) {
            if (data[i] == s_prev_keys[j]) { still_pressed = true; break; }
        }
        if (!still_pressed) {
            key_event_t evt = { s_prev_keys[j], modifier, false };
            xQueueSend(s_key_queue, &evt, 0);
        }
    }

    // Save current keycodes
    for (int i = 0; i < 6; i++) {
        s_prev_keys[i] = data[i + 2];
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HID Host interface event callback (input reports, disconnect)
// ═══════════════════════════════════════════════════════════════════════════════

static void hid_host_interface_event_cb(hid_host_device_handle_t hid_device,
                                         const hid_host_interface_event_t event,
                                         void* arg)
{
    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
            hid_host_dev_params_t dev_params;
            hid_host_device_get_params(hid_device, &dev_params);

            if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE &&
                dev_params.proto == HID_PROTOCOL_KEYBOARD) {

                uint8_t data[16] = {0};
                unsigned int data_length = sizeof(data);

                esp_err_t err = hid_host_device_get_raw_input_report_data(
                    hid_device, data, data_length, (size_t*)&data_length);

                if (err == ESP_OK) {
                    process_keyboard_report(data, data_length);
                }
            }
            break;
        }

        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Keyboard disconnected.");
            s_connected = false;
            hid_host_device_close(hid_device);
            break;

        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGE(TAG, "Transfer error.");
            break;

        default:
            ESP_LOGE(TAG, "Unknown interface event: %d", event);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HID Host device event callback (new device connected)
// ═══════════════════════════════════════════════════════════════════════════════

static void hid_host_device_event_cb(hid_host_device_handle_t hid_device,
                                      const hid_host_driver_event_t event,
                                      void* arg)
{
    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED: {
            hid_host_dev_params_t dev_params;
            hid_host_device_get_params(hid_device, &dev_params);

            ESP_LOGI(TAG, "HID Device Connected — sub_class: %d, protocol: %d",
                     dev_params.sub_class, dev_params.proto);

            if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
                ESP_LOGI(TAG, "Keyboard detected. Opening...");

                const hid_host_device_config_t dev_config = {
                    .callback = hid_host_interface_event_cb,
                    .callback_arg = NULL,
                };

                esp_err_t err = hid_host_device_open(hid_device, &dev_config);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Device open failed: 0x%X", err);
                    return;
                }

                // Request boot protocol for simple 8-byte reports
                if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
                    err = hid_class_request_set_protocol(hid_device, HID_REPORT_PROTOCOL_BOOT);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "Set boot protocol warning: 0x%X", err);
                    }
                }

                err = hid_host_device_start(hid_device);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Device start failed: 0x%X", err);
                    return;
                }

                s_connected = true;
                memset(s_prev_keys, 0, sizeof(s_prev_keys));
                s_prev_modifier = 0;
                ESP_LOGI(TAG, "Keyboard ready.");
            } else {
                ESP_LOGW(TAG, "Non-keyboard HID device (protocol=%d), ignoring.",
                         dev_params.proto);
            }
            break;
        }
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// USB host library event task (runs on Core 0)
// ═══════════════════════════════════════════════════════════════════════════════

static void usb_host_lib_task(void* arg)
{
    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════════════

void hid_host_begin(hid_key_callback_t callback)
{
    s_callback  = callback;
    s_key_queue = xQueueCreate(32, sizeof(key_event_t));
    configASSERT(s_key_queue);
    memset(s_prev_keys, 0, sizeof(s_prev_keys));
    s_prev_modifier = 0;

    // Force USB bus reset: drive D-/D+ low briefly so the keyboard
    // sees a disconnect and will re-enumerate after host starts.
    // ESP32-S3 USB pins: GPIO19 = D-, GPIO20 = D+
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_20, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_19, 0);
    gpio_set_level(GPIO_NUM_20, 0);
    vTaskDelay(pdMS_TO_TICKS(250));  // 250ms — some keyboards need longer
    // Release pins: set to input with NO pull-ups/pull-downs so the
    // USB PHY can cleanly reclaim them via usb_host_install().
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_NUM_20, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_19, GPIO_FLOATING);
    gpio_set_pull_mode(GPIO_NUM_20, GPIO_FLOATING);
    vTaskDelay(pdMS_TO_TICKS(150));

    // 1. Install USB Host Library
    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB Host install failed: 0x%X", err);
        return;
    }

    // 2. Create USB host library event handler task on Core 0
    xTaskCreatePinnedToCore(
        usb_host_lib_task,
        "usb_host_lib",
        4096,
        NULL,
        2,
        NULL,
        0  // Core 0
    );

    // 3. Install HID Host driver (creates its own background task on Core 0)
    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_event_cb,
        .callback_arg = NULL,
    };
    err = hid_host_install(&hid_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HID Host install failed: 0x%X", err);
        return;
    }

    Serial.println("[USB_KBD] USB Host + HID driver started on Core 0.");
    Serial.println("[USB_KBD] Connect USB keyboard to GPIO19(D-)/GPIO20(D+).");
    Serial.println("[USB_KBD] VBUS 5V must be supplied externally.");
    Serial.println("[USB_KBD] Serial monitor via UART port (not USB).");
}

void hid_host_process()
{
    key_event_t evt;
    while (xQueueReceive(s_key_queue, &evt, 0) == pdTRUE) {
        if (s_callback) {
            s_callback(evt.usage, evt.modifiers, evt.pressed);
        }
    }
}

bool hid_host_is_connected()
{
    return s_connected;
}
