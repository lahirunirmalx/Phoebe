/**
 * @file app_meeting.h
 * @brief Next meeting countdown. Reads DataService::meeting().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppMeeting : public ui::ScreenApp {
public:
    AppMeeting();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _when = nullptr;
    lv_obj_t* _title = nullptr;
};
