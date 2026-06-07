/**
 * @file backlight.h
 * @brief Backlight + notification component base class.
 *
 * On boards with a controllable backlight (e.g. the Freenove Mini TV) this owns
 * the panel backlight and a small non-blocking "pulse" animation used to signal
 * Claude events while the screen is otherwise dark. The base class is a no-op so
 * platforms without a controllable backlight (desktop) are unaffected.
 */
#pragma once
#include <cstdint>

namespace hal_components {

class BacklightBase {
public:
    // Notification kinds, surfaced by AppClaudeMeter on fetch-state changes.
    enum NotifyEvent {
        Notify_FetchOk = 0,   // a successful poll
        Notify_FetchErr,      // network / HTTP / parse failure
        Notify_LimitReached,  // usage crossed the danger threshold
    };

    virtual ~BacklightBase() = default;

    virtual void init() {}

    // True on platforms with a real, controllable backlight. The app uses this
    // to decide whether to run display-sleep logic (no-op platforms stay lit).
    virtual bool controllable() { return false; }

    virtual void on() {}                          // turn fully on
    virtual void off() {}                         // turn off (dark)
    virtual void setLevel(std::uint8_t /*pct*/) {} // 0..100
    virtual bool isOn() const { return false; }

    // Queue a notification pulse pattern. Safe to call from another core/task;
    // the actual LED writes happen in tick() on the UI task.
    virtual void notify(NotifyEvent /*ev*/) {}

    // Advance any in-progress pulse animation. Call periodically from the UI loop.
    virtual void tick(std::uint32_t /*now_ms*/) {}
};

} // namespace hal_components
