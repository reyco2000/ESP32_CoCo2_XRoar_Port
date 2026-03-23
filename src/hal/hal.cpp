/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : hal.cpp
 *  Module : HAL top-level — initialization and per-frame dispatch to subsystem drivers
 * ============================================================
 */

/*
 * hal.cpp - Top-level HAL initialization and frame dispatch
 *
 * Calls into individual subsystem init/update functions.
 */

#include "hal.h"
#include "../utils/debug.h"
#include "usb_kbd_host.h"

void hal_init(void) {
    DEBUG_PRINT("HAL: initializing subsystems...");
    hal_storage_init();
    // NOTE: hal_video_init() is called AFTER ROM loading in setup()
    // because TFT_eSPI takes over the shared SPI bus (pins 11/12/13)
    // and SD card reads would hang after TFT init.
    hal_audio_init();
    hal_keyboard_init();
    hal_joystick_init();
    DEBUG_PRINT("HAL: init complete (video deferred)");
}

void hal_process_input(void) {
    // Tick deferred key releases (ensures keys stay held for min frames)
    hal_keyboard_tick();
    // Drain USB HID key events from Core 0 queue
    hid_host_process();
    // Update joystick ADC readings
    hal_joystick_update();
}

void hal_render_frame(void) {
    // Video present is called by machine_run_frame() after all scanlines
    // Nothing additional needed here
}
