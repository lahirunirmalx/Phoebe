/**
 * @file data_service.cpp
 * @brief See data_service.h. Network/fetch logic extracted from the former
 *        monolithic AppClaudeMeter so every app shares one fetch thread.
 */
#include "data_service.h"

#include "hal/hal.h"
#include "weather_locations.h"
#include "../../app_claudemeter/ics_recur.h"
#include <ArduinoJson.h>
#include <mooncake_log.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

namespace appdata {

namespace {
const char* TAG = "data";

constexpr int FETCH_TIMEOUT_SEC = 8;
constexpr float DANGER_THRESHOLD = 90.0f;

// Per-source poll cadences (seconds). All fetches are serialized on one thread.
constexpr std::uint32_t POLL_CLAUDE_SEC   = 180;
constexpr std::uint32_t POLL_WEATHER_SEC  = 600;
constexpr std::uint32_t POLL_DAILY_SEC    = 600;
constexpr std::uint32_t POLL_AQI_SEC      = 900;
constexpr std::uint32_t POLL_CURRENCY_SEC = 1800;
constexpr std::uint32_t POLL_MEETING_SEC  = 300;
constexpr std::uint32_t POLL_NET_SEC      = 90;
constexpr std::uint32_t POLL_UPTIME_SEC   = 120;

std::string trim_trailing_slash(const std::string& s)
{
    std::string r = s;
    while (!r.empty() && r.back() == '/') r.pop_back();
    return r;
}
} // namespace

DataService& DataService::instance()
{
    static DataService svc;
    return svc;
}

DataService::~DataService()
{
    stop();
}

void DataService::start()
{
    if (_thread.joinable()) return;
    _stop.store(false);
    _thread = std::thread([this] { loop(); });
}

void DataService::stop()
{
    _stop.store(true);
    if (_thread.joinable()) _thread.join();
}

/* --------------------------------- getters -------------------------------- */

ClaudeData   DataService::claude()   const { std::lock_guard<std::mutex> l(_mtx); return _claude; }
WeatherData  DataService::weather()  const { std::lock_guard<std::mutex> l(_mtx); return _weather; }
ForecastData DataService::forecast() const { std::lock_guard<std::mutex> l(_mtx); return _forecast; }
SunData      DataService::sun()      const { std::lock_guard<std::mutex> l(_mtx); return _sun; }
AqiData      DataService::aqi()      const { std::lock_guard<std::mutex> l(_mtx); return _aqi; }
CurrencyData DataService::currency() const { std::lock_guard<std::mutex> l(_mtx); return _currency; }
NetData      DataService::net()      const { std::lock_guard<std::mutex> l(_mtx); return _net; }
MeetingData  DataService::meeting()  const { std::lock_guard<std::mutex> l(_mtx); return _meeting; }
UpData       DataService::uptime()   const { std::lock_guard<std::mutex> l(_mtx); return _uptime; }
SpeedData    DataService::speed()    const { std::lock_guard<std::mutex> l(_mtx); return _speed; }

void DataService::requestSpeedTest() { _speed_req.store(true); }

/* ---------------------------------- loop ---------------------------------- */

void DataService::loop()
{
    using BL = hal_components::BacklightBase;
    std::uint32_t last_claude = 0, last_weather = 0, last_daily = 0, last_aqi = 0;
    std::uint32_t last_cur = 0, last_net = 0, last_up = 0, last_meet = 0;
    bool first = true;
    bool prev_err = false, prev_limit = false;

    while (!_stop.load()) {
        const std::uint32_t now = HAL::SysCtrl().millis() / 1000;
        auto due = [&](std::uint32_t& last, std::uint32_t period) {
            if (first || now - last >= period) { last = now; return true; }
            return false;
        };
        auto retry = [&](std::uint32_t& last, std::uint32_t period, bool ok) {
            if (!ok && period > 25) last = now - period + 20;
        };

        // On-demand speed test (downloads a few MB) -- run ASAP when requested.
        if (_speed_req.exchange(false)) {
            { std::lock_guard<std::mutex> l(_mtx); _speed.running = true; _speed.err.clear(); }
            const int kbps = fetch_speed_kbps();
            {
                std::lock_guard<std::mutex> l(_mtx);
                _speed.running = false;
                if (kbps > 0) { _speed.down_kbps = kbps; _speed.ok = true; }
                else { _speed.ok = false; _speed.err = "failed"; }
            }
            mclog::tagInfo(TAG, "speed {}: {} kbps", kbps > 0 ? "ok" : "err", kbps);
        }

        if (due(last_claude, POLL_CLAUDE_SEC)) {
            ClaudeData f;
            bool ok = fetch_claude(f);
            {
                std::lock_guard<std::mutex> l(_mtx);
                if (ok) _claude = f;
                else { _claude.state = Fetch_Err; _claude.last_err = f.last_err; }
            }
            if (ok) {
                const bool limit = (f.pct_five_hour >= DANGER_THRESHOLD) ||
                                   (f.pct_seven_day >= DANGER_THRESHOLD);
                HAL::Backlight().notify((limit && !prev_limit) ? BL::Notify_LimitReached : BL::Notify_FetchOk);
                prev_limit = limit; prev_err = false;
                mclog::tagInfo(TAG, "claude ok: 5h={:.1f}% 7d={:.1f}%", f.pct_five_hour, f.pct_seven_day);
            } else {
                if (!prev_err) HAL::Backlight().notify(BL::Notify_FetchErr);
                prev_err = true; prev_limit = false;
                mclog::tagWarn(TAG, "claude err: {}", f.last_err);
            }
            retry(last_claude, POLL_CLAUDE_SEC, ok);
        }
        if (due(last_weather, POLL_WEATHER_SEC)) {
            WeatherData w; bool ok = fetch_weather(w);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) _weather = w; else { _weather.ok = false; _weather.err = w.err; } }
            mclog::tagInfo(TAG, "weather {}", ok ? "ok" : w.err);
            retry(last_weather, POLL_WEATHER_SEC, ok);
        }
        if (due(last_daily, POLL_DAILY_SEC)) {
            ForecastData fc; SunData sun; bool ok = fetch_daily(fc, sun);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) { _forecast = fc; _sun = sun; }
              else { _forecast.ok = false; _forecast.err = fc.err; _sun.ok = false; _sun.err = sun.err; } }
            mclog::tagInfo(TAG, "daily {}", ok ? "ok" : fc.err);
            retry(last_daily, POLL_DAILY_SEC, ok);
        }
        if (due(last_aqi, POLL_AQI_SEC)) {
            AqiData a; bool ok = fetch_aqi(a);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) _aqi = a; else { _aqi.ok = false; _aqi.err = a.err; } }
            mclog::tagInfo(TAG, "aqi {}", ok ? "ok" : a.err);
            retry(last_aqi, POLL_AQI_SEC, ok);
        }
        if (due(last_cur, POLL_CURRENCY_SEC)) {
            CurrencyData c; bool ok = fetch_currency(c);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) _currency = c; else { _currency.ok = false; _currency.err = c.err; } }
            mclog::tagInfo(TAG, "currency {}", ok ? "ok" : c.err);
            retry(last_cur, POLL_CURRENCY_SEC, ok);
        }
        if (due(last_meet, POLL_MEETING_SEC)) {
            MeetingData m; bool ok = fetch_meeting(m);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) _meeting = m; else { _meeting.ok = false; _meeting.err = m.err; } }
            retry(last_meet, POLL_MEETING_SEC, ok);
        }
        if (due(last_net, POLL_NET_SEC)) {
            NetData n; bool ok = fetch_net(n);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) _net = n; else { _net.ok = false; _net.err = n.err; } }
            retry(last_net, POLL_NET_SEC, ok);
        }
        if (due(last_up, POLL_UPTIME_SEC)) {
            UpData u; bool ok = fetch_uptime(u);
            { std::lock_guard<std::mutex> l(_mtx);
              if (ok) _uptime = u; else { _uptime.ok = false; _uptime.err = u.err; } }
            retry(last_up, POLL_UPTIME_SEC, ok);
        }

        first = false;
        for (int i = 0; i < 4 && !_stop.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

/* -------------------------------- fetchers -------------------------------- */

bool DataService::fetch_claude(ClaudeData& out)
{
    if (!HAL::SysCfg().isClaudeReady()) { out.last_err = "no cfg"; return false; }
    const auto& cfg = HAL::SysCfg().getConfig();

    std::string url = trim_trailing_slash(cfg.claudeBase) + "/usage";
    auto resp = HAL::Http().get(url, cfg.claudeBearer, FETCH_TIMEOUT_SEC);
    if (resp.http_code == 0) {
        out.last_err = resp.error.empty() ? std::string("net err") : resp.error;
        return false;
    }
    if (resp.http_code != 200) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "HTTP %d", resp.http_code);
        out.last_err = buf;
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { out.last_err = "bad json"; return false; }
    if (doc["five_hour"]["utilization"].is<float>()) out.pct_five_hour = doc["five_hour"]["utilization"].as<float>();
    if (doc["seven_day"]["utilization"].is<float>()) out.pct_seven_day = doc["seven_day"]["utilization"].as<float>();
    if (out.pct_five_hour < 0.0f && out.pct_seven_day < 0.0f) { out.last_err = "no fields"; return false; }
    out.state = Fetch_OK;
    return true;
}

bool DataService::fetch_weather(WeatherData& out)
{
    const auto& loc = weather::find(HAL::SysCfg().getConfig().weatherCity);
    char url[200];
    std::snprintf(url, sizeof(url),
                  "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
                  "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code",
                  loc.lat, loc.lon);
    auto resp = HAL::Http().get(url, "", 15);
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? (resp.error.empty() ? "net err" : resp.error)
                                      : "HTTP " + std::to_string(resp.http_code);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { out.err = "bad json"; return false; }
    JsonObject cur = doc["current"];
    if (cur.isNull()) { out.err = "no data"; return false; }
    out.temp_c = cur["temperature_2m"] | -1000.0f;
    out.humidity = cur["relative_humidity_2m"] | -1.0f;
    out.wind_kmh = cur["wind_speed_10m"] | -1.0f;
    out.code = cur["weather_code"] | -1;
    out.ok = (out.temp_c > -100.0f);
    if (!out.ok) out.err = "no fields";
    return out.ok;
}

bool DataService::fetch_daily(ForecastData& fc, SunData& sun)
{
    const auto& loc = weather::find(HAL::SysCfg().getConfig().weatherCity);
    char url[256];
    std::snprintf(url, sizeof(url),
                  "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
                  "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
                  "&forecast_days=3&timezone=auto",
                  loc.lat, loc.lon);
    auto resp = HAL::Http().get(url, "", 15);
    if (resp.http_code != 200) {
        fc.err = sun.err = (resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code));
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { fc.err = sun.err = "bad json"; return false; }
    JsonObject d = doc["daily"];
    if (d.isNull()) { fc.err = sun.err = "no data"; return false; }
    for (int i = 0; i < 3; ++i) {
        fc.d[i].code = d["weather_code"][i] | -1;
        fc.d[i].tmax = d["temperature_2m_max"][i] | 0.0f;
        fc.d[i].tmin = d["temperature_2m_min"][i] | 0.0f;
        const char* date = d["time"][i] | "";
        if (strlen(date) >= 10) {
            int y = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + (date[3]-'0');
            int mo = (date[5]-'0')*10 + (date[6]-'0') - 1;
            int da = (date[8]-'0')*10 + (date[9]-'0');
            long days = ics::tm_to_utc_epoch(y, mo, da, 0, 0, 0) / 86400;
            fc.d[i].wday = (int)(((days % 7) + 4 + 7) % 7); // 1970-01-01 = Thursday(4)
        }
    }
    fc.ok = true;
    const char* sr = d["sunrise"][0] | "";
    const char* ssr = d["sunset"][0] | "";
    if (strlen(sr) >= 16) { memcpy(sun.rise, sr + 11, 5); sun.rise[5] = 0; }
    if (strlen(ssr) >= 16) { memcpy(sun.set, ssr + 11, 5); sun.set[5] = 0; }
    sun.ok = true;
    return true;
}

bool DataService::fetch_aqi(AqiData& out)
{
    const auto& loc = weather::find(HAL::SysCfg().getConfig().weatherCity);
    char url[200];
    std::snprintf(url, sizeof(url),
                  "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.3f&longitude=%.3f"
                  "&current=european_aqi,pm2_5",
                  loc.lat, loc.lon);
    auto resp = HAL::Http().get(url, "", 12);
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { out.err = "bad json"; return false; }
    JsonObject cur = doc["current"];
    if (cur.isNull()) { out.err = "no data"; return false; }
    out.aqi = cur["european_aqi"] | -1;
    out.pm25 = cur["pm2_5"] | -1.0f;
    out.ok = (out.aqi >= 0);
    if (!out.ok) out.err = "no fields";
    return out.ok;
}

bool DataService::fetch_currency(CurrencyData& out)
{
    auto resp = HAL::Http().get("https://open.er-api.com/v6/latest/USD", "", 12);
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { out.err = "bad json"; return false; }
    float lkr = doc["rates"]["LKR"] | -1.0f;
    float eur = doc["rates"]["EUR"] | -1.0f;
    float gbp = doc["rates"]["GBP"] | -1.0f;
    if (lkr <= 0) { out.err = "no rates"; return false; }
    out.usd = lkr;
    out.eur = (eur > 0) ? lkr / eur : -1;
    out.gbp = (gbp > 0) ? lkr / gbp : -1;
    out.ok = true;
    return true;
}

bool DataService::fetch_meeting(MeetingData& out)
{
    const std::string url = HAL::SysCfg().getConfig().icsUrl;
    if (url.empty()) { out.err = "set .ics URL"; return false; }

    const int tz = HAL::SysCfg().getConfig().tzOffsetMin;
    const long now = (long)time(nullptr);
    const long now_local = now + (long)tz * 60;
    long best = 0;
    std::string best_title, cur_summary, cur_dt, cur_rrule;
    std::vector<long> cur_exdates;
    bool in_event = false;

    int code = HAL::Http().getLines(url, "", 15, [&](const char* line) {
        if (strncmp(line, "BEGIN:VEVENT", 12) == 0) {
            in_event = true;
            cur_summary.clear(); cur_dt.clear(); cur_rrule.clear(); cur_exdates.clear();
        } else if (strncmp(line, "END:VEVENT", 10) == 0) {
            const long st_utc = ics::parse_ics_dt(cur_dt, tz);
            long cand = 0;
            if (st_utc != 0) {
                if (cur_rrule.empty()) {
                    cand = (st_utc >= now) ? st_utc : 0;
                } else {
                    long until_utc = 0;
                    const ics::Recur r = ics::parse_rrule(cur_rrule, until_utc, tz);
                    const long until_local = until_utc ? until_utc + (long)tz * 60 : 0;
                    const long occ_local = ics::next_occurrence_local(
                        st_utc + (long)tz * 60, now_local, r, until_local, cur_exdates);
                    cand = occ_local ? occ_local - (long)tz * 60 : 0;
                }
            }
            if (cand >= now && cand != 0 && (best == 0 || cand < best)) {
                best = cand; best_title = cur_summary;
            }
            in_event = false;
        } else if (in_event) {
            if (strncmp(line, "SUMMARY", 7) == 0) {
                const char* c = strchr(line, ':');
                if (c) cur_summary = c + 1;
            } else if (strncmp(line, "DTSTART", 7) == 0) {
                const char* c = strchr(line, ':');
                if (c) cur_dt = c + 1;
            } else if (strncmp(line, "RRULE", 5) == 0) {
                const char* c = strchr(line, ':');
                if (c) cur_rrule = c + 1;
            } else if (strncmp(line, "EXDATE", 6) == 0) {
                const char* c = strchr(line, ':');
                if (c) {
                    std::string v = c + 1;
                    size_t j = 0;
                    while (j < v.size()) {
                        size_t k = v.find(',', j);
                        if (k == std::string::npos) k = v.size();
                        const long e = ics::parse_ics_dt(v.substr(j, k - j), tz);
                        if (e != 0) cur_exdates.push_back(e + (long)tz * 60);
                        j = k + 1;
                    }
                }
            }
        }
    });

    if (code != 200) {
        out.err = code == 0 ? "net err" : "HTTP " + std::to_string(code);
        return false;
    }
    if (best == 0) { out.err = "no upcoming"; return false; }
    out.start_epoch = best;
    out.title = best_title.empty() ? "(no title)" : best_title;
    out.ok = true;
    return true;
}

bool DataService::fetch_net(NetData& out)
{
    const std::uint32_t t0 = HAL::SysCtrl().millis();
    const int code = HAL::Http().status("http://www.gstatic.com/generate_204", 8);
    const std::uint32_t dt = HAL::SysCtrl().millis() - t0;
    if (code <= 0) { out.err = "no link"; return false; }
    out.latency_ms = (int)dt;
    out.ok = true;
    return true;
}

bool DataService::fetch_uptime(UpData& out)
{
    const std::string& cfg = HAL::SysCfg().getConfig().uptimeUrls;
    if (cfg.empty()) { out.err = "set URLs"; return false; }

    std::string urls[5];
    int n = 0;
    size_t i = 0;
    while (i < cfg.size() && n < 5) {
        while (i < cfg.size() && (cfg[i] == ' ' || cfg[i] == ',' || cfg[i] == '\n' || cfg[i] == '\r' || cfg[i] == '\t')) ++i;
        size_t start = i;
        while (i < cfg.size() && cfg[i] != ' ' && cfg[i] != ',' && cfg[i] != '\n' && cfg[i] != '\r' && cfg[i] != '\t') ++i;
        if (i > start) urls[n++] = cfg.substr(start, i - start);
    }
    if (n == 0) { out.err = "set URLs"; return false; }

    for (int k = 0; k < n; ++k) {
        std::string host = urls[k];
        size_t p = host.find("://");
        if (p != std::string::npos) host = host.substr(p + 3);
        size_t slash = host.find('/');
        if (slash != std::string::npos) host = host.substr(0, slash);
        std::snprintf(out.sites[k].host, sizeof(out.sites[k].host), "%s", host.c_str());

        const std::uint32_t t0 = HAL::SysCtrl().millis();
        const int code = HAL::Http().status(urls[k], 12);
        const std::uint32_t dt = HAL::SysCtrl().millis() - t0;
        out.sites[k].up = (code > 0 && code < 500);
        out.sites[k].ms = (int)dt;
    }
    out.count = n;
    out.ok = true;
    return true;
}

int DataService::fetch_speed_kbps()
{
    // Cloudflare's free, keyless speed endpoint returns exactly N bytes; we
    // stream + discard them and measure throughput. 2 MB ~ a few seconds.
    return HAL::Http().measureDownload("https://speed.cloudflare.com/__down?bytes=2000000", 20, 2000000);
}

} // namespace appdata
