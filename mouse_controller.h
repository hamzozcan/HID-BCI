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
    }

    // Call with the latest gesture result and dx/dy from classifier
    void handle(Gesture g, int8_t dx, int8_t dy, uint32_t now_ms) {
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
                _flash(1);
                break;

            case GESTURE_CLICK_R:
                _releaseAll();
                Mouse.click(MOUSE_RIGHT);
                _flash(2);
                break;

            case GESTURE_CLICK_M:
                _releaseAll();
                Mouse.click(MOUSE_MIDDLE);
                _flash(3);
                break;

            case GESTURE_HOLD_L:
                Mouse.press(MOUSE_LEFT);
                _held_left = true;
                _flash(0);  // LED stays on while held
                break;

            case GESTURE_HOLD_L_END:
                Mouse.release(MOUSE_LEFT);
                _held_left = false;
                digitalWrite(PIN_LED, LOW);
                break;

            case GESTURE_SCROLL_UP:
                if (now_ms - _last_scroll_ms >= 120) {
                    Mouse.move(0, 0, SCROLL_SPEED);
                    _last_scroll_ms = now_ms;
                }
                break;

            case GESTURE_SCROLL_DOWN:
                if (now_ms - _last_scroll_ms >= 120) {
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
    bool     _held_left      = false;

    void _releaseAll() {
        if (_held_left) {
            Mouse.release(MOUSE_LEFT);
            _held_left = false;
        }
        Mouse.release(MOUSE_RIGHT);
        Mouse.release(MOUSE_MIDDLE);
        digitalWrite(PIN_LED, LOW);
    }

    // Flash LED n times for visual feedback (non-blocking for move, brief for clicks)
    void _flash(uint8_t times) {
        if (times == 0) { digitalWrite(PIN_LED, HIGH); return; }
        for (uint8_t i = 0; i < times; i++) {
            digitalWrite(PIN_LED, HIGH); delay(30);
            digitalWrite(PIN_LED, LOW);  delay(30);
        }
    }
};
