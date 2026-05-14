/**
 * @file haptic_engine.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-11
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <hal/hal.h>
#include "../utils/Adafruit_DRV2605/Adafruit_DRV2605.h"

class HapticEngineDRV2605L : public hal_components::HapticEngineBase {
public:
    HapticEngineDRV2605L();
    ~HapticEngineDRV2605L();

    bool init() override;
    void enable() override;
    void disable() override;
    void playEffect(const HapticEffect::HapticEffect_t& effect) override;
    void playEffects(const std::vector<HapticEffect::HapticEffect_t>& effectSequence) override;

    // Doesn't seem necessary
    // void stop() override;
    // bool isPlaying() override;

private:
    Adafruit_DRV2605* _drv2605 = nullptr;
};
