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
#include "components/utils/sharpe_mlcd/sharpe_mlcd.h"
#include "components/system_ctrl/system_ctrl.h"
#include "components/imu/imu.h"
#include "components/buzzer/buzzer.h"
#include "components/haptic_engine/haptic_engine.h"
#include "components/battery_monitor/battery_monitor.h"
#include "components/button/button.h"
#include "components/display/display.h"
#include "components/wifi_manager/wifi_manager_esp32.h"
#include "components/claude_config/claude_config_esp32.h"
#include "components/http_client/http_client_arduino.h"
extern "C" {
#include "components/utils/wear_levelling/wear_levelling.h"
}
#include <cstdint>
#include <mooncake_log.h>
#include <Arduino.h>
#include <driver/i2c.h>
#include <lvgl.h>
#include <esp_timer.h>

// Component instance creation and miscellaneous initialization
void HalEsp32::init()
{
    initArduino();

    // File system
    fs_init();

    // System control
    _components.system_control = std::make_unique<SystemControlArduino>();
    _components.system_control->init();

    // Button
    _components.button = std::make_unique<ButtonArduino>();
    _components.button->init();

    // I2C
    i2c_init();

    // Battery monitor
    _components.battery_monitor = std::make_unique<BatteryMonitorMAX17048>();
    _components.battery_monitor->init();

    // IMU
    _components.imu = std::make_unique<ImuBmi270>();
    _components.imu->init();

    // Buzzer
    _components.buzzer = std::make_unique<BuzzerArduino>();

    // Linear haptic motor
    _components.haptic_engine = std::make_unique<HapticEngineDRV2605L>();
    _components.haptic_engine->init();

    // Sharp MLCD
    mlcd_init();

    // Lvgl
    lvgl_init();

    // Display component
    _components.display = std::make_unique<DisplayMlcd>();
    _components.display->init();

    // WiFi (NVS-backed credentials; ESP-IDF wifi stack drives the radio).
    _components.wifi = std::make_unique<WifiManagerEsp32>();
    _components.wifi->load();
    if (_components.wifi->hasCredentials()) {
        _components.wifi->connect();
    }
    _components.wifi->logState();

    // Claude API config (NVS namespace "claude", keys "base" / "bearer").
    _components.claude_config = std::make_unique<ClaudeConfigEsp32>();
    _components.claude_config->load();
    _components.claude_config->logState();

    // HTTP client (Arduino HTTPClient on ESP32).
    _components.http_client = std::make_unique<HttpClientArduino>();

    hal_test();
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
/* -------------------------------------------------------------------------- */

static SharpeMlcd* _sharp_mlcd = nullptr;

// Exposed for use by the Display component
SharpeMlcd* __get_sharp_mlcd()
{
    return _sharp_mlcd;
}

void HalEsp32::mlcd_init()
{
    const std::string tag = "display";
    mclog::tagInfo(tag, "init");

    _sharp_mlcd = new SharpeMlcd;
    _sharp_mlcd->setConfig().screen_width = HAL_SCREEN_WIDTH;
    _sharp_mlcd->setConfig().screen_height = HAL_SCREEN_HEIGHT;
    _sharp_mlcd->setConfig().pin_scs = HAL_PIN_MLCD_SCS;
    _sharp_mlcd->setConfig().pin_sclk = HAL_PIN_MLCD_SCLK;
    _sharp_mlcd->setConfig().pin_si = HAL_PIN_MLCD_SI;
    _sharp_mlcd->setConfig().spi_clk_freq = 4000000;
    _sharp_mlcd->setConfig().rotation = 2;
    _sharp_mlcd->init();

    _sharp_mlcd->clearScreen();
}

/* -------------------------------------------------------------------------- */
/*                                    Lvgl                                    */
/* -------------------------------------------------------------------------- */
static uint8_t* _lvgl_buffer = nullptr;

// Exposed for use by the Display component
uint8_t* __get_lvgl_buffer()
{
    return _lvgl_buffer;
}

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    // mclog::info("{} {} {} {}", area->x1, area->x2, area->y1, area->y2);
    // for (int i = 0; i < 20; i++) {
    //     mclog::info("{}", ((uint16_t*)px_map)[i]);
    // }

    _sharp_mlcd->copyBuffer((uint16_t*)px_map);
    _sharp_mlcd->refreshScreen();

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
    const std::string tag = "fs";
    mclog::tagInfo(tag, "init");

    if (!wl_fs_init()) {
        mclog::tagError(tag, "failed");
    }
}
