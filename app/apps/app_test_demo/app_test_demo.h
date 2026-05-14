/**
 * @file app_test_demo.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-14
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <mooncake.h>

/**
 * @brief Derived App
 *
 */
class AppTestDemo : public mooncake::AppAbility {
public:
    AppTestDemo();

    // Override lifecycle callbacks
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;
};
