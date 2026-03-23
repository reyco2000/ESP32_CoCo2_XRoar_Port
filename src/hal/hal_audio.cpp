/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : hal_audio.cpp
 *  Module : Audio HAL — LEDC PWM pseudo-DAC at 22050 Hz for single-bit and 6-bit CoCo audio
 * ============================================================
 */

/*
 * hal_audio.cpp - CoCo audio output via LEDC PWM on ESP32-S3
 *
 * ESP32-S3 has NO internal DAC — uses LEDC PWM at 78 kHz with
 * 8-bit resolution for pseudo-analog audio output.
 * Timer ISR at 22050 Hz outputs current sample level to PWM.
 *
 * Two audio paths (matching real CoCo hardware):
 *   1. Single-bit: PIA1 port B bit 1 (cassette, simple beeps)
 *   2. 6-bit DAC:  PIA1 port A bits 2-7 (SOUND command, music)
 *
 * Based on CoCo_Audio_Test/CoCoAudio implementation.
 */

#include "hal.h"
#include "../utils/debug.h"
#include "soc/ledc_struct.h"

// Audio output pin (LEDC PWM)
#define AUDIO_DAC_PIN       17
#define AUDIO_LEDC_FREQ     78125    // 78.125 kHz PWM
#define AUDIO_LEDC_BITS     8        // 8-bit resolution (0-255)
#define AUDIO_ISR_RATE      22050    // Sample rate

// LEDC channel
static volatile uint8_t audio_ledc_channel = 0;

// Current audio output level (0-255), set by either audio path
static volatile uint8_t audio_current_level = 128;  // midpoint = silence

static hw_timer_t* audio_timer = nullptr;

// ISR — outputs current level to PWM
static void IRAM_ATTR audio_timer_isr() {
    uint8_t sample = audio_current_level;
    LEDC.channel_group[0].channel[audio_ledc_channel].duty.duty = sample << 4;
    LEDC.channel_group[0].channel[audio_ledc_channel].conf0.low_speed_update = 1;
    LEDC.channel_group[0].channel[audio_ledc_channel].conf1.duty_start = 1;
}

void hal_audio_init(void) {
    // Initialize LEDC PWM
    audio_ledc_channel = 0;
    ledcAttachChannel(AUDIO_DAC_PIN, AUDIO_LEDC_FREQ, AUDIO_LEDC_BITS, audio_ledc_channel);
    ledcWrite(AUDIO_DAC_PIN, 128);  // Start at mid-point (silence)

    // Initialize timer ISR at sample rate
    audio_timer = timerBegin(1000000);  // 1 MHz tick
    timerAttachInterrupt(audio_timer, audio_timer_isr);
    timerAlarm(audio_timer, 1000000 / AUDIO_ISR_RATE, true, 0);

    audio_current_level = 128;

    DEBUG_PRINTF("  Audio: LEDC PWM on GPIO%d, %d Hz ISR", AUDIO_DAC_PIN, AUDIO_ISR_RATE);
}

void hal_audio_write_sample(int16_t left, int16_t right) {
    (void)left;
    (void)right;
}

void hal_audio_set_volume(uint8_t volume) {
    DEBUG_PRINTF("  Audio: volume set to %d", volume);
}

// Single-bit audio: PIA1 port B bit 1
void hal_audio_write_bit(bool value) {
    audio_current_level = value ? 255 : 0;
}

// 6-bit DAC audio: PIA1 port A bits 2-7 (value 0-63)
void hal_audio_write_dac(uint8_t dac6) {
    // Scale 6-bit (0-63) to 8-bit (0-255)
    audio_current_level = (dac6 << 2) | (dac6 >> 4);
}
