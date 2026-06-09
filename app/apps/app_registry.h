/**
 * @file app_registry.h
 * @brief Lookup table of installed screen-apps (name -> Mooncake app ID), in
 *        their canonical cycle order. Populated by on_install_apps(); read by
 *        the navigator to drive open/close on the tap gesture.
 */
#pragma once

#include <string>
#include <vector>

namespace app_registry {

struct Entry {
    std::string name;
    int id;
};

inline std::vector<Entry>& entries()
{
    static std::vector<Entry> e;
    return e;
}

// Resolve a screen key to its installed app ID, or -1 if not found.
inline int id_of(const std::string& name)
{
    for (const auto& e : entries())
        if (e.name == name) return e.id;
    return -1;
}

} // namespace app_registry
