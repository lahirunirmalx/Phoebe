/**
 * @file app_meeting.cpp
 * @brief See app_meeting.h.
 */
#include "app_meeting.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>
#include <ctime>

using namespace ui;

AppMeeting::AppMeeting()
{
    setAppInfo().name = "meeting";
}

void AppMeeting::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "NEXT MEETING");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _when = lv_label_create(root);
    lv_obj_set_style_text_color(_when, COLOR_FG, 0);
    lv_obj_set_style_text_font(_when, &lv_font_montserrat_48, 0);
    lv_label_set_text(_when, "--");
    lv_obj_align(_when, LV_ALIGN_CENTER, 0, -16);

    _title = lv_label_create(root);
    lv_obj_set_style_text_color(_title, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_title, SCREEN_W - 24);
    lv_obj_set_style_text_align(_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_title, "");
    lv_obj_align(_title, LV_ALIGN_CENTER, 0, 44);
}

void AppMeeting::tick()
{
    if (!_when) return;
    const appdata::MeetingData m = appdata::DataService::instance().meeting();

    if (!m.ok) {
        if (m.err == "no upcoming" || m.err.empty()) {
            lv_label_set_text(_when, "Free");
            lv_obj_set_style_text_color(_when, COLOR_OK, 0);
            lv_label_set_text(_title, "");
        } else {
            lv_label_set_text(_when, "--");
            lv_obj_set_style_text_color(_when, COLOR_FG, 0);
            lv_label_set_text(_title, m.err.c_str());
        }
        return;
    }

    long secs = m.start_epoch - (long)time(nullptr);
    if (secs < 0) secs = 0;
    if (secs > 2 * 3600) {
        lv_label_set_text(_when, "Free");
        lv_obj_set_style_text_color(_when, COLOR_OK, 0);
        lv_label_set_text(_title, "");
        return;
    }

    char when[24];
    if (secs <= 15 * 60) {
        std::snprintf(when, sizeof(when), "%02ld:%02ld", secs / 60, secs % 60);
        lv_obj_set_style_text_color(_when, COLOR_ACCENT, 0);
    } else {
        const long mins = secs / 60;
        if (mins < 60) std::snprintf(when, sizeof(when), "in %ldm", mins);
        else std::snprintf(when, sizeof(when), "in %ldh %ldm", mins / 60, mins % 60);
        lv_obj_set_style_text_color(_when, COLOR_FG, 0);
    }
    lv_label_set_text(_when, when);
    lv_label_set_text(_title, m.title.c_str());
}
