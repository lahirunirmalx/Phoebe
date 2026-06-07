/**
 * @file app_main.cpp
 * @brief Dual-core supervisor for the Freenove ESP32 Mini TV.
 *
 *   - UI task   -> APP_CPU (core 1): LVGL/Mooncake rendering, touch indev,
 *                  backlight idle/notify management, captive-portal screen.
 *   - Net task  -> PRO_CPU (core 0): WiFi reconnect + the captive portal.
 *
 * AppClaudeMeter's own HTTP fetch std::thread is pinned to core 0 via
 * CONFIG_PTHREAD_TASK_CORE_DEFAULT (sdkconfig.defaults), keeping all network
 * work off the UI core.
 */
#include <app.h>
#include <memory>
#include <atomic>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "hal/hal_esp32.h"
#include "hal/hal_config.h"
#include "hal/ui_signals.h"
#include "hal/components/touch/touch_t9.h"
#include "hal/components/captive_portal/captive_portal.h"

// Cross-core flags (declared in ui_signals.h).
namespace ui_signals {
std::atomic<bool> portal_request{false};
std::atomic<bool> portal_active{false};
}

static constexpr BaseType_t kCoreUi = 1;   // APP_CPU
static constexpr BaseType_t kCoreNet = 0;  // PRO_CPU

// One-time full-screen overlay shown while the captive portal is up.
static void show_portal_overlay()
{
    lv_obj_t* scr = lv_layer_top();
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_obj_set_pos(box, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x12121c), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto label = [&](const char* txt, uint32_t color, const lv_font_t* font) {
        lv_obj_t* l = lv_label_create(box);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
        if (font) lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_pad_ver(l, 4, 0);
    };
    label("WiFi Setup", 0x99ff00, &lv_font_montserrat_24);
    label("Join WiFi:", 0xaaaaaa, nullptr);
    label(captive_portal::AP_SSID, 0xffffff, &lv_font_montserrat_24);
    label("pass: 12345678", 0xaaaaaa, nullptr);
    label("then open", 0xaaaaaa, nullptr);
    label("http://192.168.4.1", 0x99ff00, nullptr);
}

static void ui_task(void*)
{
    // HAL is already injected (in app_main); APP::Init just wires up Mooncake +
    // installs apps on this (UI) core.
    APP::InitCallback_t cb;
    cb.onHalInjection = []() {};
    APP::Init(cb);

    bool portal_shown = false;

    while (!APP::IsDone()) {
        const std::uint32_t now = HAL::SysCtrl().millis();
        touch::tick(now);

        // Captive portal up -> show the setup screen, stop normal UI flow.
        if (ui_signals::portal_active.load()) {
            if (!portal_shown) {
                show_portal_overlay();
                HAL::Backlight().on();
                portal_shown = true;
            }
            lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Long-press -> ask the network task to open the captive portal.
        if (touch::take_long_press()) {
            mclog::tagInfo("ui", "long-press: requesting captive portal");
            ui_signals::portal_request.store(true);
        }

        // Tap/double-tap handling and the display-sleep timer live in
        // AppClaudeMeter (driven by the LVGL touch indev). Here we only step the
        // backlight notification pulses.
        HAL::Backlight().tick(now);

        APP::Update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    APP::Destroy();
    vTaskDelete(nullptr);
}

static void net_task(void*)
{
    std::uint32_t last_try = 0;
    while (true) {
        if (ui_signals::portal_request.load()) {
            captive_portal::run(); // never returns (reboots on save)
        }

        // Reconnect transparently if the link drops (best-effort, ~10s cadence).
        const std::uint32_t now = HAL::SysCtrl().millis();
        if (!HAL::Wifi().isConnected() && HAL::Wifi().hasCredentials() &&
            (now - last_try > 10000)) {
            last_try = now;
            HAL::Wifi().connect();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{
    // Full HAL bring-up (display, backlight, touch, NVS config, WiFi) on the
    // main task before the UI/network tasks start.
    HAL::Inject(std::make_unique<HalEsp32>());

    xTaskCreatePinnedToCore(ui_task, "ui", 16384, nullptr, 5, nullptr, kCoreUi);
    xTaskCreatePinnedToCore(net_task, "net", 8192, nullptr, 4, nullptr, kCoreNet);
}
