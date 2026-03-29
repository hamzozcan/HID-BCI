#pragma once
#include <Arduino.h>

// ─── EEPROM Layout ────────────────────────────────────────────────────────────
#define EEPROM_MAGIC_VAL      0xBC
#define EEPROM_MAGIC_ADDR     0
#define EEPROM_H_BASE_ADDR    2   // int16_t horizontal baseline (ADC)
#define EEPROM_V_BASE_ADDR    4   // int16_t vertical baseline
#define EEPROM_H_LEFT_ADDR    6   // int16_t left-gaze threshold (negative)
#define EEPROM_H_RIGHT_ADDR   8   // int16_t right-gaze threshold (positive)
#define EEPROM_V_UP_ADDR      10  // int16_t up-gaze threshold (positive)
#define EEPROM_V_DOWN_ADDR    12  // int16_t down-gaze threshold (negative)
#define EEPROM_BLINK_ADDR     14  // int16_t blink peak threshold (positive)
#define EEPROM_CLENCH_ADDR    16  // int16_t jaw-clench RMS threshold
#define EEPROM_VER_ADDR       18  // uint8_t calibration version

// ─── Pin Definitions ──────────────────────────────────────────────────────────
// AD8232 #1 → Horizontal EOG  (Left temple = IN+, Right temple = IN-)
#define PIN_H_EOG    A0
#define PIN_H_LO_P   4     // Lead-off detect +
#define PIN_H_LO_M   5     // Lead-off detect –

// AD8232 #2 → Vertical EOG   (Above eye = IN+, Below eye = IN-)
#define PIN_V_EOG    A1
#define PIN_V_LO_P   6
#define PIN_V_LO_M   7

// Optional: status LED
#define PIN_LED      LED_BUILTIN

// ─── Sampling ─────────────────────────────────────────────────────────────────
#define SAMPLE_RATE_HZ        250
#define SAMPLE_INTERVAL_US    (1000000UL / SAMPLE_RATE_HZ)

// ─── Signal Filters ───────────────────────────────────────────────────────────
// EOG low-pass: ~10 Hz cutoff (alpha = 1 / (1 + 2π·fc/fs))
#define LP_ALPHA_EOG          0.239f   // Butterworth 1st-order @ 10Hz / 250Hz
// High-pass for EMG (jaw clench): removes baseline drift
#define HP_ALPHA_EMG          0.95f
// Moving-average window for smooth cursor (10 = daha pürüzsüz)
#define MA_WINDOW             10

// ─── Gesture Thresholds (overridden by calibration) ──────────────────────────
// These are fallback defaults in ADC units (0-1023 range, baseline ~512)
#define DEFAULT_GAZE_H        80    // ±ADC counts from baseline
#define DEFAULT_GAZE_V        70
#define DEFAULT_BLINK         180   // blink causes large V spike
#define DEFAULT_CLENCH        60    // RMS above baseline

// ─── Gesture Timing ───────────────────────────────────────────────────────────
#define BLINK_MIN_MS          40    // too short = noise
#define BLINK_MAX_MS          250   // too long = sustained squint
#define DOUBLE_BLINK_GAP_MS   350   // max gap between two blinks
#define CLENCH_MIN_MS         150   // minimum jaw clench duration
#define DEBOUNCE_MS           80    // gesture lockout after click
#define SCROLL_HOLD_MS        600   // hold gaze up/down this long → scroll mode

// ─── Mouse Movement ───────────────────────────────────────────────────────────
#define MOUSE_DEADZONE        40    // ADC counts below which cursor doesn't move
#define MOUSE_MAX_SPEED       18    // max pixels per update cycle
#define MOUSE_ACCEL_FACTOR    0.04f // non-linear acceleration
#define MOUSE_UPDATE_MS       16    // ~60 Hz cursor update
#define SCROLL_SPEED          3     // scroll ticks per trigger

// ─── Calibration ──────────────────────────────────────────────────────────────
#define CAL_BASELINE_MS       2000  // baseline recording duration
#define CAL_GESTURE_MS        2000  // per-gesture recording duration
#define CAL_SAMPLES           (SAMPLE_RATE_HZ * CAL_GESTURE_MS / 1000)
#define EEPROM_H_NOISE_ADDR   20   // int16_t H channel noise floor (std dev)
#define EEPROM_V_NOISE_ADDR   22   // int16_t V channel noise floor

// ─── Sinyal Kalitesi ──────────────────────────────────────────────────────────
// Kalite skoru 0-5 (0=elektrot kopuk, 5=mükemmel)
#define QUALITY_WINDOW        64   // rolling variance penceresi (örnekler)
#define QUALITY_PRINT_MS      5000 // kaç ms'de bir kalite skoru yazdır

// Adaptive deadzone: kalite düştükçe deadzone artar (titreme bastırma)
#define DEADZONE_Q5           40   // mükemmel
#define DEADZONE_Q4           48
#define DEADZONE_Q3           58
#define DEADZONE_Q2           70
#define DEADZONE_Q1           90   // çok kötü sinyal

// ─── Misc ─────────────────────────────────────────────────────────────────────
#define CAL_VERSION           0x04  // increment when EEPROM layout changes
