/**
 * @file hal_config.h
 * @brief Pin map + screen geometry for the Freenove ESP32 Mini TV (FNK0112).
 *
 * Classic ESP32 (esp32dev, 4 MB), 240x240 ST7789 over HSPI, capacitive touch
 * pad T9 on GPIO32, backlight + panel-VDD both active-low. Pins were confirmed
 * on real hardware via platforms/tft_touch_test (board's ESPHome config).
 */
#pragma once

// ---- Screen ---------------------------------------------------------------
#define HAL_SCREEN_WIDTH  240
#define HAL_SCREEN_HEIGHT 240

// ---- ST7789 display (HSPI) ------------------------------------------------
#define HAL_PIN_LCD_SCLK 14
#define HAL_PIN_LCD_MOSI 13
#define HAL_PIN_LCD_DC    2
#define HAL_PIN_LCD_CS   15
#define HAL_PIN_LCD_RST  (-1)   // no reset pin wired -> software reset only
#define HAL_LCD_SPI_HOST  SPI2_HOST
#define HAL_LCD_SPI_HZ   (20 * 1000 * 1000)   // 20 MHz (board does not work at 80 MHz)

// ---- Backlight + panel power (both ACTIVE-LOW) ----------------------------
#define HAL_PIN_BACKLIGHT 19    // LEDC PWM, active-low (drive low = on)
#define HAL_PIN_PANEL_VDD 21    // active-low enable; must be low to power the panel
#define HAL_BACKLIGHT_LEDC_CH    0
#define HAL_BACKLIGHT_LEDC_TIMER 0

// ---- Capacitive touch -----------------------------------------------------
#define HAL_TOUCH_PAD_NUM  TOUCH_PAD_NUM9   // T9 == GPIO32
#define HAL_TOUCH_LONGPRESS_MS 3000         // hold this long -> open captive portal
#define HAL_TOUCH_ACTIVE_MS    60000        // backlight/active window after a tap

// Watchdog timeout (unused on this board's minimal SysCtrl, kept for parity)
#define HAL_WATCH_DOG_TIMEOUT_S 6
