/**
 * @file captive_portal.cpp
 * @brief See captive_portal.h. ESP32 (arduino-esp32) WebServer + DNSServer,
 *        modelled on dev-mini-screen/esp_mini_screen_claude_clock.
 */
#include "captive_portal.h"
#include "../../ui_signals.h"
#include <hal/hal.h>
#include <weather_locations.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Arduino.h>
#include <mooncake_log.h>
#include <string>

namespace captive_portal {

static const char* TAG = "portal";
static WebServer server(80);
static DNSServer dns;

static std::string html_escape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&#39;"; break;
            default: o += c;
        }
    }
    return o;
}

// Build a <select name=..> with options; marks `current` selected.
static std::string select_field(const char* name, const char* const* opts, int n,
                                 const std::string& current)
{
    std::string s = "<select name='";
    s += name;
    s += "'>";
    for (int i = 0; i < n; ++i) {
        s += "<option value='";
        s += opts[i];
        s += (current == opts[i]) ? "' selected>" : "'>";
        s += opts[i];
        s += "</option>";
    }
    s += "</select>";
    return s;
}

static std::string scan_networks_options(const std::string& current)
{
    std::string s;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && i < 20; ++i) {
        std::string ssid = std::string(WiFi.SSID(i).c_str());
        if (ssid.empty()) continue;
        s += "<option value='";
        s += html_escape(ssid);
        s += (ssid == current) ? "' selected>" : "'>";
        s += html_escape(ssid);
        s += " (" + std::to_string(WiFi.RSSI(i)) + "dBm)</option>";
    }
    WiFi.scanDelete();
    return s;
}

static void handle_root()
{
    const auto& c = HAL::SysCfg().getConfig();

    static const char* faces[] = {"analog", "digital", "animated", "seg7", "vfd", "flip"};
    static const char* widgets[] = {"time", "date", "battery"};

    std::string h;
    h.reserve(4096);
    h += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>Phoebe Setup</title><style>"
         "body{font-family:sans-serif;background:#12121c;color:#eee;margin:0;padding:22px}"
         "h2{color:#9f0;margin-top:0}.c{max-width:430px;margin:0 auto}"
         "label{display:block;margin:13px 0 4px;font-size:14px;color:#bbb}"
         "input,select{width:100%;box-sizing:border-box;padding:10px;border-radius:8px;"
         "border:1px solid #333;background:#1d1d2b;color:#eee;font-size:15px}"
         "input[type=checkbox]{width:auto}"
         ".row{display:flex;gap:8px;align-items:center;margin-top:12px}"
         "button{margin-top:22px;width:100%;padding:12px;border:0;border-radius:8px;"
         "background:#9f0;color:#000;font-size:16px;font-weight:600}"
         "small{color:#888}</style></head><body><div class='c'><h2>Phoebe Setup</h2>"
         "<form method='POST' action='/save'>";

    h += "<label>WiFi network</label><select name='ssid'>";
    if (!c.wifiSsid.empty()) {
        h += "<option value='" + html_escape(c.wifiSsid) + "' selected>" +
             html_escape(c.wifiSsid) + " (current)</option>";
    }
    h += scan_networks_options(c.wifiSsid);
    h += "</select>";
    h += "<input name='ssid_manual' placeholder='or type a different SSID'>";

    h += "<label>WiFi password</label><input name='pass' type='password' value='" +
         html_escape(c.wifiPassword) + "'>";

    h += "<label>Claude base URL</label><input name='base' placeholder='http://192.168.1.10:7878' value='" +
         html_escape(c.claudeBase) + "'>";
    h += "<label>Claude bearer token</label><input name='bearer' type='password' value='" +
         html_escape(c.claudeBearer) + "'>";

    h += "<label>Watch face</label>" + select_field("watchFace", faces, 6, c.watchFace);
    h += "<label>Widget A</label>" + select_field("widgetA", widgets, 3, c.widgetA);
    h += "<label>Widget B</label>" + select_field("widgetB", widgets, 3, c.widgetB);

    // Weather location (Sri Lanka cities).
    h += "<label>Weather location</label><select name='city'>";
    for (int i = 0; i < weather::kLocationCount; ++i) {
        const char* n = weather::kLocations[i].name;
        h += "<option value='" + std::string(n) + "'";
        if (c.weatherCity == n) h += " selected";
        h += ">" + std::string(n) + "</option>";
    }
    h += "</select>";

    // Timezone (offset from UTC, in minutes).
    static const int tz_off[]  = {-480, -420, -360, -300, -240, -180, -60, 0, 60, 120, 180,
                                  210, 240, 270, 300, 330, 345, 360, 420, 480, 540, 570, 600, 660, 720};
    static const char* tz_lbl[] = {"UTC-8", "UTC-7", "UTC-6", "UTC-5", "UTC-4", "UTC-3", "UTC-1",
                                   "UTC", "UTC+1", "UTC+2", "UTC+3", "UTC+3:30", "UTC+4", "UTC+4:30",
                                   "UTC+5", "UTC+5:30", "UTC+5:45", "UTC+6", "UTC+7", "UTC+8",
                                   "UTC+9", "UTC+9:30", "UTC+10", "UTC+11", "UTC+12"};
    h += "<label>Timezone</label><select name='tz'>";
    for (size_t i = 0; i < sizeof(tz_off) / sizeof(tz_off[0]); ++i) {
        h += "<option value='" + std::to_string(tz_off[i]) + "'";
        if (tz_off[i] == c.tzOffsetMin) h += " selected";
        h += ">" + std::string(tz_lbl[i]) + "</option>";
    }
    h += "</select>";

    h += "<label>Calendar .ics URL (next meeting)</label>"
         "<input name='ics' placeholder='https://calendar.../basic.ics' value='" +
         html_escape(c.icsUrl) + "'>";

    h += "<label>Uptime URLs (up to 5, space/comma separated)</label>"
         "<input name='urls' placeholder='https://a.com https://b.com' value='" +
         html_escape(c.uptimeUrls) + "'>";

    // Screen rotation: ordered, comma-separated keys. Reorder to change the cycle
    // order; delete a key to hide that screen. Empty = all, default order.
    h += "<label>Screens (ordered; remove to hide, reorder to change)</label>"
         "<input name='screens' placeholder='clock,meter,weather,...' value='" +
         html_escape(c.screenOrder) + "'>"
         "<small>keys: clock meter weather pomodoro world meeting currency aqi "
         "forecast sunmoon network uptime pet saver life matrix</small>";

    h += "<div class='row'><input type='checkbox' name='mute' value='1'";
    if (c.mute) h += " checked";
    h += "><label style='margin:0'>Mute</label></div>";
    h += "<div class='row'><input type='checkbox' name='haptic' value='1'";
    if (c.hapticFeedback) h += " checked";
    h += "><label style='margin:0'>Haptic feedback</label></div>";

    h += "<button type='submit'>Save &amp; Reboot</button></form>"
         "<a href='/cancel' style='display:block;text-align:center;margin-top:14px;"
         "color:#9aa;font-size:14px'>Cancel &mdash; back to clock</a>"
         "</div></body></html>";

    server.send(200, "text/html", h.c_str());
}

static void handle_save()
{
    auto& c = HAL::SysCfg().setConfig();

    std::string ssid = server.hasArg("ssid_manual") && server.arg("ssid_manual").length()
                           ? std::string(server.arg("ssid_manual").c_str())
                           : std::string(server.arg("ssid").c_str());
    if (!ssid.empty()) c.wifiSsid = ssid;
    c.wifiPassword = std::string(server.arg("pass").c_str());
    c.claudeBase = std::string(server.arg("base").c_str());
    c.claudeBearer = std::string(server.arg("bearer").c_str());
    if (server.hasArg("watchFace")) c.watchFace = std::string(server.arg("watchFace").c_str());
    if (server.hasArg("widgetA")) c.widgetA = std::string(server.arg("widgetA").c_str());
    if (server.hasArg("widgetB")) c.widgetB = std::string(server.arg("widgetB").c_str());
    if (server.hasArg("tz")) c.tzOffsetMin = server.arg("tz").toInt();
    if (server.hasArg("city")) c.weatherCity = std::string(server.arg("city").c_str());
    if (server.hasArg("ics")) c.icsUrl = std::string(server.arg("ics").c_str());
    if (server.hasArg("urls")) c.uptimeUrls = std::string(server.arg("urls").c_str());
    if (server.hasArg("screens")) c.screenOrder = std::string(server.arg("screens").c_str());
    c.mute = server.hasArg("mute");
    c.hapticFeedback = server.hasArg("haptic");

    HAL::SysCfg().saveConfig();

    server.send(200, "text/html",
                "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#12121c;color:#eee;text-align:center;padding:48px}"
                "h2{color:#9f0}</style></head><body><h2>Saved</h2>"
                "<p>Rebooting into the watch...</p></body></html>");
    mclog::tagInfo(TAG, "settings saved, rebooting");
    delay(800);
    esp_restart();
}

static void handle_cancel()
{
    server.send(200, "text/html",
                "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#12121c;color:#eee;text-align:center;padding:48px}"
                "h2{color:#9f0}</style></head><body><h2>Cancelled</h2>"
                "<p>Returning to the clock...</p></body></html>");
    mclog::tagInfo(TAG, "setup cancelled, rebooting (no changes saved)");
    delay(800);
    esp_restart();
}

static void handle_captive()
{
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void run()
{
    mclog::tagInfo(TAG, "starting AP '{}'", AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(150);
    IPAddress ip = WiFi.softAPIP();
    mclog::tagInfo(TAG, "AP IP {}", ip.toString().c_str());

    dns.start(53, "*", ip);

    server.on("/", HTTP_GET, handle_root);
    server.on("/save", HTTP_POST, handle_save);
    server.on("/cancel", HTTP_GET, handle_cancel);
    server.on("/generate_204", handle_captive);       // Android
    server.on("/hotspot-detect.html", handle_captive); // Apple
    server.on("/connecttest.txt", handle_captive);     // Windows
    server.onNotFound(handle_captive);
    server.begin();

    ui_signals::portal_active.store(true);

    // Service forever; a successful /save reboots the device.
    while (true) {
        dns.processNextRequest();
        server.handleClient();
        delay(5);
    }
}

} // namespace captive_portal
