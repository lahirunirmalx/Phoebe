/**
 * @file app_template.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date <date></date>
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "app_template.h"
#include <mooncake_log.h>
#include <hal/hal.h>
#include <lvgl.h>

using namespace mooncake;

#define _tag (getAppInfo().name)

AppTemplate::AppTemplate()
{
    // Configure app info
    setAppInfo().name = "AppTemplate";
}

void AppTemplate::onCreate()
{
    mclog::tagInfo(_tag, "on create");

    // Open self
    open();
}

void AppTemplate::onOpen()
{
    mclog::tagInfo(_tag, "on open");
}

void AppTemplate::onRunning()
{
    HAL::BtnUpdate();
    if (HAL::BtnPower().wasClicked()) {
        mclog::info("??");
    }
    if (HAL::BtnPower().wasDoubleClicked()) {
        mclog::info("bye");
        HAL::SysCtrl().powerOff();
    }
    if (HAL::BtnPower().wasHold()) {
        mclog::info("jijiji");
    }
}

void AppTemplate::onClose()
{
    mclog::tagInfo(_tag, "on close");
}
