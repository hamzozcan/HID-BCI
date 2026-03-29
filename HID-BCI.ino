// ─────────────────────────────────────────────────────────────────────────────
//  HID-BCI  —  EOG/EMG Mouse Controller  v2.0
//  Hardware: Arduino Micro (ATmega32U4) + 2x AD8232
//
//  Elektrot Yerleşimi:
//    AD8232 #1 (Horizontal EOG):
//      IN+  → Sol şakak
//      IN−  → Sağ şakak
//      REF  → Kulak memesi veya alın merkezi (GND)
//
//    AD8232 #2 (Vertical EOG + Blink):
//      IN+  → Sol göz üstü (kaşın altı)
//      IN−  → Sol göz altı (elmacık kemeri üstü)
//      REF  → Aynı REF elektrotu (AD8232 #1 ile paylaşılabilir)
//
//  Gesturlar:
//    • Sola/sağa/yukarı/aşağı bak → orantılı + ivmeli imleç hareketi
//    • Tek göz kırp → sol tık
//    • Çift göz kırp (< 350ms ara) → sağ tık
//    • Kısa çene sık (150-600ms) → orta tık
//    • Uzun çene sık (> 600ms) → sol tık basılı tut / bırak
//    • Yukarı/aşağı bakışı 600ms tut → scroll
//
//  Serial Komutları (115200 baud):
//    R → Tam yeniden kalibrasyon
//    B → Sadece baseline yenile (elektrot konumu değişince)
//    Q → Anlık kalite skoru göster
//    D → Debug çıktısı aç/kapat
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
    Serial.println(F("║  HID-BCI v2.0  Hazır     ║"));
    Serial.println(F("╚══════════════════════════╝"));

    int16_t hNoise = 8, vNoise = 8;

    // ── Kalibrasyon yükle veya sihirbazı başlat ───────────────────────────────
    if (!Trainer::hasCalibration()) {
        Serial.println(F("İlk açılış — kalibrasyon sihirbazı başlıyor."));
        mouse.enabled = false;
        Trainer::runWizard(hChannel, vChannel, classifier.cal, hNoise, vNoise);
        mouse.enabled = true;
    } else {
        Trainer::load(classifier.cal, hNoise, vNoise);
        hChannel.setBaseline(classifier.cal.h_base);
        vChannel.setBaseline(classifier.cal.v_base);
        hChannel.noise_floor = hNoise;
        vChannel.noise_floor = vNoise;
        Serial.println(F("Kalibrasyon EEPROM'dan yüklendi."));
        _printCalib();
    }

    mouse.begin();

    Serial.println(F("Mouse HID aktif."));
    Serial.println(F("Komutlar: R=kalibrasyon  B=baseline  Q=kalite  D=debug\n"));

    // Hazır sinyali: 3 kısa yanıp sönme
    for (uint8_t i = 0; i < 3; i++) {
        digitalWrite(PIN_LED, HIGH); delay(80);
        digitalWrite(PIN_LED, LOW);  delay(80);
    }

    _last_sample_us  = micros();
    _last_quality_ms = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // ── 1. Serial komutları ───────────────────────────────────────────────────
    if (Serial.available()) {
        char c = toupper(Serial.read());
        while (Serial.available()) Serial.read();

        if (c == 'R') {
            Serial.println(F("Tam yeniden kalibrasyon başlatılıyor..."));
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

    // ── 2. Örnekleme: 250 Hz ──────────────────────────────────────────────────
    uint32_t now_us = micros();
    if ((uint32_t)(now_us - _last_sample_us) >= SAMPLE_INTERVAL_US) {
        _last_sample_us = now_us;

        bool hLoP = digitalRead(PIN_H_LO_P);
        bool hLoM = digitalRead(PIN_H_LO_M);
        bool vLoP = digitalRead(PIN_V_LO_P);
        bool vLoM = digitalRead(PIN_V_LO_M);

        hChannel.update(analogRead(PIN_H_EOG), hLoP, hLoM);
        vChannel.update(analogRead(PIN_V_EOG), vLoP, vLoM);

        classifier.setLeadOff(hChannel.lead_off || vChannel.lead_off);

        if (_debug) _printDebug();
    }

    // ── 3. Gesture + Mouse: ~60 Hz ────────────────────────────────────────────
    uint32_t now_ms = millis();
    if ((uint32_t)(now_ms - _last_mouse_ms) >= MOUSE_UPDATE_MS) {
        _last_mouse_ms = now_ms;

        // Adaptive deadzone: sinyalin o anki kalitesine göre ayarla
        uint8_t dz = max(hChannel.deadzone, vChannel.deadzone);
        classifier.setDeadzone(dz);

        Gesture g = classifier.update(hChannel.eog, vChannel.eog, hChannel.emg_env, now_ms);
        mouse.handle(g, classifier.dx, classifier.dy, now_ms);

        // LED: elektrot kopuksa hızlı yanıp söner
        if (hChannel.lead_off || vChannel.lead_off) {
            digitalWrite(PIN_LED, (now_ms / 100) & 1);
        }
    }

    // ── 4. Otomatik kalite raporu: her 5 saniyede bir ─────────────────────────
    if ((uint32_t)(now_ms - _last_quality_ms) >= QUALITY_PRINT_MS) {
        _last_quality_ms = now_ms;
        _printQuality();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void _printCalib() {
    CalibData& c = classifier.cal;
    Serial.println(F("--- Kalibrasyon ---"));
    Serial.print(F("  H base=")); Serial.print(c.h_base);
    Serial.print(F("  V base=")); Serial.println(c.v_base);
    Serial.print(F("  left="));  Serial.print(c.h_left);
    Serial.print(F("  right=")); Serial.print(c.h_right);
    Serial.print(F("  up="));    Serial.print(c.v_up);
    Serial.print(F("  blink=")); Serial.print(c.blink);
    Serial.print(F("  clench="));Serial.println(c.clench);
    Serial.println(F("-------------------"));
}

void _printQuality() {
    Serial.println(F("─── Sinyal Kalitesi ───"));
    hChannel.printQuality("H (Yatay)");
    vChannel.printQuality("V (Dikey) ");
    uint8_t worst = min(hChannel.quality, vChannel.quality);
    const __FlashStringHelper* msg;
    switch (worst) {
        case 5:  msg = F("Mükemmel — sistem tam verimde"); break;
        case 4:  msg = F("İyi — normal kullanım"); break;
        case 3:  msg = F("Orta — jel ekle veya elektrotu düzelt"); break;
        case 2:  msg = F("Zayıf — 'B' ile baseline yenile önerilir"); break;
        case 1:  msg = F("Çok Zayıf — elektrot temasını kontrol et!"); break;
        default: msg = F("KOPUK — elektrot bağlantısı yok!"); break;
    }
    Serial.print(F("Genel: ")); Serial.println(msg);
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
    if (hChannel.lead_off || vChannel.lead_off) Serial.print(F(" [KOPUK]"));
    Serial.println();
}
