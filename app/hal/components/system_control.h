/**
 * @file system_control.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace hal_components {

/**
 * @brief System control component base class
 *
 */
class SystemControlBase {
public:
    ~SystemControlBase() = default;

    /**
     * @brief Initialize
     *
     */
    virtual void init() {}

    /**
     * @brief Sleep the current thread
     *
     * @param ms
     */
    virtual void delay(std::uint32_t ms) {}

    /**
     * @brief Get the current system uptime in milliseconds
     *
     * @return std::uint32_t
     */
    virtual std::uint32_t millis()
    {
        return 0;
    }

    /**
     * @brief Reboot
     *
     */
    virtual void reboot() {}

    /**
     * @brief Power off
     *
     */
    virtual void powerOff() {}

    /**
     * @brief Feed the system watchdog
     *
     */
    virtual void feedTheDog() {}

    virtual size_t freeHeapSize()
    {
        return 0;
    }
};

} // namespace hal_components
