/**
 * @file display.cpp
 * @brief DisplaySt7789 -- the LGFX_Sprite shim mooncake's DisplayBase expects.
 *        The real pixel path is the LVGL flush callback in hal_esp32.cpp, which
 *        calls st7789::blit directly. The ST7789 chip bring-up lives in
 *        st7789.cpp; HalEsp32::init() calls st7789::chip_init().
 */
#include "display.h"
#include "st7789.h"
#include "../../hal_config.h"

void DisplaySt7789::init()
{
    // mooncake's DisplayBase is an LGFX_Sprite; allocate a matching sprite so
    // its drawing APIs work. We never push through it -- LVGL flushes straight
    // to the panel.
    setColorDepth(lgfx::color_depth_t::rgb565_nonswapped);
    createSprite(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
}

void DisplaySt7789::push_buffer_to_display(void* buffer)
{
    // Full-frame fallback push (mooncake's pushToScreen). RGB565 already in the
    // panel's byte order is assumed; the normal LVGL path swaps in the flush cb.
    st7789::blit(0, 0, HAL_SCREEN_WIDTH - 1, HAL_SCREEN_HEIGHT - 1,
                 static_cast<const uint16_t*>(buffer));
}
