/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : CoCoJoystick.cpp
 *  Module : CoCo joystick driver — dual analog ports via ESP32-S3 ADC with NVS calibration
 * ============================================================
 */

#include "CoCoJoystick.h"
#include <Preferences.h>

static Preferences prefs;

void CoCoJoystick::init_axis(AxisState& st, uint8_t pin) {
    st.pin = pin;
    st.raw_min = 150;       // Default minimum (mV)
    st.raw_max = 3050;      // Default maximum (mV)
    st.raw_center = 1650;   // Default center (mV) — midpoint of 3.3V
    st.sample_idx = 0;
    st.filtered_mv = 1650;
    st.scaled = 31;
    for (int i = 0; i < AVG_SAMPLES; i++) {
        st.samples[i] = 1650;
    }
}

void CoCoJoystick::init_button(ButtonState& btn, uint8_t pin) {
    btn.pin = pin;
    btn.pressed = false;
    btn.last_raw = false;
    btn.last_change_ms = 0;
}

void CoCoJoystick::begin() {
    // Set ADC attenuation for full 0–3.3V range
    analogSetAttenuation(ADC_11db);

    // Initialize axes
    init_axis(axis_x[0], JOY1_X_PIN);
    init_axis(axis_y[0], JOY1_Y_PIN);
    init_axis(axis_x[1], JOY2_X_PIN);
    init_axis(axis_y[1], JOY2_Y_PIN);

    // Initialize buttons
    init_button(button[0], JOY1_BTN_PIN);
    init_button(button[1], JOY2_BTN_PIN);
    pinMode(JOY1_BTN_PIN, INPUT_PULLUP);
    pinMode(JOY2_BTN_PIN, INPUT_PULLUP);

    calibrating = false;

    // Try to load saved calibration
    load_calibration();

    // Prime the moving average buffers with real readings
    for (int i = 0; i < AVG_SAMPLES; i++) {
        read_filtered(axis_x[0]);
        read_filtered(axis_y[0]);
        read_filtered(axis_x[1]);
        read_filtered(axis_y[1]);
    }
}

int CoCoJoystick::read_filtered(AxisState& st) {
    int mv = analogReadMilliVolts(st.pin);
    st.samples[st.sample_idx] = mv;
    st.sample_idx = (st.sample_idx + 1) % AVG_SAMPLES;

    int sum = 0;
    for (int i = 0; i < AVG_SAMPLES; i++) {
        sum += st.samples[i];
    }
    st.filtered_mv = sum / AVG_SAMPLES;
    return st.filtered_mv;
}

uint8_t CoCoJoystick::scale_axis(int raw_mv, const AxisState& st) {
    // Dead zone around center
    if (abs(raw_mv - st.raw_center) < DEAD_ZONE_MV) {
        return 31;
    }

    if (raw_mv <= st.raw_min) return 0;
    if (raw_mv >= st.raw_max) return 63;

    // Two-segment linear interpolation
    if (raw_mv < st.raw_center) {
        // Map [raw_min..raw_center] → [0..31]
        int range = st.raw_center - st.raw_min;
        if (range <= 0) return 0;
        return (int32_t)(raw_mv - st.raw_min) * 31 / range;
    } else {
        // Map [raw_center..raw_max] → [31..63]
        int range = st.raw_max - st.raw_center;
        if (range <= 0) return 63;
        return 31 + (int32_t)(raw_mv - st.raw_center) * 32 / range;
    }
}

void CoCoJoystick::debounce_button(ButtonState& btn) {
    bool raw = !digitalRead(btn.pin);  // Active LOW → invert
    if (raw != btn.last_raw) {
        btn.last_raw = raw;
        btn.last_change_ms = millis();
    }
    if ((millis() - btn.last_change_ms) >= BTN_DEBOUNCE_MS) {
        btn.pressed = btn.last_raw;
    }
}

void CoCoJoystick::update() {
    // Read and filter all axes
    int mv;

    mv = read_filtered(axis_x[0]);
    axis_x[0].scaled = scale_axis(mv, axis_x[0]);

    mv = read_filtered(axis_y[0]);
    axis_y[0].scaled = scale_axis(mv, axis_y[0]);

    mv = read_filtered(axis_x[1]);
    axis_x[1].scaled = scale_axis(mv, axis_x[1]);

    mv = read_filtered(axis_y[1]);
    axis_y[1].scaled = scale_axis(mv, axis_y[1]);

    // Debounce buttons
    debounce_button(button[0]);
    debounce_button(button[1]);

    // Update calibration min/max if in calibration mode
    if (calibrating) {
        calibrate_update();
    }
}

uint8_t CoCoJoystick::get_x(uint8_t port) {
    if (port > 1) return 31;
    return axis_x[port].scaled;
}

uint8_t CoCoJoystick::get_y(uint8_t port) {
    if (port > 1) return 31;
    return axis_y[port].scaled;
}

bool CoCoJoystick::get_button(uint8_t port) {
    if (port > 1) return false;
    return button[port].pressed;
}

bool CoCoJoystick::compare_x(uint8_t dac_threshold, uint8_t port) {
    return get_x(port) >= dac_threshold;
}

bool CoCoJoystick::compare_y(uint8_t dac_threshold, uint8_t port) {
    return get_y(port) >= dac_threshold;
}

int CoCoJoystick::get_raw_mv(uint8_t port, uint8_t axis) {
    if (port > 1) return 0;
    if (axis == 0) return axis_x[port].filtered_mv;
    return axis_y[port].filtered_mv;
}

// --- Calibration ---

void CoCoJoystick::calibrate_begin() {
    calibrating = true;
    // Reset min/max to inverse extremes so they get updated
    for (int p = 0; p < 2; p++) {
        axis_x[p].raw_min = 3300;
        axis_x[p].raw_max = 0;
        axis_y[p].raw_min = 3300;
        axis_y[p].raw_max = 0;
    }
    Serial.println("CALIBRATION: Move both sticks to all extremes...");
}

void CoCoJoystick::calibrate_update() {
    for (int p = 0; p < 2; p++) {
        int mx = axis_x[p].filtered_mv;
        int my = axis_y[p].filtered_mv;
        if (mx < axis_x[p].raw_min) axis_x[p].raw_min = mx;
        if (mx > axis_x[p].raw_max) axis_x[p].raw_max = mx;
        if (my < axis_y[p].raw_min) axis_y[p].raw_min = my;
        if (my > axis_y[p].raw_max) axis_y[p].raw_max = my;
    }
}

void CoCoJoystick::calibrate_end() {
    calibrating = false;
    // Record center as current position (user should release sticks)
    for (int p = 0; p < 2; p++) {
        axis_x[p].raw_center = axis_x[p].filtered_mv;
        axis_y[p].raw_center = axis_y[p].filtered_mv;
    }
    Serial.println("CALIBRATION complete. Release sticks to center.");
    print_calibration();
    save_calibration();
}

void CoCoJoystick::print_calibration() {
    for (int p = 0; p < 2; p++) {
        Serial.printf("JOY%d X: min=%d center=%d max=%d\n",
                      p, axis_x[p].raw_min, axis_x[p].raw_center, axis_x[p].raw_max);
        Serial.printf("JOY%d Y: min=%d center=%d max=%d\n",
                      p, axis_y[p].raw_min, axis_y[p].raw_center, axis_y[p].raw_max);
    }
}

void CoCoJoystick::save_calibration() {
    prefs.begin("joycal", false);
    for (int p = 0; p < 2; p++) {
        char key[16];
        snprintf(key, sizeof(key), "x%d_min", p); prefs.putInt(key, axis_x[p].raw_min);
        snprintf(key, sizeof(key), "x%d_max", p); prefs.putInt(key, axis_x[p].raw_max);
        snprintf(key, sizeof(key), "x%d_ctr", p); prefs.putInt(key, axis_x[p].raw_center);
        snprintf(key, sizeof(key), "y%d_min", p); prefs.putInt(key, axis_y[p].raw_min);
        snprintf(key, sizeof(key), "y%d_max", p); prefs.putInt(key, axis_y[p].raw_max);
        snprintf(key, sizeof(key), "y%d_ctr", p); prefs.putInt(key, axis_y[p].raw_center);
    }
    prefs.end();
    Serial.println("Calibration saved to NVS.");
}

void CoCoJoystick::load_calibration() {
    prefs.begin("joycal", true);
    // Only load if data exists (check for first key)
    if (!prefs.isKey("x0_min")) {
        prefs.end();
        Serial.println("No saved calibration found, using defaults.");
        return;
    }
    for (int p = 0; p < 2; p++) {
        char key[16];
        snprintf(key, sizeof(key), "x%d_min", p); axis_x[p].raw_min = prefs.getInt(key, 100);
        snprintf(key, sizeof(key), "x%d_max", p); axis_x[p].raw_max = prefs.getInt(key, 3200);
        snprintf(key, sizeof(key), "x%d_ctr", p); axis_x[p].raw_center = prefs.getInt(key, 1650);
        snprintf(key, sizeof(key), "y%d_min", p); axis_y[p].raw_min = prefs.getInt(key, 100);
        snprintf(key, sizeof(key), "y%d_max", p); axis_y[p].raw_max = prefs.getInt(key, 3200);
        snprintf(key, sizeof(key), "y%d_ctr", p); axis_y[p].raw_center = prefs.getInt(key, 1650);
    }
    prefs.end();
    Serial.println("Calibration loaded from NVS.");
}
