#include <assert.h>
#include "../display/mascot_logic.h"

int main(void) {
    struct mascot_input s = {0};
    assert(!mascot_running(&s, 0));
    s.typed = true;
    s.last_press = 1000;
    assert(mascot_running(&s, 1849));
    assert(!mascot_running(&s, 1850));

    /* Idle whenever no key was pressed recently, whatever the WPM says. */
    assert(mascot_gait_next(MASCOT_RUN, false, 200) == MASCOT_IDLE);
    /* Slow typing walks; fast typing rides the scooter, with hysteresis. */
    assert(mascot_gait_next(MASCOT_IDLE, true, 0) == MASCOT_WALK);
    assert(mascot_gait_next(MASCOT_WALK, true, 39) == MASCOT_WALK);
    assert(mascot_gait_next(MASCOT_WALK, true, 40) == MASCOT_RUN);
    assert(mascot_gait_next(MASCOT_RUN, true, 30) == MASCOT_RUN);
    assert(mascot_gait_next(MASCOT_RUN, true, 29) == MASCOT_WALK);

    assert(mascot_frame_ms(MASCOT_WALK, 0) == 100);
    assert(mascot_frame_ms(MASCOT_WALK, 39) == 100);
    assert(mascot_frame_ms(MASCOT_RUN, 40) == 100);
    assert(mascot_frame_ms(MASCOT_RUN, 72) == 84);
    assert(mascot_frame_ms(MASCOT_RUN, 140) == 50);
    assert(mascot_frame_ms(MASCOT_RUN, 255) == 50);

    /* Frame 0 idles; walking cycles 1..6; the scooter cycles 7..11. */
    assert(MASCOT_FRAME_COUNT == 12);
    assert(mascot_next_frame(MASCOT_IDLE, 9) == 0);
    assert(mascot_next_frame(MASCOT_WALK, 0) == 1);
    assert(mascot_next_frame(MASCOT_WALK, 1) == 2);
    assert(mascot_next_frame(MASCOT_WALK, 6) == 1);
    assert(mascot_next_frame(MASCOT_WALK, 9) == 1);
    assert(mascot_next_frame(MASCOT_RUN, 3) == 7);
    assert(mascot_next_frame(MASCOT_RUN, 7) == 8);
    assert(mascot_next_frame(MASCOT_RUN, 11) == 7);
    return 0;
}
