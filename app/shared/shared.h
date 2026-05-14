/**
 * @file shared.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-31
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Shared data layer; provides a global shared-data singleton protected by a mutex
 *
 */
namespace SharedData {

/**
 * @brief Shared data definition
 *
 */
struct SharedData_t {
    std::mutex mutex;

    struct Notification_t {
    };
    Notification_t Notification;

    struct Weather_t {
        std::string weather = "SUNNY";
        int temperature = 26;
    };
    Weather_t Weather;

    struct Ble_t {
        std::vector<std::string> messageList;
    };
    Ble_t Ble;
};

/**
 * @brief Get the shared data instance
 *
 * @return SharedData_t&
 */
SharedData_t& Get();

/**
 * @brief Destroy the shared data instance
 *
 */
void Destroy();

/**
 * @brief Borrow shared data (acquire the mutex)
 *
 */
inline void Borrow()
{
    Get().mutex.lock();
}

/**
 * @brief Return shared data (release the mutex)
 *
 */
inline void Return()
{
    Get().mutex.unlock();
}

// Convenience wrappers to keep call sites short
inline SharedData_t::Notification_t& Notification()
{
    return Get().Notification;
}
inline SharedData_t::Weather_t& Weather()
{
    return Get().Weather;
}
inline SharedData_t::Ble_t& Ble()
{
    return Get().Ble;
}

} // namespace SharedData
