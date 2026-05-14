/**
 * @file page_notification.cpp
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

static const char* _tag = "PageNotification";

static constexpr int _canvas_start_up_x = 0;
static constexpr int _canvas_start_up_y = -20;
static constexpr int _canvas_start_up_w = 52;
static constexpr int _canvas_start_up_h = 17;
static constexpr int _canvas_x = 0;
static constexpr int _canvas_y = 6;
static constexpr int _canvas_w = 132;
static constexpr int _canvas_h = 40;

void LauncherPageNotification::onCreate()
{
    _canvas_list.resize(4);
}

void LauncherPageNotification::onShow()
{
    mclog::tagInfo(_tag, "on show");

    int i = 0;
    for (auto& canvas : _canvas_list) {
        if (!canvas) {
            canvas = std::make_unique<smooth_widget::SmoothWidgetBase>(lv_screen_active());
            canvas->setAlign("lv_align_top_mid");
            canvas->setRadius(16);
            canvas->smoothPosition().setTransitionPath(EasingPath::easeOutBack);
            canvas->smoothPosition().jumpTo(_canvas_start_up_x, _canvas_start_up_y);
            canvas->smoothSize().setTransitionPath(EasingPath::easeOutBack);
            canvas->smoothSize().jumpTo(_canvas_start_up_w, _canvas_start_up_h);
            canvas->moveBackground();
        }

        canvas->smoothPosition().setDelay(i * 30);
        canvas->smoothPosition().setDuration(600);
        canvas->smoothPosition().moveTo(_canvas_x, _canvas_y + i * 47);
        canvas->smoothSize().setDelay((_canvas_list.size() - i - 1) * 30);
        canvas->smoothSize().setDuration(1000);
        canvas->smoothSize().moveTo(_canvas_w, _canvas_h);

        i++;
    }
}

void LauncherPageNotification::onForeground()
{
    if (isOnSubPage() && HAL::BtnPower().wasClicked()) {
        quitSubPage();
    }

    for (auto& canvas : _canvas_list) {
        if (canvas) {
            canvas->updateSmoothing();
        }
    }
}

void LauncherPageNotification::onBackground()
{
    for (auto& canvas : _canvas_list) {
        if (canvas) {
            canvas->updateSmoothing();
            if (canvas->isAllSmoothingFinish()) {
                canvas.reset();
            }
        }
    }
}

void LauncherPageNotification::onHide()
{
    mclog::tagInfo(_tag, "on hide");

    int i = 0;
    for (auto& canvas : _canvas_list) {
        canvas->smoothPosition().setDelay(i * 20);
        canvas->smoothPosition().setDuration(600);
        canvas->smoothPosition().moveTo(_canvas_start_up_x, _canvas_start_up_y);
        canvas->smoothSize().setDelay(i * 20);
        canvas->smoothSize().setDuration(400);
        canvas->smoothSize().moveTo(_canvas_start_up_w, _canvas_start_up_h);

        i++;
    }
}
