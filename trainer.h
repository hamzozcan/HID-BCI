#pragma once
#include <EEPROM.h>
#include "config.h"
#include "signal_processor.h"
#include "gesture_classifier.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Trainer — Serial-guided calibration wizard
//  Saves CalibData to EEPROM. Runs on first boot or when user requests recal.
// ─────────────────────────────────────────────────────────────────────────────
class Trainer {
public:
    // Returns true if EEPROM has valid calibration data
    static bool hasCalibration() {
        uint8_t magic = EEPROM.read(EEPROM_MAGIC_ADDR);
        uint8_t ver   = EEPROM.read(EEPROM_VER_ADDR);
        return (magic == EEPROM_MAGIC_VAL) && (ver == CAL_VERSION);
    }

    // Load calibration from EEPROM into CalibData + noise floors
    static void load(CalibData& cal, int16_t& h_noise, int16_t& v_noise) {
        EEPROM.get(EEPROM_H_BASE_ADDR,  cal.h_base);
        EEPROM.get(EEPROM_V_BASE_ADDR,  cal.v_base);
        EEPROM.get(EEPROM_H_LEFT_ADDR,  cal.h_left);
        EEPROM.get(EEPROM_H_RIGHT_ADDR, cal.h_right);
        EEPROM.get(EEPROM_V_UP_ADDR,    cal.v_up);
        EEPROM.get(EEPROM_V_DOWN_ADDR,  cal.v_down);
        EEPROM.get(EEPROM_BLINK_ADDR,   cal.blink);
        EEPROM.get(EEPROM_CLENCH_ADDR,  cal.clench);
        EEPROM.get(EEPROM_H_NOISE_ADDR, h_noise);
        EEPROM.get(EEPROM_V_NOISE_ADDR, v_noise);
    }

    // Save calibration to EEPROM
    static void save(const CalibData& cal, int16_t h_noise = 8, int16_t v_noise = 8) {
        EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VAL);
        EEPROM.write(EEPROM_VER_ADDR,   CAL_VERSION);
        EEPROM.put(EEPROM_H_BASE_ADDR,  cal.h_base);
        EEPROM.put(EEPROM_V_BASE_ADDR,  cal.v_base);
        EEPROM.put(EEPROM_H_LEFT_ADDR,  cal.h_left);
        EEPROM.put(EEPROM_H_RIGHT_ADDR, cal.h_right);
        EEPROM.put(EEPROM_V_UP_ADDR,    cal.v_up);
        EEPROM.put(EEPROM_V_DOWN_ADDR,  cal.v_down);
        EEPROM.put(EEPROM_BLINK_ADDR,   cal.blink);
        EEPROM.put(EEPROM_CLENCH_ADDR,  cal.clench);
        EEPROM.put(EEPROM_H_NOISE_ADDR, h_noise);
        EEPROM.put(EEPROM_V_NOISE_ADDR, v_noise);
    }

    // ─── Full calibration wizard (blocking; call from setup() or on demand) ───
    static void runWizard(SignalChannel& hCh, SignalChannel& vCh, CalibData& cal,
                          int16_t& h_noise_out, int16_t& v_noise_out) {
        Serial.println(F("\n╔═══════════════════════════════════════╗"));
        Serial.println(F("║   HID-BCI Calibration Wizard v2       ║"));
        Serial.println(F("╚═══════════════════════════════════════╝"));
        Serial.println(F("Attach the electrodes and press ENTER when ready."));
        _waitEnter();

        // ── Step 1: Baseline + Noise Floor ──────────────────────────────────
        Serial.println(F("\n[1/7] BASELINE  — Look straight ahead and relax."));
        Serial.println(F("      Measuring for 2 seconds..."));
        delay(500);
        int16_t hBase = _recordMean(PIN_H_EOG, CAL_BASELINE_MS);
        int16_t vBase = _recordMean(PIN_V_EOG, CAL_BASELINE_MS);
        cal.h_base = hBase;
        cal.v_base = vBase;
        hCh.setBaseline(hBase);
        vCh.setBaseline(vBase);

        // Measure the noise floor to estimate electrode contact quality
        int16_t hNoise = max(_recordNoise(PIN_H_EOG, hBase, 1000), (int16_t)4);
        int16_t vNoise = max(_recordNoise(PIN_V_EOG, vBase, 1000), (int16_t)4);
        hCh.noise_floor = hNoise;
        vCh.noise_floor = vNoise;
        h_noise_out = hNoise;
        v_noise_out = vNoise;

        Serial.print(F("  Baseline H=")); Serial.print(hBase);
        Serial.print(F("  V=")); Serial.print(vBase);
        Serial.print(F("  Noise H=")); Serial.print(hNoise);
        Serial.print(F("  V=")); Serial.println(vNoise);

        // ── Step 2: Gaze LEFT ─────────────────────────────────────────────────
        Serial.println(F("\n[2/7] LOOK LEFT — Press ENTER when ready, then look left."));
        _waitEnter();
        _recordingPrompt();
        int16_t hLeft = _recordPeak(PIN_H_EOG, hBase, false, CAL_GESTURE_MS);
        if (hLeft > -CAL_MIN_GAZE_H) hLeft = -DEFAULT_GAZE_H;
        cal.h_left = hLeft;
        Serial.print(F("  Left threshold: ")); Serial.println(hLeft);

        // ── Step 3: Gaze RIGHT ────────────────────────────────────────────────
        Serial.println(F("\n[3/7] LOOK RIGHT — Press ENTER, then look right."));
        _waitEnter();
        _recordingPrompt();
        int16_t hRight = _recordPeak(PIN_H_EOG, hBase, true, CAL_GESTURE_MS);
        if (hRight < CAL_MIN_GAZE_H) hRight = DEFAULT_GAZE_H;
        cal.h_right = hRight;
        Serial.print(F("  Right threshold: ")); Serial.println(hRight);

        // ── Step 4: Gaze UP ───────────────────────────────────────────────────
        Serial.println(F("\n[4/7] LOOK UP — Press ENTER, then look up."));
        _waitEnter();
        _recordingPrompt();
        int16_t vUp = _recordPeak(PIN_V_EOG, vBase, true, CAL_GESTURE_MS);
        if (vUp < CAL_MIN_GAZE_V) vUp = DEFAULT_GAZE_V;
        cal.v_up = vUp;
        Serial.print(F("  Up threshold: ")); Serial.println(vUp);

        // ── Step 5: Gaze DOWN ─────────────────────────────────────────────────
        Serial.println(F("\n[5/7] LOOK DOWN — Press ENTER, then look down."));
        _waitEnter();
        _recordingPrompt();
        int16_t vDown = _recordPeak(PIN_V_EOG, vBase, false, CAL_GESTURE_MS);
        if (vDown > -CAL_MIN_GAZE_V) vDown = -DEFAULT_GAZE_V;
        cal.v_down = vDown;
        Serial.print(F("  Down threshold: ")); Serial.println(vDown);

        // ── Step 6: Blink ─────────────────────────────────────────────────────
        Serial.println(F("\n[6/7] BLINK — Press ENTER, then blink 3 times."));
        _waitEnter();
        _recordingPrompt();
        int16_t blinkPeak = _recordPeak(PIN_V_EOG, vBase, true, CAL_GESTURE_MS);
        // Blink threshold = 60% of peak (conservative)
        cal.blink = max((int16_t)(blinkPeak * CAL_BLINK_RATIO), (int16_t)CAL_MIN_BLINK);
        Serial.print(F("  Blink threshold: ")); Serial.println(cal.blink);

        // ── Step 7: Jaw Clench ────────────────────────────────────────────────
        Serial.println(F("\n[7/7] CLENCH JAW — Press ENTER, then clench your jaw (1 s)."));
        _waitEnter();
        _recordingPrompt();
        int16_t clenchRMS = _recordRMS(PIN_H_EOG, hBase, CAL_GESTURE_MS);
        cal.clench = max((int16_t)(clenchRMS * CAL_CLENCH_RATIO), (int16_t)CAL_MIN_CLENCH);
        Serial.print(F("  Jaw threshold (RMS): ")); Serial.println(cal.clench);

        // ── Save ──────────────────────────────────────────────────────────────
        save(cal, h_noise_out, v_noise_out);
        Serial.println(F("\n✓ Calibration complete and saved to EEPROM."));
        Serial.println(F("  Mouse control active."));
        Serial.println(F("  Commands: R=calibration  B=baseline  Q=quality  D=debug\n"));
    }

    // Quick re-calibration: update baseline + noise floor
    static void recalBaseline(SignalChannel& hCh, SignalChannel& vCh, CalibData& cal) {
        Serial.println(F("\n[Quick Baseline] Look straight ahead..."));
        delay(1000);
        int16_t hBase = _recordMean(PIN_H_EOG, 1500);
        int16_t vBase = _recordMean(PIN_V_EOG, 1500);
        cal.h_base = hBase;
        cal.v_base = vBase;
        hCh.setBaseline(hBase);
        vCh.setBaseline(vBase);

        int16_t hNoise = max(_recordNoise(PIN_H_EOG, hBase, 800), (int16_t)4);
        int16_t vNoise = max(_recordNoise(PIN_V_EOG, vBase, 800), (int16_t)4);
        hCh.noise_floor = hNoise;
        vCh.noise_floor = vNoise;

        EEPROM.put(EEPROM_H_BASE_ADDR,  hBase);
        EEPROM.put(EEPROM_V_BASE_ADDR,  vBase);
        EEPROM.put(EEPROM_H_NOISE_ADDR, hNoise);
        EEPROM.put(EEPROM_V_NOISE_ADDR, vNoise);

        Serial.print(F("Baseline updated. H=")); Serial.print(hBase);
        Serial.print(F("  V=")); Serial.print(vBase);
        Serial.print(F("  Noise H=")); Serial.print(hNoise);
        Serial.print(F("  V=")); Serial.println(vNoise);
    }

private:
    static void _waitEnter() {
        while (Serial.available()) Serial.read();
        while (true) {
            if (Serial.available()) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') break;
            }
        }
    }

    static void _recordingPrompt() {
        Serial.println(F("  *** RECORDING STARTS ***"));
        for (uint8_t i = 3; i > 0; i--) {
            Serial.print(i); Serial.println(F("..."));
            delay(400);
        }
    }

    // Record mean of an ADC pin over duration_ms
    static int16_t _recordMean(uint8_t pin, uint16_t duration_ms) {
        uint32_t t0 = millis();
        int32_t  acc = 0;
        uint16_t cnt = 0;
        while (millis() - t0 < duration_ms) {
            acc += analogRead(pin);
            cnt++;
            delay(4); // ~250 Hz
        }
        return (int16_t)(acc / cnt);
    }

    // Record peak deviation from baseline (positive or negative)
    static int16_t _recordPeak(uint8_t pin, int16_t base, bool positive, uint16_t duration_ms) {
        uint32_t t0 = millis();
        int16_t  peak = 0;
        while (millis() - t0 < duration_ms) {
            int16_t val = analogRead(pin) - base;
            if (positive && val > peak)  peak = val;
            if (!positive && val < peak) peak = val;
            delay(4);
        }
        return peak;
    }

    // Noise floor: RMS around baseline while the signal is still
    static int16_t _recordNoise(uint8_t pin, int16_t base, uint16_t duration_ms) {
        uint32_t t0  = millis();
        int64_t  acc = 0;
        uint16_t cnt = 0;
        while (millis() - t0 < duration_ms) {
            int16_t v = analogRead(pin) - base;
            acc += (int32_t)v * v;
            cnt++;
            delay(4);
        }
        return (int16_t)sqrt((float)acc / cnt);
    }

    // Record RMS deviation from baseline
    static int16_t _recordRMS(uint8_t pin, int16_t base, uint16_t duration_ms) {
        uint32_t t0  = millis();
        int64_t  acc = 0;
        uint16_t cnt = 0;
        while (millis() - t0 < duration_ms) {
            int16_t v = analogRead(pin) - base;
            acc += (int32_t)v * v;
            cnt++;
            delay(4);
        }
        return (int16_t)sqrt((float)acc / cnt);
    }
};
