// ─────────────────────────────────────────────────────────────────────────────
//  HID-BCI  —  EOG/EMG Mouse Controller  v2.0
//  Hardware: Arduino Micro (ATmega32U4) + 2x AD8232
//
//  Electrode Placement:
//    AD8232 #1 (Horizontal EOG):
//      IN+  → Left temple
//      IN−  → Right temple
//      REF  → Ear lobe or center forehead (GND)
//
//    AD8232 #2 (Vertical EOG + Blink):
//      IN+  → Above left eye (below eyebrow)
//      IN−  → Below left eye (upper cheekbone)
//      REF  → Same REF electrode (can be shared with AD8232 #1)
//
//  Gestures:
//    • Look left/right/up/down → proportional + accelerated cursor movement
//    • Single blink → left click
//    • Double blink (< 350 ms gap) → right click
//    • Short jaw clench (150-600 ms) → middle click
//    • Long jaw clench (> 600 ms) → hold / release left click
//    • Hold upward/downward gaze for 600 ms → scroll
//
//  Serial Commands (115200 baud):
//    R → Full recalibration
//    B → Refresh baseline only (after electrode repositioning)
//    Q → Show current quality score
//    D → Toggle debug output
// ─────────────────────────────────────────────────────────────────────────────

#include <Mouse.h>
#include <EEPROM.h>
#include "config.h"
#include "signal_processor.h"
#include "gesture_classifier.h"
#include "mouse_controller.h"
#include "trainer.h"

// ─── Global Objects ───────────────────────────────────────────────────────────
SignalChannel      hChannel;
SignalChannel      vChannel;
GestureClassifier  classifier;
MouseController    mouse;

// ─── Timing ───────────────────────────────────────────────────────────────────
uint32_t _last_sample_us  = 0;
uint32_t _last_mouse_ms   = 0;
uint32_t _last_quality_ms = 0;

// ─── Debug ────────────────────────────────────────────────────────────────────
bool     _debug           = false;
uint32_t _debug_print_ms  = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    pinMode(PIN_LED,    OUTPUT);
    pinMode(PIN_H_LO_P, INPUT);
    pinMode(PIN_H_LO_M, INPUT);
    pinMode(PIN_V_LO_P, INPUT);
    pinMode(PIN_V_LO_M, INPUT);

    Serial.begin(115200);
    delay(1500);

    Serial.println(F("\n╔══════════════════════════╗"));
    Serial.println(F("║  HID-BCI v2.0  Ready     ║"));
    Serial.println(F("╚══════════════════════════╝"));

    int16_t hNoise = 8, vNoise = 8;

    // ── Load calibration or start the wizard ────────────────────────────────
    if (!Trainer::hasCalibration()) {
        Serial.println(F("First boot - calibration wizard starting."));
        mouse.enabled = false;
        Trainer::runWizard(hChannel, vChannel, classifier.cal, hNoise, vNoise);
        mouse.enabled = true;
    } else {
        Trainer::load(classifier.cal, hNoise, vNoise);
        hChannel.setBaseline(classifier.cal.h_base);
        vChannel.setBaseline(classifier.cal.v_base);
        hChannel.noise_floor = hNoise;
        vChannel.noise_floor = vNoise;
        Serial.println(F("Calibration loaded from EEPROM."));
        _printCalib();
    }

    mouse.begin();

    Serial.println(F("Mouse HID active."));
    Serial.println(F("Commands: R=calibration  B=baseline  Q=quality  D=debug\n"));

    // Ready signal: 3 short blinks
    for (uint8_t i = 0; i < 3; i++) {
        digitalWrite(PIN_LED, HIGH); delay(80);
        digitalWrite(PIN_LED, LOW);  delay(80);
    }

    _last_sample_us  = micros();
    _last_quality_ms = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // ── 1. Serial commands ───────────────────────────────────────────────────
    if (Serial.available()) {
        char c = toupper(Serial.read());
        while (Serial.available()) Serial.read();

        if (c == 'R') {
            Serial.println(F("Starting full recalibration..."));
            mouse.releaseAll();
            mouse.enabled = false;
            int16_t hN = 8, vN = 8;
            Trainer::runWizard(hChannel, vChannel, classifier.cal, hN, vN);
            mouse.enabled = true;

        } else if (c == 'B') {
            mouse.enabled = false;
            Trainer::recalBaseline(hChannel, vChannel, classifier.cal);
            mouse.enabled = true;

        } else if (c == 'Q') {
            _printQuality();

        } else if (c == 'D') {
            _debug = !_debug;
            Serial.print(F("Debug: ")); Serial.println(_debug ? F("ON") : F("OFF"));
        }
    }

    // ── 2. Sampling: 250 Hz ──────────────────────────────────────────────────
    uint32_t now_us = micros();
    if ((uint32_t)(now_us - _last_sample_us) >= SAMPLE_INTERVAL_US) {
        _last_sample_us = now_us;

        bool hLoP = digitalRead(PIN_H_LO_P);
        bool hLoM = digitalRead(PIN_H_LO_M);
        bool vLoP = digitalRead(PIN_V_LO_P);
        bool vLoM = digitalRead(PIN_V_LO_M);

        hChannel.update(analogRead(PIN_H_EOG), hLoP, hLoM);
        vChannel.update(analogRead(PIN_V_EOG), vLoP, vLoM);

        bool leadOff = hChannel.lead_off || vChannel.lead_off;
        classifier.setLeadOff(leadOff);
        mouse.setLeadOff(leadOff);

        if (_debug) _printDebug();
    }

    // ── 3. Gesture + Mouse: ~60 Hz ───────────────────────────────────────────
    uint32_t now_ms = millis();
    if ((uint32_t)(now_ms - _last_mouse_ms) >= MOUSE_UPDATE_MS) {
        _last_mouse_ms = now_ms;

        // Adaptive deadzone: adjust based on current signal quality
        uint8_t dz = max(hChannel.deadzone, vChannel.deadzone);
        classifier.setDeadzone(dz);

        Gesture g = classifier.update(hChannel.eog, vChannel.eog, hChannel.emg_env, now_ms);
        mouse.handle(g, classifier.dx, classifier.dy, now_ms);

    }

    // ── 4. Automatic quality report every 5 seconds ─────────────────────────
    if ((uint32_t)(now_ms - _last_quality_ms) >= QUALITY_PRINT_MS) {
        _last_quality_ms = now_ms;
        _printQuality();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void _printCalib() {
    CalibData& c = classifier.cal;
    Serial.println(F("--- Calibration ---"));
    Serial.print(F("  H base=")); Serial.print(c.h_base);
    Serial.print(F("  V base=")); Serial.println(c.v_base);
    Serial.print(F("  left="));  Serial.print(c.h_left);
    Serial.print(F("  right=")); Serial.print(c.h_right);
    Serial.print(F("  up="));    Serial.print(c.v_up);
    Serial.print(F("  down="));  Serial.print(c.v_down);
    Serial.print(F("  blink=")); Serial.print(c.blink);
    Serial.print(F("  clench="));Serial.println(c.clench);
    Serial.println(F("-------------------"));
}

void _printQuality() {
    Serial.println(F("─── Signal Quality ───"));
    hChannel.printQuality("H (Horizontal)");
    vChannel.printQuality("V (Vertical) ");
    uint8_t worst = min(hChannel.quality, vChannel.quality);
    const __FlashStringHelper* msg;
    switch (worst) {
        case 5:  msg = F("Excellent - full performance"); break;
        case 4:  msg = F("Good - normal operation"); break;
        case 3:  msg = F("Fair - add gel or adjust the electrode"); break;
        case 2:  msg = F("Weak - baseline refresh with 'B' is recommended"); break;
        case 1:  msg = F("Very Weak - check electrode contact"); break;
        default: msg = F("LEAD OFF - no electrode connection"); break;
    }
    Serial.print(F("Overall: ")); Serial.println(msg);
    Serial.println(F("───────────────────────"));
}

void _printDebug() {
    uint32_t now_ms = millis();
    if ((uint32_t)(now_ms - _debug_print_ms) < 50) return;
    _debug_print_ms = now_ms;

    Serial.print(F("H=")); Serial.print((int)hChannel.eog);
    Serial.print(F(" V=")); Serial.print((int)vChannel.eog);
    Serial.print(F(" EMG=")); Serial.print((int)hChannel.emg_env);
    Serial.print(F(" Q=")); Serial.print(hChannel.quality);
    Serial.print(F("/"));   Serial.print(vChannel.quality);
    Serial.print(F(" dz=")); Serial.print(hChannel.deadzone);
    if (hChannel.lead_off || vChannel.lead_off) Serial.print(F(" [LEAD OFF]"));
    Serial.println();
}
