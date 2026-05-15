# ESP-IDF test platform: classic ESP32 dev board + SSD1306 128x64 OLED on I2C.
#
# Pin map (set in main/hal_config.h):
#   SDA = GPIO21
#   SCL = GPIO22
#   I2C addr = 0x3C
#
# Standalone -- no Arduino-as-component, no LVGL. Just enough to bring up
# the OLED, read WiFi creds from NVS, connect, and show something on screen.

cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_idf_test_build)
