#include "util/Time.h"

#include <cstdio>
#include <ctime>

namespace pokersoc::util {

namespace {

std::tm toUtc(std::time_t time) {
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &time);
#else
    gmtime_r(&time, &result);
#endif
    return result;
}

std::tm toLocal(std::time_t time) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &time);
#else
    localtime_r(&time, &result);
#endif
    return result;
}

std::time_t fromUtc(std::tm& tm) {
#ifdef _WIN32
    return _mkgmtime(&tm);
#else
    return timegm(&tm);
#endif
}

std::string format(const std::tm& tm, const char* pattern) {
    char buffer[128];
    const std::size_t length = std::strftime(buffer, sizeof buffer, pattern, &tm);
    return std::string(buffer, length);
}

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

} // namespace

std::string nowUtc() {
    return format(toUtc(std::time(nullptr)), "%Y-%m-%d %H:%M:%S");
}

std::string utcToLocal(const std::string& utc, const char* pattern) {
    std::tm tm{};
    if (std::sscanf(utc.c_str(), "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return utc;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    const std::time_t time = fromUtc(tm);
    if (time == static_cast<std::time_t>(-1)) {
        return utc;
    }
    return format(toLocal(time), pattern);
}

std::string localNow(const char* pattern) {
    return format(toLocal(std::time(nullptr)), pattern);
}

std::string adultCutoffDate() {
    std::tm tm = toLocal(std::time(nullptr));
    tm.tm_year -= 18;
    if (tm.tm_mon == 1 && tm.tm_mday == 29 && !isLeapYear(tm.tm_year + 1900)) {
        tm.tm_mday = 28;
    }
    return format(tm, "%d %b %Y");
}

} // namespace pokersoc::util
