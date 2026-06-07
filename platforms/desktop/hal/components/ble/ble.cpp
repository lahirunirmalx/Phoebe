/**
 * @file ble.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-11-22
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "ble.h"
#include <mooncake_log.h>
#include <iostream>
#include <thread>
#include <cstdio>
#include <memory>
#include <sys/stat.h>
#include <shared/shared.h>

#ifndef PHOEBE_SOURCE_DIR
#define PHOEBE_SOURCE_DIR "."
#endif

static const std::string _tag = "BlePython";

static void _ble_python_daemon()
{
    mclog::tagInfo(_tag, "start ble python daemon");

    // Resolve the BLE simulator script path relative to the repo source dir
    // (passed in via CMake) so it works regardless of where the binary is run from.
    std::string script_path =
        std::string(PHOEBE_SOURCE_DIR) + "/platforms/desktop/hal/components/ble/desktop_ble_server.py";

    struct stat st;
    if (stat(script_path.c_str(), &st) != 0) {
        mclog::tagWarn(_tag, "ble python script not found at {}, skipping daemon", script_path);
        return;
    }

    FILE* pipe = popen(("python3 -u " + script_path).c_str(), "r");
    if (!pipe) {
        mclog::tagError(_tag, "popen python script failed");
        return;
    }

    // Read output in real time
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        mclog::tagInfo(_tag, "get msg: {}", buffer);
        SharedData::Borrow();
        SharedData::Ble().messageList.push_back(buffer);
        SharedData::Return();
    }

    // Close pipe
    pclose(pipe);
    mclog::tagInfo(_tag, "ble python daemon stop");
}

void BlePython::init()
{
    mclog::tagInfo(_tag, "init");

    std::thread ble_python_daemon(_ble_python_daemon);
    ble_python_daemon.detach();
}
