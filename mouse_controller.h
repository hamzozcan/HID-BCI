#pragma once
#include <Mouse.h>
#include "config.h"
#include "gesture_classifier.h"

// ─────────────────────────────────────────────────────────────────────────────
//  MouseController — translates Gesture events into USB HID Mouse actions
// ─────────────────────────────────────────────────────────────────────────────
class MouseController {
public:
    bool enabled = true;  // false during calibration / training

    void begin() {
        Mouse.begin();
        digitalWrite(PIN_LED, LOW);
    }

    void setLeadOff(bool state) { _lead_off = state; }

    // Call with the latest gesture result and dx/dy from classifier
    void handle(Gesture g, int8_t dx, int8_t dy, uint32_t now_ms) {
        _updateLed(now_ms);
        if (!enabled) return;

        switch (g) {
            case GESTURE_MOVE:
                if (now_ms - _last_move_ms >= MOUSE_UPDATE_MS) {
                    Mouse.move(dx, dy, 0);
                    _last_move_ms = now_ms;
                }
                break;

            case GESTURE_CLICK_L:
                _releaseAll();
                Mouse.click(MOUSE_LEFT);
                _queueFlash(1, now_ms);
                break;

            case GESTURE_CLICK_R:
                _releaseAll();
                Mouse.click(MOUSE_RIGHT);
                _queueFlash(2, now_ms);
                break;

            case GESTURE_CLICK_M:
                _releaseAll();
                Mouse.click(MOUSE_MIDDLE);
                _queueFlash(3, now_ms);
                break;

            case GESTURE_HOLD_L:
                Mouse.press(MOUSE_LEFT);
                _held_left = true;
                _flash_toggles_remaining = 0;
                digitalWrite(PIN_LED, HIGH);
                break;

            case GESTURE_HOLD_L_END:
                Mouse.release(MOUSE_LEFT);
                _held_left = false;
                digitalWrite(PIN_LED, LOW);
                break;

            case GESTURE_SCROLL_UP:
                if (now_ms - _last_scroll_ms >= SCROLL_REPEAT_MS) {
                    Mouse.move(0, 0, SCROLL_SPEED);
                    _last_scroll_ms = now_ms;
                }
                break;

            case GESTURE_SCROLL_DOWN:
                if (now_ms - _last_scroll_ms >= SCROLL_REPEAT_MS) {
                    Mouse.move(0, 0, -SCROLL_SPEED);
                    _last_scroll_ms = now_ms;
                }
                break;

            default:
                break;
        }
    }

    void releaseAll() { _releaseAll(); }

private:
    uint32_t _last_move_ms   = 0;
    uint32_t _last_scroll_ms = 0;
    uint32_t _last_led_ms    = 0;
    bool     _held_left      = false;
    bool     _lead_off       = false;
    bool     _led_state      = false;
    uint8_t  _flash_toggles_remaining = 0;

    void _releaseAll() {
        if (_held_left) {
            Mouse.release(MOUSE_LEFT);
            _held_left = false;
        }
        Mouse.release(MOUSE_RIGHT);
        Mouse.release(MOUSE_MIDDLE);
        _flash_toggles_remaining = 0;
        _led_state = false;
        digitalWrite(PIN_LED, LOW);
    }

    void _queueFlash(uint8_t times, uint32_t now_ms) {
        if (times == 0) return;
        _led_state = true;
        digitalWrite(PIN_LED, HIGH);
        _last_led_ms = now_ms;
        _flash_toggles_remaining = (times * 2) - 1;
    }

    void _updateLed(uint32_t now_ms) {
        if (_lead_off) {
            digitalWrite(PIN_LED, (now_ms / 100) & 1);
            return;
        }

        if (_held_left) {
            digitalWrite(PIN_LED, HIGH);
            return;
        }

        if (_flash_toggles_remaining == 0) return;
        if (now_ms - _last_led_ms < 30) return;

        _last_led_ms = now_ms;
        _led_state = !_led_state;
        digitalWrite(PIN_LED, _led_state ? HIGH : LOW);
        _flash_toggles_remaining--;
    }
};
