/**
 * @file page_app_list.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "page.h"
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;
using namespace SmoothUIToolKit;

static const char* _tag = "PageAppList";

static constexpr int _canvas_start_up_x = 0;
static constexpr int _canvas_start_up_y = 175;
static constexpr int _canvas_x = 0;
static constexpr int _canvas_y = 20;
static constexpr int _canvas_w = 186;
static constexpr int _canvas_h = 172;

void LauncherPageAppList::onShow()
{
    mclog::tagInfo(_tag, "on show");

    if (!_canvas) {
        _canvas = std::make_unique<smooth_widget::SmoothWidgetBase>(lv_screen_active());
        _canvas->setSize(_canvas_w, _canvas_h);
        _canvas->setRadius(42);
        _canvas->setAlign("lv_align_center");
        _canvas->smoothPosition().setTransitionPath(EasingPath::easeOutBack);
        _canvas->smoothPosition().jumpTo(_canvas_start_up_x, _canvas_start_up_y);
    }

    _canvas->smoothPosition().setDuration(700);
    _canvas->smoothPosition().moveTo(_canvas_x, _canvas_y);
}

void LauncherPageAppList::onForeground()
{
    if (isOnSubPage() && HAL::BtnPower().wasClicked()) {
        quitSubPage();
    }

    if (_canvas) {
        _canvas->updateSmoothing();
    }
}

void LauncherPageAppList::onBackground()
{
    if (_canvas) {
        _canvas->updateSmoothing();
        if (_canvas->isAllSmoothingFinish()) {
            _canvas.reset();
        }
    }
}

void LauncherPageAppList::onHide()
{
    mclog::tagInfo(_tag, "on hide");

    _canvas->smoothPosition().setDuration(600);
    _canvas->smoothPosition().moveTo(_canvas_start_up_x, _canvas_start_up_y);
}
