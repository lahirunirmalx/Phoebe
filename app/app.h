/**
 * @file app.h
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
#include <functional>

/**
 * @brief Application layer
 *
 */
namespace APP {

// Dependency injection callbacks
struct InitCallback_t {
    std::function<void()> onHalInjection = nullptr;
};

/**
 * @brief Initialize the application layer
 *
 * @param callback
 */
void Init(InitCallback_t callback);

/**
 * @brief Update the application layer
 *
 */
void Update();

/**
 * @brief Whether the application layer is done
 *
 * @return true
 * @return false
 */
bool IsDone();

/**
 * @brief Destroy the application layer
 *
 */
void Destroy();

} // namespace APP
