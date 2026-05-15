/**
 * @file hal_config.h
 * @brief Pin map + I2C address for the esp32_idf_test platform.
 *        Classic ESP32 dev board + 128x64 SSD1306 OLED on I2C.
 */
#pragma once

#define HAL_I2C_PORT 0
#define HAL_I2C_FREQ_HZ 400000

#define HAL_PIN_OLED_SDA 21
#define HAL_PIN_OLED_SCL 22

#define HAL_OLED_I2C_ADDR 0x3C
#define HAL_OLED_WIDTH 128
#define HAL_OLED_HEIGHT 64
