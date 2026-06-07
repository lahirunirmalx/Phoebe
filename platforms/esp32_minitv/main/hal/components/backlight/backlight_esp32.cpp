/**
 * @file backlight_esp32.cpp
 * @brief See backlight_esp32.h. Uses the Arduino LEDC API (arduino-esp32 is
 *        already a component on this board) for the active-low backlight PWM.
 */
#include "backlight_esp32.h"
#include "../../hal_config.h"
#include <Arduino.h>
#include <mooncake_log.h>

namespace {
constexpr int LEDC_FREQ = 5000;
constexpr int LEDC_RES_BITS = 8;
constexpr int LEDC_MAX = 255;
}

// A pulse pattern is a sequence of (level%, duration ms) steps. After the last
// step the backlight returns to its base level (on -> 100%, off -> 0%).
struct PulseStep {
    std::uint8_t pct;
    std::uint16_t ms;
};

// Distinct, recognisable patterns per event.
static const PulseStep kPulseOk[]    = {{40, 130}};                                   // one dim blink
static const PulseStep kPulseErr[]   = {{100, 80}, {0, 80}, {100, 80}, {0, 80}};       // two fast bright
static const PulseStep kPulseLimit[] = {{100, 250}, {0, 180}, {100, 250},
                                        {0, 180}, {100, 250}, {0, 180}};               // three long bright

void BacklightEsp32::apply_level(std::uint8_t pct)
{
    if (pct > 100) pct = 100;
    if ((int)pct == _last_written) return;
    _last_written = pct;
    // Active-low: full brightness = output mostly LOW = small duty.
    std::uint32_t duty = (std::uint32_t)LEDC_MAX * (100 - pct) / 100;
    ledcWrite(HAL_BACKLIGHT_LEDC_CH, duty);
}

void BacklightEsp32::init()
{
    // Panel VDD enable (active-low) -> power the panel and keep it on.
    pinMode(HAL_PIN_PANEL_VDD, OUTPUT);
    digitalWrite(HAL_PIN_PANEL_VDD, LOW);

    ledcSetup(HAL_BACKLIGHT_LEDC_CH, LEDC_FREQ, LEDC_RES_BITS);
    ledcAttachPin(HAL_PIN_BACKLIGHT, HAL_BACKLIGHT_LEDC_CH);

    _base_on.store(false);
    apply_level(0); // start dark
    mclog::tagInfo("backlight", "init (BL=GPIO{} VDD=GPIO{}), default off",
                   HAL_PIN_BACKLIGHT, HAL_PIN_PANEL_VDD);
}

void BacklightEsp32::on()  { _base_on.store(true);  if (!_pulsing) apply_level(100); }
void BacklightEsp32::off() { _base_on.store(false); if (!_pulsing) apply_level(0); }

void BacklightEsp32::setLevel(std::uint8_t pct)
{
    if (!_pulsing) apply_level(pct);
}

void BacklightEsp32::notify(NotifyEvent ev)
{
    _pending.store((int)ev + 1); // consumed in tick() on the UI task
}

void BacklightEsp32::tick(std::uint32_t now_ms)
{
    // Start a queued pulse if idle.
    if (!_pulsing) {
        int pend = _pending.exchange(0);
        if (pend != 0) {
            switch ((NotifyEvent)(pend - 1)) {
                case Notify_FetchOk:       _steps = kPulseOk;    _step_count = 1; break;
                case Notify_FetchErr:      _steps = kPulseErr;   _step_count = 4; break;
                case Notify_LimitReached:  _steps = kPulseLimit; _step_count = 6; break;
                default: return;
            }
            _pulsing = true;
            _step_idx = 0;
            _step_start = now_ms;
            apply_level(_steps[0].pct);
        }
        return;
    }

    // Advance the active pulse.
    if (now_ms - _step_start >= _steps[_step_idx].ms) {
        _step_idx++;
        if (_step_idx >= _step_count) {
            _pulsing = false;
            apply_level(_base_on.load() ? 100 : 0); // restore base
            return;
        }
        _step_start = now_ms;
        apply_level(_steps[_step_idx].pct);
    }
}
