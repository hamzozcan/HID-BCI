# HID-BCI - Eye and Muscle Controlled Mouse

Arduino Micro + 2x AD8232 ECG/EMG modules + 6 electrodes for a full USB HID mouse.
Move the cursor with eye motion, click with blinks, and hold/drag with a jaw clench.

---

## Table of Contents

1. [Required Parts](#1-required-parts)
2. [Electrode Placement](#2-electrode-placement)
3. [Wiring](#3-wiring)
4. [Arduino IDE Setup](#4-arduino-ide-setup)
5. [Uploading the Sketch](#5-uploading-the-sketch)
6. [First Boot and Calibration](#6-first-boot-and-calibration)
7. [Gesture Map](#7-gesture-map)
8. [Signal Quality System](#8-signal-quality-system)
9. [Serial Commands](#9-serial-commands)
10. [LED States](#10-led-states)
11. [Fine Tuning with config.h](#11-fine-tuning-with-configh)
12. [Troubleshooting](#12-troubleshooting)
13. [System Architecture](#13-system-architecture)

---

## 1. Required Parts

| Component | Qty | Notes |
|-----------|-----|-------|
| Arduino Micro (ATmega32U4) | 1 | A Pro Micro or Leonardo also works; 32U4 is required |
| AD8232 ECG/EMG module | 2 | SparkFun or clone, 3.3V output |
| Ag/AgCl adhesive electrodes (10 mm) | 6 | Disposable ECG electrodes |
| Snap leads / electrode cables | 6 | Choose the right type for your AD8232 board |
| Electrode gel / conductive gel | 1 | Critical for low-noise contact |
| Breadboard + jumper wires | - | For wiring |
| USB Micro cable | 1 | Connects the Arduino Micro to the computer |

> Note: Arduino Uno and Nano will not work for this project. Native USB HID requires an ATmega32U4-based board.

---

## 2. Electrode Placement

### Head placement diagram

```text
                  FOREHEAD (optional GND)
                        o

   LEFT TEMPLE                     RIGHT TEMPLE
       o  <-- AD8232 #1 IN+ -->    o
  (near eyebrow line)         (same height)


            o <-- AD8232 #2 IN+  (just below eyebrow)
            |
            O  (left eye)
            |
            o <-- AD8232 #2 IN-  (upper cheekbone)


          EAR LOBE
            o <-- GND (shared reference for both AD8232 modules)
```

### Exact electrode positions

| Electrode | Position | Module connection |
|-----------|----------|-------------------|
| #1 | Left temple, between eyebrow and hairline | AD8232 #1 -> IN+ |
| #2 | Right temple, same height | AD8232 #1 -> IN- |
| #3 | Just below the left eyebrow | AD8232 #2 -> IN+ |
| #4 | Below the left eye, above the cheekbone | AD8232 #2 -> IN- |
| #5 | Left ear lobe or center forehead | GND for both modules |
| #6 | Optional right ear lobe | Extra reference stability |

### Gel application

1. Apply a small amount of conductive gel before placing each electrode.
2. Press the electrode onto the skin for 5-10 seconds.
3. The cable should not pull the electrode loose.
4. Do not place electrodes on top of hair; place them directly on skin.

---

## 3. Wiring

### AD8232 #1 -> Arduino Micro (horizontal EOG / temples)

```text
AD8232 #1          Arduino Micro
---------          -------------
OUTPUT      ----->  A0
LO+         ----->  D4
LO-         ----->  D5
3.3V        ----->  3.3V
GND         ----->  GND
SDN         ----->  leave unconnected (optional shutdown)
```

### AD8232 #2 -> Arduino Micro (vertical EOG / blink channel)

```text
AD8232 #2          Arduino Micro
---------          -------------
OUTPUT      ----->  A1
LO+         ----->  D6
LO-         ----->  D7
3.3V        ----->  3.3V
GND         ----->  GND
```

> Important:
> Both AD8232 GND pins must be tied to Arduino GND.
> The AD8232 must be powered from 3.3V, not 5V.

### Suggested breadboard layout

```text
[Arduino Micro]
      |
      +-- 3.3V --+-- AD8232 #1 (3.3V)
      |          +-- AD8232 #2 (3.3V)
      +-- GND ---+-- AD8232 #1 (GND)
      |          +-- AD8232 #2 (GND)
      +-- A0  ------ AD8232 #1 OUTPUT
      +-- A1  ------ AD8232 #2 OUTPUT
      +-- D4  ------ AD8232 #1 LO+
      +-- D5  ------ AD8232 #1 LO-
      +-- D6  ------ AD8232 #2 LO+
      +-- D7  ------ AD8232 #2 LO-
```

---

## 4. Arduino IDE Setup

### 4.1 Install Arduino IDE

If you do not already have it, download Arduino IDE 2.x from:
https://www.arduino.cc/en/software

### 4.2 Install board support

1. Open Arduino IDE.
2. Go to `Tools -> Board -> Board Manager`.
3. Search for `Arduino AVR`.
4. Install `Arduino AVR Boards`.

### 4.3 Select the board and port

1. Connect the Arduino Micro by USB.
2. Select `Tools -> Board -> Arduino AVR Boards -> Arduino Micro`.
3. Select the correct serial port under `Tools -> Port`.

On Linux, if the port does not appear:

```bash
sudo usermod -a -G dialout $USER
```

Then log out and log back in.

---

## 5. Uploading the Sketch

### 5.1 Open the folder

1. In Arduino IDE, choose `File -> Open`.
2. Open the `HID-BCI` folder.
3. Select `HID-BCI.ino`.

The IDE should open these files as tabs:

```text
HID-BCI.ino
config.h
signal_processor.h
gesture_classifier.h
mouse_controller.h
trainer.h
```

As long as they stay in the same folder, they compile together.

### 5.2 Verify

Before uploading, you can run:

- `Sketch -> Verify/Compile` (`Ctrl+R`)

If the output ends with `Done compiling`, the sketch built successfully.

### 5.3 Upload

1. Click `Sketch -> Upload` (`Ctrl+U`) or the upload arrow.
2. Wait for `Compiling...` and then `Uploading...`.
3. If you see `Done uploading.`, the board is ready.

> Do not disconnect the Arduino during upload.

---

## 6. First Boot and Calibration

### Why calibration matters

Eye and muscle signals vary from person to person. Calibration teaches the system your baseline, gaze thresholds, blink strength, jaw-clench level, and noise floor.

### Open Serial Monitor

Before calibration:

- Open `Tools -> Serial Monitor` (`Ctrl+Shift+M`)
- Set the baud rate to `115200`

On first boot you will see:

```text
╔══════════════════════════╗
║  HID-BCI v2.0  Ready     ║
╚══════════════════════════╝
First boot - calibration wizard starting.
```

### Step-by-step calibration

The current firmware uses a 7-step calibration flow so both upward and downward gaze are measured explicitly.

**[1/7] BASELINE**

```text
[1/7] BASELINE  — Look straight ahead and relax.
      Measuring for 2 seconds...
```

- Look straight at the screen or a fixed point.
- Stay relaxed and still.
- The device records your neutral baseline automatically.

**[2/7] LOOK LEFT**

```text
[2/7] LOOK LEFT — Press ENTER when ready, then look left.
```

- Press Enter in Serial Monitor.
- Look left and hold for about 2 seconds.
- A comfortable gaze is enough; do not strain.

**[3/7] LOOK RIGHT**

- Press Enter.
- Look right and hold for about 2 seconds.

**[4/7] LOOK UP**

- Press Enter.
- Look up and hold for about 2 seconds.

**[5/7] LOOK DOWN**

```text
[5/7] LOOK DOWN — Press ENTER, then look down.
```

- Press Enter.
- Look down and hold for about 2 seconds.

**[6/7] BLINK**

```text
[6/7] BLINK — Press ENTER, then blink 3 times.
```

- Press Enter.
- Blink normally three times.
- A natural blink speed works best.

**[7/7] CLENCH JAW**

```text
[7/7] CLENCH JAW — Press ENTER, then clench your jaw (1 s).
```

- Press Enter.
- Clench your teeth or jaw for about 1 second.
- Medium force is enough.

### When calibration finishes

```text
✓ Calibration complete and saved to EEPROM.
Mouse control active.
```

Calibration is stored in EEPROM, so the board remembers it across power cycles.

---

## 7. Gesture Map

### Cursor movement

| Action | Result |
|--------|--------|
| Look left | Cursor moves left |
| Look right | Cursor moves right |
| Look up | Cursor moves up |
| Look down | Cursor moves down |

The farther you look, the faster the cursor moves. Returning to center stops movement.

### Click actions

| Action | Result |
|--------|--------|
| Single blink | Left click |
| Fast double blink (within 350 ms) | Right click |
| Short jaw clench (150-600 ms) | Middle click |

If the two blinks are too far apart, the system interprets them as two separate single blinks.

### Hold and drag

| Action | Result |
|--------|--------|
| Jaw clench longer than 600 ms | Hold left mouse button |
| Move gaze while holding | Drag |
| Release jaw clench | Release left mouse button |

### Scroll

| Action | Result |
|--------|--------|
| Hold upward gaze for 600 ms | Scroll up |
| Hold downward gaze for 600 ms | Scroll down |
| Return gaze to center | Stop scrolling |

---

## 8. Signal Quality System

The firmware continuously estimates electrode contact quality and prints a report every 5 seconds:

```text
─── Signal Quality ───
H (Horizontal): ★★★★☆ (4/5)  noise=12  floor=8  dz=48
V (Vertical) : ★★★★★ (5/5)  noise=7  floor=6  dz=40
Overall: Good - normal operation
───────────────────────
```

| Score | Meaning | What to do |
|-------|---------|------------|
| ★★★★★ | Excellent | Nothing; the system is in ideal condition |
| ★★★★☆ | Good | Normal use |
| ★★★☆☆ | Fair | Add a little more gel or press electrodes more firmly |
| ★★☆☆☆ | Weak | Run `B` to refresh baseline and inspect electrode contact |
| ★☆☆☆☆ | Very weak | Reapply gel and replace the electrode |
| 0 / Lead off | Disconnected | Cable or electrode is no longer connected |

### Adaptive deadzone

As quality drops, the firmware increases the cursor deadzone automatically:

- Good quality -> smaller deadzone -> more precise movement
- Poor quality -> larger deadzone -> less jitter

This keeps the cursor from vibrating when the signal gets noisy.

---

## 9. Serial Commands

With Serial Monitor open at `115200`, type a letter and press Enter:

| Command | Function |
|---------|----------|
| `R` | Start the full calibration wizard again |
| `B` | Refresh baseline and noise floor only |
| `Q` | Print the current signal quality |
| `D` | Toggle debug output (`H`, `V`, `EMG`, quality, deadzone) |

### When to use `B`

- After removing and reapplying electrodes
- After a long session if the cursor starts drifting
- If the neutral position has shifted

### When to use `R`

- When a different person will use the headset
- When gesture detection feels consistently wrong
- When you want to rebuild all thresholds from scratch

---

## 10. LED States

| LED behavior | Meaning |
|--------------|---------|
| 3 short blinks on startup | System ready |
| Fast blinking (100 ms interval) | Lead-off detected; check electrodes |
| 1 short blink | Left click |
| 2 short blinks | Right click |
| 3 short blinks | Middle click |
| Solid on | Left button is being held for drag mode |
| Off | Normal operation |

LED feedback is non-blocking, so click indication no longer stalls cursor updates.

---

## 11. Fine Tuning with `config.h`

Edit `config.h` if you want to change sensitivity or timing.

### Cursor speed and sensitivity

```cpp
#define MOUSE_MAX_SPEED    18   // Max pixels per update; lower = slower
#define MOUSE_DEADZONE     40   // Higher = less jitter, lower sensitivity
#define MOUSE_ACCEL_FACTOR 0.04 // Higher = faster at extreme gaze angles
#define MOUSE_UPDATE_MS    16   // Lower = smoother updates (~60 Hz)
```

### Blink detection

```cpp
#define BLINK_MIN_MS        40  // Shorter than this = noise
#define BLINK_MAX_MS       250  // Longer than this = prolonged closure
#define DOUBLE_BLINK_GAP_MS 350 // Max gap for a double blink
```

### Scroll behavior

```cpp
#define SCROLL_HOLD_MS     600  // Gaze hold time before scroll starts
#define SCROLL_REPEAT_MS   120  // Repeat interval while scrolling
#define SCROLL_TRIGGER_RATIO 0.60f // Portion of calibrated up/down threshold
#define SCROLL_SPEED         3  // Scroll ticks per trigger
```

### Jaw clench behavior

```cpp
#define CLENCH_MIN_MS      150  // Shorter than this = ignored
#define CLENCH_HOLD_MS     600  // Longer than this = hold left click
// 150-600 ms -> middle click
// >600 ms    -> hold left click
```

### Smoothing and signal quality

```cpp
#define MA_WINDOW          10   // Higher = smoother, slower response
#define QUALITY_WINDOW     64   // Noise estimation window
#define DEADZONE_Q5        40   // Deadzone when quality is excellent
#define DEADZONE_Q1        90   // Deadzone when quality is very poor
```

### Calibration safeguards

```cpp
#define CAL_MIN_GAZE_H     45   // Reject too-small horizontal calibration peaks
#define CAL_MIN_GAZE_V     40   // Reject too-small vertical calibration peaks
#define CAL_MIN_BLINK      40   // Minimum blink threshold
#define CAL_MIN_CLENCH     20   // Minimum jaw-clench threshold
```

---

## 12. Troubleshooting

### Cursor drifts in one direction

Cause: baseline drift after long wear or electrode movement.

Fix: send `B` in Serial Monitor to refresh baseline.

### Blink is not detected

Cause 1: blink threshold was calibrated too high.

Fix: run `R` and blink more clearly during step `[6/7]`.

Cause 2: the vertical electrodes are misplaced.

Fix: move the upper electrode closer to the eyebrow and the lower one closer to the upper cheekbone.

### Double blink becomes two left clicks

Cause: the gap between blinks is longer than `DOUBLE_BLINK_GAP_MS`.

Fix: blink twice a little faster, or increase `DOUBLE_BLINK_GAP_MS` in `config.h`.

### Cursor shakes too much

Cause: noisy signal, usually due to poor gel contact or loose electrodes.

Fix 1: send `Q` and inspect the quality score.

Fix 2: increase `MA_WINDOW` from `10` to `14` for more smoothing.

Fix 3: reapply gel and press the electrodes down again.

### LED keeps blinking rapidly

Cause: lead-off detection says an electrode or cable is disconnected.

Fix: check cable seating on the AD8232 and press electrodes down again.

### Serial port does not appear on Linux

```bash
sudo usermod -a -G dialout $USER
# log out and back in
```

### `Mouse.h not found` compile error

Cause: the wrong board is selected.

Fix: select `Tools -> Board -> Arduino Micro`. Uno/Nano do not support native `Mouse.h`.

### `avrdude: stk500v2_getsync()` during upload

1. Unplug and reconnect the Arduino.
2. Re-check the selected port.
3. Press reset on the board and upload again immediately.

---

## 13. System Architecture

### File structure

```text
HID-BCI/
├── HID-BCI.ino            Main loop, serial commands, timing
├── config.h               Constants, pins, EEPROM layout
├── signal_processor.h     LP/HP filters, EMG envelope, quality score
├── gesture_classifier.h   Gesture state machine + adaptive deadzone
├── mouse_controller.h     USB HID Mouse wrapper
└── trainer.h              Calibration wizard + EEPROM save/load
```

### Data flow

```text
              AD8232 #1 (A0)               AD8232 #2 (A1)
             left/right temples          above/below the eye
                    |                            |
                    v                            v
         SignalChannel hChannel        SignalChannel vChannel
            |- moving average             |- moving average
            |- LP Butterworth             |- LP Butterworth
            |- HP filter (EMG)            |- quality score
            `- quality score
                    |                            |
                    `-----------+----------------'
                                v
                 GestureClassifier::update()
                   |- adaptive deadzone
                   |- blink state machine  -> CLICK_L / CLICK_R
                   |- clench detector      -> CLICK_M / HOLD_L
                   |- scroll timer         -> SCROLL_UP / SCROLL_DOWN
                   `- proportional EOG     -> MOVE (dx, dy)
                                |
                                v
                 MouseController::handle()
                                |
                                v
                      USB HID mouse on host PC
```

### EEPROM layout

| Address | Size | Content |
|---------|------|---------|
| 0 | 1 byte | Magic byte (`0xBC`) |
| 1 | 1 byte | Calibration version |
| 2-3 | int16 | Horizontal baseline |
| 4-5 | int16 | Vertical baseline |
| 6-7 | int16 | Left-gaze threshold |
| 8-9 | int16 | Right-gaze threshold |
| 10-11 | int16 | Up-gaze threshold |
| 12-13 | int16 | Down-gaze threshold |
| 14-15 | int16 | Blink threshold |
| 16-17 | int16 | Jaw-clench threshold |
| 20-21 | int16 | Horizontal channel noise floor |
| 22-23 | int16 | Vertical channel noise floor |

---

## Quick Start Summary

```text
1. Wire the two AD8232 boards to the Arduino Micro
2. Place the 6 electrodes with conductive gel
3. Connect the board by USB
4. Open HID-BCI.ino in Arduino IDE and upload
5. Open Serial Monitor at 115200 and complete the 7 calibration steps
6. Use gaze, blinks, and jaw clenches like a mouse
```
