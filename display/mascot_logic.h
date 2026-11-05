#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Packed frame order (pack_mascot.py, tests/patch_modules.py): the sitting idle
 * pose, then the walking cycle, then the scooter cycle. */
#define MASCOT_IDLE_FRAME 0
#define MASCOT_WALK_FRAMES 6
#define MASCOT_RUN_FRAMES 5
#define MASCOT_FRAME_COUNT (1 + MASCOT_WALK_FRAMES + MASCOT_RUN_FRAMES)

/* Walking plays at the source clip's rate. Above MASCOT_RUN_WPM the mascot
 * switches to the scooter; it walks again only below MASCOT_WALK_WPM. */
#define MASCOT_WALK_FRAME_MS 100
#define MASCOT_RUN_WPM 40
#define MASCOT_WALK_WPM 30

enum mascot_gait { MASCOT_IDLE, MASCOT_WALK, MASCOT_RUN };

struct mascot_input {
    bool typed;
    int64_t last_press;
};

static inline bool mascot_running(const struct mascot_input *s, int64_t now) {
    return s->typed && now - s->last_press < 850;
}

static inline enum mascot_gait mascot_gait_next(enum mascot_gait previous, bool moving, uint8_t wpm) {
    if (!moving) {
        return MASCOT_IDLE;
    }
    if (previous == MASCOT_RUN) {
        return wpm < MASCOT_WALK_WPM ? MASCOT_WALK : MASCOT_RUN;
    }
    return wpm >= MASCOT_RUN_WPM ? MASCOT_RUN : MASCOT_WALK;
}

static inline uint32_t mascot_frame_ms(enum mascot_gait gait, uint8_t wpm) {
    if (gait == MASCOT_WALK) {
        return MASCOT_WALK_FRAME_MS;
    }
    return wpm >= 140 ? 50 : 120 - wpm / 2;
}

/* Frame after `current` in the gait's cycle; entering a gait starts its cycle. */
static inline uint8_t mascot_next_frame(enum mascot_gait gait, uint8_t current) {
    if (gait == MASCOT_IDLE) {
        return MASCOT_IDLE_FRAME;
    }
    uint8_t first = gait == MASCOT_WALK ? 1 : 1 + MASCOT_WALK_FRAMES;
    uint8_t count = gait == MASCOT_WALK ? MASCOT_WALK_FRAMES : MASCOT_RUN_FRAMES;
    if (current < first || current >= first + count) {
        return first;
    }
    return first + (current - first + 1) % count;
}
