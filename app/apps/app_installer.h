/**
 * @file app_installer.h
 * @brief Installs every Phoebe screen-app and records its ID in app_registry,
 *        in canonical cycle order. The navigator (app_main) opens one at a time.
 */
#pragma once
#include <mooncake.h>
#include <memory>

#include "app_registry.h"
#include "app_clock/app_clock.h"
#include "app_meter/app_meter.h"
#include "app_weather/app_weather.h"
#include "app_pomodoro/app_pomodoro.h"
#include "app_world/app_world.h"
#include "app_meeting/app_meeting.h"
#include "app_currency/app_currency.h"
#include "app_aqi/app_aqi.h"
#include "app_forecast/app_forecast.h"
#include "app_sunmoon/app_sunmoon.h"
#include "app_network/app_network.h"
#include "app_uptime/app_uptime.h"
#include "app_pet/app_pet.h"
#include "app_saver/app_saver.h"
#include "app_life/app_life.h"
#include "app_matrix/app_matrix.h"
/* Header files locator (Don't remove) */

inline void on_install_apps()
{
    auto& mc = mooncake::GetMooncake();
    auto add = [&](std::unique_ptr<mooncake::AppAbility> app, const char* name) {
        const int id = mc.installApp(std::move(app));
        app_registry::entries().push_back({name, id});
    };

    // Canonical cycle order (the captive-portal "Screens" list reorders/hides
    // these by name at runtime).
    add(std::make_unique<AppClock>(), "clock");
    add(std::make_unique<AppMeter>(), "meter");
    add(std::make_unique<AppWeather>(), "weather");
    add(std::make_unique<AppPomodoro>(), "pomodoro");
    add(std::make_unique<AppWorld>(), "world");
    add(std::make_unique<AppMeeting>(), "meeting");
    add(std::make_unique<AppCurrency>(), "currency");
    add(std::make_unique<AppAqi>(), "aqi");
    add(std::make_unique<AppForecast>(), "forecast");
    add(std::make_unique<AppSunMoon>(), "sunmoon");
    add(std::make_unique<AppNetwork>(), "network");
    add(std::make_unique<AppUptime>(), "uptime");
    add(std::make_unique<AppPet>(), "pet");
    add(std::make_unique<AppSaver>(), "saver");
    add(std::make_unique<AppLife>(), "life");
    add(std::make_unique<AppMatrix>(), "matrix");
    /* Install app locator (Don't remove) */
}
