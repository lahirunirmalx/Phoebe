/**
 * @file app_claudemeter.cpp
 * @brief Claude usage meter app -- clock + meter views, click toggles.
 *        Real-time data: a background thread polls <base>/usage and updates
 *        a mutex-guarded snapshot. The UI reads the snapshot each tick.
 */
#include "app_claudemeter.h"
#include "hal/hal.h"
#include <ArduinoJson.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <curl/curl.h>
#include <mooncake_log.h>
#include <lvgl.h>

using namespace mooncake;

namespace {

constexpr int SCREEN_W = 144;
constexpr int SCREEN_H = 168;

// Clock canvas geometry (square)
constexpr int CLOCK_CANVAS_W = 110;
constexpr int CLOCK_CANVAS_H = 110;

// Poll cadence for /usage. M5Cardputer-UserDemo uses 5 minutes; mirror it.
constexpr int FETCH_PERIOD_SEC = 300;
constexpr int FETCH_TIMEOUT_SEC = 8;

constexpr float WARN_THRESHOLD = 70.0f;
constexpr float DANGER_THRESHOLD = 90.0f;

const lv_color_t COLOR_BG = LV_COLOR_MAKE(0x00, 0x00, 0x00);
const lv_color_t COLOR_FG = LV_COLOR_MAKE(0xE6, 0xE6, 0xE6);
const lv_color_t COLOR_LABEL_DIM = LV_COLOR_MAKE(0x9A, 0x9A, 0x9A);
const lv_color_t COLOR_BAR_BG = LV_COLOR_MAKE(0x33, 0x33, 0x38);
const lv_color_t COLOR_OK = LV_COLOR_MAKE(0x99, 0xFF, 0x00);
const lv_color_t COLOR_WARN = LV_COLOR_MAKE(0xFF, 0xB0, 0x60);
const lv_color_t COLOR_DANGER = LV_COLOR_MAKE(0xFF, 0x64, 0x64);
const lv_color_t COLOR_ACCENT = LV_COLOR_MAKE(0x99, 0xFF, 0x00);

lv_color_t bar_color_for(float pct)
{
    if (pct >= DANGER_THRESHOLD) return COLOR_DANGER;
    if (pct >= WARN_THRESHOLD) return COLOR_WARN;
    return COLOR_OK;
}

size_t curl_collect(void* contents, size_t size, size_t nmemb, void* userp)
{
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string trim_trailing_slash(const std::string& s)
{
    size_t end = s.size();
    while (end > 0 && s[end - 1] == '/') end--;
    return s.substr(0, end);
}

} // namespace

AppClaudeMeter::AppClaudeMeter()
{
    setAppInfo().name = "AppClaudeMeter";
}

void AppClaudeMeter::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
    open();
}

void AppClaudeMeter::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(lv_screen_active(), COLOR_BG, 0);

    _build_ui();
    _show_view(_view);
    _update_clock();
    _update_meter();

    _start_fetch_thread();
}

void AppClaudeMeter::onRunning()
{
    lv_timer_handler();

    const std::uint32_t now_ms = HAL::SysCtrl().millis();
    if (now_ms - _last_tick_ms < 500) return;
    _last_tick_ms = now_ms;

    if (_view == VIEW_CLOCK) {
        _update_clock();
    } else {
        // If we have no live data yet, drift the mock values so the bars
        // remain visibly alive during development.
        Snapshot snap;
        {
            std::lock_guard<std::mutex> lock(_snapshot_mutex);
            snap = _snapshot;
        }
        if (snap.state != Fetch_OK) {
            _mock_pct_five_hour += 0.7f;
            if (_mock_pct_five_hour > 100.0f) _mock_pct_five_hour = 0.0f;
            _mock_pct_seven_day += 0.3f;
            if (_mock_pct_seven_day > 100.0f) _mock_pct_seven_day = 0.0f;
        }
        _update_meter();
    }
}

void AppClaudeMeter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _stop_fetch_thread();

    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    delete[] _clock_canvas_buf;
    _clock_canvas_buf = nullptr;
}

void AppClaudeMeter::_build_ui()
{
    _root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_set_style_bg_color(_root, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
    lv_obj_add_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_root, &AppClaudeMeter::_on_root_clicked, LV_EVENT_CLICKED, this);

    _build_clock_view();
    _build_meter_view();
}

void AppClaudeMeter::_build_clock_view()
{
    _clock_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_clock_container);
    lv_obj_set_size(_clock_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_clock_container, 0, 0);
    lv_obj_set_style_bg_color(_clock_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_clock_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_clock_container, LV_OBJ_FLAG_CLICKABLE);

    // Analog clock face fills most of the screen; digital time sits below it.
    // The canvas no longer overlaps the date text because the digital text
    // is anchored to the bottom edge.
    _clock_canvas_buf = new std::uint8_t[CLOCK_CANVAS_W * CLOCK_CANVAS_H * 2];
    _clock_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_canvas, _clock_canvas_buf, CLOCK_CANVAS_W, CLOCK_CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_canvas, LV_ALIGN_TOP_MID, 0, 6);

    _clock_time_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_BOTTOM_MID, 0, -22);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_build_meter_view()
{
    _meter_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_meter_container);
    lv_obj_set_size(_meter_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_meter_container, 0, 0);
    lv_obj_set_style_bg_color(_meter_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_meter_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_meter_container, LV_OBJ_FLAG_CLICKABLE);

    _meter_title_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_title_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_meter_title_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_title_label, "CLAUDE METER");
    lv_obj_align(_meter_title_label, LV_ALIGN_TOP_MID, 0, 6);

    // 5-hour row
    _meter_h5_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_h5_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_h5_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_h5_label, "5H");
    lv_obj_align(_meter_h5_label, LV_ALIGN_TOP_LEFT, 6, 36);

    _meter_h5_pct_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_font(_meter_h5_pct_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_meter_h5_pct_label, "--");
    lv_obj_align(_meter_h5_pct_label, LV_ALIGN_TOP_RIGHT, -6, 28);

    _meter_h5_bar = lv_bar_create(_meter_container);
    lv_obj_set_size(_meter_h5_bar, SCREEN_W - 16, 10);
    lv_obj_align(_meter_h5_bar, LV_ALIGN_TOP_LEFT, 8, 60);
    lv_bar_set_range(_meter_h5_bar, 0, 100);
    lv_obj_set_style_bg_color(_meter_h5_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_h5_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_h5_bar, 2, LV_PART_INDICATOR);

    // 7-day row
    _meter_d7_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_d7_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_d7_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_d7_label, "7D");
    lv_obj_align(_meter_d7_label, LV_ALIGN_TOP_LEFT, 6, 92);

    _meter_d7_pct_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_font(_meter_d7_pct_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_meter_d7_pct_label, "--");
    lv_obj_align(_meter_d7_pct_label, LV_ALIGN_TOP_RIGHT, -6, 84);

    _meter_d7_bar = lv_bar_create(_meter_container);
    lv_obj_set_size(_meter_d7_bar, SCREEN_W - 16, 10);
    lv_obj_align(_meter_d7_bar, LV_ALIGN_TOP_LEFT, 8, 116);
    lv_bar_set_range(_meter_d7_bar, 0, 100);
    lv_obj_set_style_bg_color(_meter_d7_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_d7_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_d7_bar, 2, LV_PART_INDICATOR);

    _meter_status_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_status_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_status_label, "");
    lv_obj_align(_meter_status_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_show_view(View v)
{
    _view = v;
    if (v == VIEW_CLOCK) {
        lv_obj_clear_flag(_clock_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_meter_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_clock_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_meter_container, LV_OBJ_FLAG_HIDDEN);
    }
}

void AppClaudeMeter::_toggle_view()
{
    _show_view(_view == VIEW_CLOCK ? VIEW_METER : VIEW_CLOCK);
    if (_view == VIEW_CLOCK) {
        _update_clock();
    } else {
        _update_meter();
    }
}

void AppClaudeMeter::_update_clock()
{
    if (!_clock_canvas) return;

    time_t now;
    struct tm* tm_info;
    time(&now);
    tm_info = localtime(&now);
    if (!tm_info) return;

    char time_buf[16];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    lv_label_set_text(_clock_time_label, time_buf);

    char date_buf[24];
    std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
                  tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
    lv_label_set_text(_clock_date_label, date_buf);

    lv_canvas_fill_bg(_clock_canvas, COLOR_BG, LV_OPA_COVER);

    const int cx = CLOCK_CANVAS_W / 2;
    const int cy = CLOCK_CANVAS_H / 2;
    const int hour = tm_info->tm_hour % 12;
    const int minute = tm_info->tm_min;
    const int second = tm_info->tm_sec;

    const float hour_angle = (hour + minute / 60.0f) * 30.0f * (float)M_PI / 180.0f;
    const float minute_angle = (minute + second / 60.0f) * 6.0f * (float)M_PI / 180.0f;
    const float second_angle = second * 6.0f * (float)M_PI / 180.0f;

    lv_layer_t layer;
    lv_canvas_init_layer(_clock_canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = COLOR_FG;

    auto draw_hand = [&](float angle, int length, int width) {
        dsc.width = width;
        dsc.p1.x = cx;
        dsc.p1.y = cy;
        dsc.p2.x = cx + (int)(length * std::cos(angle - (float)M_PI / 2.0f));
        dsc.p2.y = cy + (int)(length * std::sin(angle - (float)M_PI / 2.0f));
        lv_draw_line(&layer, &dsc);
    };

    draw_hand(hour_angle, 24, 5);
    draw_hand(minute_angle, 36, 3);
    dsc.color = COLOR_ACCENT;
    draw_hand(second_angle, 44, 2);

    lv_canvas_finish_layer(_clock_canvas, &layer);
}

void AppClaudeMeter::_update_meter()
{
    if (!_meter_h5_bar) return;

    Snapshot snap;
    {
        std::lock_guard<std::mutex> lock(_snapshot_mutex);
        snap = _snapshot;
    }

    float p5;
    float p7;
    const char* status_text;
    if (snap.state == Fetch_OK && snap.pct_five_hour >= 0.0f && snap.pct_seven_day >= 0.0f) {
        p5 = snap.pct_five_hour;
        p7 = snap.pct_seven_day;
        status_text = "live";
    } else {
        p5 = _mock_pct_five_hour;
        p7 = _mock_pct_seven_day;
        status_text = snap.state == Fetch_Err ? snap.last_err.c_str() : "mock";
    }

    char buf[8];
    lv_color_t h5c = bar_color_for(p5);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(p5 + 0.5f));
    lv_label_set_text(_meter_h5_pct_label, buf);
    lv_obj_set_style_text_color(_meter_h5_pct_label, h5c, 0);
    lv_bar_set_value(_meter_h5_bar, (int)(p5 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_h5_bar, h5c, LV_PART_INDICATOR);

    lv_color_t d7c = bar_color_for(p7);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(p7 + 0.5f));
    lv_label_set_text(_meter_d7_pct_label, buf);
    lv_obj_set_style_text_color(_meter_d7_pct_label, d7c, 0);
    lv_bar_set_value(_meter_d7_bar, (int)(p7 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_d7_bar, d7c, LV_PART_INDICATOR);

    lv_label_set_text(_meter_status_label, status_text);
}

void AppClaudeMeter::_on_root_clicked(lv_event_t* e)
{
    auto* self = static_cast<AppClaudeMeter*>(lv_event_get_user_data(e));
    if (!self) return;
    self->_toggle_view();
}

/* ------------------------------ HTTP fetch ------------------------------ */

void AppClaudeMeter::_start_fetch_thread()
{
    if (_fetch_thread.joinable()) return;
    _fetch_stop.store(false);
    _fetch_thread = std::thread([this] { _fetch_loop(); });
}

void AppClaudeMeter::_stop_fetch_thread()
{
    _fetch_stop.store(true);
    if (_fetch_thread.joinable()) _fetch_thread.join();
}

void AppClaudeMeter::_fetch_loop()
{
    // First-fetch attempt on startup, then poll every FETCH_PERIOD_SEC.
    while (!_fetch_stop.load()) {
        Snapshot fresh;
        bool ok = _fetch_once(fresh);

        {
            std::lock_guard<std::mutex> lock(_snapshot_mutex);
            if (ok) {
                _snapshot = fresh;
            } else {
                _snapshot.state = Fetch_Err;
                _snapshot.last_err = fresh.last_err;
            }
        }

        // Sleep in short slices so stop is responsive.
        for (int i = 0; i < FETCH_PERIOD_SEC * 4 && !_fetch_stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

bool AppClaudeMeter::_fetch_once(Snapshot& out)
{
    auto& cfg = HAL::ClaudeCfg();
    if (!cfg.isReady()) {
        out.last_err = "no cfg";
        return false;
    }

    std::string url = trim_trailing_slash(cfg.getBaseUrl()) + "/usage";
    std::string auth_header = "Authorization: Bearer " + cfg.getBearer();

    CURL* curl = curl_easy_init();
    if (!curl) {
        out.last_err = "curl init";
        return false;
    }

    std::string body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)FETCH_TIMEOUT_SEC);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        out.last_err = std::string("net: ") + curl_easy_strerror(rc);
        return false;
    }
    if (http_code != 200) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "HTTP %ld", http_code);
        out.last_err = buf;
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        out.last_err = "bad json";
        return false;
    }

    if (doc["five_hour"]["utilization"].is<float>()) {
        out.pct_five_hour = doc["five_hour"]["utilization"].as<float>();
    }
    if (doc["seven_day"]["utilization"].is<float>()) {
        out.pct_seven_day = doc["seven_day"]["utilization"].as<float>();
    }

    if (out.pct_five_hour < 0.0f && out.pct_seven_day < 0.0f) {
        out.last_err = "no fields";
        return false;
    }

    out.state = Fetch_OK;
    return true;
}
