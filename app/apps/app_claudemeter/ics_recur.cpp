/**
 * @file ics_recur.cpp
 * @brief See ics_recur.h.
 */
#include "ics_recur.h"

#include <cctype>
#include <cstdlib>
#include <ctime>

namespace ics {

long tm_to_utc_epoch(int year, int mon0, int mday, int hh, int mm, int ss)
{
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    long days = (long)(year - 1970) * 365 + (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
    days += cum[mon0 % 12];
    if (mon0 > 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) days += 1;
    days += mday - 1;
    return ((days * 24 + hh) * 60 + mm) * 60 + ss;
}

long parse_ics_dt(const std::string& s, int tz_offset_min)
{
    if (s.size() < 8) return 0;
    for (int i = 0; i < 8; ++i) if (!isdigit((unsigned char)s[i])) return 0;
    int year = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    int mon0 = (s[4]-'0')*10 + (s[5]-'0') - 1;
    int mday = (s[6]-'0')*10 + (s[7]-'0');
    int hh = 0, mm = 0, ss = 0;
    if (s.size() >= 15 && s[8] == 'T') {
        hh = (s[9]-'0')*10 + (s[10]-'0');
        mm = (s[11]-'0')*10 + (s[12]-'0');
        ss = (s[13]-'0')*10 + (s[14]-'0');
    }
    long epoch = tm_to_utc_epoch(year, mon0, mday, hh, mm, ss);
    // No 'Z' suffix means the value is wall-clock local, not UTC: UTC = local - offset.
    const bool is_utc = s.find('Z') != std::string::npos;
    if (!is_utc) epoch -= (long)tz_offset_min * 60;
    return epoch;
}

namespace {
// "MO" / "2MO" / "-1FR" -> weekday bit index (Mon=0..Sun=6), or -1.
int rrule_weekday_bit(const std::string& tok)
{
    if (tok.size() < 2) return -1;
    const std::string d = tok.substr(tok.size() - 2); // strip any "2"/"-1" prefix
    static const char* const names[7] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
    for (int i = 0; i < 7; ++i)
        if (d == names[i]) return i;
    return -1;
}

bool epoch_excluded(long occ, const std::vector<long>& ex)
{
    for (long e : ex) if (e == occ) return true;
    return false;
}
} // namespace

Recur parse_rrule(const std::string& s, long& until_utc, int tz_offset_min)
{
    Recur r;
    until_utc = 0;
    size_t i = 0;
    while (i < s.size()) {
        size_t e = s.find(';', i);
        if (e == std::string::npos) e = s.size();
        const std::string kv = s.substr(i, e - i);
        const size_t eq = kv.find('=');
        if (eq != std::string::npos) {
            const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
            if (k == "FREQ") {
                if (v == "DAILY") r.freq = Recur::DAILY;
                else if (v == "WEEKLY") r.freq = Recur::WEEKLY;
                else if (v == "MONTHLY") r.freq = Recur::MONTHLY;
                else if (v == "YEARLY") r.freq = Recur::YEARLY;
            } else if (k == "INTERVAL") {
                r.interval = atoi(v.c_str());
                if (r.interval < 1) r.interval = 1;
            } else if (k == "COUNT") {
                r.count = atoi(v.c_str());
            } else if (k == "UNTIL") {
                until_utc = parse_ics_dt(v, tz_offset_min);
            } else if (k == "BYDAY") {
                size_t j = 0;
                while (j < v.size()) {
                    size_t c = v.find(',', j);
                    if (c == std::string::npos) c = v.size();
                    const int b = rrule_weekday_bit(v.substr(j, c - j));
                    if (b >= 0) r.byday |= (std::uint8_t)(1 << b);
                    j = c + 1;
                }
            }
        }
        i = e + 1;
    }
    return r;
}

long next_occurrence_local(long start_local, long now_local, const Recur& r,
                           long until_local, const std::vector<long>& ex)
{
    if (r.freq == Recur::NONE) return start_local;

    const long DAY = 86400;
    const int GUARD = 800;
    const long INTV = r.interval > 0 ? r.interval : 1;

    struct tm st;
    { time_t t = (time_t)start_local; gmtime_r(&t, &st); }
    const long tod = (long)st.tm_hour * 3600 + st.tm_min * 60 + st.tm_sec;

    auto valid_day = [](int y, int mon0, int day) -> bool {
        static const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int dim = md[mon0 % 12];
        if (mon0 == 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) dim = 29;
        return day >= 1 && day <= dim;
    };

    // Fixed-length periods (daily, or weekly with no BYDAY): closed-form jump
    // to the first index >= now, then step past any EXDATE holes.
    if (r.freq == Recur::DAILY || (r.freq == Recur::WEEKLY && r.byday == 0)) {
        const long period = (r.freq == Recur::DAILY ? 1 : 7) * INTV * DAY;
        long idx = (start_local < now_local)
                       ? (now_local - start_local + period - 1) / period : 0;
        for (int g = 0; g < GUARD; ++g, ++idx) {
            if (r.count > 0 && idx > r.count - 1) return 0;
            const long occ = start_local + idx * period;
            if (until_local > 0 && occ > until_local) return 0;
            if (occ >= now_local && !epoch_excluded(occ, ex)) return occ;
        }
        return 0;
    }

    if (r.freq == Recur::WEEKLY) { // BYDAY set
        const int sdow = (st.tm_wday + 6) % 7;             // Mon=0..Sun=6
        const long startMon = (start_local - (start_local % DAY)) - (long)sdow * DAY;
        struct tm nt;
        { time_t t = (time_t)now_local; gmtime_r(&t, &nt); }
        const int ndow = (nt.tm_wday + 6) % 7;
        const long nowMon = (now_local - (now_local % DAY)) - (long)ndow * DAY;
        long wk = (nowMon - startMon) / (7 * DAY);
        if (wk < 0) wk = 0;
        wk -= wk % INTV;
        int g = 0;
        for (long w = wk; g < GUARD; w += INTV) {
            for (int d = 0; d < 7 && g < GUARD; ++d, ++g) {
                if (!(r.byday & (1 << d))) continue;
                const long occ = startMon + w * 7 * DAY + (long)d * DAY + tod;
                if (occ < start_local) continue;
                if (until_local > 0 && occ > until_local) return 0;
                if (occ >= now_local && !epoch_excluded(occ, ex)) return occ;
            }
        }
        return 0;
    }

    // Calendar-stepped periods (monthly by month-day, yearly by date).
    const int sy = st.tm_year + 1900, smon0 = st.tm_mon, sday = st.tm_mday;
    struct tm nt;
    { time_t t = (time_t)now_local; gmtime_r(&t, &nt); }

    if (r.freq == Recur::MONTHLY) {
        const long sm = (long)sy * 12 + smon0;
        long k = ((long)(nt.tm_year + 1900) * 12 + nt.tm_mon) - sm;
        if (k < 0) k = 0;
        k -= k % INTV;
        for (int g = 0; g < GUARD; ++g, k += INTV) {
            const long m = sm + k;
            const int cy = (int)(m / 12), cmon0 = (int)(m % 12);
            if (!valid_day(cy, cmon0, sday)) continue; // e.g. day 31 in a short month
            const long occ = tm_to_utc_epoch(cy, cmon0, sday, st.tm_hour, st.tm_min, st.tm_sec);
            if (r.count > 0 && k / INTV > r.count - 1) return 0;
            if (until_local > 0 && occ > until_local) return 0;
            if (occ >= now_local && occ >= start_local && !epoch_excluded(occ, ex)) return occ;
        }
        return 0;
    }

    if (r.freq == Recur::YEARLY) {
        long k = (long)(nt.tm_year + 1900) - sy;
        if (k < 0) k = 0;
        k -= k % INTV;
        for (int g = 0; g < GUARD; ++g, k += INTV) {
            const int cy = sy + (int)k;
            if (!valid_day(cy, smon0, sday)) continue; // Feb 29 on a common year
            const long occ = tm_to_utc_epoch(cy, smon0, sday, st.tm_hour, st.tm_min, st.tm_sec);
            if (r.count > 0 && k / INTV > r.count - 1) return 0;
            if (until_local > 0 && occ > until_local) return 0;
            if (occ >= now_local && occ >= start_local && !epoch_excluded(occ, ex)) return occ;
        }
        return 0;
    }

    return 0;
}

} // namespace ics
