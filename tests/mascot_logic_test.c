#include <assert.h>
#include "../display/mascot_logic.h"

int main(void) {
    struct mascot_input s = {0};
    assert(!mascot_running(&s, 0));
    s.typed = true;
    s.last_press = 1000;
    assert(mascot_running(&s, 1849));
    assert(!mascot_running(&s, 1850));
    assert(mascot_frame_ms(0) == 120);
    assert(mascot_frame_ms(72) == 84);
    assert(mascot_frame_ms(140) == 50);
    assert(mascot_frame_ms(255) == 50);
    return 0;
}
