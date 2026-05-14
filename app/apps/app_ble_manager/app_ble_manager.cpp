/**
 * @file app_ble_manager.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-11-22
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "app_ble_manager.h"
#include <mooncake_log.h>
#include <hal/hal.h>
#include <shared/shared.h>
#include <lvgl.h>

using namespace mooncake;

#define _tag (getAppInfo().name)

AppBleManager::AppBleManager()
{
    // Configure app info
    setAppInfo().name = "AppBleManager";
}

void AppBleManager::onCreate()
{
    mclog::tagInfo(_tag, "on create");

    // Open self
    open();
}

void AppBleManager::onOpen()
{
    mclog::tagInfo(_tag, "on open");
}

void AppBleManager::onRunning()
{
    handle_ble_message();
}

void AppBleManager::onClose()
{
    mclog::tagInfo(_tag, "on close");
}

void AppBleManager::handle_ble_message()
{
    // Handle in every 100ms
    if (HAL::SysCtrl().millis() - _time_count > 100) {
        SharedData::Borrow();

        for (const auto& message : SharedData::Ble().messageList) {
        }

        SharedData::Ble().messageList.clear();
        SharedData::Return();
        _time_count = HAL::SysCtrl().millis();
    }
}
