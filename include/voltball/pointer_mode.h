/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Read-only view of &ptr state for the Prospector screen (same central). */
struct voltball_ptr_status {
    bool scroll;
    /* Ratio of the current mode's speed step. */
    int32_t mul;
    int32_t div;
    /* Bumped on every speed key press, including at the end of the table. */
    uint32_t speed_presses;
};

void voltball_ptr_get_status(struct voltball_ptr_status *out);

/* Called by auto mouse on each exit-key non-modifier press. Typing ends a
 * latched drag scroll; returns false for &ptr itself, which is a pointer
 * control and neither types nor exits auto mouse. */
struct device;
bool voltball_ptr_on_typing(const struct device *behavior);
