/**
 * @file hal_config.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

// Screen -- WROOM-32D test board uses a 128x64 SSD1306 I2C OLED instead of
// the Sharp MLCD. The HAL_SCREEN_* macros drive the LVGL buffer size.
#define HAL_SCREEN_WIDTH  128
#define HAL_SCREEN_HEIGHT 64

// OLED on I2C (shared with the IMU bus below)
#define HAL_OLED_I2C_ADDR 0x3C

// I2C
#define HAL_I2C_BUS_PORT_NUM    I2C_NUM_0
#define HAL_PIN_IMU_I2C_BUS_SDA 21
#define HAL_PIN_IMU_I2C_BUS_SCL 22

// IMU
#define HAL_IMU_DEVICE_ADDR 0x68
#define HAL_PIN_IMU_INT1    3
#define HAL_PIN_IMU_INT2    2

// Power, battery
#define HAL_PIN_PWR_HOLD           4
#define HAL_PIN_PWR_IS_USB_IN      5
#define HAL_PIN_BAT_MON_INT        23
#define HAL_BATTERY_LOW_THRESHOLD  (20.0f)
#define HAL_BATTERY_DEAD_THRESHOLD (5.0f)

// Buttons
#define HAL_PIN_BTN_PWR  6
#define HAL_PIN_BTN_UP   19
#define HAL_PIN_BTN_OK   9
#define HAL_PIN_BTN_DOWN 18

// Linear haptic motor
#define HAL_PIN_HAPTIC_TRIG 10
#define HAL_PIN_HAPTIC_EN   11

// Buzzer
#define HAL_PIN_BUZZ_CTRL 20

// Watchdog timeout
#define HAL_WATCH_DOG_TIMEOUT_S 6
