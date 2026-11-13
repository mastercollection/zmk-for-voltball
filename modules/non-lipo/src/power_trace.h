/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum voltball_shutdown_reason {
    VOLTBALL_SHUTDOWN_LOW_VOLTAGE = 1,
    VOLTBALL_SHUTDOWN_ADVERTISING = 2,
    VOLTBALL_SHUTDOWN_IDLE = 3,
};

#ifdef CONFIG_VOLTBALL_POWER_TRACE
void voltball_trace_voltage(uint16_t adc_mv);
void voltball_trace_shutdown(enum voltball_shutdown_reason reason, bool refresh);
#else
static inline void voltball_trace_voltage(uint16_t adc_mv) {}
static inline void voltball_trace_shutdown(enum voltball_shutdown_reason reason, bool refresh) {}
#endif
