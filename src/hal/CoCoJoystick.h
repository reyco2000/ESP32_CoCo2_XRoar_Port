/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : CoCoJoystick.h
 *  Module : CoCo joystick interface — dual 6-bit ADC axes with DAC comparator emulation for PIA
 * ============================================================
 */

#ifndef COCO_JOYSTICK_H
#define COCO_JOYSTICK_H

#include <Arduino.h>

// Pin definitions from config.h (PIN_JOY0_X, PIN_JOY0_Y, etc.)
#include "../../config.h"

#define JOY1_X_PIN    PIN_JOY0_X
#define JOY1_Y_PIN    PIN_JOY0_Y
#define JOY1_BTN_PIN  PIN_JOY0_BTN

#define JOY2_X_PIN    PIN_JOY1_X
#define JOY2_Y_PIN    PIN_JOY1_Y
#define JOY2_BTN_PIN  PIN_JOY1_BTN

// Debounce hold time (ms)
#define BTN_DEBOUNCE_MS  20

// Dead zone in millivolts around center
#define DEAD_ZONE_MV     50

// Number of samples for moving average
#define AVG_SAMPLES      4

class CoCoJoystick {
public:
    void begin();
    void update();

    // 6-bit axis values (0–63)
    uint8_t get_x(uint8_t port = 0);
    uint8_t get_y(uint8_t port = 0);
    bool    get_button(uint8_t port = 0);

    // DAC comparator emulation (used by PIA emulation)
    // Returns true if axis_value >= dac_threshold
    bool compare_x(uint8_t dac_threshold, uint8_t port = 0);
    bool compare_y(uint8_t dac_threshold, uint8_t port = 0);

    // Calibration
    void calibrate_begin();
    void calibrate_update();
    void calibrate_end();
    void save_calibration();
    void load_calibration();

    // Debug: get raw millivolt reading for an axis
    int get_raw_mv(uint8_t port, uint8_t axis); // axis: 0=X, 1=Y

    // Print calibration values to Serial
    void print_calibration();

private:
    struct AxisState {
        uint8_t pin;
        int raw_min;        // Calibrated minimum (mV)
        int raw_max;        // Calibrated maximum (mV)
        int raw_center;     // Calibrated center (mV)
        int samples[AVG_SAMPLES];
        uint8_t sample_idx;
        int filtered_mv;    // Last filtered reading (mV)
        uint8_t scaled;     // 0–63
    };

    struct ButtonState {
        uint8_t pin;
        bool pressed;           // Debounced state (true = pressed)
        bool last_raw;          // Last raw reading
        uint32_t last_change_ms;
    };

    AxisState axis_x[2];
    AxisState axis_y[2];
    ButtonState button[2];
    bool calibrating;

    uint8_t scale_axis(int raw_mv, const AxisState& st);
    int  read_filtered(AxisState& st);
    void debounce_button(ButtonState& btn);
    void init_axis(AxisState& st, uint8_t pin);
    void init_button(ButtonState& btn, uint8_t pin);
};

#endif // COCO_JOYSTICK_H
