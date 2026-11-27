#ifndef TRIGGER_GESTURE_H
#define TRIGGER_GESTURE_H

#include <stdint.h>

/*
 * The right trigger on FDS games (main.c; tests/trigger_gesture_test.c runs it on the host): held alone for a second,
 * the next disk side; tapped (let go within TRIGGER_TAP_FRAMES), the frame display's next mode, on the release so a
 * hold doesn't also change it. The left trigger pressed at any point of a press (both triggers is the recording's save
 * gesture) spoils it: nothing happens until the right trigger is let go. Pressed above TRIGGER_PRESSED, let go below
 * TRIGGER_IDLE (0-255).
 */
#define TRIGGER_PRESSED 200
#define TRIGGER_IDLE 64
#define TRIGGER_HOLD_FRAMES 60
#define TRIGGER_TAP_FRAMES 24

#define TRIGGER_NONE 0
#define TRIGGER_TAP 1
#define TRIGGER_HOLD 2

typedef struct {
    uint32_t frames;
    uint8_t down, spoiled, held;
} trigger_gesture_t;

// once a frame with the triggers' positions: TRIGGER_TAP, TRIGGER_HOLD (on the frame the hold completes) or none
static inline uint32_t trigger_gesture(trigger_gesture_t* g, uint8_t rtrig, uint8_t ltrig) {
    if (!g -> down) {
        if (rtrig <= TRIGGER_PRESSED) {
            return TRIGGER_NONE;
        }
        g -> down = 1;
        g -> frames = 0;
        g -> spoiled = g -> held = 0;
    }
    if (rtrig < TRIGGER_IDLE) {
        g -> down = 0;
        return (!g -> spoiled && !g -> held && (g -> frames < TRIGGER_TAP_FRAMES)) ? TRIGGER_TAP : TRIGGER_NONE;
    }
    if (ltrig >= TRIGGER_IDLE) {
        g -> spoiled = 1;
    }
    if (++g -> frames == TRIGGER_HOLD_FRAMES) {
        if (!g -> spoiled) {
            g -> held = 1;
            return TRIGGER_HOLD;
        }
    }
    return TRIGGER_NONE;
}

#endif
