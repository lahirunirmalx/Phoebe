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
#include <string>
#include <vector>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include "hal/hal_esp32.h"
#include "hal/hal_config.h"
#include "hal/ui_signals.h"
#include "hal/components/touch/touch_t9.h"
#include "hal/components/captive_portal/captive_portal.h"
#include "apps/app_registry.h"
#include "apps/utils/data_service/data_service.h"
#include "apps/utils/ui_common/ui_common.h"

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
    label("hold 3s to cancel", 0x666666, nullptr);
}

// Build the ordered list of app IDs to cycle through, from SysCfg().screenOrder
// ("clock,meter,weather" -> those, in order; omitted = hidden). Empty or fully
// invalid -> every installed app in canonical order.
static std::vector<int> build_cycle()
{
    std::vector<int> cyc;
    const std::string& order = HAL::SysCfg().getConfig().screenOrder;
    size_t i = 0;
    while (i < order.size()) {
        while (i < order.size() && (order[i] == ',' || order[i] == ' ' || order[i] == '\t' ||
                                    order[i] == '\n' || order[i] == '\r')) ++i;
        size_t j = i;
        while (j < order.size() && order[j] != ',' && order[j] != ' ' && order[j] != '\t' &&
               order[j] != '\n' && order[j] != '\r') ++j;
        if (j > i) {
            const int id = app_registry::id_of(order.substr(i, j - i));
            if (id >= 0) {
                bool dup = false;
                for (int c : cyc) if (c == id) { dup = true; break; }
                if (!dup) cyc.push_back(id);
            }
        }
        i = j;
    }
    if (cyc.empty())
        for (const auto& e : app_registry::entries()) cyc.push_back(e.id);
    return cyc;
}

static void ui_task(void*)
{
    // HAL is already injected; APP::Init wires up Mooncake + installs every app.
    APP::InitCallback_t cb;
    cb.onHalInjection = []() {};
    APP::Init(cb);

    // Shared data service (one network thread feeding all apps).
    appdata::DataService::instance().start();

    // Pin indicator: accent border on the top layer, shown while pinned.
    lv_obj_t* pin_border = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(pin_border);
    lv_obj_set_size(pin_border, HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_obj_set_pos(pin_border, 0, 0);
    lv_obj_set_style_bg_opa(pin_border, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(pin_border, ui::COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(pin_border, 4, 0);
    lv_obj_clear_flag(pin_border, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(pin_border, LV_OBJ_FLAG_HIDDEN);

    std::vector<int> cycle = build_cycle();
    int cur = 0;
    if (!cycle.empty()) mooncake::GetMooncake().openApp(cycle[cur]);

    // Backlight is off until explicitly enabled -- turn it on for the first app.
    HAL::Backlight().on();

    bool portal_shown = false;
    bool display_on = true;
    bool pinned = false;
    bool press_active = false;
    std::uint32_t press_start = 0;
    std::uint32_t last_interaction = HAL::SysCtrl().millis();

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
            if (touch::take_long_press()) {
                mclog::tagInfo("ui", "long-press: cancelling portal, rebooting to clock");
                esp_restart();
            }
            lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // >=3s hold -> open the captive portal.
        if (touch::take_long_press()) {
            mclog::tagInfo("ui", "long-press: requesting captive portal");
            ui_signals::portal_request.store(true);
        }

        // Tap / hold gesture (sub-3s), decided on release by hold duration.
        const bool down = touch::pressed();
        if (down && !press_active) {
            press_active = true;
            press_start = now;
        } else if (!down && press_active) {
            press_active = false;
            const std::uint32_t held = now - press_start;
            last_interaction = now;
            if (!display_on) {
                display_on = true;
                HAL::Backlight().on();
            } else if (held < ui::TAP_MAX_MS) {
                // Tap: cycle to the next app, clear pin.
                pinned = false;
                lv_obj_add_flag(pin_border, LV_OBJ_FLAG_HIDDEN);
                if (!cycle.empty()) {
                    mooncake::GetMooncake().closeApp(cycle[cur]);
                    cur = (cur + 1) % (int)cycle.size();
                    mooncake::GetMooncake().openApp(cycle[cur]);
                }
            } else if (held < ui::PIN_MAX_MS) {
                // Hold: toggle pin on the current app.
                pinned = !pinned;
                if (pinned) lv_obj_clear_flag(pin_border, LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(pin_border, LV_OBJ_FLAG_HIDDEN);
            }
            // held >= PIN_MAX_MS: reserved for the 3s portal hold -> no-op here.
        }

        // Idle sleep: backlight off after the window unless pinned.
        if (display_on && !pinned && (now - last_interaction > ui::DISPLAY_SLEEP_MS)) {
            display_on = false;
            HAL::Backlight().off();
        }

        HAL::Backlight().tick(now);
        APP::Update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    appdata::DataService::instance().stop();
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
