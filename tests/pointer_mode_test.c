/* Host regression tests for the pointer speed and scroll mode state. */
#include <assert.h>
#include <stdio.h>
#include "../modules/pointer-mode/src/pointer_mode_state.h"

int main(void) {
    struct ptr_state s = {.pointer_step = 3, .scroll_step = 2};
    /* Steps clamp at both ends of the table. */
    assert(ptr_step(&s.pointer_step, 1, 7) && s.pointer_step == 4);
    assert(ptr_step(&s.pointer_step, 1, 7) && ptr_step(&s.pointer_step, 1, 7));
    assert(s.pointer_step == 6 && !ptr_step(&s.pointer_step, 1, 7));
    s.scroll_step = 0;
    assert(!ptr_step(&s.scroll_step, -1, 6) && s.scroll_step == 0);
    /* Slow ratios carry remainders, so small motion is not lost. */
    struct ptr_ratio quarter = {1, 4};
    int32_t rem = 0, total = 0;
    for (int i = 0; i < 8; i++) {
        total += ptr_scale(1, quarter, &rem);
    }
    assert(total == 2 && rem == 0);
    /* Negative motion carries symmetrically. */
    total = 0;
    for (int i = 0; i < 8; i++) {
        total += ptr_scale(-1, quarter, &rem);
    }
    assert(total == -2 && rem == 0);
    /* Fast ratios multiply directly; 1/10 matches the former SCROLL layer. */
    rem = 0;
    assert(ptr_scale(5, (struct ptr_ratio){3, 2}, &rem) == 7 && rem == 1);
    assert(ptr_scale(1, (struct ptr_ratio){3, 2}, &rem) == 2 && rem == 0);
    assert(ptr_scale(95, (struct ptr_ratio){1, 10}, &rem) == 9 && rem == 5);
    /* Toggle and hold combine; releasing the hold keeps a latched toggle. */
    assert(!ptr_scroll_active(&s));
    s.scroll_held++;
    assert(ptr_scroll_active(&s));
    s.scroll_toggled = true;
    s.scroll_held--;
    assert(ptr_scroll_active(&s));
    s.scroll_toggled = false;
    assert(!ptr_scroll_active(&s));
    s.remainder[PTR_SCROLL_Y] = 7;
    ptr_clear_remainders(&s);
    assert(s.remainder[PTR_SCROLL_Y] == 0);
    puts("PASS: clamped steps, carried remainders, toggle and hold scroll mode");
    return 0;
}
