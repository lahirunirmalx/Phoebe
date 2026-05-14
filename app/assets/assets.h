/**
 * @file assets.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <lvgl.h>
#include <string>
#include <cstdint>

/**
 * @brief Asset pool layer: manages static resources like fonts and images, exposed as a global lazy-loaded singleton for convenient app access
 *
 */
namespace AssetPool {

/**
 * @brief Asset pool definition; place all static resources here
 *
 */
struct AssetPool_t {
    struct Font_t {
        const lv_font_t* RajdhaniBold16 = nullptr;
        const lv_font_t* RajdhaniBold24 = nullptr;
        const lv_font_t* RajdhaniBold36 = nullptr;
        const lv_font_t* RajdhaniBold48 = nullptr;
        const lv_font_t* RajdhaniBold64 = nullptr;
        const lv_font_t* RajdhaniBold72 = nullptr;
        const lv_font_t* RajdhaniBold96 = nullptr;
        const lv_font_t* RajdhaniBold144 = nullptr;
        const lv_font_t* Zpix12 = nullptr;
    };
    Font_t Font;
};

/**
 * @brief Asset pool init callback; assign your image pointers and similar resources here
 *
 * @param assetPool
 */
void on_asset_pool_init(AssetPool_t& assetPool);

/* -------------------------------------------------------------------------- */
/*                                  Singleton                                 */
/* -------------------------------------------------------------------------- */
// Global singleton

/**
 * @brief Get the asset pool
 *
 * @return AssetPool_t&
 */
AssetPool_t& Get();

/**
 * @brief Destroy the current asset pool instance
 *
 */
void Destroy();

// Convenience wrapper to keep call sites short
inline const AssetPool_t::Font_t& Font()
{
    return Get().Font;
}

} // namespace AssetPool
