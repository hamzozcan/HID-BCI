#pragma once
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
//  SignalChannel — processes one AD8232 analog channel in real-time
//  Provides: LP-filtered EOG, HP-filtered EMG envelope, rolling quality score
// ─────────────────────────────────────────────────────────────────────────────
class SignalChannel {
public:
    int16_t  baseline    = 512;  // calibrated neutral ADC value
    float    eog         = 0;    // LP-filtered EOG relative to baseline (signed)
    float    emg_env     = 0;    // HP-filtered EMG envelope (always positive)
    float    raw_hp      = 0;    // HP filtered raw value
    bool     lead_off    = false;

    // Kalite sistemi
    uint8_t  quality     = 0;    // 0=kopuk, 1=çok kötü … 5=mükemmel
    float    noise_rms   = 0;    // rolling noise RMS (ADC counts)
    int16_t  noise_floor = 8;    // kalibrasyon sırasında ölçülen beklenen gürültü
    uint8_t  deadzone    = DEADZONE_Q5; // adaptive deadzone (kaliteye göre)

    SignalChannel() {
        for (uint8_t i = 0; i < MA_WINDOW; i++) _ma_buf[i] = 512;
        for (uint8_t i = 0; i < QUALITY_WINDOW; i++) _q_buf[i] = 0;
    }

    // 250 Hz'de çağrılır. lo_p/lo_m: lead-off detect pinleri
    void update(int16_t adc_raw, bool lo_p, bool lo_m) {
        lead_off = lo_p || lo_m;
        if (lead_off) { quality = 0; deadzone = DEADZONE_Q1; _reset(); return; }

        // ── Moving-average ön-filtresi ──────────────────────────────────────
        _ma_buf[_ma_idx] = adc_raw;
        _ma_idx = (_ma_idx + 1) % MA_WINDOW;
        float ma = 0;
        for (uint8_t i = 0; i < MA_WINDOW; i++) ma += _ma_buf[i];
        float smoothed = ma / MA_WINDOW;

        // ── 1st-order Butterworth LP (EOG kanalı) ──────────────────────────
        float centered = smoothed - baseline;
        _lp_prev = LP_ALPHA_EOG * centered + (1.0f - LP_ALPHA_EOG) * _lp_prev;
        eog = _lp_prev;

        // ── High-pass filtresi (EMG: temporal kas / çene) ──────────────────
        raw_hp   = HP_ALPHA_EMG * (raw_hp + smoothed - _hp_prev);
        _hp_prev = smoothed;

        // ── EMG envelope (yarı dalga doğrulama + LP) ───────────────────────
        emg_env  = 0.1f * fabsf(raw_hp) + 0.9f * emg_env;

        // ── Rolling noise RMS (ham sinyal - baseline, QUALITY_WINDOW örnekli) ──
        float diff = adc_raw - baseline;
        _q_sum_sq -= (float)_q_buf[_q_idx] * _q_buf[_q_idx];
        _q_buf[_q_idx] = (int16_t)diff;
        _q_sum_sq += diff * diff;
        _q_idx = (_q_idx + 1) % QUALITY_WINDOW;
        noise_rms = sqrtf(_q_sum_sq / QUALITY_WINDOW);

        // ── Kalite skoru hesapla ────────────────────────────────────────────
        _updateQuality();
    }

    void setBaseline(int16_t val) {
        baseline  = val;
        _lp_prev  = 0;
        raw_hp    = 0;
        _hp_prev  = val;
        emg_env   = 0;
        eog       = 0;
        _q_sum_sq = 0;
        for (uint8_t i = 0; i < QUALITY_WINDOW; i++) _q_buf[i] = 0;
    }

    // Kalibrasyon sırasında gürültü tabanını kaydet (baseline aşamasında çağrılır)
    void calibrateNoiseFloor() {
        // noise_rms şu anki rolling değeri — bu kalibrasyondan hemen sonra çağrılmalı
        noise_floor = max((int16_t)noise_rms, (int16_t)4);
    }

    // Kalite skoru metnini döner ("★★★★☆ (4/5) - İyi" formatı)
    void printQuality(const char* label) const {
        Serial.print(label);
        Serial.print(F(": "));
        for (uint8_t i = 0; i < 5; i++)
            Serial.print(i < quality ? F("★") : F("☆"));
        Serial.print(F(" ("));
        Serial.print(quality);
        Serial.print(F("/5)  noise="));
        Serial.print((int)noise_rms);
        Serial.print(F("  floor="));
        Serial.print(noise_floor);
        Serial.print(F("  dz="));
        Serial.print(deadzone);
        if (lead_off) Serial.print(F("  [KOPUK!]"));
        Serial.println();
    }

    // Static yardımcılar (Trainer tarafından kullanılır)
    static float computeRMS(int16_t* buf, uint16_t len, int16_t base) {
        float acc = 0;
        for (uint16_t i = 0; i < len; i++) {
            float v = buf[i] - base;
            acc += v * v;
        }
        return sqrtf(acc / len);
    }

    static float computeMean(int16_t* buf, uint16_t len) {
        float acc = 0;
        for (uint16_t i = 0; i < len; i++) acc += buf[i];
        return acc / len;
    }

    static int16_t computePeak(int16_t* buf, uint16_t len, int16_t base, bool positive) {
        int16_t peak = 0;
        for (uint16_t i = 0; i < len; i++) {
            int16_t d = buf[i] - base;
            if (positive && d > peak)  peak = d;
            if (!positive && d < peak) peak = d;
        }
        return peak;
    }

private:
    // ── Filtre durumları ────────────────────────────────────────────────────
    float   _lp_prev  = 0;
    float   _hp_prev  = 512;
    int16_t _ma_buf[MA_WINDOW]      = {};
    uint8_t _ma_idx                 = 0;

    // ── Kalite: rolling RMS penceresi ───────────────────────────────────────
    int16_t _q_buf[QUALITY_WINDOW]  = {};
    uint8_t _q_idx                  = 0;
    float   _q_sum_sq               = 0;

    void _reset() {
        _lp_prev  = 0;
        raw_hp    = 0;
        _hp_prev  = baseline;
        emg_env   = 0;
        eog       = 0;
        noise_rms = 999;
    }

    void _updateQuality() {
        // Gürültü / beklenen gürültü oranına göre skor
        float ratio = noise_rms / max((float)noise_floor, 1.0f);

        if      (ratio < 1.5f)  { quality = 5; deadzone = DEADZONE_Q5; }
        else if (ratio < 2.5f)  { quality = 4; deadzone = DEADZONE_Q4; }
        else if (ratio < 4.5f)  { quality = 3; deadzone = DEADZONE_Q3; }
        else if (ratio < 8.0f)  { quality = 2; deadzone = DEADZONE_Q2; }
        else                    { quality = 1; deadzone = DEADZONE_Q1; }
    }
};
