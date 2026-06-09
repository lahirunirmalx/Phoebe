/**
 * @file app_network.cpp
 * @brief See app_network.h.
 */
#include "app_network.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppNetwork::AppNetwork()
{
    setAppInfo().name = "network";
}

void AppNetwork::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "NETWORK PING");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _arc = make_ring(root, 150, 12);
    lv_arc_set_range(_arc, 0, 100);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, -6);

    _value = lv_label_create(root);
    lv_obj_set_style_text_color(_value, COLOR_FG, 0);
    lv_obj_set_style_text_font(_value, &lv_font_montserrat_48, 0);
    lv_label_set_text(_value, "--");
    lv_obj_align(_value, LV_ALIGN_CENTER, 0, -14);

    _sub = lv_label_create(root);
    lv_obj_set_style_text_color(_sub, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_sub, &lv_font_montserrat_14, 0);
    lv_label_set_text(_sub, "");
    lv_obj_align(_sub, LV_ALIGN_CENTER, 0, 20);
}

void AppNetwork::tick()
{
    if (!_value) return;
    const appdata::NetData n = appdata::DataService::instance().net();
    if (!n.ok) {
        lv_label_set_text(_value, "--");
        lv_label_set_text(_sub, n.err.empty() ? "measuring..." : n.err.c_str());
        return;
    }
    const lv_color_t c = (n.latency_ms < 100) ? COLOR_OK : (n.latency_ms < 300 ? COLOR_WARN : COLOR_DANGER);
    char b[16];
    std::snprintf(b, sizeof(b), "%d", n.latency_ms);
    lv_label_set_text(_value, b);
    lv_obj_set_style_text_color(_value, c, 0);
    int v = 100 - n.latency_ms / 5; if (v < 0) v = 0; if (v > 100) v = 100;
    lv_arc_set_value(_arc, v);
    lv_obj_set_style_arc_color(_arc, c, LV_PART_INDICATOR);
    lv_label_set_text(_sub, n.latency_ms < 100 ? "ms  good" : (n.latency_ms < 300 ? "ms  ok" : "ms  poor"));
}
