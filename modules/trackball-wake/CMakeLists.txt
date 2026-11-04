# SPDX-License-Identifier: MIT

if(CONFIG_VOLTBALL_TRACKBALL_WAKE)
  zephyr_library()
  zephyr_library_sources(src/trackball_wake.c)
endif()
