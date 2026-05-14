/**
 * @file main.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-29
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <app.h>
#include <memory>
#include <hal/hal.h>
#include "hal/hal_desktop.h"

int main()
{
    // Application layer initialization callback
    APP::InitCallback_t callback;

    callback.onHalInjection = []() {
        // Inject the desktop platform hardware abstraction
        HAL::Inject(std::make_unique<HalDesktop>());
    };

    // Start application layer
    APP::Init(callback);
    while (!APP::IsDone()) {
        APP::Update();
    }
    APP::Destroy();

    return 0;
}
