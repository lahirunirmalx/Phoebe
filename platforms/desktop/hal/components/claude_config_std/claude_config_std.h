/**
 * @file claude_config_std.h
 * @brief Desktop Claude API config: base URL + bearer persisted to JSON.
 *        Seeds from PHOEBE_CLAUDE_BASE / PHOEBE_CLAUDE_BEARER env vars on
 *        first run when claude_config.json doesn't exist yet.
 */
#pragma once
#include "hal/components/claude_config.h"
#include <string>

class ClaudeConfigStd : public hal_components::ClaudeConfigBase {
public:
    explicit ClaudeConfigStd(const std::string& rootPath = "./");

    bool load() override;
    bool save() override;
    void logState() const override;

private:
    std::string _path;

    void _maybe_seed_from_env();
};
