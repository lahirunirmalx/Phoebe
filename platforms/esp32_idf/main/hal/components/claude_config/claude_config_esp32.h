/**
 * @file claude_config_esp32.h
 * @brief NVS-backed Claude API config (namespace "claude", keys "base"/"bearer").
 *        Schema matches M5Cardputer-UserDemo's app_claudemeter.
 */
#pragma once
#include "hal/components/claude_config.h"
#include <string>

class ClaudeConfigEsp32 : public hal_components::ClaudeConfigBase {
public:
    ClaudeConfigEsp32() = default;

    bool load() override;
    bool save() override;
    void logState() const override;
};
