#include "touch_t9.h"
#include "driver/touch_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// T9 == GPIO32 on the classic ESP32. (T8=GPIO33, T9=GPIO32.)
#define TOUCH_CH          TOUCH_PAD_NUM9
#define FILTER_PERIOD_MS  10

uint16_t touch_t9_baseline(void) {
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_config(TOUCH_CH, 0);                 // threshold unused; we poll the raw value
    touch_pad_filter_start(FILTER_PERIOD_MS);

    vTaskDelay(pdMS_TO_TICKS(200));                // let the IIR filter settle (untouched)
    uint32_t sum = 0;
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        uint16_t v = 0;
        touch_pad_read_filtered(TOUCH_CH, &v);
        sum += v;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return (uint16_t)(sum / n);
}

uint16_t touch_t9_read(void) {
    uint16_t v = 0;
    touch_pad_read_filtered(TOUCH_CH, &v);
    return v;
}
