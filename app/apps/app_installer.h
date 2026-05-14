/**
 * @file app_installer.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-29
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <mooncake.h>
#include <memory>
#include "app_template/app_template.h"
#include "app_test_clock/app_test_clock.h"
#include "app_test_demo/app_test_demo.h"
#include "app_claudemeter/app_claudemeter.h"
/* Header files locator (Don't remove) */

/**
 * @brief App install callback
 *
 * @param mooncake
 */
inline void on_install_apps()
{
    // Install App
    // mooncake::GetMooncake().installApp(std::make_unique<MyApp>());
    // mooncake::GetMooncake().installApp(std::make_unique<AppTemplate>());
    // mooncake::GetMooncake().installApp(std::make_unique<AppTestClock>());
    // mooncake::GetMooncake().installApp(std::make_unique<AppTestDemo>());
    mooncake::GetMooncake().installApp(std::make_unique<AppClaudeMeter>());
    /* Install app locator (Don't remove) */
}
