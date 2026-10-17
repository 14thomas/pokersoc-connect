#pragma once

#include <string>

namespace pokersoc::util {

/// Current UTC time as "YYYY-MM-DD HH:MM:SS" (the format stored in the database).
std::string nowUtc();

/// Convert a stored UTC "YYYY-MM-DD HH:MM:SS" to local time using a strftime
/// `format`. Returns the input unchanged if it cannot be parsed.
std::string utcToLocal(const std::string& utc, const char* format = "%H:%M:%S");

/// Current local time formatted with strftime, e.g. for default file names.
std::string localNow(const char* format);

/// The latest birth date that makes someone 18 today, e.g. "17 Oct 2008".
std::string adultCutoffDate();

} // namespace pokersoc::util
