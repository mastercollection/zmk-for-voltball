#pragma once
#include <stdbool.h>
#include <stdint.h>

struct mascot_input {
    bool typed;
    int64_t last_press;
};

static inline bool mascot_running(const struct mascot_input *s, int64_t now) {
    return s->typed && now - s->last_press < 850;
}

static inline uint32_t mascot_frame_ms(uint8_t wpm) {
    return wpm >= 140 ? 50 : 120 - wpm / 2;
}
