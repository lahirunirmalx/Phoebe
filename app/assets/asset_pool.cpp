/**
 * @file asset_pool.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "assets.h"
#include "lvgl_assets/ui.h"

void AssetPool::on_asset_pool_init(AssetPool_t& assetPool)
{
    // Initialize static resources here
    assetPool.Font.RajdhaniBold16 = &ui_font_RajdhaniBold16;
    assetPool.Font.RajdhaniBold24 = &ui_font_RajdhaniBold24;
    assetPool.Font.RajdhaniBold36 = &ui_font_RajdhaniBold36;
    assetPool.Font.RajdhaniBold48 = &ui_font_RajdhaniBold48;
    assetPool.Font.RajdhaniBold64 = &ui_font_RajdhaniBold64;
    assetPool.Font.RajdhaniBold72 = &ui_font_RajdhaniBold72;
    assetPool.Font.RajdhaniBold96 = &ui_font_RajdhaniBold96;
    assetPool.Font.RajdhaniBold144 = &ui_font_RajdhaniBold144;
    assetPool.Font.Zpix12 = &ui_font_zpix12;
}
