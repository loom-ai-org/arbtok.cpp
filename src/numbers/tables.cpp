#include "tables.hpp"

#include <cstdlib>
#include <stdexcept>

#include "json.hpp"

namespace arbtok::numbers::tables {

namespace {

using json::Value;

U32 word(const Value& v) { return text::u(v.str); }

double number(const Value& v) {
    if (v.kind == Value::Kind::Object) return std::strtod(v.at("float").str.c_str(), nullptr);
    return std::strtod(v.str.c_str(), nullptr);
}

std::vector<U32> words(const Value& v) {
    std::vector<U32> out;
    for (const auto& item : v.items) out.push_back(word(item));
    return out;
}

WordSet word_set(const Value& v) {
    WordSet out;
    for (const auto& item : v.items) out.insert(word(item));
    return out;
}

std::map<long long, U32> by_int(const Value& v) {
    std::map<long long, U32> out;
    for (const auto& [k, item] : v.members) out[std::stoll(k)] = word(item);
    return out;
}

WordMap<long long> int_values(const Value& v) {
    WordMap<long long> out;
    for (const auto& [k, item] : v.members) out[text::u(k)] = item.integer();
    return out;
}

WordMap<double> float_values(const Value& v) {
    WordMap<double> out;
    for (const auto& [k, item] : v.members) out[text::u(k)] = number(item);
    return out;
}

WordMap<U32> word_values(const Value& v) {
    WordMap<U32> out;
    for (const auto& [k, item] : v.members) out[text::u(k)] = word(item);
    return out;
}

Value load(std::string_view name) { return json::parse(embedded(name)); }

}  // namespace

const NumbersAr& numbers_ar() {
    static const NumbersAr t = [] {
        const Value d = load("numbers_ar.json");
        NumbersAr o;
        o.ones = words(d.at("_ONES_AR"));
        o.ones_fem = words(d.at("_ONES_FEM_AR"));
        o.teens_fem = by_int(d.at("_TEENS_FEM_AR"));
        o.twelve_fem_oblique = word(d.at("_TWELVE_FEM_OBLIQUE_AR"));
        o.tens = by_int(d.at("_TENS_AR"));
        o.hundreds = by_int(d.at("_HUNDREDS_AR"));
        for (const auto& s : d.at("_SCALES_AR").items) {
            o.scales.push_back({s[0].integer(), word(s[1]), word(s[2]), word(s[3])});
        }
        o.minus = word(d.at("_MINUS_AR"));
        o.decimal = word(d.at("_DECIMAL_AR"));
        o.infinity = word(d.at("_INFINITY_AR"));
        for (const auto& [k, f] : d.at("_FRACTIONS_AR").members) o.fractions[std::stoll(k)] = {word(f[0]), word(f[1]), word(f[2])};
        o.ordinal_stems = by_int(d.at("_ORDINAL_STEMS_AR"));
        o.ordinal_compound_unit = by_int(d.at("_ORDINAL_COMPOUND_UNIT_AR"));
        o.ordinal_stems_fem = by_int(d.at("_ORDINAL_STEMS_FEM_AR"));
        o.ordinal_compound_unit_fem = by_int(d.at("_ORDINAL_COMPOUND_UNIT_FEM_AR"));
        for (const auto& [k, v] : d.at("AR_DIALECT_DEFAULT_CASE").members) o.dialect_default_case[k] = v.str;
        for (const auto& [lect, forms] : d.at("AR_LECT_FORMS").members) {
            LectForms lf{text::u(lect), {}};
            for (const auto& [k, v] : forms.members) lf.forms.emplace_back(std::stoll(k), word(v));
            o.lect_forms.push_back(std::move(lf));
        }
        o.units_lookup = int_values(d.at("_UNITS_LOOKUP"));
        o.tens_lookup = int_values(d.at("_TENS_LOOKUP"));
        o.hundreds_lookup = int_values(d.at("_HUNDREDS_LOOKUP"));
        o.scales_lookup = int_values(d.at("_SCALES_LOOKUP"));
        o.scale_duals_lookup = int_values(d.at("_SCALE_DUALS_LOOKUP"));
        o.fractions_lookup = float_values(d.at("_FRACTIONS_LOOKUP"));
        o.accusative_or_construct_dual = word_values(d.at("_ACCUSATIVE_OR_CONSTRUCT_DUAL"));
        o.teen_first_lookup = int_values(d.at("_TEEN_FIRST_LOOKUP"));
        o.teen_second_lookup = word_set(d.at("_TEEN_SECOND_LOOKUP"));
        o.and_one = word(d.at("_AND_ONE"));
        o.fused_teens_lookup = int_values(d.at("_FUSED_TEENS_LOOKUP"));
        o.minus_lookup = word_set(d.at("_MINUS_LOOKUP"));
        o.decimal_lookup = word_set(d.at("_DECIMAL_LOOKUP"));
        o.hundred_mult_lookup = word_set(d.at("_HUNDRED_MULT_LOOKUP"));
        o.construct_hundred_lookup = int_values(d.at("_CONSTRUCT_HUNDRED_LOOKUP"));
        o.spoken_half_lookup = float_values(d.at("_SPOKEN_HALF_LOOKUP"));
        for (const auto& v : d.at("_LECTS_WITH_SPOKEN_HALF").items) o.lects_with_spoken_half.insert(v.str);
        o.water_hundred = word(d.at("_WATER_HUNDRED"));
        o.dead_two_hundred = word(d.at("_DEAD_TWO_HUNDRED"));
        o.construct_hundred = word(d.at("_CONSTRUCT_HUNDRED"));
        o.day_names = word_set(d.at("_DAY_NAMES"));
        o.hundred_unit_homographs = word_set(d.at("_HUNDRED_UNIT_HOMOGRAPHS"));
        o.construct_dual_scales = word_values(d.at("_CONSTRUCT_DUAL_SCALES"));
        o.price_or_eighth = word(d.at("_PRICE_OR_EIGHTH"));
        o.homographs = word_set(d.at("_HOMOGRAPHS_AR"));
        o.day_words = word_set(d.at("_DAY_WORDS"));
        o.per_cent_words = word_set(d.at("_PER_CENT_WORDS"));
        o.per_cent_after_fi = word_set(d.at("_PER_CENT_AFTER_FI"));
        o.unit_words = word_set(d.at("_UNIT_WORDS_AR"));
        o.context_canonical = word_values(d.at("_CONTEXT_CANONICAL_AR"));
        o.ordinal_units_lookup = int_values(d.at("_ORDINAL_UNITS_LOOKUP"));
        o.number_words = word_set(d.at("_NUMBER_WORDS"));
        o.proclitics = word(d.at("_PROCLITICS_AR"));
        o.proclitic_hosts = word_set(d.at("_PROCLITIC_HOSTS"));
        o.proclitic_not_numbers = word_set(d.at("_PROCLITIC_NOT_NUMBERS"));
        o.proclitic_not_hosts = word_set(d.at("_PROCLITIC_NOT_HOSTS"));
        o.clock_word = word(d.at("_CLOCK_WORD"));
        o.first_ordinals = word_set(d.at("_FIRST_ORDINALS"));
        o.second_or_a_second = word(d.at("_SECOND_OR_A_SECOND"));
        o.broken_plural_heads = word_set(d.at("_BROKEN_PLURAL_HEADS"));
        o.punct = word(d.at("_PUNCT_AR"));
        o.fused_literary_teens = by_int(d.at("_FUSED_LITERARY_TEENS"));
        o.dual_noun_tokens = word_set(d.at("_DUAL_NOUN_TOKENS"));
        return o;
    }();
    return t;
}

const DatesAr& dates_ar() {
    static const DatesAr t = [] {
        const Value d = load("dates_ar.json");
        DatesAr o;
        o.fem_teens = by_int(d.at("_FEM_TEENS_AR"));
        o.hour_names = by_int(d.at("_HOUR_NAMES_AR"));
        o.to_the_hour = by_int(d.at("_TO_THE_HOUR_AR"));
        o.units_lookup = int_values(d.at("_UNITS_LOOKUP_AR"));
        o.duals_lookup = int_values(d.at("_DUALS_LOOKUP_AR"));
        o.months_lookup = int_values(d.at("_MONTHS_LOOKUP_AR"));
        o.weekdays_lookup = int_values(d.at("_WEEKDAYS_LOOKUP_AR"));
        o.hour_lookup = int_values(d.at("_HOUR_LOOKUP_AR"));
        o.minute_words = word_set(d.at("_MINUTE_WORDS_AR"));
        o.today_words = word_set(d.at("_TODAY_WORDS"));
        o.tomorrow_words = word_set(d.at("_TOMORROW_WORDS"));
        o.yesterday_words = word_set(d.at("_YESTERDAY_WORDS"));
        o.am_words = word_set(d.at("_AM_WORDS"));
        o.pm_words = word_set(d.at("_PM_WORDS"));
        o.noon_words = word_set(d.at("_NOON_WORDS"));
        o.midnight_word = word(d.at("_MIDNIGHT_WORD"));
        o.next_words = word_set(d.at("_NEXT_WORDS"));
        o.prev_words = word_set(d.at("_PREV_WORDS"));
        o.after_words = word_set(d.at("_AFTER_WORDS"));
        o.before_words = word_set(d.at("_BEFORE_WORDS"));
        o.clock_words = word_set(d.at("_CLOCK_WORDS"));
        for (const auto& [k, v] : d.at("_CALENDAR_UNITS").members) o.calendar_units_days[text::u(k)] = v.at("days").integer();
        o.except_word = word(d.at("_EXCEPT_WORD"));
        o.quarter_words = word_set(d.at("_QUARTER_WORDS"));
        o.third_words = word_set(d.at("_THIRD_WORDS"));
        o.half_words = word_set(d.at("_HALF_WORDS"));
        o.day_word = word(d.at("_DAY_WORD"));
        o.of_word = word(d.at("_OF_WORD"));
        o.in_word = word(d.at("_IN_WORD"));
        return o;
    }();
    return t;
}

const DateTimeAr& date_time_ar() {
    static const DateTimeAr t = [] {
        const Value d = load("date_time_ar.json");
        DateTimeAr o;
        auto format = [&](const char* name) {
            const Value& f = d.at(name);
            DateTimeAr::Format out;
            for (int i = 1;; ++i) {
                const std::string key = std::to_string(i);
                bool found = false;
                for (const auto& [k, v] : f.members) {
                    if (k == key) {
                        out.rules.emplace_back(v.at("match").str, v.at("format").str);
                        found = true;
                    }
                }
                if (!found) break;
            }
            out.fallback = f.at("default").str;
            return out;
        };
        o.decade_format = format("decade_format");
        o.hundreds_format = format("hundreds_format");
        o.thousand_format = format("thousand_format");
        o.year_format = format("year_format");
        o.bc = d.at("year_format").at("bc").str;
        o.date_full = d.at("date_format").at("date_full").str;
        for (const char* name : {"weekday", "date", "month", "number"}) {
            auto& target = std::string(name) == "weekday" ? o.weekday
                         : std::string(name) == "date"    ? o.date
                         : std::string(name) == "month"   ? o.month
                                                          : o.number;
            for (const auto& [k, v] : d.at(name).members) target[k] = v.str;
        }
        return o;
    }();
    return t;
}

const HolidaysAr& holidays_ar() {
    static const HolidaysAr t = [] {
        const Value d = load("holidays_ar.json");
        HolidaysAr o;
        o.phrases = words(d.at("phrases"));
        for (const auto& [k, v] : d.at("fold").members) {
            const U32 c = text::u(k);
            if (c.size() != 1) throw std::runtime_error("holidays_ar.json: fold key is not one code point");
            o.fold[c[0]] = word(v);
        }
        for (const auto& [k, v] : d.at("answers").members) {
            if (v.is_null()) {
                o.answers[k] = std::nullopt;
            } else {
                o.answers[k] = std::make_pair(v[0].str, v[1].str);
            }
        }
        return o;
    }();
    return t;
}

const std::vector<std::string>& rbnf_languages() {
    static const std::vector<std::string> t = [] {
        std::vector<std::string> out;
        for (const auto& v : load("rbnf_languages.json").items) out.push_back(v.str);
        return out;
    }();
    return t;
}

}  // namespace arbtok::numbers::tables
