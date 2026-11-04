# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_VOLTBALL_POINTER_MODE)
  zephyr_library()
  zephyr_library_sources(src/pointer_mode.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
  zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../include)
  # Shared typing-callback declaration from the auto mouse module.
  zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../threshold-layer/src)
endif()
