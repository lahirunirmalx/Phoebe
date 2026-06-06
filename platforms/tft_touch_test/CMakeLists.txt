cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

# Standalone bring-up / pin-discovery harness. Deliberately does NOT pull in
# the phoebe app layer or LVGL -- it only drives the ST7789 over SPI and reads
# the ESP32 capacitive touch pad T9 (GPIO32). Once this lights the panel and
# registers touch, the pins below are the ones to copy into phoebe's
# platforms/esp32_idf*/main/hal/hal_config.h.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tft_touch_test)
