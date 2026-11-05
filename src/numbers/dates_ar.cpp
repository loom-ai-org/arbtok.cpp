// ovos_date_parser.dates_ar and the parts of ovos_date_parser that route to it.
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>

#include "arbtok/numbers/errors.hpp"
#include "arbtok/numbers/ovos_date_parser.hpp"
#include "numbers_ar.hpp"
#include "tables.hpp"

namespace arbtok::numbers::ovos_date_parser {

namespace {

using text::U32;
using text::U32View;
using numbers_ar::Tokens;
using tables::has;
__extension__ typedef __int128 i128;

const tables::DatesAr& D() { return tables::dates_ar(); }

const U32 kAnd = U"و";
constexpr long long kUsPerDay = 86400000000LL;
constexpr long long kMaxDeltaDays = 999999999;
constexpr long long kMaxOrdinal = 3652059;  // date(9999, 12, 31).toordinal()

i128 floor_div(i128 a, i128 b) {
    i128 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

// ---------------------------------------------------------------------------
// datetime and timedelta
// ---------------------------------------------------------------------------

bool is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

int days_in_month(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && is_leap(y) ? 29 : days[m - 1];
}

// date.toordinal()
long long to_ordinal(int y, int m, int d) {
    const long long p = y - 1;
    long long days = p * 365 + p / 4 - p / 100 + p / 400;
    for (int k = 1; k < m; ++k) days += days_in_month(y, k);
    return days + d;
}

// date.fromordinal()
void from_ordinal(long long ordinal, int& y, int& m, int& d) {
    // Howard Hinnant's civil_from_days, with day 0 at 1970-01-01
    long long z = ordinal - 719163 + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
}

// A timedelta, in microseconds.
struct Delta {
    i128 us = 0;
};

Delta checked(i128 us) {
    const i128 days = floor_div(us, kUsPerDay);
    if (days > kMaxDeltaDays || days < -kMaxDeltaDays) {
        throw OverflowError("days=" + std::to_string(static_cast<long long>(days)) +
                            "; must have magnitude <= 999999999");
    }
    return {us};
}

Delta operator+(Delta a, Delta b) { return checked(a.us + b.us); }
Delta operator*(int sign, Delta a) { return checked(sign * a.us); }

Delta days_delta(long long days) { return checked(static_cast<i128>(days) * kUsPerDay); }

i128 to_i128(const BigInt& n) {
    if (n.digits.size() > 30) throw OverflowError("Python int too large to convert to C int");
    i128 v = 0;
    for (char c : n.digits) v = v * 10 + (c - '0');
    return n.negative ? -v : v;
}

i128 double_to_i128(double value) {
    if (std::isinf(value)) throw OverflowError("cannot convert float infinity to integer");
    if (std::isnan(value)) throw ValueError("cannot convert float NaN to integer");
    if (std::fabs(value) >= 1e30) throw OverflowError("Python int too large to convert to C int");
    return static_cast<i128>(value);
}

// timedelta(seconds=seconds), as CPython's timedelta_new computes it.
Delta seconds_delta(const Number& seconds) {
    if (const auto* n = std::get_if<BigInt>(&seconds)) return checked(to_i128(*n * BigInt(1000000)));
    const double value = std::get<double>(seconds);
    double intpart = 0;
    double fracpart = std::modf(value, &intpart);
    i128 x = double_to_i128(intpart) * 1000000;
    if (fracpart != 0.0) {
        fracpart = std::modf(1e6 * fracpart, &intpart);
        x += double_to_i128(intpart);
        const double leftover = fracpart;
        if (leftover != 0.0) {
            double whole = std::round(leftover);
            if (std::fabs(whole - leftover) == 0.5) {
                const int odd = static_cast<int>(x & 1);
                whole = 2.0 * std::round((leftover + odd) * 0.5) - odd;
            }
            x += static_cast<long>(whole);
        }
    }
    return checked(x);
}

// A datetime, in microseconds from 0001-01-01T00:00.
struct Moment {
    i128 us = 0;
};

Moment moment(const DateTime& dt) {
    const i128 days = to_ordinal(dt.year, dt.month, dt.day) - 1;
    return {days * kUsPerDay + ((static_cast<i128>(dt.hour) * 60 + dt.minute) * 60 + dt.second) * 1000000 +
            dt.microsecond};
}

DateTime fields(Moment m) {
    DateTime dt;
    const i128 days = floor_div(m.us, kUsPerDay);
    long long rest = static_cast<long long>(m.us - days * kUsPerDay);
    from_ordinal(static_cast<long long>(days) + 1, dt.year, dt.month, dt.day);
    dt.microsecond = static_cast<int>(rest % 1000000);
    rest /= 1000000;
    dt.second = static_cast<int>(rest % 60);
    rest /= 60;
    dt.minute = static_cast<int>(rest % 60);
    dt.hour = static_cast<int>(rest / 60);
    return dt;
}

Moment operator+(Moment m, Delta d) {
    const i128 us = m.us + d.us;
    const i128 days = floor_div(us, kUsPerDay);
    if (days < 0 || days >= kMaxOrdinal) throw OverflowError("date value out of range");
    return {us};
}

int weekday(Moment m) { return static_cast<int>((floor_div(m.us, kUsPerDay) + 1 + 6) % 7); }

DateTime replace_date(const DateTime& dt, int year, int month, int day) {
    if (year < 1 || year > 9999) throw ValueError("year " + std::to_string(year) + " is out of range");
    if (month < 1 || month > 12) throw ValueError("month must be in 1..12");
    if (day < 1 || day > days_in_month(year, month)) throw ValueError("day is out of range for month");
    DateTime out = dt;
    out.year = year;
    out.month = month;
    out.day = day;
    return out;
}

DateTime replace_time(const DateTime& dt, int hour, int minute) {
    if (hour < 0 || hour > 23) throw ValueError("hour must be in 0..23");
    if (minute < 0 || minute > 59) throw ValueError("minute must be in 0..59");
    DateTime out = dt;
    out.hour = hour;
    out.minute = minute;
    out.second = 0;
    out.microsecond = 0;
    return out;
}

std::string two(int v) { return (v < 10 ? "0" : "") + std::to_string(v); }

// ---------------------------------------------------------------------------
// nice_time
// ---------------------------------------------------------------------------

U32 ones_fem(int number) { return tables::numbers_ar().ones_fem.at(static_cast<size_t>(number)); }

// _fem_cardinal_ar
U32 fem_cardinal(int number) {
    if (number <= 10) return ones_fem(number);
    if (const auto it = D().fem_teens.find(number); it != D().fem_teens.end()) return it->second;
    if (number < 20) return ones_fem(number - 10) + U" عشرة";
    const int tens = number / 10, unit = number % 10;
    const U32& tens_word = tables::numbers_ar().tens.at(tens * 10);
    if (unit == 0) return tens_word;
    return ones_fem(unit) + U" و" + tens_word;
}

U32 nice_minutes(int minutes) {
    if (minutes == 1) return U"دقيقة";
    if (minutes == 2) return U"دقيقتان";
    if (minutes <= 10) return fem_cardinal(minutes) + U" دقائق";
    return fem_cardinal(minutes) + U" دقيقة";
}

U32 nice_time_ar(const DateTime& dt, bool speech, bool use_24hour, bool use_ampm) {
    if (!speech) {
        std::string s;
        if (use_24hour) {
            s = two(dt.hour) + ":" + two(dt.minute);
        } else {
            s = two(dt.hour % 12 == 0 ? 12 : dt.hour % 12) + ":" + two(dt.minute);
            if (use_ampm) s += dt.hour < 12 ? " AM" : " PM";
            if (s[0] == '0') s.erase(0, 1);
        }
        return text::u(s);
    }
    if (use_24hour) {
        U32 speak = numbers_ar::pronounce_number_ar(BigInt(dt.hour));
        if (dt.minute) speak += U" و" + nice_minutes(dt.minute);
        return speak;
    }
    if (dt.hour == 0 && dt.minute == 0) return U"منتصف الليل";
    if (dt.hour == 12 && dt.minute == 0) return use_ampm ? U"الساعة الثانية عشرة ظهراً" : U"الظهر";
    const int hour = dt.hour % 12 ? dt.hour % 12 : 12;
    U32 speak;
    if (const auto it = D().to_the_hour.find(dt.minute); it != D().to_the_hour.end()) {
        const int next_hour = (dt.hour + 1) % 12 ? (dt.hour + 1) % 12 : 12;
        speak = U"الساعة " + D().hour_names.at(next_hour) + U" " + it->second;
    } else {
        speak = U"الساعة " + D().hour_names.at(hour);
        if (dt.minute == 15) {
            speak += U" والربع";
        } else if (dt.minute == 20) {
            speak += U" والثلث";
        } else if (dt.minute == 30) {
            speak += U" والنصف";
        } else if (dt.minute) {
            speak += U" و" + nice_minutes(dt.minute);
        }
    }
    if (use_ampm) speak += dt.hour >= 12 ? U" مساءً" : U" صباحاً";
    return speak;
}

// ---------------------------------------------------------------------------
// nice_date: DateTimeFormat over res/ar/date_time.json
// ---------------------------------------------------------------------------

// re.match(pattern, s) for the patterns of date_time.json: ^, $, \d, [a-b],
// literal characters, and the quantifiers ? and {n}.
bool pattern_match(std::string_view pattern, std::string_view s) {
    struct Atom {
        std::function<bool(char)> test;
        int min, max;
    };
    std::vector<Atom> atoms;
    bool anchored_end = false;
    for (size_t p = 0; p < pattern.size();) {
        const char c = pattern[p];
        if (c == '^' && p == 0) {
            ++p;
            continue;
        }
        if (c == '$' && p + 1 == pattern.size()) {
            anchored_end = true;
            ++p;
            continue;
        }
        Atom atom{nullptr, 1, 1};
        if (c == '\\' && p + 1 < pattern.size() && pattern[p + 1] == 'd') {
            atom.test = [](char x) { return x >= '0' && x <= '9'; };
            p += 2;
        } else if (c == '[' && p + 4 < pattern.size() && pattern[p + 2] == '-' && pattern[p + 4] == ']') {
            const char lo = pattern[p + 1], hi = pattern[p + 3];
            atom.test = [lo, hi](char x) { return x >= lo && x <= hi; };
            p += 5;
        } else if (c == '\\' || c == '[' || c == '(' || c == '.' || c == '*' || c == '+' || c == '|') {
            throw NotPortedError("date_time.json pattern not ported: " + std::string(pattern));
        } else {
            atom.test = [c](char x) { return x == c; };
            ++p;
        }
        if (p < pattern.size() && pattern[p] == '?') {
            atom.min = 0;
            ++p;
        } else if (p < pattern.size() && pattern[p] == '{') {
            const size_t close = pattern.find('}', p);
            atom.min = atom.max = std::stoi(std::string(pattern.substr(p + 1, close - p - 1)));
            p = close + 1;
        }
        atoms.push_back(atom);
    }
    std::function<bool(size_t, size_t)> match = [&](size_t a, size_t i) -> bool {
        if (a == atoms.size()) return !anchored_end || i == s.size() || (i + 1 == s.size() && s[i] == '\n');
        const Atom& atom = atoms[a];
        size_t count = 0;
        while (count < static_cast<size_t>(atom.max) && i + count < s.size() && atom.test(s[i + count])) ++count;
        for (size_t k = count + 1; k-- > static_cast<size_t>(atom.min);) {
            if (match(a + 1, i + k)) return true;
        }
        return false;
    };
    return match(0, 0);
}

// str.format(**fields) for a template with plain {name} fields.
std::string format_fields(const std::string& tmpl, const std::map<std::string, std::string>& fields) {
    std::string out;
    for (size_t p = 0; p < tmpl.size();) {
        if (tmpl[p] == '{') {
            const size_t close = tmpl.find('}', p);
            out += fields.at(tmpl.substr(p + 1, close - p - 1));
            p = close + 1;
        } else {
            out.push_back(tmpl[p++]);
        }
    }
    return out;
}

std::string format_string(long long number, const tables::DateTimeAr::Format& section) {
    const std::string s = std::to_string(number);
    for (const auto& [match, format] : section.rules) {
        if (pattern_match(match, s)) return format;
    }
    return section.fallback;
}

std::string year_format(int year) {
    const auto& t = tables::date_time_ar();
    // _number_strings: number.get(str(k)) or str(k)
    auto word = [&](long long k) {
        const auto it = t.number.find(std::to_string(k));
        return it == t.number.end() || it->second.empty() ? std::to_string(k) : it->second;
    };
    const long long n = year;
    std::map<std::string, std::string> f = {
        {"x", word(n % 10)},
        {"xx", word(n % 100)},
        {"x_in_x0", word(n % 100 / 10)},
        {"x0", word(n % 100 / 10 * 10)},
        {"xxx", word(n % 1000)},
        {"x00", word(n % 1000 / 100 * 100)},
        {"x_in_x00", word(n % 1000 / 100)},
        {"xx00", word(n % 10000 / 100 * 100)},
        {"xx_in_xx00", word(n % 10000 / 100)},
        {"x000", word(n % 10000 / 1000 * 1000)},
        {"x_in_x000", word(n % 10000 / 1000)},
        {"x0_in_x000", word(n % 10000 / 1000 * 10)},
    };
    const auto x_in_0x00 = t.number.find(std::to_string(n % 1000 / 100));
    f["x_in_0x00"] = x_in_0x00 == t.number.end() ? "None" : x_in_0x00->second;
    f["formatted_decade"] = format_fields(format_string(n % 100, t.decade_format), {
        {"x", f["x"]}, {"xx", f["xx"]}, {"x0", f["x0"]}, {"x_in_x0", f["x_in_x0"]},
        {"number", std::to_string(n % 100)}});
    f["formatted_hundreds"] = format_fields(format_string(n % 1000, t.hundreds_format), {
        {"xxx", f["xxx"]}, {"x00", f["x00"]}, {"x_in_x00", f["x_in_x00"]},
        {"formatted_decade", f["formatted_decade"]}, {"number", std::to_string(n % 1000)}});
    f["number"] = std::to_string(n % 10000);
    f["formatted_thousand"] = format_fields(format_string(n % 10000, t.thousand_format), f);
    const std::map<std::string, std::string> year_fields = {
        {"year", std::to_string(n)},
        {"century", std::to_string(n / 100)},
        {"decade", std::to_string(n % 100)},
        {"formatted_hundreds", f["formatted_hundreds"]},
        {"formatted_decade", f["formatted_decade"]},
        {"formatted_thousand", f["formatted_thousand"]},
        {"bc", ""},
    };
    // re.sub(' +', ' ', ...).strip()
    const std::string spaced = format_fields(format_string(n, t.year_format), year_fields);
    std::string single;
    for (char c : spaced) {
        if (c == ' ' && !single.empty() && single.back() == ' ') continue;
        single.push_back(c);
    }
    return text::s8(text::strip_ws(text::u(single)));
}

// ---------------------------------------------------------------------------
// extract_datetime_ar
// ---------------------------------------------------------------------------

std::optional<std::pair<int, int>> match_hhmm(const U32& token) {
    // ^(\d{1,2}):(\d{2})$
    const size_t colon = token.find(U':');
    if (colon == U32::npos || colon < 1 || colon > 2 || token.size() != colon + 3) return std::nullopt;
    int h = 0, m = 0;
    for (size_t k = 0; k < token.size(); ++k) {
        if (k == colon) continue;
        if (!text::is_decimal(token[k])) return std::nullopt;
        (k < colon ? h : m) = (k < colon ? h : m) * 10 + text::decimal_value(token[k]);
    }
    return std::make_pair(h, m);
}

bool between(const Number& v, long long lo, long long hi) {
    return compare(v, Number(BigInt(lo))) >= 0 && compare(v, Number(BigInt(hi))) <= 0;
}
bool strictly_between(const Number& v, long long lo, long long hi) {
    return compare(v, Number(BigInt(lo))) > 0 && compare(v, Number(BigInt(hi))) < 0;
}
int small_int(const Number& v) { return static_cast<int>(*to_int(v).to_ll()); }

using Duration = std::optional<std::pair<Delta, bool>>;

// _parse_duration_span
std::pair<Duration, size_t> parse_duration_span(const Tokens& tokens, size_t i) {
    const auto& d = D();
    const size_t n = tokens.size();
    if (i >= n) return {std::nullopt, i};
    const U32& tok = tokens[i];
    long long unit = 0;
    Number secs = BigInt(0);
    size_t k = 0;
    if (const long long* dual = tables::find(d.duals_lookup, tok)) {
        unit = *dual;
        secs = BigInt(2 * unit);
        k = i + 1;
    } else if (const long long* single = tables::find(d.units_lookup, tok)) {
        unit = *single;
        secs = BigInt(unit);
        k = i + 1;
    } else {
        const auto [value, j] = numbers_ar::parse_number_span(tokens, i);
        if (!value || j >= n || !d.units_lookup.count(tokens[j])) return {std::nullopt, i};
        unit = d.units_lookup.at(tokens[j]);
        secs = mul(*value, Number(BigInt(unit)));
        k = j + 1;
    }
    if (k + 1 < n && tokens[k] == kAnd) {
        const auto [frac, jf] = numbers_ar::parse_number_span(tokens, k + 1);
        if (frac && strictly_between(*frac, 0, 1)) {
            secs = add(secs, mul(*frac, Number(BigInt(unit))));
            k = jf;
        }
    }
    return {std::make_pair(seconds_delta(secs), unit < 86400), k};
}

std::optional<Extracted> extract_datetime_ar(U32View text, const DateTime& anchor_date) {
    const auto& d = D();
    if (text.empty()) return std::nullopt;
    U32 normalized = numbers_ar::normalize_ar(text);
    normalized = text::replace(normalized, U"بعد غد", U"بعدغد");
    normalized = text::replace(normalized, U"اول امس", U"اولامس");
    normalized = text::replace(normalized, U"امس الاول", U"اولامس");
    normalized = text::replace(normalized, U"منتصف الليل", U"منتصفالليل");
    normalized = text::replace(normalized, U"بعد الظهر", U"بعدالظهر");
    normalized = text::replace(normalized, U"في غضون", U"غضون");
    const Tokens tokens = numbers_ar::tokenize_ar(normalized);
    const DateTime today = replace_time(anchor_date, 0, 0);
    bool date_found = false;
    std::optional<DateTime> result_date;
    std::optional<int> hour;
    int minute = 0;
    std::string ampm;
    bool noon_seen = false;
    Delta delta;
    int delta_sign = 0;
    bool delta_is_time = false;
    Tokens remainder;
    const U32 day_after = numbers_ar::normalize_ar(U"بعدغد");
    const U32 day_before = numbers_ar::normalize_ar(U"اولامس");
    const U32 eleventh = numbers_ar::normalize_ar(U"الحادية"), second = numbers_ar::normalize_ar(U"الثانية");
    const U32 eleventh_hour = numbers_ar::normalize_ar(U"الحادية عشرة");
    const U32 twelfth_hour = numbers_ar::normalize_ar(U"الثانية عشرة");
    const U32 ten_fem = numbers_ar::normalize_ar(U"عشرة"), ten_fem_h = numbers_ar::normalize_ar(U"عشره");
    const size_t n = tokens.size();
    auto plus_days = [&](const DateTime& dt, long long days) { return fields(moment(dt) + days_delta(days)); };

    auto parse_clock = [&](size_t i) -> size_t {
        if (i >= n) return i;
        const U32& tok = tokens[i];
        std::optional<int> matched;
        if (i + 1 < n && (tok + U" " + tokens[i + 1] == eleventh_hour || tok + U" " + tokens[i + 1] == twelfth_hour)) {
            matched = tok == eleventh ? 11 : 12;
            i += 2;
        } else if (const long long* h = tables::find(d.hour_lookup, tok)) {
            matched = static_cast<int>(*h);
            i += 1;
        }
        if (!matched) return i;
        hour = *matched;
        minute = 0;
        date_found = true;
        if (i < n && tokens[i] == kAnd && i + 1 < n) {
            const U32& nxt = tokens[i + 1];
            if (has(d.half_words, nxt)) {
                minute = 30;
                i += 2;
            } else if (has(d.quarter_words, nxt)) {
                minute = 15;
                i += 2;
            } else if (has(d.third_words, nxt)) {
                minute = 20;
                i += 2;
            } else {
                const auto [value, j] = numbers_ar::parse_number_span(tokens, i + 1);
                if (value && j < n && has(d.minute_words, tokens[j]) && strictly_between(*value, 0, 60)) {
                    minute = small_int(*value);
                    i = j + 1;
                } else if (has(d.minute_words, nxt) && d.duals_lookup.count(nxt)) {
                    minute = 2;
                    i += 2;
                }
            }
        }
        if (i < n && tokens[i] == d.except_word && i + 1 < n) {
            if (has(d.quarter_words, tokens[i + 1])) {
                *hour -= 1;
                minute = 45;
                i += 2;
            } else if (has(d.third_words, tokens[i + 1])) {
                *hour -= 1;
                minute = 40;
                i += 2;
            } else {
                auto [value, j] = numbers_ar::parse_number_span(tokens, i + 1);
                if (value && strictly_between(*value, 0, 60) && is_integer(*value)) {
                    if (j < n && has(d.minute_words, tokens[j])) ++j;
                    *hour -= 1;
                    minute = 60 - small_int(*value);
                    i = j;
                }
            }
            if (*hour == 0) hour = 12;
        }
        return i;
    };

    size_t i = 0;
    while (i < n) {
        const U32& tok = tokens[i];
        if (const auto hm = match_hhmm(tok)) {
            const auto [h, mi] = *hm;
            if (0 <= h && h < 24 && 0 <= mi && mi < 60) {
                hour = h;
                minute = mi;
                if (ampm.empty()) ampm = h > 12 || h == 0 ? "h24" : "";
                date_found = true;
                ++i;
                continue;
            }
        }
        if (has(d.today_words, tok)) {
            result_date = today;
            date_found = true;
            ++i;
            continue;
        }
        if (tok == day_after) {
            result_date = plus_days(today, 2);
            date_found = true;
            ++i;
            continue;
        }
        if (has(d.tomorrow_words, tok)) {
            result_date = plus_days(today, 1);
            date_found = true;
            ++i;
            continue;
        }
        if (tok == day_before) {
            result_date = plus_days(today, -2);
            date_found = true;
            ++i;
            continue;
        }
        if (has(d.yesterday_words, tok)) {
            result_date = plus_days(today, -1);
            date_found = true;
            ++i;
            continue;
        }
        if (tok == d.day_word && i + 1 < n && d.weekdays_lookup.count(tokens[i + 1])) {
            ++i;
            continue;
        }
        if (const long long* target = tables::find(d.weekdays_lookup, tok)) {
            long long offset = ((*target - weekday(moment(today))) % 7 + 7) % 7;
            if (i + 1 < n && has(d.next_words, tokens[i + 1])) {
                if (offset == 0) offset = 7;
                ++i;
            } else if (i + 1 < n && has(d.prev_words, tokens[i + 1])) {
                offset = offset ? offset - 7 : -7;
                ++i;
            }
            result_date = plus_days(today, offset);
            date_found = true;
            ++i;
            continue;
        }
        if (const long long* month = tables::find(d.months_lookup, tok)) {
            int day = 1;
            long back = static_cast<long>(remainder.size()) - 1;
            if (back >= 0 && remainder[static_cast<size_t>(back)] == d.of_word) --back;
            if (back >= 0) {
                const auto [val, j] = numbers_ar::parse_number_span(remainder, static_cast<size_t>(back));
                if (val && j == static_cast<size_t>(back) + 1 && between(*val, 1, 31) && is_integer(*val)) {
                    day = small_int(*val);
                    remainder.resize(static_cast<size_t>(back));
                } else {
                    for (long start = std::max(0L, back - 2); start <= back; ++start) {
                        const auto [ord, oj] = numbers_ar::parse_ordinal_span(remainder, static_cast<size_t>(start));
                        if (ord && oj == static_cast<size_t>(back) + 1 && *ord >= 1 && *ord <= 31) {
                            day = static_cast<int>(*ord);
                            remainder.resize(static_cast<size_t>(start));
                            break;
                        }
                    }
                }
            }
            int year = today.year;
            ++i;
            if (i < n) {
                const auto [val, j] = numbers_ar::parse_number_span(tokens, i);
                if (val && between(*val, 1000, 9999) && is_integer(*val)) {
                    year = small_int(*val);
                    i = j;
                }
            }
            try {
                result_date = replace_date(today, year, static_cast<int>(*month), day);
            } catch (const ValueError&) {
                continue;
            }
            date_found = true;
            continue;
        }
        if (const long long* days = tables::find(d.calendar_units_days, tok);
            days && i + 1 < n && (has(d.next_words, tokens[i + 1]) || has(d.prev_words, tokens[i + 1]))) {
            const int sign = has(d.next_words, tokens[i + 1]) ? 1 : -1;
            result_date = fields(moment(result_date ? *result_date : today) + sign * days_delta(*days));
            date_found = true;
            i += 2;
            continue;
        }
        if (has(d.after_words, tok) || has(d.before_words, tok)) {
            const int sign = has(d.after_words, tok) ? 1 : -1;
            const auto [duration, j] = parse_duration_span(tokens, i + 1);
            if (duration) {
                delta = delta + sign * duration->first;
                delta_sign = sign;
                delta_is_time = duration->second;
                date_found = true;
                i = j;
                continue;
            }
            remainder.push_back(tok);
            ++i;
            continue;
        }
        if (d.hour_lookup.count(tok) ||
            ((tok == eleventh || tok == second) && i + 1 < n && (tokens[i + 1] == ten_fem || tokens[i + 1] == ten_fem_h))) {
            const size_t j = parse_clock(i);
            if (j > i) {
                i = j;
                continue;
            }
        }
        if (has(d.clock_words, tok)) {
            const size_t j = parse_clock(i + 1);
            if (j > i + 1) {
                i = j;
                continue;
            }
            if (i + 1 < n) {
                auto m = match_hhmm(tokens[i + 1]);
                if (m && !(0 <= m->first && m->first < 24 && 0 <= m->second && m->second < 60)) m.reset();
                std::optional<Number> val;
                size_t jj = 0;
                if (!m) std::tie(val, jj) = numbers_ar::parse_number_span(tokens, i + 1);
                if (m || (val && between(*val, 0, 24) && is_integer(*val))) {
                    if (m) {
                        hour = m->first;
                        minute = m->second;
                        i += 2;
                    } else {
                        hour = small_int(*val);
                        minute = 0;
                        i = jj;
                    }
                    date_found = true;
                    continue;
                }
            }
            remainder.push_back(tok);
            ++i;
            continue;
        }
        if (tok == d.midnight_word) {
            hour = 0;
            minute = 0;
            date_found = true;
            ++i;
            continue;
        }
        if (has(d.noon_words, tok)) {
            ampm = "pm";
            noon_seen = true;
            if (hour || result_date) date_found = true;
            ++i;
            continue;
        }
        if (has(d.am_words, tok)) {
            ampm = "am";
            if (hour || result_date) date_found = true;
            ++i;
            continue;
        }
        if (has(d.pm_words, tok)) {
            ampm = "pm";
            if (hour || result_date) date_found = true;
            ++i;
            continue;
        }
        if (tok == kAnd && (hour || delta_sign)) {
            ++i;
            continue;
        }
        if (tok == d.in_word && i + 1 < n && (has(d.clock_words, tokens[i + 1]) || d.months_lookup.count(tokens[i + 1]))) {
            ++i;
            continue;
        }
        if (delta_sign && (d.units_lookup.count(tok) || d.duals_lookup.count(tok) ||
                           numbers_ar::parse_number_span(tokens, i).first)) {
            const auto [duration, j] = parse_duration_span(tokens, i);
            if (duration) {
                delta = delta + delta_sign * duration->first;
                delta_is_time = delta_is_time || duration->second;
                i = j;
                continue;
            }
        }
        remainder.push_back(tok);
        ++i;
    }
    if (!hour && noon_seen) {
        hour = 12;
        minute = 0;
        date_found = true;
    }
    if (!date_found) return std::nullopt;
    if (delta.us != 0) {
        const DateTime& base = delta_is_time ? anchor_date : today;
        result_date = fields(moment(result_date ? *result_date : base) + delta);
    }
    if (!result_date) result_date = today;
    if (hour) {
        if (ampm == "pm" && *hour < 12) {
            *hour += 12;
        } else if (ampm == "am" && *hour == 12) {
            hour = 0;
        }
        result_date = replace_time(*result_date, ((*hour % 24) + 24) % 24, minute);
    }
    return Extracted{*result_date, text::s8(text::strip_ws(text::join(remainder, U" ")))};
}

// ---------------------------------------------------------------------------
// the holiday door of ovos_date_parser.holidays
// ---------------------------------------------------------------------------

// _names_a_holiday(text, "ar")
bool names_a_holiday(U32View text) {
    const auto& h = tables::holidays_ar();
    U32 folded;
    for (char32_t c : text) {
        if (c < 0x80) {
            folded.push_back(c >= U'A' && c <= U'Z' ? c - U'A' + U'a' : c);
        } else if (c >= 0x0600 && c <= 0x06FF) {
            const auto it = h.fold.find(c);
            folded += it == h.fold.end() ? U32(1, c) : it->second;
        } else if (text::is_space(c)) {
            folded.push_back(c);
        } else {
            throw NotPortedError("holiday folding outside ASCII and the Arabic block is not ported");
        }
    }
    // _loosen: re.findall(r"\w+(?:'\w+)?"), each word without a final 's or s
    std::vector<U32> words;
    for (size_t p = 0; p < folded.size();) {
        if (!text::is_word(folded[p])) {
            ++p;
            continue;
        }
        size_t q = p;
        while (q < folded.size() && text::is_word(folded[q])) ++q;
        if (q + 1 < folded.size() && folded[q] == U'\'' && text::is_word(folded[q + 1])) {
            ++q;
            while (q < folded.size() && text::is_word(folded[q])) ++q;
        }
        U32 w = folded.substr(p, q - p);
        U32 cut = w;
        if (text::ends_with(w, U"'s")) {
            cut = w.substr(0, w.size() - 2);
        } else if (text::ends_with(w, U"s")) {
            cut = w.substr(0, w.size() - 1);
        }
        words.push_back(cut.empty() ? w : cut);
        p = q;
    }
    const U32 loose = text::join(words, U" ");
    for (const U32& phrase : h.phrases) {
        for (size_t at = loose.find(phrase); at != U32::npos; at = loose.find(phrase, at + 1)) {
            const size_t end = at + phrase.size();
            if ((at == 0 || !text::is_word(loose[at - 1])) && (end == loose.size() || !text::is_word(loose[end]))) {
                return true;
            }
        }
    }
    return false;
}

bool starts(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }

std::string base_lang(std::string_view lang) {
    std::string out;
    for (char c : lang) {
        if (c == '-') break;
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

bool starts_any(std::string_view s, std::initializer_list<std::string_view> prefixes) {
    for (auto p : prefixes) {
        if (starts(s, p)) return true;
    }
    return false;
}

// A code the reference reads with the module of another language, or with
// dateparser, which the port does not hold.
[[noreturn]] void not_ported(std::string_view lang) {
    throw NotPortedError("language not ported: '" + std::string(lang) + "'");
}

// The prefixes nice_time has a module for, other than "ar".
bool nice_time_module(std::string_view lang) {
    return starts_any(lang, {"ast", "an", "az", "gl", "el", "he", "oc", "ro", "ca", "cs", "sk", "hr", "bg", "da",
                             "de", "en", "es", "et", "eu", "fa", "fi", "fr", "hu", "it", "kab", "fy", "nl", "nn",
                             "nb", "no", "pl", "pt", "ru", "sv", "sl", "uk", "ms", "id", "tr"});
}

// The codes whose ovos_date_parser/res/<code>/date_time.json holds the formats
// of nice_date, and "no", which reads those of nb.
bool has_date_formats(std::string_view lang) {
    static const std::array<std::string_view, 29> codes = {
        "ar", "az", "bg", "ca", "cs", "da", "de", "en", "et", "eu", "fa", "fi", "fr", "hr", "hu",
        "it", "kab", "nb", "nl", "nn", "pl", "ro", "ru", "sk", "sl", "sv", "tr", "uk", "no"};
    return std::find(codes.begin(), codes.end(), lang) != codes.end();
}

}  // namespace

std::string isoformat(const DateTime& dt) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute,
                  dt.second);
    std::string out = buf;
    if (dt.microsecond) {
        std::snprintf(buf, sizeof buf, ".%06d", dt.microsecond);
        out += buf;
    }
    return out;
}

std::string nice_time(const DateTime& dt, std::string_view lang, bool speech, bool use_24hour, bool use_ampm) {
    if (!starts(lang, "ar")) {
        if (nice_time_module(lang)) not_ported(lang);
        throw NotImplementedError("Unsupported language: " + std::string(lang));
    }
    return text::s8(nice_time_ar(dt, speech, use_24hour, use_ampm));
}

std::string nice_date(const DateTime& dt, std::string_view lang) {
    // lang.lower().split("-")[0], then the modules, then the formats of res/<lang>
    const std::string base = base_lang(lang);
    if (base != "ar") {
        if (starts_any(base, {"pt", "es", "gl", "el", "he", "oc", "ro", "ast", "an", "fy"}) || has_date_formats(base)) {
            not_ported(lang);
        }
        throw NotImplementedError("Unsupported language: " + base);
    }
    const auto& t = tables::date_time_ar();
    const int wd = weekday(moment(dt));
    return format_fields(t.date_full, {
        {"weekday", t.weekday.at(std::to_string(wd))},
        {"day", t.date.at(std::to_string(dt.day))},
        {"month", t.month.at(std::to_string(dt.month))},
        {"formatted_year", year_format(dt.year)},
    });
}

std::optional<Extracted> extract_datetime(std::string_view text, std::string_view lang, const DateTime& anchor) {
    // _extract_datetime_engine reads every code that starts with "ar" with
    // extract_datetime_ar, and any other code with a module or with dateparser.
    if (!starts(lang, "ar")) not_ported(lang);
    const U32 written = text::u(text);
    if (!written.empty() && names_a_holiday(written)) {
        const auto& answers = tables::holidays_ar().answers;
        const auto it = answers.find(std::string(text));
        if (lang != "ar" || !(anchor == DateTime{2000, 1, 1}) || it == answers.end()) {
            throw NotPortedError("the holiday layer is ported only for the texts arbtok sends it");
        }
        if (!it->second) return std::nullopt;
        const std::string& iso = it->second->first;
        DateTime dt;
        std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &dt.year, &dt.month, &dt.day, &dt.hour, &dt.minute, &dt.second);
        if (iso.size() > 19) dt.microsecond = std::stoi(iso.substr(20));
        return Extracted{dt, it->second->second};
    }
    return extract_datetime_ar(written, anchor);
}

}  // namespace arbtok::numbers::ovos_date_parser
