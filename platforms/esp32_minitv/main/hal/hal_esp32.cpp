/**
 * @file hal_esp32.cpp
 * @brief HAL implementation for the Freenove ESP32 Mini TV. Brings up the
 *        backlight/VDD, the ST7789 panel, LVGL (partial double-buffer + touch
 *        pointer indev), NVS-backed config, WiFi and HTTP.
 */
#include "hal_esp32.h"
#include "hal_config.h"
#include "components/display/display.h"
#include "components/display/st7789.h"
#include "components/backlight/backlight_esp32.h"
#include "components/touch/touch_t9.h"
#include "components/system_config/system_config_esp32.h"
#include "components/wifi_manager/wifi_manager_esp32.h"
#include "components/http_client/http_client_arduino.h"

#include <cstdint>
#include <mooncake_log.h>
#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Minimal SystemControl: millis()/delay() via esp_timer / FreeRTOS, no power
// latch and no app-level watchdog (nothing would feed it on this board).
class SystemControlMiniTv : public hal_components::SystemControlBase {
public:
    std::uint32_t millis() override { return (std::uint32_t)(esp_timer_get_time() / 1000); }
    void delay(std::uint32_t ms) override { vTaskDelay(pdMS_TO_TICKS(ms)); }
    void reboot() override { esp_restart(); }
};

void HalEsp32::init()
{
    initArduino();

    fs_init();

    _components.system_control = std::make_unique<SystemControlMiniTv>();

    // No physical buttons on this board -- a single capacitive pad drives an
    // LVGL pointer indev instead (see lvgl_init()).
    _components.button = std::make_unique<hal_components::ButtonBase>();

    // No IMU / buzzer / haptic / battery hardware on the Mini TV -- base stubs.
    _components.battery_monitor = std::make_unique<hal_components::BatteryMonitorBase>();
    _components.imu = std::make_unique<hal_components::ImuBase>();
    _components.buzzer = std::make_unique<hal_components::BuzzerBase>();
    _components.haptic_engine = std::make_unique<hal_components::HapticEngineBase>();

    // Backlight + panel power FIRST: this asserts the active-low VDD enable so
    // the panel is powered before the ST7789 init sequence runs. Backlight
    // starts off (the watch is dark until touched / notified).
    _components.backlight = std::make_unique<BacklightEsp32>();
    _components.backlight->init();

    // ST7789 panel bring-up (SPI bus + init sequence).
    st7789::chip_init();

    // LVGL: display, flush callback, touch pointer indev.
    lvgl_init();

    _components.display = std::make_unique<DisplaySt7789>();
    _components.display->init();

    // Capacitive touch (T9 / GPIO32) baseline calibration.
    touch::init();

    // Persistent settings from NVS (wifi / claude / phoebe namespaces).
    _components.system_config = std::make_unique<SystemConfigEsp32>();
    _components.system_config->loadConfig();

    // WiFi: credentials live in NVS "wifi"; SystemConfig also mirrors them.
    _components.wifi = std::make_unique<WifiManagerEsp32>();
    _components.wifi->load();
    if (_components.wifi->hasCredentials()) {
        _components.wifi->connect();
    }
    _components.wifi->logState();

    // Re-apply the timezone: WifiManager::connect() calls configTime() which
    // resets TZ to UTC, so set our configured offset back afterwards.
    _components.system_config->applyConfig();

    // HTTP client (Arduino HTTPClient).
    _components.http_client = std::make_unique<HttpClientArduino>();
}

/* -------------------------------------------------------------------------- */
/*                                    LVGL                                    */
/* -------------------------------------------------------------------------- */
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;
    // LVGL renders little-endian RGB565; the ST7789 wants big-endian. Swap in
    // place (the draw buffer is re-rendered each frame, so mutating is fine).
    uint16_t* p = reinterpret_cast<uint16_t*>(px_map);
    const int count = w * h;
    for (int i = 0; i < count; ++i) {
        const uint16_t v = p[i];
        p[i] = (uint16_t)((v >> 8) | (v << 8));
    }
    st7789::blit(area->x1, area->y1, area->x2, area->y2, p);
    lv_display_flush_ready(disp);
}

static void lvgl_tick_timer(void* /*arg*/) { lv_tick_inc(10); }

// LVGL pointer indev: a touch on the pad reports a press at screen center, so a
// tap fires LV_EVENT_CLICKED on AppClaudeMeter's clickable root (-> view toggle).
static void touch_read_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data)
{
    if (touch::pressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = HAL_SCREEN_WIDTH / 2;
        data->point.y = HAL_SCREEN_HEIGHT / 2;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void HalEsp32::lvgl_init()
{
    const std::string tag = "lvgl";
    mclog::tagInfo(tag, "init");

    lv_init();

    auto display = lv_display_create(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    // Partial double-buffer (40 rows each ~= 19 KB) -- far cheaper on RAM than a
    // 115 KB full-frame buffer, leaving headroom for WiFi + the captive portal.
    constexpr int BUF_LINES = 40;
    const size_t buf_bytes = (size_t)HAL_SCREEN_WIDTH * BUF_LINES * sizeof(uint16_t);
    void* buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    void* buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    lv_display_set_buffers(display, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Touch pointer indev
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, display);

    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer,
                                                          .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
}

void HalEsp32::fs_init()
{
    mclog::tagInfo("fs", "skipped (mini tv)");
}
