/**
 * @file system_config_std.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "system_config_std.h"
#include "fmt/base.h"
#include <cstdio>
#include <mooncake_log.h>
#include <ArduinoJson.h>
#include <string>

static const char* _tag = "syscfg";

SystemConfigStd::SystemConfigStd(const std::string& rootPath)
{
    _system_config_path = rootPath + "system_config.json";
}

bool SystemConfigStd::loadConfig()
{
    mclog::tagInfo(_tag, "load config from fs");

    // Open config file
    FILE* config_file = fopen(_system_config_path.c_str(), "rb");
    if (config_file == NULL) {
        // If it does not exist, create a new one
        mclog::warn("{} not exist, try creating", _system_config_path);
        saveConfig();
        backup_config_file();
        return true;
    }

    // Read file content
    char* file_content = 0;
    long file_length = 0;
    fseek(config_file, 0, SEEK_END);
    file_length = ftell(config_file);
    fseek(config_file, 0, SEEK_SET);
    file_content = (char*)malloc(file_length);

    // If memory allocation fails (usually due to a file size issue from corruption), try reading the backup file
    if (!file_content) {
        fclose(config_file);

        mclog::error("malloc failed, size: {}", file_length);

        std::string backup_path = _system_config_path + ".bk";
        mclog::tagInfo(_tag, "try {}", backup_path);

        config_file = fopen(backup_path.c_str(), "rb");

        // If opening the backup fails, recreate everything
        if (config_file == NULL) {
            mclog::error("open backup failed, try recreating..");
            saveConfig();
            backup_config_file();
            return true;
        }
    } else {
        fread(file_content, 1, file_length, config_file);
        fclose(config_file);
    }

    // Parse the json content and save into the current config
    if (!parse_json_and_copy_config(file_content)) {
        // If parsing fails, recreate
        mclog::tagInfo(_tag, " try recreating..");
        saveConfig();
        backup_config_file();
    }

    free(file_content);
    return true;
}

bool SystemConfigStd::saveConfig()
{
    mclog::tagInfo(_tag, "save config to fs");

    // Back up the original
    backup_config_file();

    // Serialize current config to json
    std::string json_content = create_config_json();

    // Open config file and write
    mclog::tagInfo(_tag, "open {}", _system_config_path);
    FILE* config_file = fopen(_system_config_path.c_str(), "wb");
    if (config_file == NULL) {
        mclog::error("open failed");
        return false;
    }

    fputs(json_content.c_str(), config_file);
    fclose(config_file);

    mclog::tagInfo(_tag, "config saved at {}", _system_config_path);
    return true;
}

std::string SystemConfigStd::create_config_json()
{
    JsonDocument doc;

    // Copy config
    doc["mute"] = _config.mute;
    doc["hapticFeedback"] = _config.hapticFeedback;
    doc["watchFace"] = _config.watchFace;
    doc["widgetA"] = _config.widgetA;
    doc["widgetB"] = _config.widgetB;

    // Serialize json
    std::string json_content;
    if (serializeJson(doc, json_content) == 0) {
        json_content.clear();
        mclog::error("serialize failed");
    }

    return json_content;
}

bool SystemConfigStd::parse_json_and_copy_config(char* jsonContent)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonContent);
    if (error != DeserializationError::Ok) {
        mclog::error("parse json failed");
        return false;
    }

    // Copy config
    _config.mute = doc["mute"];
    _config.hapticFeedback = doc["hapticFeedback"];
    _config.watchFace = doc["watchFace"].as<std::string>();
    _config.widgetA = doc["widgetA"].as<std::string>();
    _config.widgetB = doc["widgetB"].as<std::string>();

    return true;
}

void SystemConfigStd::backup_config_file()
{
    mclog::tagInfo(_tag, "create config backup");

    // Open config file
    mclog::tagInfo(_tag, "try open {}", _system_config_path);
    FILE* config_file = fopen(_system_config_path.c_str(), "rb");
    if (config_file == NULL) {
        mclog::error("open failed");
        return;
    }

    // Create backup config file
    std::string backup_path = _system_config_path + ".bk";
    mclog::tagInfo(_tag, "try open {}", backup_path);
    FILE* config_backup_file = fopen(backup_path.c_str(), "wb");
    if (config_backup_file == NULL) {
        mclog::error("open failed");
        return;
    }

    // Copy
    char* buffer = new char[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, 1024, config_file)) > 0) {
        fwrite(buffer, 1, bytesRead, config_backup_file);
    }
    delete[] buffer;

    fclose(config_file);
    fclose(config_backup_file);
}

void SystemConfigStd::logConfig()
{
    mclog::tagInfo(_tag, "current system config:");
    fmt::println("mute: {}", _config.mute);
    fmt::println("hapticFeedback: {}", _config.hapticFeedback);
    fmt::println("watchFace: {}", _config.watchFace);
    fmt::println("widgetA: {}", _config.widgetA);
    fmt::println("widgetB: {}", _config.widgetB);
}
