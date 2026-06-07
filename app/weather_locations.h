/**
 * @file weather_locations.h
 * @brief Sri Lanka city list (name + lat/lon) shared by the captive portal
 *        (location dropdown) and AppClaudeMeter's weather screen (Open-Meteo
 *        fetch). Keeping one table means the portal and the app never disagree.
 */
#pragma once
#include <string>

namespace weather {

struct Location {
    const char* name;
    float lat;
    float lon;
};

static const Location kLocations[] = {
    {"Colombo", 6.927f, 79.861f},
    {"Kandy", 7.291f, 80.636f},
    {"Galle", 6.033f, 80.217f},
    {"Jaffna", 9.661f, 80.025f},
    {"Negombo", 7.209f, 79.838f},
    {"Anuradhapura", 8.311f, 80.403f},
    {"Trincomalee", 8.587f, 81.215f},
    {"Batticaloa", 7.717f, 81.700f},
    {"Matara", 5.948f, 80.535f},
    {"Kurunegala", 7.487f, 80.365f},
    {"Ratnapura", 6.683f, 80.400f},
    {"Badulla", 6.993f, 81.055f},
    {"Nuwara Eliya", 6.970f, 80.782f},
};

static constexpr int kLocationCount = sizeof(kLocations) / sizeof(kLocations[0]);

// Look up a location by name; falls back to the first entry (Colombo).
inline const Location& find(const std::string& name)
{
    for (int i = 0; i < kLocationCount; ++i) {
        if (name == kLocations[i].name) return kLocations[i];
    }
    return kLocations[0];
}

} // namespace weather
