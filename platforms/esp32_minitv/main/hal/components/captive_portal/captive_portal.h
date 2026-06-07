/**
 * @file captive_portal.h
 * @brief WiFi setup captive portal for the Freenove Mini TV.
 *
 * Brings up a SoftAP + DNS catch-all + web form pre-filled from the current
 * SystemConfig. On submit it writes the settings to NVS (via HAL::SysCfg) and
 * reboots. Runs on the network core; the UI core shows a "WiFi Setup" screen
 * while ui_signals::portal_active is set.
 */
#pragma once

namespace captive_portal {

constexpr const char* AP_SSID = "Phoebe-Setup";
constexpr const char* AP_PASS = "12345678";

// Bring up AP + server + DNS, then service requests forever (a successful save
// reboots the device). Call from the network task when a portal is requested.
void run();

} // namespace captive_portal
