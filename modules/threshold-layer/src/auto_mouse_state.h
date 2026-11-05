/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Pure state shared by the firmware and the host test. */
struct auto_mouse_state {
    uint32_t distance;
    int64_t blocked_until;
    bool active;
};
static inline void auto_mouse_typing(struct auto_mouse_state *s, int64_t now, uint32_t idle_ms) {
    s->distance = 0;
    s->active = false;
    s->blocked_until = now + idle_ms;
}
/* Sensor counts, not screen pixels. Zero reports must not activate the layer. */
static inline bool auto_mouse_motion(struct auto_mouse_state *s, int32_t delta,
                                     int64_t now, uint32_t threshold) {
    if (delta == 0 || now < s->blocked_until || s->active) {
        return false;
    }
    s->distance += delta < 0 ? -(uint32_t)delta : (uint32_t)delta;
    s->active = s->distance >= threshold;
    return s->active;
}
