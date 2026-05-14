/**
 * @file display.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <lgfx_slim.h>
#include <lvgl.h>

namespace hal_components {

/**
 * @brief Display base class providing convenient drawing APIs
 *
 * Inherits LGFX_Sprite's graphics rendering methods directly so we don't have to write them ourselves.
 */
class DisplayBase : public lgfx::LGFX_Sprite {
public:
    ~DisplayBase() = default;

    /**
     * @brief Implement LGFX_Sprite buffer creation and color depth setup here, e.g.: createSprite(144, 168); setColorDepth(16);
     *
     */
    virtual void init() {}

    /**
     * @brief Reset the screen contents
     *
     */
    void resetScreen()
    {
        lv_obj_clean(lv_screen_active());
        lv_obj_invalidate(lv_screen_active());
        lv_timer_handler();
    }

    /**
     * @brief Push changes to the display
     *
     */
    void pushToScreen()
    {
        push_buffer_to_display(getBuffer());
    }

protected:
    /**
     * @brief Implement pushing the LGFX_Sprite buffer to the display here
     *
     * @param buffer
     */
    virtual void push_buffer_to_display(void* buffer) {}
};

} // namespace hal_components
