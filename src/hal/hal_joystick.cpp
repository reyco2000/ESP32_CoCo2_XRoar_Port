/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : hal_joystick.cpp
 *  Module : Joystick HAL — CoCoJoystick ADC reads mapped to PIA comparator interface
 * ============================================================
 */

/*
 * hal_joystick.cpp - Joystick input via CoCoJoystick library
 *
 * CoCo joystick: two analog axes (0-63) + fire button per port.
 * Read via ESP32 ADC with calibration, dead zone, and debounce.
 *
 * The CoCo reads joysticks through a DAC comparator:
 *   - PIA1 DA bits 2-7 = DAC threshold (6-bit)
 *   - PIA0 CRA bit 3 (CA2 output) = axis select (0=X, 1=Y)
 *   - PIA0 CRB bit 3 (CB2 output) = port select (0=right, 1=left)
 *   - PIA0 PA bit 7 = comparator result (1 if joystick >= DAC)
 */

#include "hal.h"
#include "CoCoJoystick.h"
#include "../utils/debug.h"

static CoCoJoystick coco_joy;

void hal_joystick_init(void) {
    DEBUG_PRINT("  Joystick: CoCoJoystick init");
    coco_joy.begin();
    DEBUG_PRINTF("  Joystick 0: X=GPIO%d Y=GPIO%d BTN=GPIO%d",
                 PIN_JOY0_X, PIN_JOY0_Y, PIN_JOY0_BTN);
    DEBUG_PRINTF("  Joystick 1: X=GPIO%d Y=GPIO%d BTN=GPIO%d",
                 PIN_JOY1_X, PIN_JOY1_Y, PIN_JOY1_BTN);
    // Print raw ADC readings for diagnostics
    coco_joy.update();
    DEBUG_PRINTF("  JOY0 raw: X=%dmV Y=%dmV  scaled: X=%d Y=%d",
                 coco_joy.get_raw_mv(0, 0), coco_joy.get_raw_mv(0, 1),
                 coco_joy.get_x(0), coco_joy.get_y(0));
}

uint8_t hal_joystick_read_axis(int port, int axis) {
    if (axis == 0)
        return coco_joy.get_x(port);
    else
        return coco_joy.get_y(port);
}

uint8_t hal_joystick_read_button(int port) {
    return coco_joy.get_button(port) ? 1 : 0;
}

// Called by machine_read() before PIA0 port A read.
// Matches XRoar's joystick_update():
//   port = (PIA0 CRB bit 3) >> 3
//   axis = (PIA0 CRA bit 3) >> 3
//   dac  = (PIA1 DA & 0xFC) + 2
//   result on PIA0 PA bit 7
bool hal_joystick_compare(int port, int axis, uint8_t dac_value) {
    if (axis == 0)
        return coco_joy.compare_x(dac_value, port);
    else
        return coco_joy.compare_y(dac_value, port);
}

// Update joystick ADC readings - call once per frame
void hal_joystick_update(void) {
    coco_joy.update();
}
