/**
 * @file app_test_clock.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date <date></date>
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <cstdint>
#include <mooncake.h>

/**
 * @brief Derived App
 *
 */
class AppTestClock : public mooncake::AppAbility {
public:
    AppTestClock();

    // Override lifecycle callbacks
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::uint32_t _clock_time_count = 0;
    void update_clock();
};
