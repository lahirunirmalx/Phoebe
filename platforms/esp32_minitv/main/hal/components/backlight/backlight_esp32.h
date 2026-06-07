/**
 * @file backlight_esp32.h
 * @brief Backlight + panel-power control for the Freenove Mini TV.
 *
 *   - GPIO21 (panel VDD enable, active-low): asserted low at init so the panel
 *     is powered. Kept on for the device's life -- only the backlight toggles,
 *     so we never have to re-init the ST7789.
 *   - GPIO19 (backlight, active-low): driven by an LEDC PWM channel.
 *
 * notify() queues a non-blocking pulse pattern; the actual PWM writes happen in
 * tick() which the UI task calls each loop. notify() is therefore safe to call
 * from the network core.
 */
#pragma once
#include <atomic>
#include <cstdint>
#include <hal/hal.h>

class BacklightEsp32 : public hal_components::BacklightBase {
public:
    void init() override;
    bool controllable() override { return true; }
    void on() override;
    void off() override;
    void setLevel(std::uint8_t pct) override;
    bool isOn() const override { return _base_on.load(); }
    void notify(NotifyEvent ev) override;
    void tick(std::uint32_t now_ms) override;

private:
    void apply_level(std::uint8_t pct);   // writes LEDC (active-low)

    std::atomic<bool> _base_on{false};
    std::atomic<int> _pending{0};         // 0 = none, else NotifyEvent+1

    // Active pulse animation state (UI task only).
    bool _pulsing = false;
    const struct PulseStep* _steps = nullptr;
    int _step_count = 0;
    int _step_idx = 0;
    std::uint32_t _step_start = 0;
    int _last_written = -1;               // last pct written, for change-detection
};
