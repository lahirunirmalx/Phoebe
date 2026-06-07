/**
 * @file system_config_std.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <hal/hal.h>
#include <string>

// Standard C file library + ArduinoJson implementation
class SystemConfigStd : public hal_components::SystemConfigBase {
public:
    SystemConfigStd(const std::string& rootPath = "./");

    bool loadConfig() override;
    bool saveConfig() override;
    void logConfig() override;

private:
    std::string _system_config_path;

    std::string create_config_json();
    bool parse_json_and_copy_config(char* jsonContent);
    void backup_config_file();
    void apply_env_overrides();
};
