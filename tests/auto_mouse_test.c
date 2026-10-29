/* Host regression tests for the state machine used by the firmware. */
#include <assert.h>
#include <stdio.h>
#include "../modules/threshold-layer/src/auto_mouse_state.h"

int main(void) {
    struct auto_mouse_state s = {0};
    assert(!auto_mouse_motion(&s, 0, 0, 0));
    assert(!auto_mouse_motion(&s, 60, 0, 100));
    assert(auto_mouse_motion(&s, -40, 1, 100));
    /* No timeout: an arbitrarily long stop does not remove mouse mode. */
    assert(!auto_mouse_motion(&s, 0, 3600000, 100));
    assert(s.active);
    /* Typing cancels and discards both old and suppressed movement. */
    auto_mouse_typing(&s, 3600000, 200);
    assert(!s.active && s.distance == 0);
    assert(!auto_mouse_motion(&s, 1000, 3600199, 100));
    assert(s.distance == 0);
    assert(!auto_mouse_motion(&s, 99, 3600200, 100));
    assert(auto_mouse_motion(&s, 1, 3600201, 100));
    /* Typing before activation also clears a partial threshold. */
    auto_mouse_typing(&s, 4000000, 200);
    assert(!auto_mouse_motion(&s, 90, 4000200, 100));
    auto_mouse_typing(&s, 4000201, 200);
    assert(!auto_mouse_motion(&s, 10, 4000401, 100));
    assert(s.distance == 10);
    /* Repeated typing renews lockout, not merely the first exit. */
    auto_mouse_typing(&s, 4000500, 200);
    assert(!auto_mouse_motion(&s, 1000, 4000699, 100));
    assert(auto_mouse_motion(&s, 100, 4000700, 100));
    auto_mouse_typing(&s, 5000000, 0);
    assert(auto_mouse_motion(&s, INT32_MIN, 5000000, 100));
    puts("PASS: latched mouse, typing cancellation, cooldown, fresh threshold, integer bounds");
    return 0;
}
