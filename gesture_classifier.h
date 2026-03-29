#pragma once
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Gesture IDs
// ─────────────────────────────────────────────────────────────────────────────
enum Gesture : uint8_t {
    GESTURE_NONE        = 0,
    GESTURE_MOVE,       // continuous – use dx/dy from classifier
    GESTURE_CLICK_L,    // left click (single blink)
    GESTURE_CLICK_R,    // right click (double blink)
    GESTURE_CLICK_M,    // middle click (jaw clench, brief)
    GESTURE_HOLD_L,     // left button hold (jaw clench, sustained)
    GESTURE_HOLD_L_END, // release held left button
    GESTURE_SCROLL_UP,
    GESTURE_SCROLL_DOWN,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Calibration thresholds (loaded from EEPROM or defaults)
// ─────────────────────────────────────────────────────────────────────────────
struct CalibData {
    int16_t h_base  = 512;
    int16_t v_base  = 512;
    int16_t h_left  = -DEFAULT_GAZE_H;
    int16_t h_right =  DEFAULT_GAZE_H;
    int16_t v_up    =  DEFAULT_GAZE_V;
    int16_t v_down  = -DEFAULT_GAZE_V;
    int16_t blink   =  DEFAULT_BLINK;
    int16_t clench  =  DEFAULT_CLENCH;
};

// ─────────────────────────────────────────────────────────────────────────────
//  GestureClassifier
//  Input: filtered EOG (H, V) + EMG envelope — Output: Gesture enum + dx/dy
// ─────────────────────────────────────────────────────────────────────────────
class GestureClassifier {
public:
    CalibData cal;

    int8_t  dx       = 0;    // signed mouse delta X (-127..127)
    int8_t  dy       = 0;    // signed mouse delta Y
    uint8_t deadzone = MOUSE_DEADZONE; // adaptive — set from signal quality

    void setDeadzone(uint8_t dz) { deadzone = dz; }

    // Call every MOUSE_UPDATE_MS with current filtered values
    Gesture update(float h_eog, float v_eog, float emg_env, uint32_t now_ms) {
        // ── Lead-off: freeze ──
        if (_lead_off) return GESTURE_NONE;

        // ── Blink detector (vertical channel fast transient) ──
        Gesture blink_result = _detectBlink(v_eog, now_ms);
        if (blink_result != GESTURE_NONE) return blink_result;

        // ── Jaw clench detector (EMG RMS spike) ──
        Gesture clench_result = _detectClench(emg_env, now_ms);
        if (clench_result != GESTURE_NONE) return clench_result;

        // ── Hold-release check ──
        if (_holding_l && !_clench_active) {
            _holding_l = false;
            return GESTURE_HOLD_L_END;
        }
        if (_holding_l) return GESTURE_NONE; // suppress move while holding

        // ── Scroll mode: sustained vertical gaze ──
        if (_inScrollMode(v_eog, now_ms)) {
            if (v_eog > cal.v_up * 0.6f)  return GESTURE_SCROLL_UP;
            if (v_eog < cal.v_down * 0.6f) return GESTURE_SCROLL_DOWN;
        }

        // ── Cursor movement (proportional EOG → mouse delta) ──
        dx = _eogToMouseDelta(h_eog,  cal.h_left,  cal.h_right);
        dy = _eogToMouseDelta(-v_eog, -cal.v_up,  -cal.v_down); // invert for screen Y
        if (dx != 0 || dy != 0) return GESTURE_MOVE;

        return GESTURE_NONE;
    }

    void setLeadOff(bool state) { _lead_off = state; }

private:
    // ── Blink state machine ──
    enum BlinkState { BS_IDLE, BS_IN_BLINK, BS_WAIT_SECOND };
    BlinkState _bs       = BS_IDLE;
    uint32_t   _bs_t0    = 0;
    uint32_t   _last_blink_ms = 0;
    bool       _debounce = false;
    uint32_t   _debounce_t = 0;

    // ── Clench state machine ──
    bool     _clench_active  = false;
    uint32_t _clench_t0      = 0;
    bool     _holding_l      = false;

    // ── Scroll mode ──
    uint32_t _gaze_v_start   = 0;
    float    _last_v         = 0;

    // ── Lead off ──
    bool _lead_off = false;

    // ─────────────────────────────────────────────────────────────────────────
    Gesture _detectBlink(float v_eog, uint32_t now_ms) {
        bool peak = (v_eog > cal.blink);

        // Debounce lockout after any click
        if (_debounce) {
            if (now_ms - _debounce_t > DEBOUNCE_MS) _debounce = false;
            else return GESTURE_NONE;
        }

        switch (_bs) {
            case BS_IDLE:
                if (peak) { _bs = BS_IN_BLINK; _bs_t0 = now_ms; }
                break;

            case BS_IN_BLINK:
                if (!peak) {
                    uint32_t dur = now_ms - _bs_t0;
                    if (dur >= BLINK_MIN_MS && dur <= BLINK_MAX_MS) {
                        // Valid blink ended — wait for possible second
                        _bs           = BS_WAIT_SECOND;
                        _last_blink_ms = now_ms;
                    } else {
                        _bs = BS_IDLE; // too short or too long
                    }
                } else if (now_ms - _bs_t0 > BLINK_MAX_MS) {
                    _bs = BS_IDLE; // sustained squint, ignore
                }
                break;

            case BS_WAIT_SECOND:
                if (peak) {
                    // Second blink detected → right click
                    _bs = BS_IDLE;
                    _triggerDebounce(now_ms);
                    return GESTURE_CLICK_R;
                }
                if (now_ms - _last_blink_ms > DOUBLE_BLINK_GAP_MS) {
                    // Timeout → single blink → left click
                    _bs = BS_IDLE;
                    _triggerDebounce(now_ms);
                    return GESTURE_CLICK_L;
                }
                break;
        }
        return GESTURE_NONE;
    }

    // ─────────────────────────────────────────────────────────────────────────
    Gesture _detectClench(float emg_env, uint32_t now_ms) {
        bool above = (emg_env > cal.clench);

        if (!_clench_active && above) {
            _clench_active = true;
            _clench_t0     = now_ms;
        }

        if (_clench_active && !above) {
            uint32_t dur = now_ms - _clench_t0;
            _clench_active = false;

            if (dur >= CLENCH_MIN_MS && dur < 600) {
                // Short clench → middle click
                return GESTURE_CLICK_M;
            }
            // Long clench handled below
        }

        // Sustained clench (>600ms) → hold left button
        if (_clench_active && (now_ms - _clench_t0 > 600) && !_holding_l) {
            _holding_l = true;
            return GESTURE_HOLD_L;
        }

        return GESTURE_NONE;
    }

    // ─────────────────────────────────────────────────────────────────────────
    bool _inScrollMode(float v_eog, uint32_t now_ms) {
        bool strong_vertical = (v_eog > cal.v_up * 0.6f) || (v_eog < cal.v_down * 0.6f);
        if (strong_vertical) {
            if (_gaze_v_start == 0) _gaze_v_start = now_ms;
            return (now_ms - _gaze_v_start) > SCROLL_HOLD_MS;
        } else {
            _gaze_v_start = 0;
        }
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Non-linear proportional mapping: EOG offset → mouse delta pixels
    int8_t _eogToMouseDelta(float eog_val, int16_t neg_thr, int16_t pos_thr) {
        float deadzone = (float)this->deadzone;
        float range_pos = pos_thr > 0 ? pos_thr : DEFAULT_GAZE_H;
        float range_neg = neg_thr < 0 ? -neg_thr : DEFAULT_GAZE_H;

        if (eog_val > deadzone) {
            float norm = (eog_val - deadzone) / (range_pos - deadzone);
            norm = constrain(norm, 0.0f, 1.0f);
            // Quadratic acceleration
            float speed = norm * norm * MOUSE_MAX_SPEED * (1.0f + MOUSE_ACCEL_FACTOR * norm * MOUSE_MAX_SPEED);
            return (int8_t)constrain((int)speed, 0, MOUSE_MAX_SPEED);
        } else if (eog_val < -deadzone) {
            float norm = (-eog_val - deadzone) / (range_neg - deadzone);
            norm = constrain(norm, 0.0f, 1.0f);
            float speed = norm * norm * MOUSE_MAX_SPEED * (1.0f + MOUSE_ACCEL_FACTOR * norm * MOUSE_MAX_SPEED);
            return -(int8_t)constrain((int)speed, 0, MOUSE_MAX_SPEED);
        }
        return 0;
    }

    void _triggerDebounce(uint32_t now_ms) {
        _debounce   = true;
        _debounce_t = now_ms;
    }
};
