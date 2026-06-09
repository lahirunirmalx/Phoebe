/**
 * @file screen_app.h
 * @brief Base class for Phoebe's full-screen apps.
 *
 * Each screen is an independent mooncake::AppAbility. The navigator opens one at
 * a time; onOpen() builds a full-screen root container and the subclass widgets,
 * onRunning() repaints on a throttled cadence, onClose() tears everything down.
 * Subclasses implement build()/tick()/teardown() and never touch lifecycle.
 */
#pragma once

#include "hal/hal.h"
#include "apps/utils/ui_common/ui_common.h"
#include <lvgl.h>
#include <mooncake.h>

namespace ui {

class ScreenApp : public mooncake::AppAbility {
public:
    void onOpen() override
    {
        _root = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(_root);
        lv_obj_set_size(_root, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(_root, 0, 0);
        lv_obj_set_style_bg_color(_root, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
        lv_obj_clear_flag(_root, LV_OBJ_FLAG_CLICKABLE);
        build(_root);
        _last_ms = 0;
        tick(); // first paint
    }

    void onRunning() override
    {
        const std::uint32_t now = HAL::SysCtrl().millis();
        if (now - _last_ms < _refresh_ms) return;
        _last_ms = now;
        tick();
    }

    void onClose() override
    {
        teardown();
        if (_root) {
            lv_obj_delete(_root);
            _root = nullptr;
        }
    }

protected:
    virtual void build(lv_obj_t* root) = 0; // create widgets under root
    virtual void tick() {}                  // repaint from latest data
    virtual void teardown() {}              // stop anims/timers before root is deleted

    void setRefreshMs(std::uint32_t ms) { _refresh_ms = ms; }

    lv_obj_t* _root = nullptr;

private:
    std::uint32_t _last_ms = 0;
    std::uint32_t _refresh_ms = 500;
};

} // namespace ui
