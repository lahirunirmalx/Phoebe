/**
 * @file app_test_demo.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-14
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "app_test_demo.h"
#include <mooncake_log.h>
#include <hal/hal.h>
#include <src/display/lv_display.h>
#include <src/misc/lv_timer.h>
#include "../utils/widget/widget.h"
#include "../utils/page/page.h"

using namespace mooncake;
using namespace widget;
using namespace smooth_widget;
using namespace page;

#define _tag (getAppInfo().name)

AppTestDemo::AppTestDemo()
{
    setAppInfo().name = "AppTestDemo";
}

void AppTestDemo::onCreate()
{
    mclog::tagInfo(_tag, "on create");
    open();

    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);
}

void AppTestDemo::onOpen()
{
    mclog::tagInfo(_tag, "on open");
}

void AppTestDemo::onRunning()
{
    auto ret = CreateSelecMenuPageAndWaitResult(
        [](std::vector<std::string>& optionList, size_t& startupIndex) {
            optionList.push_back("Option 1");
            optionList.push_back("Option 2");
            optionList.push_back("Option 3");
            optionList.push_back("Option 4");
            optionList.push_back("Option 5");
            optionList.push_back("Option 6");
            optionList.push_back("Option 7");
            optionList.push_back("Option 8");
            optionList.push_back("Option 9");
            optionList.push_back("Option 10");
            optionList.push_back("Option 11");
            optionList.push_back("Option 12");
            optionList.push_back("Option 13");
            optionList.push_back("Option 14");

            // startupIndex = 2;
        },
        [](int selectIndex, std::string& option) { mclog::info("on select {} {}", selectIndex, option); });
    mclog::info("ret: {}", ret);
}

void AppTestDemo::onClose()
{
    mclog::tagInfo(_tag, "on close");
}
