# HID-BCI — Göz & Kas ile Mouse Kontrolü

Arduino Micro + 2x AD8232 EKG/EMG modülü + 6 elektrot ile tam işlevli USB HID mouse.
Göz hareketleriyle imleç, göz kırpmayla tık, çene sıkmayla basılı tut.

---

## İçindekiler

1. [Gerekli Malzemeler](#1-gerekli-malzemeler)
2. [Elektrot Yerleşimi](#2-elektrot-yerleşimi)
3. [Devre Bağlantısı](#3-devre-bağlantısı)
4. [Arduino IDE Kurulumu](#4-arduino-ide-kurulumu)
5. [Kodu Yükleme](#5-kodu-yükleme)
6. [İlk Açılış ve Kalibrasyon](#6-i̇lk-açılış-ve-kalibrasyon)
7. [Gestur Haritası — Ne Yapınca Ne Olur](#7-gestur-haritası--ne-yapınca-ne-olur)
8. [Kalite Skoru Sistemi](#8-kalite-skoru-sistemi)
9. [Serial Komutlar](#9-serial-komutlar)
10. [LED Durumları](#10-led-durumları)
11. [İnce Ayar — config.h](#11-i̇nce-ayar--configh)
12. [Sorun Giderme](#12-sorun-giderme)
13. [Sistem Mimarisi](#13-sistem-mimarisi)

---

## 1. Gerekli Malzemeler

| Bileşen | Adet | Not |
|---------|------|-----|
| Arduino Micro (ATmega32U4) | 1 | **Pro Micro veya Leonardo da olur** — 32U4 şart |
| AD8232 EKG/EMG modülü | 2 | SparkFun veya klonu, 3.3V çıkışlı |
| Ag/AgCl yapışkan elektrot (10mm) | 6 | Tek kullanımlık EKG elektrotu |
| Snap kablo (3.5mm AUX uçlu veya doğrudan snap) | 6 | AD8232 modülüne göre seç |
| Elektrot jeli (iletken jel) | 1 kutu | **Kritik** — kuru elektrot gürültülü sinyal verir |
| Breadboard + jumper kablo | — | Bağlantı için |
| USB Micro kablo | 1 | Arduino Micro'yu bilgisayara bağlar |

> **Not:** Arduino Uno veya Nano **çalışmaz** — USB HID için ATmega32U4 gereklidir.

---

## 2. Elektrot Yerleşimi

### Kafaya yapıştırma şeması

```
                    ALIN (opsiyonel GND)
                          ●

     SOL ŞAKAK                     SAĞ ŞAKAK
         ●  ◄── AD8232 #1 IN+ ──►  ●
    (kaş hizası,                (kaş hizası,
     şakaklara yakın)            şakaklara yakın)


              ● ◄── AD8232 #2 IN+  (kaşın hemen altı)
              |
              👁  (sol göz)
              |
              ● ◄── AD8232 #2 IN−  (göz altı, elmacık kemiği üstü)


         KULAK MEMESİ
              ● ◄── GND (her iki AD8232 için ortak referans)
```

### Elektrotları nereye tam yapıştırırsın

| Elektrot | Konum | Modül Bağlantısı |
|----------|-------|-----------------|
| #1 | Sol şakak — kaş ile saç çizgisi arası, şakağın yumuşak kısmı | AD8232 #1 → IN+ |
| #2 | Sağ şakak — aynı hiza | AD8232 #1 → IN− |
| #3 | Sol kaşın hemen altı (kaş tüyü bitimi) | AD8232 #2 → IN+ |
| #4 | Sol göz altı — elmacık kemiğinin hemen üstü | AD8232 #2 → IN− |
| #5 | Sol kulak memesi (veya alın merkezi) | GND — her iki modül |
| #6 | (opsiyonel) Sağ kulak memesi | Ekstra GND kararlılığı için |

### Jel uygulama

1. Elektrotu yapıştırmadan önce cilde küçük bir miktar **elektrot jeli** sür
2. Elektrotu jel üstüne bastır ve 5–10 saniye tutarak yapıştır
3. Kablo hafifçe çekilince elektrot yerinden oynamamalı
4. Saç varsa elektrotu saçın altına değil **saça temas etmeden** deriye yapıştır

---

## 3. Devre Bağlantısı

### AD8232 #1 → Arduino Micro (Yatay EOG — şakaklar)

```
AD8232 #1          Arduino Micro
─────────          ─────────────
  OUTPUT    ──────►  A0
  LO+       ──────►  D4
  LO−       ──────►  D5
  3.3V      ──────►  3.3V
  GND       ──────►  GND
  SDN       ──────►  (bağlama — opsiyonel shutdown)
```

### AD8232 #2 → Arduino Micro (Dikey EOG — göz çevresi)

```
AD8232 #2          Arduino Micro
─────────          ─────────────
  OUTPUT    ──────►  A1
  LO+       ──────►  D6
  LO−       ──────►  D7
  3.3V      ──────►  3.3V
  GND       ──────►  GND
```

> **Önemli:** Her iki AD8232'nin GND pinleri Arduino'nun GND'sine bağlanmalı.
> 3.3V **kesinlikle** 3.3V'a bağla, 5V bağlarsan AD8232 bozulur.

### Breadboard Düzeni (Önerilen)

```
[Arduino Micro]
      |
      ├── 3.3V ──┬── AD8232 #1 (3.3V)
      │          └── AD8232 #2 (3.3V)
      ├── GND  ──┬── AD8232 #1 (GND)
      │          └── AD8232 #2 (GND)
      ├── A0  ──── AD8232 #1 OUTPUT
      ├── A1  ──── AD8232 #2 OUTPUT
      ├── D4  ──── AD8232 #1 LO+
      ├── D5  ──── AD8232 #1 LO−
      ├── D6  ──── AD8232 #2 LO+
      └── D7  ──── AD8232 #2 LO−
```

---

## 4. Arduino IDE Kurulumu

### 4.1 Arduino IDE İndir

Henüz yoksa: https://www.arduino.cc/en/software adresinden Arduino IDE 2.x indir ve kur.

### 4.2 Board Tanımla

1. Arduino IDE'yi aç
2. Üst menüden: `Tools → Board → Board Manager`
3. Arama kutusuna: `Arduino AVR` yaz
4. `Arduino AVR Boards` yanındaki `Install` butonuna tıkla
5. Kurulum bitince kapat

### 4.3 Board Seç

1. Arduino Micro'yu USB ile bilgisayara tak
2. `Tools → Board → Arduino AVR Boards → Arduino Micro` seç
3. `Tools → Port` menüsünde `/dev/ttyACM0` (Linux) veya `COM3` (Windows) gibi bir port görünmeli — onu seç

> **Linux kullanıcısı:** Port görünmüyorsa terminalde şunu çalıştır:
> ```bash
> sudo usermod -a -G dialout $USER
> ```
> Sonra oturumu kapat ve tekrar gir.

---

## 5. Kodu Yükleme

### 5.1 Klasörü Aç

1. Arduino IDE'de: `File → Open`
2. `HID-BCI` klasörüne git
3. `HID-BCI.ino` dosyasını seç ve aç

IDE otomatik olarak şu sekmeleri açacak:
```
HID-BCI.ino
config.h
signal_processor.h
gesture_classifier.h
mouse_controller.h
trainer.h
```

> Tüm bu dosyalar **aynı klasörde** olduğu sürece tek seferde derlenir — başka bir şey yapman gerekmez.

### 5.2 Derleme Kontrolü

Yüklemeden önce derlemeyi kontrol etmek istersen:
- `Sketch → Verify/Compile` (Ctrl+R)
- Alt kısımda `Done compiling` yazarsa her şey yolunda

### 5.3 Yükleme

1. `Sketch → Upload` (Ctrl+U) veya ok butonuna tıkla
2. Alt kısımda önce `Compiling...` sonra `Uploading...` yazacak
3. `Done uploading.` mesajı çıkınca başarılı

> **Dikkat:** Upload sırasında Arduino'yu çıkarma.

---

## 6. İlk Açılış ve Kalibrasyon

### Neden Kalibrasyon Gerekli?

Her kişinin göz hareketi ve kas sinyali farklıdır. Kalibrasyon sisteme senin özgün sinyal seviyelerini öğretir.

### Kalibrasyon Adımları

**Önce:** Serial Monitor'u aç
- Arduino IDE'de: `Tools → Serial Monitor` (Ctrl+Shift+M)
- Sağ alttaki baud hızını **115200** yap

Arduino'yu taktığında veya ilk açılışta ekranda şunu göreceksin:

```
╔══════════════════════════╗
║  HID-BCI v2.0  Hazır     ║
╚══════════════════════════╝
İlk açılış — kalibrasyon sihirbazı başlıyor.
```

---

**[1/6] BASELINE — Gözleri Ortalama**

```
[1/6] BASELINE  — Gözlerinize düz bakın, rahat olun.
      2 saniye boyunca ölçüm yapılıyor...
```

- Ekrana veya bir noktaya düz bak
- Hareket etme — sistem ölçüyor
- 2 saniye sonra otomatik geçer

---

**[2/6] SOLA BAK**

```
[2/6] SOLA BAK — Hazır olunca ENTER'a basın, sonra sola bakın.
```

- Hazır olunca Serial Monitor'daki mesaj kutusuna tıkla ve **Enter'a bas**
- Hemen sola bak ve 2 saniye öyle kal
- Gözleri çok uçta tutmana gerek yok — rahat bir sola bakış yeterli

---

**[3/6] SAĞA BAK**

- Enter'a bas → sağa bak → 2 saniye kal

---

**[4/6] YUKARI BAK**

- Enter'a bas → yukarı bak → 2 saniye kal

---

**[5/6] GÖZ KIRP**

```
[5/6] GÖZ KIRP — Enter'a basın, sonra 3 kez göz kırpın.
```

- Enter'a bas → normal hızda **3 kez göz kırp**
- İki gözünü birden kırpman yeterli, tek göz kapatman gerekmez
- Çok hızlı veya çok yavaş kırpma — normal tempo

---

**[6/6] ÇENE SIK**

```
[6/6] ÇENE SIK — Enter'a basın, sonra çenenizi sıkın (1 sn).
```

- Enter'a bas → dişlerini sık → 1 saniye kal → bırak
- Fazla kuvvet gerekmez, orta düzey bir sıkış yeterli

---

**Kalibrasyon Tamamlandı**

```
✓ Kalibrasyon tamamlandı ve EEPROM'a kaydedildi.
Mouse kontrolü aktif.
```

Kalibrasyon Arduino'nun dahili hafızasına (EEPROM) kaydedilir.
**Bir daha yapman gerekmez** — güç kesip açsan bile hatırlar.

---

## 7. Gestur Haritası — Ne Yapınca Ne Olur

### İmleç Hareketi

| Yaptığın | Sonuç |
|----------|-------|
| Sola bak | İmleç sola kayar |
| Sağa bak | İmleç sağa kayar |
| Yukarı bak | İmleç yukarı kayar |
| Aşağı bak | İmleç aşağı kayar |

> Bakış açısı ne kadar uçtaysa imleç o kadar hızlı gider.
> Orta pozisyona döndüğünde imleç durur.

### Tıklama

| Yaptığın | Sonuç |
|----------|-------|
| **1 kez göz kırp** | Sol tık |
| **Hızlı 2 kez göz kırp** (350ms içinde) | Sağ tık |
| **Dişleri kısa sık** (150ms – 600ms) | Orta tık |

> Tek ve çift kırpma farkı: çift kırpmada iki kırp arasındaki süre 350ms'den az olmalı.
> Yavaş kırparsan sistem iki ayrı sol tık olarak değerlendirir.

### Basılı Tutma ve Sürükleme

| Yaptığın | Sonuç |
|----------|-------|
| Dişleri **1 saniyeden fazla** sık | Sol tık **basılı kalır** |
| Basılı tutarken gözle istediğin yere git | Sürükleme |
| Dişleri **bırak** | Sol tık serbest bırakılır |

> Dosya sürükleme örneği:
> 1. İmleci dosyanın üstüne getir
> 2. Dişleri sık ve 1 sn bekle (tık basılı)
> 3. Gözle hedef klasöre git
> 4. Dişleri bırak (bırak)

### Scroll

| Yaptığın | Sonuç |
|----------|-------|
| Yukarı bak + **600ms tut** | Sayfa yukarı kaydırır |
| Aşağı bak + **600ms tut** | Sayfa aşağı kaydırır |
| Gözü orta konuma getir | Scroll durur |

---

## 8. Kalite Skoru Sistemi

Sistem sürekli olarak elektrot temas kalitesini ölçer ve her **5 saniyede bir** rapor verir:

```
─── Sinyal Kalitesi ───
H (Yatay) : ★★★★☆ (4/5)  noise=12  floor=8  dz=48
V (Dikey) : ★★★★★ (5/5)  noise=7   floor=6  dz=40
Genel: İyi — normal kullanım
───────────────────────
```

| Skor | Anlam | Ne Yapmalı |
|------|-------|------------|
| ★★★★★ | Mükemmel | Hiçbir şey — sistem tam verimde |
| ★★★★☆ | İyi | Normal kullanım |
| ★★★☆☆ | Orta | Biraz daha jel ekle, elektrotu hafifçe bastır |
| ★★☆☆☆ | Zayıf | `B` komutu ile baseline yenile, elektrotu kontrol et |
| ★☆☆☆☆ | Çok Zayıf | Elektrotu çıkar, yeniden jel sür, tekrar yapıştır |
| ✗ (0) | Kopuk | Kablo veya elektrot tamamen ayrılmış |

### Adaptive Deadzone

Kalite düştükçe sistem otomatik olarak **deadzone**'u artırır:
- Kalite iyi → dar deadzone → hassas hareket
- Kalite kötü → geniş deadzone → titreme bastırılır

Bu sayede sinyal gürültülenince imleç kendiliğinden titremez.

---

## 9. Serial Komutlar

Serial Monitor açıkken (115200 baud) mesaj kutusuna harf yazıp Enter'a basınca:

| Komut | Ne Yapar |
|-------|----------|
| `R` | Tam kalibrasyon sihirbazını başlatır (tüm adımlar tekrar) |
| `B` | Sadece baseline'ı günceller — elektrot konumu değişince kullan |
| `Q` | Anlık kalite skoru gösterir |
| `D` | Debug modunu aç/kapat — H_EOG, V_EOG, EMG değerlerini 20Hz'de yazar |

### Ne Zaman `B` Kullanmalısın?

- Elektrotu çıkarıp yeniden yapıştırdıktan sonra
- Uzun süre kullanıp oturumun başına döndükten sonra
- İmleç bir yönde sürekli sürükleniyorsa (baseline kayması)

### Ne Zaman `R` Kullanmalısın?

- Kalibrasyonu tamamen sıfırlamak istersen
- Çok farklı bir kişi kullanacaksa
- Gesturlar hiç algılanmıyorsa

---

## 10. LED Durumları

| LED Davranışı | Anlam |
|--------------|-------|
| 3 kısa yanıp söner (açılışta) | Sistem hazır, kalibrasyon yüklendi |
| Hızlı yanıp söner (100ms aralık) | Elektrot kopuk — bağlantıyı kontrol et |
| 1 kez kısa yanıp söner | Sol tık gerçekleşti |
| 2 kez kısa yanıp söner | Sağ tık gerçekleşti |
| 3 kez kısa yanıp söner | Orta tık gerçekleşti |
| Sabit yanar | Sol tık basılı tutuluyor (sürükleme modu) |
| Sönük | Normal çalışma, imleç hareket ediyor |

---

## 11. İnce Ayar — config.h

Sistemin davranışını değiştirmek için `config.h` dosyasını Arduino IDE'de düzenle.

### İmleç Hızı ve Hassasiyeti

```cpp
#define MOUSE_MAX_SPEED    18   // Maksimum piksel/güncelleme — azalt: daha yavaş
#define MOUSE_DEADZONE     40   // Ölü bölge — artır: titreme azalır, hassasiyet düşer
#define MOUSE_ACCEL_FACTOR 0.04 // İvme katsayısı — artır: uç bakışta daha hızlı
#define MOUSE_UPDATE_MS    16   // Güncelleme aralığı — azalt: daha akıcı (~60Hz)
```

### Göz Kırpma Algılama

```cpp
#define BLINK_MIN_MS       40   // Bu süreden kısa → gürültü sayılır, tık olmaz
#define BLINK_MAX_MS       250  // Bu süreden uzun → göz kırpma değil, uzatılmış kapama
#define DOUBLE_BLINK_GAP_MS 350 // İki kırpma arasında bu kadar vakit varsa çift kırpma
```

### Scroll Hassasiyeti

```cpp
#define SCROLL_HOLD_MS     600  // Kaç ms yukarı/aşağı bakınca scroll başlar
#define SCROLL_SPEED       3    // Her tetiklemede kaç tick scroll
```

### Çene Sıkma

```cpp
#define CLENCH_MIN_MS      150  // Bu süreden kısa çene sıkma → yok sayılır
// 150–600ms → orta tık
// >600ms    → sol tık basılı tut
```

---

## 12. Sorun Giderme

### İmleç Sürekli Bir Yöne Kayıyor

**Neden:** Baseline kayması — elektrot uzun süre takılı kalınca referans noktası kayar.
**Çözüm:** Serial'e `B` yaz, baseline'ı yenile.

### Göz Kırpma Algılanmıyor

**Neden 1:** Blink eşiği çok yüksek kalibre edilmiş.
**Çözüm:** `R` ile yeniden kalibrasyon yap, [5/6] adımında daha net kırp.

**Neden 2:** Dikey elektrot yanlış konumda.
**Çözüm:** Göz üstü elektrotu kaşın hemen altına, göz altı elektrotu elmacık kemiği üstüne taşı.

### Çift Kırpma Yerine Hep Sol Tık

**Neden:** İki kırpma arasındaki süre 350ms'yi aşıyor.
**Çözüm:** Biraz daha hızlı çift kırp. Ya da `config.h`'de `DOUBLE_BLINK_GAP_MS` değerini 450'ye çıkar.

### İmleç Çok Titriyor

**Neden:** Sinyal gürültüsü yüksek — büyük ihtimalle jel azalmış veya elektrot gevşemiş.
**Çözüm 1:** `Q` yaz, kalite skoruna bak. 3'ün altındaysa elektrotu yenile.
**Çözüm 2:** `config.h`'de `MA_WINDOW` değerini 10'dan 14'e çıkar (daha fazla yumuşatma).

### LED Sürekli Hızlı Yanıp Sönüyor

**Neden:** Elektrot kopuk (lead-off algılandı) — sistem güvenlik için mouse'u durdurdu.
**Çözüm:** Kabloların AD8232'ye tam takılı olduğunu kontrol et. Elektrotu hafifçe bastır.

### Port Görünmüyor (Linux)

```bash
sudo usermod -a -G dialout $USER
# Çıkış yap ve tekrar gir
```

### "Mouse.h not found" Derleme Hatası

Board seçimi yanlış. `Tools → Board → Arduino Micro` olduğundan emin ol.
Arduino Uno/Nano seçiliyse `Mouse.h` desteklenmez.

### Yükleme Sırasında "avrdude: stk500v2_getsync()" Hatası

1. Arduino'yu çıkar, yeniden tak
2. `Tools → Port` menüsünden doğru portu seç
3. Hâlâ olmuyorsa Arduino Micro'da reset butonuna bas, hemen ardından Upload yap

---

## 13. Sistem Mimarisi

### Dosya Yapısı

```
HID-BCI/
├── HID-BCI.ino            Ana döngü, Serial komutlar, zamanlama
├── config.h               Tüm sabitler, pin tanımları, EEPROM haritası
├── signal_processor.h     IIR LP/HP filtreler, EMG envelope, kalite skoru
├── gesture_classifier.h   Gesture state machine + adaptive deadzone
├── mouse_controller.h     USB HID Mouse wrapper
└── trainer.h              Kalibrasyon sihirbazı + EEPROM kayıt/yükleme
```

### Veri Akışı

```
                    AD8232 #1 (A0)              AD8232 #2 (A1)
                  Sol/Sağ Şakak              Göz Üstü/Altı
                         │                         │
                         ▼                         ▼
              SignalChannel hChannel    SignalChannel vChannel
                 ├─ MA filtre (10 örnek)    ├─ MA filtre
                 ├─ LP Butterworth @10Hz    ├─ LP Butterworth
                 ├─ HP filtre (EMG)         └─ Kalite skoru
                 └─ Kalite skoru (0-5)
                         │                         │
                         └──────────┬──────────────┘
                                    ▼
                    GestureClassifier::update()
                      ├─ Adaptive deadzone (kaliteye göre)
                      ├─ Blink state machine → CLICK_L / CLICK_R
                      ├─ Clench detector    → CLICK_M / HOLD_L
                      ├─ Scroll timer       → SCROLL_UP / DOWN
                      └─ Proportional EOG   → MOVE (dx, dy)
                                    │
                                    ▼
                    MouseController::handle()
                      └─ USB HID Mouse (Arduino Micro native)
                                    │
                                    ▼
                           💻 Bilgisayar
                    (normal mouse gibi görünür)
```

### EEPROM Haritası

| Adres | Boyut | İçerik |
|-------|-------|--------|
| 0 | 1 byte | Magic byte (0xBC) — kalibrasyon var mı? |
| 1 | 1 byte | Kalibrasyon versiyonu |
| 2–3 | int16 | Yatay baseline |
| 4–5 | int16 | Dikey baseline |
| 6–7 | int16 | Sol bakış eşiği |
| 8–9 | int16 | Sağ bakış eşiği |
| 10–11 | int16 | Yukarı bakış eşiği |
| 12–13 | int16 | (ayrılmış) |
| 14–15 | int16 | Göz kırpma eşiği |
| 16–17 | int16 | Çene sıkma eşiği |
| 20–21 | int16 | Yatay kanal gürültü tabanı |
| 22–23 | int16 | Dikey kanal gürültü tabanı |

---

## Hızlı Başlangıç Özeti

```
1. Bağla    → AD8232 x2 → Arduino Micro (pin tablosuna göre)
2. Yapıştır → 6 elektrotu jelle kafana yapıştır
3. Tak      → USB ile bilgisayara bağla
4. Aç       → Arduino IDE → HID-BCI.ino → Upload
5. Kalibrasyon → Serial Monitor (115200) → 6 adımı takip et
6. Kullan   → Kalibrasyondan sonra normal mouse gibi çalışır
```
