/**
 * @file imu.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

namespace hal_components {

/**
 * @brief IMU component base class
 *
 */
class ImuBase {
public:
    /**
     * @brief IMU data
     *
     */
    struct ImuData_t {
        float accelX = 0.0;
        float accelY = 0.0;
        float accelZ = 0.0;
        float gyroX = 0.0;
        float gyroY = 0.0;
        float gyroZ = 0.0;
        int steps = 0;
    };

    ~ImuBase() = default;

    virtual void init() {}

    /**
     * @brief Update IMU data
     *
     */
    virtual void update() {}

    /**
     * @brief Get IMU data
     *
     * @return const ImuData_t&
     */
    const ImuData_t& getData()
    {
        return _imu_data;
    }

    virtual void resetSteps() {}

protected:
    ImuData_t _imu_data;
};

} // namespace hal_components
