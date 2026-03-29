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

    // ─── Full calibration wizard (blocks — call from setup() or on demand) ────
    static void runWizard(SignalChannel& hCh, SignalChannel& vCh, CalibData& cal,
                          int16_t& h_noise_out, int16_t& v_noise_out) {
        Serial.println(F("\n╔═══════════════════════════════════════╗"));
        Serial.println(F("║   HID-BCI Kalibrasyon Sihirbazı v1    ║"));
        Serial.println(F("╚═══════════════════════════════════════╝"));
        Serial.println(F("Elektrotları takın ve hazır olunca ENTER'a basın."));
        _waitEnter();

        // ── Step 1: Baseline + Gürültü Tabanı ────────────────────────────────
        Serial.println(F("\n[1/6] BASELINE  — Gözlerinize düz bakın, rahat olun."));
        Serial.println(F("      2 saniye boyunca ölçüm yapılıyor..."));
        delay(500);
        int16_t hBase = _recordMean(PIN_H_EOG, CAL_BASELINE_MS);
        int16_t vBase = _recordMean(PIN_V_EOG, CAL_BASELINE_MS);
        cal.h_base = hBase;
        cal.v_base = vBase;
        hCh.setBaseline(hBase);
        vCh.setBaseline(vBase);

        // Gürültü tabanını ölç (elektrot teması kalitesini referans al)
        int16_t hNoise = max(_recordNoise(PIN_H_EOG, hBase, 1000), (int16_t)4);
        int16_t vNoise = max(_recordNoise(PIN_V_EOG, vBase, 1000), (int16_t)4);
        hCh.noise_floor = hNoise;
        vCh.noise_floor = vNoise;
        h_noise_out = hNoise;
        v_noise_out = vNoise;

        Serial.print(F("  Baseline H=")); Serial.print(hBase);
        Serial.print(F("  V=")); Serial.print(vBase);
        Serial.print(F("  Gürültü H=")); Serial.print(hNoise);
        Serial.print(F("  V=")); Serial.println(vNoise);

        // ── Step 2: Gaze LEFT ─────────────────────────────────────────────────
        Serial.println(F("\n[2/6] SOLA BAK — Hazır olunca ENTER'a basın, sonra sola bakın."));
        _waitEnter();
        _recordingPrompt();
        int16_t hLeft = _recordPeak(PIN_H_EOG, hBase, false, CAL_GESTURE_MS);
        cal.h_left = hLeft;
        Serial.print(F("  Sol eşiği: ")); Serial.println(hLeft);

        // ── Step 3: Gaze RIGHT ────────────────────────────────────────────────
        Serial.println(F("\n[3/6] SAĞA BAK — ENTER'a basın, sonra sağa bakın."));
        _waitEnter();
        _recordingPrompt();
        int16_t hRight = _recordPeak(PIN_H_EOG, hBase, true, CAL_GESTURE_MS);
        cal.h_right = hRight;
        Serial.print(F("  Sağ eşiği: ")); Serial.println(hRight);

        // ── Step 4: Gaze UP ───────────────────────────────────────────────────
        Serial.println(F("\n[4/6] YUKARI BAK — ENTER'a basın, sonra yukarı bakın."));
        _waitEnter();
        _recordingPrompt();
        int16_t vUp = _recordPeak(PIN_V_EOG, vBase, true, CAL_GESTURE_MS);
        cal.v_up = vUp;
        Serial.print(F("  Yukarı eşiği: ")); Serial.println(vUp);

        // ── Step 5: Blink ─────────────────────────────────────────────────────
        Serial.println(F("\n[5/6] GÖZ KIRP — ENTER'a basın, sonra 3 kez göz kırpın."));
        _waitEnter();
        _recordingPrompt();
        int16_t blinkPeak = _recordPeak(PIN_V_EOG, vBase, true, CAL_GESTURE_MS);
        // Blink threshold = 60% of peak (conservative)
        cal.blink = (int16_t)(blinkPeak * 0.6f);
        if (cal.blink < 40) cal.blink = 40;
        Serial.print(F("  Kırpma eşiği: ")); Serial.println(cal.blink);

        // ── Step 6: Jaw Clench ────────────────────────────────────────────────
        Serial.println(F("\n[6/6] ÇENE SIK — ENTER'a basın, sonra çenenizi sıkın (1 sn)."));
        _waitEnter();
        _recordingPrompt();
        int16_t clenchRMS = _recordRMS(PIN_H_EOG, hBase, CAL_GESTURE_MS);
        cal.clench = max(clenchRMS / 2, (int16_t)20);
        Serial.print(F("  Çene eşiği (RMS): ")); Serial.println(cal.clench);

        // ── Save ──────────────────────────────────────────────────────────────
        save(cal, h_noise_out, v_noise_out);
        Serial.println(F("\n✓ Kalibrasyon tamamlandı ve EEPROM'a kaydedildi."));
        Serial.println(F("  Mouse kontrolü aktif."));
        Serial.println(F("  Komutlar: R=kalibrasyon  B=baseline  Q=kalite skoru  D=debug\n"));
    }

    // Quick re-calibration: baseline + gürültü tabanı güncelle
    static void recalBaseline(SignalChannel& hCh, SignalChannel& vCh, CalibData& cal) {
        Serial.println(F("\n[Hızlı Baseline] Düz bakın..."));
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

        Serial.print(F("Baseline güncellendi. H=")); Serial.print(hBase);
        Serial.print(F("  V=")); Serial.print(vBase);
        Serial.print(F("  Gürültü H=")); Serial.print(hNoise);
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
        Serial.println(F("  *** KAYIT BAŞLIYOR ***"));
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

    // Gürültü tabanı: baseline etrafındaki RMS (sinyal hareketsizken)
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
