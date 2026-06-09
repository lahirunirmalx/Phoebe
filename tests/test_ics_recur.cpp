// Host unit tests for the pure ICS/recurrence engine.
// Build & run (from repo root):
//   c++ -std=c++17 -I app/apps/app_claudemeter tests/test_ics_recur.cpp app/apps/app_claudemeter/ics_recur.cpp -o /tmp/t && /tmp/t
#include "ics_recur.h"

#include <cstdio>
#include <ctime>

static int g_fail = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fail; } \
    } while (0)

using namespace ics;

static int wday_of(long local_epoch) // 0=Sun .. 6=Sat
{
    time_t t = (time_t)local_epoch;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    return tmv.tm_wday;
}

int main()
{
    const long DAY = 86400;
    const std::vector<long> none;

    // --- parse_ics_dt: UTC (Z) vs local ----------------------------------
    const int TZ = 330; // UTC+5:30 (Colombo)
    long utc_midnight = tm_to_utc_epoch(2026, 0, 1, 0, 0, 0);
    CHECK(parse_ics_dt("20260101T000000Z", TZ) == utc_midnight);          // Z -> UTC as-is
    CHECK(parse_ics_dt("20260101T000000", TZ) == utc_midnight - TZ * 60); // local -> UTC
    CHECK(parse_ics_dt("20260101", TZ) == utc_midnight - TZ * 60);        // all-day local midnight
    CHECK(parse_ics_dt("bad", TZ) == 0);                                  // malformed

    // --- DAILY: first occurrence >= now ----------------------------------
    {
        long start = tm_to_utc_epoch(2026, 0, 1, 9, 0, 0); // local epoch
        long now = start + 10 * DAY + 100;
        Recur r; r.freq = Recur::DAILY; r.interval = 1;
        long occ = next_occurrence_local(start, now, r, 0, none);
        CHECK(occ == start + 11 * DAY);
    }

    // --- DAILY INTERVAL=3 -------------------------------------------------
    {
        long start = tm_to_utc_epoch(2026, 0, 1, 9, 0, 0);
        long now = start + 10 * DAY;          // day 10; multiples of 3 -> day 12
        Recur r; r.freq = Recur::DAILY; r.interval = 3;
        long occ = next_occurrence_local(start, now, r, 0, none);
        CHECK(occ == start + 12 * DAY);
    }

    // --- COUNT exhausted --------------------------------------------------
    {
        long start = tm_to_utc_epoch(2026, 0, 1, 9, 0, 0);
        long now = start + 10 * DAY;
        Recur r; r.freq = Recur::DAILY; r.interval = 1; r.count = 5; // last on day 4
        CHECK(next_occurrence_local(start, now, r, 0, none) == 0);
    }

    // --- UNTIL passed -----------------------------------------------------
    {
        long start = tm_to_utc_epoch(2026, 0, 1, 9, 0, 0);
        long now = start + 10 * DAY;
        long until = start + 5 * DAY;
        Recur r; r.freq = Recur::DAILY; r.interval = 1;
        CHECK(next_occurrence_local(start, now, r, until, none) == 0);
    }

    // --- EXDATE skip ------------------------------------------------------
    {
        long start = tm_to_utc_epoch(2026, 0, 1, 9, 0, 0);
        long now = start + 10 * DAY + 1;
        Recur r; r.freq = Recur::DAILY; r.interval = 1;
        std::vector<long> ex = {start + 11 * DAY}; // skip the natural next
        long occ = next_occurrence_local(start, now, r, 0, ex);
        CHECK(occ == start + 12 * DAY);
    }

    // --- WEEKLY BYDAY: result lands on an allowed weekday, >= now ---------
    {
        long start = tm_to_utc_epoch(2026, 0, 5, 9, 0, 0); // 2026-01-05 is a Monday
        long now = start + 9 * DAY;
        Recur r; r.freq = Recur::WEEKLY; r.interval = 1;
        r.byday = (1 << 2) | (1 << 4); // Wed(2), Fri(4) in Mon=0 indexing
        long occ = next_occurrence_local(start, now, r, 0, none);
        CHECK(occ >= now);
        int wd = wday_of(occ);                  // 0=Sun..6=Sat
        CHECK(wd == 3 /*Wed*/ || wd == 5 /*Fri*/);
        CHECK((occ % DAY) == (start % DAY));    // same time-of-day
    }

    // --- MONTHLY by month-day --------------------------------------------
    {
        long start = tm_to_utc_epoch(2026, 0, 15, 9, 0, 0); // 15th
        long now = tm_to_utc_epoch(2026, 3, 1, 0, 0, 0);    // Apr 1 -> next is Apr 15
        Recur r; r.freq = Recur::MONTHLY; r.interval = 1;
        long occ = next_occurrence_local(start, now, r, 0, none);
        CHECK(occ == tm_to_utc_epoch(2026, 3, 15, 9, 0, 0));
    }

    // --- YEARLY (anniversary) --------------------------------------------
    {
        long start = tm_to_utc_epoch(2020, 5, 10, 8, 0, 0);
        long now = tm_to_utc_epoch(2026, 0, 1, 0, 0, 0);
        Recur r; r.freq = Recur::YEARLY; r.interval = 1;
        long occ = next_occurrence_local(start, now, r, 0, none);
        CHECK(occ == tm_to_utc_epoch(2026, 5, 10, 8, 0, 0));
    }

    // --- parse_rrule ------------------------------------------------------
    {
        long until = 0;
        Recur r = parse_rrule("FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,FR;COUNT=10", until, 0);
        CHECK(r.freq == Recur::WEEKLY);
        CHECK(r.interval == 2);
        CHECK(r.count == 10);
        CHECK(r.byday == ((1 << 0) | (1 << 4))); // Mon, Fri
        CHECK(until == 0);

        long until2 = 0;
        Recur r2 = parse_rrule("FREQ=DAILY;UNTIL=20260101T000000Z", until2, 330);
        CHECK(r2.freq == Recur::DAILY);
        CHECK(r2.interval == 1); // default
        CHECK(until2 == tm_to_utc_epoch(2026, 0, 1, 0, 0, 0));
    }

    if (g_fail == 0) std::printf("ALL TESTS PASSED\n");
    else std::printf("%d CHECK(S) FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
