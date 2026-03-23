/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : usb_kbd_host.h
 *  Module : USB HID Host interface — key event queue and callback registration
 * ============================================================
 */

/*
 * usb_kbd_host.h — USB HID Host wrapper for ESP32-S3
 *
 * Uses the ESP32_USB_Host_HID library by esp32beans.
 * The USB event loop runs in FreeRTOS tasks on Core 0.
 * Key events are passed to the main sketch (Core 1) via a FreeRTOS queue.
 *
 * Hardware requirements
 *   GPIO19 = D-  (ESP32-S3 native USB OTG port)
 *   GPIO20 = D+
 *   VBUS   = 5V must be supplied externally (host mode)
 *
 * Board Settings
 *   Board:           ESP32S3 Dev Module
 *   USB Mode:        USB-OTG (TinyUSB)
 *   USB CDC On Boot: Disabled
 *   Serial Monitor via UART port (not USB)
 *
 * Library requirement
 *   ESP32_USB_Host_HID: https://github.com/esp32beans/ESP32_USB_Host_HID
 */

#ifndef USB_KBD_HOST_H
#define USB_KBD_HOST_H

#include <Arduino.h>
#include <stdint.h>

// Callback signature: (hid_usage_id, modifier_byte, key_down?)
// usage == 0 indicates a modifier-only change (shift/ctrl/etc.)
typedef void (*hid_key_callback_t)(uint8_t usage, uint8_t modifiers, bool pressed);

// Initialise the USB host stack and HID driver.
// callback — called from hid_host_process() whenever a key event is dequeued.
void hid_host_begin(hid_key_callback_t callback);

// Process all pending key events in the inter-task queue.
// Call once per loop() iteration (non-blocking).
void hid_host_process();

// Returns true when a USB HID keyboard is currently enumerated.
bool hid_host_is_connected();

#endif // USB_KBD_HOST_H
