/**
 * @file hal_esp32.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "hal_esp32.h"
#include "hal_config.h"
// SystemCtrl is the base stub on the test platform -- the production Arduino
// impl latches a power-hold GPIO and runs a 6s app-level watchdog that nothing
// on the test board is feeding, so it would reboot every 6s.
#include "components/display/display.h"
#include "components/wifi_manager/wifi_manager_esp32.h"
#include "components/http_client/http_client_arduino.h"
// wear_levelling helper not used on test platform; fs_init() is a no-op.

// Forward decls from components/display/display.cpp
namespace ssd1306 { void chip_init(); void blit_rgb565(const uint16_t* src); void flush(); }

// Minimal SystemControl impl for the test board: just millis() and delay()
// via esp_timer / FreeRTOS, no power-mos latch, no app-level watchdog.
class SystemControlTest : public hal_components::SystemControlBase {
public:
    std::uint32_t millis() override
    {
        return (std::uint32_t)(esp_timer_get_time() / 1000);
    }
    void delay(std::uint32_t ms) override
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    void reboot() override { esp_restart(); }
};
#include <cstdint>
#include <mooncake_log.h>
#include <Arduino.h>
#include <driver/i2c.h>
#include <lvgl.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>

// Component instance creation and miscellaneous initialization
void HalEsp32::init()
{
    initArduino();

    // File system
    fs_init();

    // System control -- minimal millis/delay impl for the test board.
    _components.system_control = std::make_unique<SystemControlTest>();

    // No buttons on the test board, and the production pin map collides with
    // GPIO 6-11 which are wired to the SPI flash on a WROOM-32D -- configuring
    // those pins triggers a watchdog reset. Use the base stub.
    _components.button = std::make_unique<hal_components::ButtonBase>();

    // I2C
    i2c_init();

    // Test board has no real IMU / buzzer / haptic / battery hardware -- use
    // base stubs so HAL accessors still work but nothing tries to talk to
    // chips that aren't on the bus.
    _components.battery_monitor = std::make_unique<hal_components::BatteryMonitorBase>();
    _components.imu = std::make_unique<hal_components::ImuBase>();
    _components.buzzer = std::make_unique<hal_components::BuzzerBase>();
    _components.haptic_engine = std::make_unique<hal_components::HapticEngineBase>();

    // SSD1306 OLED chip on the I2C bus
    ssd1306::chip_init();

    // Lvgl
    lvgl_init();

    // Display component
    _components.display = std::make_unique<DisplaySsd1306>();
    _components.display->init();

    // WiFi (NVS-backed credentials; ESP-IDF wifi stack drives the radio).
    _components.wifi = std::make_unique<WifiManagerEsp32>();
    _components.wifi->load();
    if (_components.wifi->hasCredentials()) {
        _components.wifi->connect();
    }
    _components.wifi->logState();

    // HTTP client (Arduino HTTPClient on ESP32).
    _components.http_client = std::make_unique<HttpClientArduino>();

    // Bootstrap the claude endpoint into SysCfg from NVS namespace "claude".
    // (Desktop persists this in system_config.json; on ESP32 it lives in NVS
    // alongside the wifi credentials, schema matches M5Cardputer-UserDemo.)
    {
        nvs_handle_t h;
        if (nvs_open("claude", NVS_READONLY, &h) == ESP_OK) {
            char buf[160];
            size_t sz;
            sz = sizeof(buf);
            if (nvs_get_str(h, "base", buf, &sz) == ESP_OK) {
                HAL::SysCfg().setConfig().claudeBase = buf;
            }
            sz = sizeof(buf);
            if (nvs_get_str(h, "bearer", buf, &sz) == ESP_OK) {
                HAL::SysCfg().setConfig().claudeBearer = buf;
            }
            nvs_close(h);
            mclog::tagInfo("claudecfg", "loaded base: {}  has_bearer: {}",
                           HAL::SysCfg().getConfig().claudeBase.empty()
                               ? "<unset>" : HAL::SysCfg().getConfig().claudeBase,
                           !HAL::SysCfg().getConfig().claudeBearer.empty());
        } else {
            mclog::tagWarn("claudecfg", "no NVS entries for claude namespace");
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                     I2C                                    */
/* -------------------------------------------------------------------------- */
void HalEsp32::i2c_init()
{
    const std::string tag = "i2c";
    mclog::tagInfo(tag, "init");

    // Initialize
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = HAL_PIN_IMU_I2C_BUS_SDA;
    conf.scl_io_num = HAL_PIN_IMU_I2C_BUS_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;
    i2c_param_config(HAL_I2C_BUS_PORT_NUM, &conf);

    if (i2c_driver_install(HAL_I2C_BUS_PORT_NUM, conf.mode, 0, 0, 0) != ESP_OK) {
        mclog::tagError(tag, "i2c driver install failed");
    }

    // Scan
    uint8_t device_num = 0;
    uint8_t WRITE_BIT = I2C_MASTER_WRITE;
    uint8_t ACK_CHECK_EN = 0x1;
    uint8_t address;
    mclog::tagInfo(tag, "scan bus..");
    for (int i = 0; i < 128; i += 16) {
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(HAL_I2C_BUS_PORT_NUM, cmd, portMAX_DELAY);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                if (address == 0)
                    continue;
                mclog::tagInfo(tag, ">> {:#X}", address);
                device_num++;
            }
        }
    }
    mclog::tagInfo(tag, "found {} device", device_num);
}

/* -------------------------------------------------------------------------- */
/*                                    MLCD                                    */
/* --------------------------------------------------------------------------
 * The test board uses an SSD1306 OLED instead -- the chip-level init lives
 * in components/display/display.cpp; HalEsp32::init() calls
 * ssd1306::chip_init() directly. No mlcd_init() needed here.
 */

/* -------------------------------------------------------------------------- */
/*                                    Lvgl                                    */
/* -------------------------------------------------------------------------- */
static uint8_t* _lvgl_buffer = nullptr;

// Exposed for use by the Display component
uint8_t* __get_lvgl_buffer()
{
    return _lvgl_buffer;
}

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* /*area*/, uint8_t* px_map)
{
    // Threshold-convert LVGL's RGB565 buffer onto the SSD1306 framebuffer,
    // then push the whole page-column block over I2C.
    ssd1306::blit_rgb565(reinterpret_cast<const uint16_t*>(px_map));
    ssd1306::flush();
    lv_display_flush_ready(disp);
}

static void lvgl_tick_timer(void* arg)
{
    (void)arg;
    lv_tick_inc(10);
}

void HalEsp32::lvgl_init()
{
    const std::string tag = "lvgl";
    mclog::tagInfo(tag, "init");

    lv_init();

    // Display
    mclog::tagInfo(tag, "create display");
    auto display = lv_display_create(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    mclog::tagInfo(tag, "create display buffer");
    _lvgl_buffer = (uint8_t*)malloc(HAL_SCREEN_WIDTH * HAL_SCREEN_HEIGHT * sizeof(uint16_t));
    lv_display_set_buffers(display, (void*)_lvgl_buffer, NULL, HAL_SCREEN_WIDTH * HAL_SCREEN_HEIGHT * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_FULL);

    // Tick
    mclog::tagInfo(tag, "create tick timer");
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer, .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
}

/* -------------------------------------------------------------------------- */
/*                                     FS                                     */
/* -------------------------------------------------------------------------- */
void HalEsp32::fs_init()
{
    // No-op on the test platform. The wear_levelling helper used by the
    // production phoebe board targets IDF 5.x APIs that aren't available in
    // the IDF 4.4 toolchain on this dev box; nothing in AppClaudeMeter / the
    // watch faces needs a writable FAT volume, so skip the mount.
    mclog::tagInfo("fs", "skipped (test platform)");
}
