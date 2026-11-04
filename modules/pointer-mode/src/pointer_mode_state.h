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
};

static inline bool ptr_scroll_active(const struct ptr_state *s) {
    return s->scroll_toggled || s->scroll_held > 0;
}

static inline void ptr_clear_remainders(struct ptr_state *s) {
    for (int i = 0; i < PTR_AXES; i++) {
        s->remainder[i] = 0;
    }
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
