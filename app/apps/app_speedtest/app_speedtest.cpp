/**
 * @file app_speedtest.cpp
 * @brief See app_speedtest.h.
 */
#include "app_speedtest.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppSpeedTest::AppSpeedTest()
{
    setAppInfo().name = "speedtest";
}

void AppSpeedTest::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "SPEED TEST");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _arc = make_ring(root, 150, 12);
    lv_arc_set_range(_arc, 0, 100);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, -6);

    _value = lv_label_create(root);
    lv_obj_set_style_text_color(_value, COLOR_FG, 0);
    lv_obj_set_style_text_font(_value, &lv_font_montserrat_48, 0);
    lv_label_set_text(_value, "...");
    lv_obj_align(_value, LV_ALIGN_CENTER, 0, -14);

    _sub = lv_label_create(root);
    lv_obj_set_style_text_color(_sub, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_sub, &lv_font_montserrat_14, 0);
    lv_label_set_text(_sub, "testing...");
    lv_obj_align(_sub, LV_ALIGN_CENTER, 0, 20);

    // Kick off a fresh test each time this app is opened.
    appdata::DataService::instance().requestSpeedTest();
}

void AppSpeedTest::tick()
{
    if (!_value) return;
    const appdata::SpeedData s = appdata::DataService::instance().speed();

    if (s.running) {
        lv_label_set_text(_value, "...");
        lv_label_set_text(_sub, "testing...");
        set_ring(_arc, 0, COLOR_ACCENT);
        return;
    }
    if (!s.ok) {
        lv_label_set_text(_value, "--");
        lv_label_set_text(_sub, s.err.empty() ? "reopen to retest" : "failed");
        set_ring(_arc, 0, COLOR_BAR_BG);
        return;
    }

    const float mbps = s.down_kbps / 1000.0f;
    const lv_color_t c = (mbps >= 25.0f) ? COLOR_OK : (mbps >= 5.0f ? COLOR_WARN : COLOR_DANGER);
    char b[16];
    std::snprintf(b, sizeof(b), "%.1f", mbps);
    lv_label_set_text(_value, b);
    lv_obj_set_style_text_color(_value, c, 0);
    lv_label_set_text(_sub, "Mbps  down");

    float pct = mbps; // cap the ring at 100 Mbps
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    set_ring(_arc, pct, c);
}
