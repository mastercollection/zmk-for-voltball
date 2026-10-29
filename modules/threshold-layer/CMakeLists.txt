# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_INPUT_PROCESSOR_THRESHOLD_TEMP_LAYER)
  zephyr_library()
  zephyr_library_sources(src/input_processor_threshold_temp_layer.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
endif()
