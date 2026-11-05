// The part of ovos-date-parser that arbtok calls: Arabic clock times and dates
// spoken in words, and the Arabic dates and times read out of a text. A code the
// reference sends to its Arabic module is ported. A code it reads with the
// module or the formats of another language, or with dateparser, raises
// NotPortedError. A code it rejects raises its NotImplementedError("Unsupported
// language: ...").
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace arbtok::numbers::ovos_date_parser {

// A naive datetime.datetime. A datetime.date or datetime.time is the same
// value with the fields it does not have left at their defaults.
struct DateTime {
    int year = 1, month = 1, day = 1;
    int hour = 0, minute = 0, second = 0, microsecond = 0;

    friend bool operator==(const DateTime& a, const DateTime& b) {
        return a.year == b.year && a.month == b.month && a.day == b.day && a.hour == b.hour &&
               a.minute == b.minute && a.second == b.second && a.microsecond == b.microsecond;
    }
};

// datetime.isoformat().
std::string isoformat(const DateTime& dt);

// nice_time(dt, lang, speech, use_24hour, use_ampm). Reads the hour and the
// minute of dt only.
std::string nice_time(const DateTime& dt, std::string_view lang, bool speech = true, bool use_24hour = false,
                      bool use_ampm = false);

// nice_date(dt, lang) with no "now", weekday included.
std::string nice_date(const DateTime& dt, std::string_view lang);

struct Extracted {
    DateTime moment;
    std::string remainder;
};

// extract_datetime(text, lang, anchorDate) with no default_time. nullopt where
// the reference returns None. A text that names a holiday is answered from the
// reference answers for lang "ar" at the anchor 2000-01-01, the call arbtok
// makes; any other holiday text raises NotPortedError.
std::optional<Extracted> extract_datetime(std::string_view text, std::string_view lang, const DateTime& anchor);

// The ovos-date-parser release this port holds parity with.
inline constexpr const char* kReferenceVersion = "0.35.7a1";

}  // namespace arbtok::numbers::ovos_date_parser
