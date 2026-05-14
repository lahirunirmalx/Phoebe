/**
 * @file app.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-29
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "app.h"
#include "hal/hal.h"
#include "apps/app_installer.h"
#include <mooncake.h>
#include <mooncake_log.h>
#include <string>
#include <lvgl.h>

using namespace mooncake;

static const std::string _tag = "APP";

void APP::Init(InitCallback_t callback)
{
    mclog::tagInfo(_tag, "init");

    /* ------------------------------ HAL Injection ----------------------------- */
    // Hardware abstraction layer injection
    mclog::tagInfo(_tag, "hal injection");
    if (callback.onHalInjection) {
        callback.onHalInjection();
    }

    /* -------------------------------- Mooncake -------------------------------- */
    // Initialize Mooncake
    mclog::tagInfo(_tag, "create mooncake");

    // Kick off lazy initialization
    GetMooncake();

    // Install apps
    on_install_apps();
}

void APP::Update()
{
    // Update Mooncake
    GetMooncake().update();

    // Update LVGL
    lv_timer_handler();

    // Feed the watchdog; implementing this watchdog is recommended to avoid hangs in lifecycle callbacks
    HAL::SysCtrl().feedTheDog();
}

bool APP::IsDone()
{
    return false;
}

void APP::Destroy()
{
    DestroyMooncake();
    HAL::Destroy();
}
