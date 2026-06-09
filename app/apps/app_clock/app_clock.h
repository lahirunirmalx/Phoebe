/**
 * @file app_clock.h
 * @brief Clock app with 6 watch faces (analog/digital/animated/seg7/vfd/flip),
 *        chosen from SysCfg().watchFace. Top 5H Claude-usage bar reads
 *        DataService::claude().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"
#include <cstdint>
#include <ctime>

class AppClock : public ui::ScreenApp {
public:
    AppClock();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;
    void teardown() override;

private:
    enum Face { F_Analog = 0, F_Digital, F_Animated, F_Seg7, F_VFD, F_Flip };
    Face resolve_face() const;

    void build_5h_bar(lv_obj_t* root);
    void build_analog(lv_obj_t* root);
    void build_digital(lv_obj_t* root);
    void build_animated(lv_obj_t* root);
    void build_seg7(lv_obj_t* root);
    void build_vfd(lv_obj_t* root);
    void build_flip(lv_obj_t* root);

    void update_5h_bar();
    void update_analog(const struct tm& t);
    void update_digital(const struct tm& t);
    void update_animated(const struct tm& t);
    void update_seg7(const struct tm& t);
    void update_vfd(const struct tm& t);
    void update_flip(const struct tm& t);

    Face _face = F_Analog;

    // 5H bar
    lv_obj_t* _bar = nullptr;
    lv_obj_t* _bar_pct = nullptr;
    float _mock5 = 23.0f;

    // analog
    lv_obj_t* _hour_line = nullptr;
    lv_obj_t* _min_line = nullptr;
    lv_obj_t* _sec_line = nullptr;
    lv_point_precise_t _hpts[2];
    lv_point_precise_t _mpts[2];
    lv_point_precise_t _spts[2];

    // digital / animated / shared
    lv_obj_t* _time = nullptr;
    lv_obj_t* _sec_lbl = nullptr;
    lv_obj_t* _date = nullptr;
    lv_obj_t* _anim_arc = nullptr;

    // seg7 / vfd canvas
    lv_obj_t* _canvas = nullptr;
    std::uint8_t* _buf = nullptr;
    int _last_sec = -1;

    // flip
    lv_obj_t* _hh = nullptr;
    lv_obj_t* _mm = nullptr;
    int _last_min = -1;
};
