/**
 * @file data_service.h
 * @brief Shared background data layer for the Phoebe apps.
 *
 * A single process-wide service owns ONE network thread that fetches every
 * remote source (Claude usage, weather, forecast/sun, AQI, currency, next
 * meeting, network ping, uptime) strictly one at a time -- so only one TLS
 * connection is ever open (concurrent handshakes exhaust heap on the ESP32).
 *
 * Independent apps read the latest snapshot through the thread-safe getters
 * instead of fetching themselves; the data keeps flowing regardless of which
 * app is currently open. The service also drives the ambient backlight pulse
 * on Claude fetch / error / limit-reached.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace appdata {

enum FetchState { Fetch_Idle = 0, Fetch_OK, Fetch_Err };

struct ClaudeData {
    float pct_five_hour = -1.0f;
    float pct_seven_day = -1.0f;
    FetchState state = Fetch_Idle;
    std::string last_err;
};

struct WeatherData {
    float temp_c = -1000.0f;
    float humidity = -1.0f;
    float wind_kmh = -1.0f;
    int code = -1; // WMO weather code
    bool ok = false;
    std::string err;
};

struct DayFc {
    int code = -1;
    float tmax = 0, tmin = 0;
    int wday = 0;
};

struct ForecastData {
    DayFc d[3];
    bool ok = false;
    std::string err;
};

struct SunData {
    char rise[6] = "--:--";
    char set[6] = "--:--";
    bool ok = false;
    std::string err;
};

struct AqiData {
    int aqi = -1;
    float pm25 = -1;
    bool ok = false;
    std::string err;
};

struct CurrencyData {
    float usd = -1, eur = -1, gbp = -1; // 1 unit -> LKR
    bool ok = false;
    std::string err;
};

struct NetData {
    int latency_ms = -1;
    bool ok = false;
    std::string err;
};

struct MeetingData {
    std::string title;
    long start_epoch = 0;
    bool ok = false;
    std::string err;
};

struct UpData {
    struct Site {
        char host[22] = "";
        bool up = false;
        int ms = 0;
    } sites[5];
    int count = 0;
    bool ok = false;
    std::string err;
};

/**
 * @brief Process-wide background data service (singleton).
 *
 * Call start() once after the HAL is up (e.g. from the navigator's onCreate);
 * the getters are safe to call from the UI thread at any time and return a
 * consistent copy of the latest snapshot.
 */
class DataService {
public:
    static DataService& instance();

    void start(); // idempotent: spins up the network thread if not running
    void stop();  // joins the network thread

    ClaudeData   claude() const;
    WeatherData  weather() const;
    ForecastData forecast() const;
    SunData      sun() const;
    AqiData      aqi() const;
    CurrencyData currency() const;
    NetData      net() const;
    MeetingData  meeting() const;
    UpData       uptime() const;

    DataService(const DataService&) = delete;
    DataService& operator=(const DataService&) = delete;

private:
    DataService() = default;
    ~DataService();

    void loop(); // the unified fetch loop (runs on _thread)

    // Fetchers (no LVGL; safe off the UI thread). Return true on success.
    bool fetch_claude(ClaudeData& out);
    bool fetch_weather(WeatherData& out);
    bool fetch_daily(ForecastData& fc, SunData& sun); // one Open-Meteo call feeds both
    bool fetch_aqi(AqiData& out);
    bool fetch_currency(CurrencyData& out);
    bool fetch_meeting(MeetingData& out);
    bool fetch_net(NetData& out);
    bool fetch_uptime(UpData& out);

    std::thread _thread;
    std::atomic<bool> _stop{false};
    mutable std::mutex _mtx;

    ClaudeData _claude;
    WeatherData _weather;
    ForecastData _forecast;
    SunData _sun;
    AqiData _aqi;
    CurrencyData _currency;
    NetData _net;
    MeetingData _meeting;
    UpData _uptime;
};

} // namespace appdata
