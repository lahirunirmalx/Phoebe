/**
 * @file ui_signals.h
 * @brief Cross-core flags between the UI task (core 1) and the network task
 *        (core 0). Defined in app_main.cpp.
 */
#pragma once
#include <atomic>

namespace ui_signals {
extern std::atomic<bool> portal_request; // touch long-press -> ask network task to open the portal
extern std::atomic<bool> portal_active;  // network task -> UI: portal is up, show the setup screen
}
