// The reference tables of data/numbers, read once from the copy compiled into
// the library.
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "text.hpp"

namespace arbtok::numbers::tables {

using text::U32;
using WordSet = std::unordered_set<U32>;
template <class V>
using WordMap = std::unordered_map<U32, V>;

// The JSON text of a data file, by its name under data/numbers.
std::string_view embedded(std::string_view name);

template <class V>
const V* find(const WordMap<V>& map, const U32& key) {
    const auto it = map.find(key);
    return it == map.end() ? nullptr : &it->second;
}
inline bool has(const WordSet& set, const U32& key) { return set.count(key) > 0; }

struct Scale {
    long long value;
    U32 singular, dual, plural;
};

// A lect's forms in the order of AR_LECT_FORMS.
struct LectForms {
    U32 lect;
    std::vector<std::pair<long long, U32>> forms;
};

// ovos_number_parser.numbers_ar.
struct NumbersAr {
    std::vector<U32> ones, ones_fem;
    std::map<long long, U32> teens_fem, tens, hundreds, ordinal_stems, ordinal_compound_unit, ordinal_stems_fem,
        ordinal_compound_unit_fem, fused_literary_teens;
    U32 twelve_fem_oblique, minus, decimal, infinity;
    std::vector<Scale> scales;
    std::map<long long, std::array<U32, 3>> fractions;
    std::map<std::string, std::string> dialect_default_case;
    std::vector<LectForms> lect_forms;

    WordMap<long long> units_lookup, tens_lookup, hundreds_lookup, scales_lookup, scale_duals_lookup,
        teen_first_lookup, fused_teens_lookup, construct_hundred_lookup, ordinal_units_lookup;
    WordMap<double> fractions_lookup, spoken_half_lookup;
    WordMap<U32> accusative_or_construct_dual, construct_dual_scales, context_canonical;
    WordSet teen_second_lookup, minus_lookup, decimal_lookup, hundred_mult_lookup, day_names, hundred_unit_homographs,
        homographs, day_words, per_cent_words, per_cent_after_fi, unit_words, number_words, proclitic_hosts,
        proclitic_not_numbers, proclitic_not_hosts, first_ordinals, broken_plural_heads, dual_noun_tokens;
    std::unordered_set<std::string> lects_with_spoken_half;
    U32 and_one, water_hundred, dead_two_hundred, construct_hundred, price_or_eighth, proclitics, clock_word,
        second_or_a_second, punct;
};
const NumbersAr& numbers_ar();

// ovos_date_parser.dates_ar.
struct DatesAr {
    std::map<long long, U32> fem_teens, hour_names, to_the_hour;
    WordMap<long long> units_lookup, duals_lookup, months_lookup, weekdays_lookup, hour_lookup, calendar_units_days;
    WordSet minute_words, today_words, tomorrow_words, yesterday_words, am_words, pm_words, noon_words, next_words,
        prev_words, after_words, before_words, clock_words, quarter_words, third_words, half_words;
    U32 midnight_word, except_word, day_word, of_word, in_word;
};
const DatesAr& dates_ar();

// ovos_date_parser/res/ar/date_time.json.
struct DateTimeAr {
    struct Format {
        std::vector<std::pair<std::string, std::string>> rules;  // (match, format), in key order 1, 2, ...
        std::string fallback;
    };
    Format decade_format, hundreds_format, thousand_format, year_format;
    std::string bc, date_full;
    std::map<std::string, std::string> weekday, date, month, number;
};
const DateTimeAr& date_time_ar();

// The holiday door of ovos_date_parser.holidays for "ar".
struct HolidaysAr {
    std::vector<U32> phrases;
    std::unordered_map<char32_t, U32> fold;
    // text -> (isoformat, remainder), or nullopt where the reference reads no date.
    std::unordered_map<std::string, std::optional<std::pair<std::string, std::string>>> answers;
};
const HolidaysAr& holidays_ar();

// The languages unicode-rbnf has rules for.
const std::vector<std::string>& rbnf_languages();

}  // namespace arbtok::numbers::tables
