/**
 * @file app_main.cpp
 * @brief Bring-up app for the esp32_idf_test platform.
 *
 * - Inits NVS, reads the wifi namespace (ssid/pass) and the claude namespace
 *   (base/bearer). Schema matches M5Cardputer-UserDemo's flash_nvs.sh.
 * - Inits the SSD1306 OLED on I2C (GPIO21 SDA / GPIO22 SCL / addr 0x3C).
 * - Renders a status screen and a ticking counter so the board visibly
 *   shows it's alive.
 */
#include "hal_config.h"
#include "oled/ssd1306.h"

#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>

static const char* TAG = "test";

namespace {

struct NvsCreds {
    char ssid[64] = {};
    char pass[64] = {};
    char base[160] = {};
    char bearer[160] = {};
};

bool nvs_read_string(const char* ns, const char* key, char* out, size_t cap)
{
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = cap;
    esp_err_t rc = nvs_get_str(h, key, out, &sz);
    nvs_close(h);
    return rc == ESP_OK;
}

void load_nvs(NvsCreds& c)
{
    nvs_read_string("wifi", "ssid", c.ssid, sizeof(c.ssid));
    nvs_read_string("wifi", "pass", c.pass, sizeof(c.pass));
    nvs_read_string("claude", "base", c.base, sizeof(c.base));
    nvs_read_string("claude", "bearer", c.bearer, sizeof(c.bearer));
}

void draw_status(const NvsCreds& c, int tick)
{
    oled::clear();
    oled::draw_text(0, 0, "PHOEBE TEST", true);
    oled::fill_rect(0, 10, 128, 1, true);

    // OLED only renders ~21 chars per line at 128 px / 6 px per cell, but
    // we size the buffer to fit the worst-case NVS string so the compiler's
    // format-truncation checker is happy.
    char line[200];
    std::snprintf(line, sizeof(line), "SSID: %s", c.ssid[0] ? c.ssid : "<unset>");
    oled::draw_text(0, 14, line, true);

    std::snprintf(line, sizeof(line), "API:  %s", c.base[0] ? c.base : "<unset>");
    oled::draw_text(0, 24, line, true);

    oled::draw_text(0, 34, c.bearer[0] ? "BEARER: ok" : "BEARER: missing", true);

    std::snprintf(line, sizeof(line), "TICK: %d", tick);
    oled::draw_text(0, 50, line, true);

    oled::flush();
}

} // namespace

extern "C" void app_main(void)
{
    // NVS first -- both for our own reads and so the wifi stack can init later.
    esp_err_t rc = nvs_flash_init();
    if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase, doing it now");
        nvs_flash_erase();
        nvs_flash_init();
    }

    NvsCreds creds;
    load_nvs(creds);
    ESP_LOGI(TAG, "ssid=%s pass=%s base=%s bearer=%s",
             creds.ssid[0] ? creds.ssid : "<unset>",
             creds.pass[0] ? "***" : "<unset>",
             creds.base[0] ? creds.base : "<unset>",
             creds.bearer[0] ? "***" : "<unset>");

    if (!oled::init()) {
        ESP_LOGE(TAG, "oled init failed; check wiring (SDA=GPIO%d SCL=GPIO%d addr=0x%02X)",
                 HAL_PIN_OLED_SDA, HAL_PIN_OLED_SCL, HAL_OLED_I2C_ADDR);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    int tick = 0;
    while (true) {
        draw_status(creds, tick++);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
