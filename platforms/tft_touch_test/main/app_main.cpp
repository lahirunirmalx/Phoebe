/**
 * phoebe TFT + touch pin-discovery test
 * -------------------------------------
 * 1. Drives an ST7789 240x240 over SPI and runs a color sweep
 *    (RED -> GREEN -> BLUE -> WHITE -> BLACK). If you SEE these, the display
 *    wiring matches the pins in st7789.cpp.
 * 2. Reads the ESP32 capacitive touch pad T9 (GPIO32). The screen turns GREEN
 *    while the pad is touched and BLUE when released, and every transition is
 *    logged over serial with the raw reading.
 *
 * Watch the serial monitor: even with no display attached, the touch baseline
 * and DOWN/up transitions confirm the touch half works on its own.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "st7789.h"
#include "touch_t9.h"

static const char* TAG = "tft_touch_test";

// RGB565 colors (byte-swapped to big-endian inside the driver)
#define C_RED   0xF800
#define C_GREEN 0x07E0
#define C_BLUE  0x001F
#define C_WHITE 0xFFFF
#define C_BLACK 0x0000

static void test_task(void*) {
    st7789_init();
    st7789_backlight(true);

    // --- 1) Color sweep: confirms the panel is alive + color order is right --
    const struct { uint16_t c; const char* name; } sweep[] = {
        {C_RED, "RED"}, {C_GREEN, "GREEN"}, {C_BLUE, "BLUE"},
        {C_WHITE, "WHITE"}, {C_BLACK, "BLACK"},
    };
    for (size_t i = 0; i < sizeof(sweep) / sizeof(sweep[0]); ++i) {
        ESP_LOGI(TAG, "fill %s", sweep[i].name);
        st7789_fill(sweep[i].c);
        vTaskDelay(pdMS_TO_TICKS(700));
    }

    // --- 2) Touch baseline -------------------------------------------------
    const uint16_t baseline = touch_t9_baseline();
    const uint16_t delta    = baseline / 8;     // ~12% deviation counts as a touch
    ESP_LOGI(TAG, "touch T9/GPIO32 baseline=%u delta=%u -- touch the pad; raw value streamed below",
             baseline, delta);

    // --- 3) Live touch loop ------------------------------------------------
    bool last = false;
    int  tick = 0;
    st7789_fill(C_BLUE);
    while (true) {
        const uint16_t v = touch_t9_read();
        // Accept a shift in EITHER direction so detection doesn't depend on the
        // board-specific sign of the touch delta.
        const bool touched = (v + delta < baseline) || (v > baseline + delta);
        if (touched != last) {
            ESP_LOGI(TAG, ">>> touch=%s raw=%u (baseline=%u)", touched ? "DOWN" : "up", v, baseline);
            st7789_fill(touched ? C_GREEN : C_BLUE);
            last = touched;
        }
        if (++tick % 4 == 0) {                  // stream the raw value ~2.5x/sec
            ESP_LOGI(TAG, "raw=%u (baseline=%u, touch if outside +/-%u)", v, baseline, delta);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "phoebe TFT+touch test starting");
    // Pin to core 0 explicitly (classic ESP32 is dual-core; this test is single-task).
    xTaskCreatePinnedToCore(test_task, "tft_test", 4096, nullptr, 5, nullptr, 0);
}
