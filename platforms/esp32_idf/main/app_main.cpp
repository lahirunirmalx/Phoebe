/**
 * @file app_main.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <app.h>
#include <memory>
#include <hal/hal.h>
#include "hal/hal_esp32.h"

extern "C" void app_main(void)
{
    // Application layer initialization callback
    APP::InitCallback_t callback;

    callback.onHalInjection = []() {
        // Inject the hardware abstraction for the desktop platform
        HAL::Inject(std::make_unique<HalEsp32>());
    };

    // Application layer start
    APP::Init(callback);
    while (!APP::IsDone()) {
        APP::Update();
    }
    APP::Destroy();
}
