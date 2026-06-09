/**
 * @file ics_recur.h
 * @brief Pure iCalendar (RFC 5545) date + recurrence helpers for the "next
 *        meeting" screen. No HAL/LVGL deps -> unit-testable on the host
 *        (see tests/test_ics_recur.cpp).
 *
 * All recurrence math runs in "local epoch" (seconds since 1970 in the device's
 * wall clock) so weekday / month-day land on the right local day. The caller
 * converts to/from real UTC with its configured tz offset.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ics {

// Portable struct-tm -> UTC epoch (newlib has no timegm()).
long tm_to_utc_epoch(int year, int mon0, int mday, int hh, int mm, int ss);

// Parse an iCal DTSTART value to a UTC epoch.
//   "20260607T093000Z" -> UTC (trailing Z), used as-is.
//   "20260607T093000"  -> floating / TZID local time, converted via tz_offset_min.
//   "20260607"         -> all-day; treated as local midnight, same conversion.
// Returns 0 on a malformed value.
long parse_ics_dt(const std::string& s, int tz_offset_min);

// Parsed RRULE. Supported: FREQ=DAILY/WEEKLY/MONTHLY/YEARLY, INTERVAL, WEEKLY
// BYDAY, COUNT, UNTIL. Not handled: MONTHLY/YEARLY BYDAY, BYMONTHDAY lists,
// WKST != Monday.
struct Recur {
    enum Freq { NONE, DAILY, WEEKLY, MONTHLY, YEARLY };
    Freq freq = NONE;
    int interval = 1;
    std::uint8_t byday = 0; // bit d set for weekday d (Mon=0 .. Sun=6)
    int count = 0;          // 0 = unbounded
};

// Parse an RRULE value. `until_utc` returns the UNTIL instant as a UTC epoch
// (0 if absent); UNTIL per RFC 5545 is UTC, so parse_ics_dt reads its 'Z'.
Recur parse_rrule(const std::string& s, long& until_utc, int tz_offset_min);

// First occurrence (local epoch) at or after now_local, honoring INTERVAL /
// BYDAY / COUNT / UNTIL / EXDATE (`ex` holds excluded instances as local
// epochs). Returns 0 if the series has no such occurrence.
long next_occurrence_local(long start_local, long now_local, const Recur& r,
                           long until_local, const std::vector<long>& ex);

} // namespace ics
