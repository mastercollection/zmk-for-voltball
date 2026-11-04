/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Pure state shared by the firmware and the host test. */
struct ptr_ratio {
    int32_t mul;
    int32_t div;
};

enum ptr_axis { PTR_POINTER_X, PTR_POINTER_Y, PTR_SCROLL_X, PTR_SCROLL_Y, PTR_AXES };

struct ptr_state {
    uint8_t pointer_step;
    uint8_t scroll_step;
    bool scroll_toggled;
    uint8_t scroll_held;
    int32_t remainder[PTR_AXES];
    /* Deadzone: motion is dropped until it adds up after a pause. */
    int64_t last_motion;
    uint32_t dz_distance;
    bool dz_moving;
};

static inline bool ptr_scroll_active(const struct ptr_state *s) {
    return s->scroll_toggled || s->scroll_held > 0;
}

static inline void ptr_clear_remainders(struct ptr_state *s) {
    for (int i = 0; i < PTR_AXES; i++) {
        s->remainder[i] = 0;
    }
}

/* Typing ends a latched scroll; a held scroll key stays in control. */
static inline bool ptr_end_toggle(struct ptr_state *s) {
    if (!s->scroll_toggled) {
        return false;
    }
    s->scroll_toggled = false;
    ptr_clear_remainders(s);
    return true;
}

/* Returns false while motion after a pause of reset_ms has not yet added up to
 * deadzone counts; those reports should be dropped. Once past it, all motion
 * passes until the ball rests again. */
static inline bool ptr_deadzone(struct ptr_state *s, int32_t value, int64_t now,
                                uint32_t deadzone, uint32_t reset_ms) {
    if (deadzone == 0 || value == 0) {
        return true;
    }
    if (now - s->last_motion > (int64_t)reset_ms) {
        s->dz_moving = false;
        s->dz_distance = 0;
    }
    s->last_motion = now;
    if (!s->dz_moving) {
        uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
        s->dz_distance += magnitude;
        s->dz_moving = s->dz_distance >= deadzone;
    }
    return s->dz_moving;
}

/* Move one step within [0, count); returns true when the step changed. */
static inline bool ptr_step(uint8_t *step, int direction, uint8_t count) {
    if (direction > 0 && *step + 1 < count) {
        (*step)++;
        return true;
    }
    if (direction < 0 && *step > 0) {
        (*step)--;
        return true;
    }
    return false;
}

/* Scale with a carried remainder so slow steps keep small motions. */
static inline int32_t ptr_scale(int32_t value, struct ptr_ratio ratio, int32_t *remainder) {
    int32_t total = value * ratio.mul + *remainder;
    int32_t scaled = total / ratio.div;
    *remainder = total - scaled * ratio.div;
    return scaled;
}
