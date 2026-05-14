/**
 * @file display.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-24
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "display.h"
#include "../../hal_config.h"
#include <SDL.h>
#include <cstdint>
#include <lvgl.h>
#include <src/display/lv_display.h>
#include <src/drivers/sdl/lv_sdl_window.h>

void render_from_uint16(SDL_Renderer* renderer, uint16_t* pixel_data, int width, int height)
{
    // Step 1: Create an SDL_Surface using the pixel data from the uint16_t array
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixel_data,               // Pixel data
                                                    width,                    // Width of the image
                                                    height,                   // Height of the image
                                                    16,                       // Depth (16 bits per pixel)
                                                    width * sizeof(uint16_t), // Pitch (number of bytes per row)
                                                    0xF800,                   // Red mask (5 bits for red)
                                                    0x07E0,                   // Green mask (6 bits for green)
                                                    0x001F,                   // Blue mask (5 bits for blue)
                                                    0x0000                    // Alpha mask (no alpha channel)
    );

    if (!surface) {
        SDL_Log("Unable to create surface: %s", SDL_GetError());
        return;
    }

    // Step 2: Convert the SDL_Surface to an SDL_Texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    // Step 3: Clear the renderer
    SDL_RenderClear(renderer);

    // Step 4: Copy the texture to the renderer
    SDL_RenderCopy(renderer, texture, NULL, NULL); // NULL means render the entire texture

    // Step 5: Present the updated renderer to display the image
    SDL_RenderPresent(renderer);

    // Cleanup
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void DisplaySdl::init()
{
    // Plenty of memory, just allocate one directly
    setColorDepth(lgfx::color_depth_t::rgb565_nonswapped);
    createSprite(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
}

void DisplaySdl::push_buffer_to_display(void* buffer)
{
    // Grab an SDL renderer instance from lvgl to use
    auto sdl_render = (SDL_Renderer*)lv_sdl_window_get_renderer(lv_display_get_default());
    render_from_uint16(sdl_render, (uint16_t*)buffer, width(), height());
}
