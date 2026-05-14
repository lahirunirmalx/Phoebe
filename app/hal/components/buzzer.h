/**
 * @file buzzer.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <cstdint>
#include <string>

namespace hal_components {

/**
 * @brief Buzzer component base class
 *
 */
class BuzzerBase {
public:
    ~BuzzerBase() = default;

    virtual void init() {}

    /**
     * @brief Start beeping
     *
     * @param frequency
     * @param duration
     */
    virtual void beep(float frequency, std::uint32_t duration = 0xFFFFFFFF) {}

    /**
     * @brief Stop beeping
     *
     */
    virtual void stop() {}

    /**
     * @brief Play RTTTL music
     *
     * @param rtttlMusic
     */
    virtual void playRtttlMusic(const std::string& rtttlMusic) {}
    // RTTTL format reference:
    // https://en.wikipedia.org/wiki/Ring_Tone_Text_Transfer_Language
    // https://adamonsoon.github.io/rtttl-play/
    // https://picaxe.com/rtttl-ringtones-for-tune-command/

    /**
     * @brief Whether something is currently playing
     *
     * @return true
     * @return false
     */
    virtual bool isPlaying()
    {
        return false;
    }
};

} // namespace hal_components
