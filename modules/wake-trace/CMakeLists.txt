# SPDX-License-Identifier: MIT

if(CONFIG_VOLTBALL_WAKE_TRACE)
  zephyr_library()
  zephyr_library_sources(src/wake_trace.c)
endif()
