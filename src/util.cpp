#include "arbtok/util.hpp"

#include <orthography2ipa/langcodes.hpp>

#include <algorithm>

#include "arbtok/numbers/unicode_rbnf.hpp"
#include "arbtok/textnorm.hpp"
#include "textnorm_detail.hpp"

namespace arbtok::util {

namespace {

using namespace textnorm::detail;
namespace py = textnorm::py_util;
namespace rbnf = numbers::unicode_rbnf;
using numbers::ovos_date_parser::DateTime;
using std::nullopt;
using std::optional;

Tables tables(const textnorm::py::LangTable* begin, const textnorm::py::LangTable* end) {
    Tables out;
    for (const textnorm::py::LangTable* t = begin; t != end; ++t) {
        Table& table = out.emplace_back(s8(t->lang), Table()).second;
        for (size_t i = 0; i < t->size; ++i) table.emplace_back(s8(t->items[i].first), s8(t->items[i].second));
    }
    return out;
}

// Runs fn as Python runs a body under `except Exception`: an arbtok::Error
// gives nullopt. NotImplementedError still propagates, since it marks a path
// the port does not hold and must not read as the reference's fallback.
template <class F>
optional<U32> attempt(F&& fn) {
    try {
        return fn();
    } catch (const Error& e) {
        if (e.python_type() == "NotImplementedError") throw;
        return nullopt;
    }
}

std::string first_subtag(const std::string& lang) { return lang.substr(0, lang.find('-')); }

U32 replace_all(U32View s, U32View from, U32View to) {
    if (from.empty()) {
        // str.replace("", to) puts `to` between every two characters and at both ends.
        U32 out(to);
        for (const char32_t c : s) {
            out += c;
            out += to;
        }
        return out;
    }
    return replace(s, from, to);
}

U32 replace_first(U32View s, U32View from, U32View to) {
    const size_t at = s.find(from);
    if (at == U32View::npos) return U32(s);
    return U32(s.substr(0, at)) + U32(to) + U32(s.substr(at + from.size()));
}

bool has_text(U32View s, U32View part) { return s.find(part) != U32View::npos; }

// float(text) if "." is in it, else int(text): ValueError where Python raises it.
Number read_number(U32View text) {
    const std::string t = s8(text);
    if (has_text(text, U".")) {
        const auto v = numbers::py_float(t);
        if (!v) throw ValueError("could not convert string to float: " + repr(text));
        return *v;
    }
    return to_bigint(text);
}

bool leap(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// datetime.date(year, month, day); ValueError for a day that does not exist.
DateTime make_date(const BigInt& year, const BigInt& month, const BigInt& day) {
    const auto y = year.to_ll(), m = month.to_ll(), d = day.to_ll();
    if (!y || *y < 1 || *y > 9999) throw ValueError("year is out of range");
    if (!m || *m < 1 || *m > 12) throw ValueError("month must be in 1..12");
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const long long last = *m == 2 && leap(*y) ? 29 : days[*m - 1];
    if (!d || *d < 1 || *d > last) throw ValueError("day is out of range for month");
    DateTime dt;
    dt.year = static_cast<int>(*y);
    dt.month = static_cast<int>(*m);
    dt.day = static_cast<int>(*d);
    return dt;
}

std::pair<U32, U32> separators(const std::string& full_lang) {
    const std::string code = first_subtag(full_lang);
    if (code == "pt" || code == "es" || code == "fr" || code == "de") return {U",", U"."};
    return {U".", U","};
}

U32 pronounce(const Number& n, const std::string& lang) { return pronounce_number(n, lang); }

U32 normalize_number_word(const U32& word, const std::string& full_lang, const optional<rbnf::RbnfEngine>& engine) {
    static const U32 kPunctuation = U"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    const U32 cleaned = rstrip_chars(word, kPunctuation);
    const U32 suffix = word.substr(cleaned.size());
    if (is_fraction(s8(cleaned))) {
        if (auto out = attempt([&] { return pronounce_fraction(cleaned, full_lang) + suffix; })) return *out;
        return word;
    }
    const auto [decimal, thousands] = separators(full_lang);
    U32 t = cleaned;
    const bool thousands_and_decimal =
        has_text(t, thousands) && has_text(t, decimal) && t.find(thousands) < t.find(decimal);
    if (thousands_and_decimal) {
        t = replace_all(replace_all(t, thousands, U""), decimal, U".");
    } else if (has_text(t, decimal) && is_numeric(replace_first(t, decimal, U"."))) {
        t = replace_all(t, decimal, U".");
    } else if (has_text(t, thousands)) {
        if (const auto resolved = resolve_thousands_or_decimal(s8(t), s8(thousands))) t = u(*resolved);
    }
    if (is_numeric(t)) {
        if (auto out = attempt([&] { return pronounce(read_number(t), full_lang) + suffix; })) return *out;
        return word;
    }
    if (engine && isdigit(cleaned)) {
        if (auto out = attempt([&] {
                return u(bridged([&] { return engine->format_number(s8(cleaned), rbnf::FormatPurpose::CARDINAL); })) +
                       suffix;
            }))
            return *out;
        return word;
    }
    return word;
}

U32 normalize_dates_and_times(U32 text, const std::string& full_lang, const std::string& date_format) {
    if (first_subtag(full_lang) == "en") {
        text = pyre::compiled(U"(?i)(\\d+)(am|pm)").sub(U"\\1 \\2", text);
        text = replace_all(replace_all(text, U"am", U"A M"), U"pm", U"P M");
    }
    text = pyre::compiled(U"(\\d{1,2})h(\\d{2})", pyre::IGNORECASE)
               .sub([&](const Match& m) { return u(pronounce_time(s8(m.str(0)), full_lang)); }, text);
    const auto match = pyre::compiled(U"(\\d{1,4})[/-](\\d{1,2})[/-](\\d{1,4})").search(text);
    if (!match) return text;
    const U32 s1 = match->str(1), s2 = match->str(2), s3 = match->str(3);
    const BigInt p1 = to_bigint(s1), p2 = to_bigint(s2), p3 = to_bigint(s3);
    BigInt year;
    std::vector<BigInt> rest;
    if (s1.size() == 4) {
        year = p1;
        rest = {p2, p3};
    } else if (s3.size() == 4) {
        year = p3;
        rest = {p1, p2};
    } else {
        year = p3;
        if (year < BigInt(100)) year = year < BigInt(30) ? BigInt(2000) + year : BigInt(1900) + year;
        rest = {p1, p2};
    }
    optional<BigInt> day, month;
    const bool two_digit_day = std::any_of(rest.begin(), rest.end(), [](const BigInt& p) {
        return BigInt(12) < p && p.str().size() == 2;
    });
    if (two_digit_day) {
        const auto it = std::find_if(rest.begin(), rest.end(), [](const BigInt& p) { return BigInt(12) < p; });
        day = *it;
        rest.erase(it);
        month = rest.at(0);
    }
    if (!day || !month) {
        if (s8(lower(u(date_format))) == "mdy") {
            month = rest.at(0);
            day = rest.at(1);
        } else {
            day = rest.at(0);
            month = rest.at(1);
        }
    }
    try {
        const DateTime date = make_date(year, *month, *day);
        return replace_all(text, match->str(0), u(pronounce_date(date, full_lang)));
    } catch (const Error& e) {
        if (e.python_type() != "ValueError" && e.python_type() != "IndexError") throw;
        return text;
    }
}

U32 normalize_word_hyphen_digit(U32View text) { return pyre::compiled(U"(\\w+)-(\\d+)").sub(U"\\1 \\2", text); }

U32 unit_word(const Table& units, const U32& symbol) {
    const std::string s = s8(symbol);
    for (const auto& [k, v] : units)
        if (k == s) return u(v);
    optional<std::string> found;
    const U32 low = lower(symbol);
    for (const auto& [k, v] : units)
        if (lower(u(k)) == low) found = v;
    if (!found) throw KeyError(repr(low));
    return u(*found);
}

bool is_clock_time(const Match& m) {
    const U32 number = m.str(1);
    if (!isdigit(number)) return false;
    const BigInt n = to_bigint(number);
    if (m.start() > 0 && m.string()[m.start() - 1] == U':') return n < BigInt(60);
    return !(n < BigInt(1)) && !(BigInt(12) < n);
}

const Table* lang_table(const Tables& tables, const std::string& lang) {
    for (const auto& [name, table] : tables)
        if (name == lang) return &table;
    return nullptr;
}

U32 normalize_units(U32 text, const std::string& full_lang) {
    text = replace_all(text, U"º", U"°");
    const Table* units = lang_table(UNITS, first_subtag(full_lang));
    if (units == nullptr) return text;
    const auto separator_pair = separators(full_lang);
    const U32 decimal_separator = separator_pair.first, thousands_separator = separator_pair.second;
    const U32 thousands = pyre::escape(thousands_separator), decimal = pyre::escape(decimal_separator);
    const U32 grouped_pattern = U"\\d{1,3}(?:" + thousands + U"\\d{3})+(?:" + decimal + U"\\d+)?";
    const Pattern& grouped = pyre::compiled(grouped_pattern);
    const U32 number_pattern =
        U"(?<![\\d.,])(" + grouped_pattern + U"|\\d+(?:[" + decimal + U".]\\d+)?)(?![\\d.,]*\\d)";

    Table symbolic, alphanumeric;
    for (const auto& kv : *units) (isalnum(u(kv.first)) ? alphanumeric : symbolic).push_back(kv);

    auto alternation = [](const Table& units) {
        std::vector<U32> keys;
        for (const auto& kv : units) keys.push_back(u(kv.first));
        std::stable_sort(keys.begin(), keys.end(), [](const U32& a, const U32& b) { return a.size() > b.size(); });
        std::vector<U32> escaped;
        for (const auto& k : keys) escaped.push_back(pyre::escape(k));
        return join(escaped, U"|");
    };
    auto number_of = [&](const Match& m) {
        U32 number = m.str(1);
        if (grouped.fullmatches(number)) number = replace_all(number, thousands_separator, U"");
        if (decimal_separator != U".") number = replace_all(number, decimal_separator, U".");
        return number;
    };

    const U32 symbolic_alternation = alternation(symbolic);
    if (!symbolic_alternation.empty()) {
        const Pattern pattern(U32View(number_pattern + U"\\s*(" + symbolic_alternation + U")"), pyre::IGNORECASE);
        text = pattern.sub(
            [&](const Match& m) {
                const U32 number = number_of(m);
                const U32 word = unit_word(symbolic, m.str(2));
                if (auto out = attempt([&] { return pronounce(read_number(number), full_lang) + U" " + word; }))
                    return *out;
                return m.str(0);
            },
            text);
    }
    const U32 alphanumeric_alternation = alternation(alphanumeric);
    if (!alphanumeric_alternation.empty()) {
        const Pattern pattern(U32View(number_pattern + U"\\s*(" + alphanumeric_alternation + U")\\b"),
                              pyre::IGNORECASE);
        text = pattern.sub(
            [&](const Match& m) {
                const U32 number = number_of(m);
                const U32 symbol = m.str(2);
                const U32 low = lower(symbol);
                if (textnorm::detail::in(low, py::CLOCK_MARKERS) && is_clock_time(m)) return m.str(0);
                const auto written = std::find_if(alphanumeric.begin(), alphanumeric.end(),
                                                  [&](const auto& kv) { return lower(u(kv.first)) == low; });
                if (written == alphanumeric.end()) throw Error("StopIteration", "");
                if (isupper(symbol) && !isupper(u(written->first)) && !textnorm::detail::in(low, py::CAPITALISED_UNITS))
                    return m.str(0);
                const U32 word = unit_word(alphanumeric, symbol);
                return pronounce(read_number(number), full_lang) + U" " + word;
            },
            text);
    }
    return text;
}

U32 normalize_word(const U32& word, const std::string& full_lang, const optional<rbnf::RbnfEngine>& engine) {
    const std::string code = first_subtag(full_lang);
    const std::string w = s8(word);
    for (const auto* table : {lang_table(CONTRACTIONS, code), lang_table(TITLES, code)}) {
        if (table == nullptr) continue;
        for (const auto& [k, v] : *table)
            if (k == w) return u(v);
    }
    return normalize_number_word(word, full_lang, engine);
}

U32 verbalize_residual_digits_ar(U32View input) {
    static const Table& names_table = [] () -> const Table& {
        static const Table t = textnorm::spelled_codes("ar");
        return t;
    }();
    static const textnorm::detail::Table digits = table(py::AR_INDIC_DIGITS);
    const U32 text = translate(input, digits);
    auto letters = [&](const U32& run, bool after) -> U32 {
        if (after)
            if (const auto* unit = lookup(run, py::GLUED_TIME_UNITS)) return U32(*unit);
        if (run.size() != 1) return run;
        const std::string key = s8(upper(run));
        for (const auto& [k, v] : names_table)
            if (k == key) return u(v);
        return run;
    };
    U32 out = pyre::compiled(py::GLUED_DIGIT_RUN.pattern, py::GLUED_DIGIT_RUN.flags)
                  .sub(
                      [&](const Match& m) {
                          const U32 before = m.str(1), after = m.str(3);
                          if (before.empty() && after.empty()) return m.str(0);
                          std::vector<U32> parts;
                          for (const U32& part : {letters(before, false), pronounce(to_bigint(m.str(2)), "ar"),
                                                  letters(after, true)})
                              if (!part.empty()) parts.push_back(part);
                          return join(parts, U" ");
                      },
                      text);
    out = pyre::compiled(py::RESIDUAL_NUM.pattern, py::RESIDUAL_NUM.flags)
              .sub(
                  [](const Match& m) {
                      const U32 words = pronounce(to_bigint(m.str(1)), "ar");
                      return m.matched(2) && !m.str(2).empty() ? words + U" " + U32(py::PERCENT_WORD_AR) : words;
                  },
                  out);
    return pyre::compiled(py::RESIDUAL_PERCENT.pattern, py::RESIDUAL_PERCENT.flags)
        .sub(U" " + U32(py::PERCENT_WORD_AR), out);
}

}  // namespace

const Tables CONTRACTIONS = tables(std::begin(py::CONTRACTIONS), std::end(py::CONTRACTIONS));
const Tables TITLES = tables(std::begin(py::TITLES), std::end(py::TITLES));
const Tables UNITS = tables(std::begin(py::UNITS), std::end(py::UNITS));

std::pair<std::string, int> match_lang(const std::string& target_lang, const std::vector<std::string>& valid_langs) {
    namespace lc = orthography2ipa::langcodes;
    if (std::find(valid_langs.begin(), valid_langs.end(), target_lang) != valid_langs.end()) return {target_lang, 0};
    std::string best_lang = "und";
    long long best_distance = 10000000;
    for (std::string l : valid_langs) {
        optional<int> distance = lc::tag_distance(l, target_lang);
        if (!distance) {
            const auto parts = split(u(l), U"-");
            if (parts.size() >= 2) {
                l = s8(parts[0]) + "-" + s8(parts[1]);
                distance = lc::tag_distance(l, target_lang);
            }
            if (!distance) distance = lc::tag_distance(s8(parts[0]), target_lang);
            if (!distance) continue;
        }
        if (*distance < best_distance) {
            best_lang = l;
            best_distance = *distance;
        }
    }
    if (best_distance <= 10) return {best_lang, static_cast<int>(best_distance)};
    return {"und", 10000};
}

std::optional<std::string> resolve_thousands_or_decimal(const std::string& word, const std::string& thousands_separator) {
    const U32 w = u(word), sep = u(thousands_separator);
    const U32 escaped = pyre::escape(sep);
    if (pyre::compiled(U"\\d{1,3}(?:" + escaped + U"\\d{3})+").fullmatches(w)) return s8(replace_all(w, sep, U""));
    const size_t occurrences = sep.empty() ? w.size() + 1 : count(w, sep);
    if (occurrences == 1) {
        const size_t at = w.find(sep);
        const U32 candidate = w.substr(0, at) + U"." + w.substr(at + sep.size());
        if (is_numeric(candidate)) return s8(candidate);
    }
    return std::nullopt;
}

std::string pronounce_date(const DateTime& date, const std::string& full_lang) {
    return s8(nice_date(date, full_lang));
}

std::string pronounce_time(const std::string& time_string, const std::string& full_lang) {
    const U32 text = u(time_string);
    const auto spoken = attempt([&] {
        const auto parts = split(text, U"h");
        if (parts.size() != 2) throw ValueError("wrong number of values to unpack");
        const long long hours = to_int(parts[0]), minutes = to_int(parts[1]);
        if (hours < 0 || hours > 23) throw ValueError("hour must be in 0..23");
        if (minutes < 0 || minutes > 59) throw ValueError("minute must be in 0..59");
        DateTime time;
        time.year = 1900;
        time.hour = static_cast<int>(hours);
        time.minute = static_cast<int>(minutes);
        return nice_time(time, full_lang, true, true, false);
    });
    return spoken ? s8(*spoken) : s8(replace_all(text, U"h", U" "));
}

bool is_fraction(const std::string& word) {
    const U32 w = u(word);
    if (!has_text(w, U"/")) return false;
    const auto parts = split(w, U"/");
    return parts.size() == 2 && isdigit(parts[0]) && isdigit(parts[1]);
}

std::string normalize(const std::string& text, const std::string& lang) {
    const std::string& full_lang = lang;
    const std::string code = first_subtag(full_lang);
    U32 dialog = u(text);
    const std::string date_format = s8(lower(u(full_lang))) == "en-us" ? "MDY" : "DMY";
    try {
        dialog = normalize_dates_and_times(dialog, full_lang, date_format);
    } catch (const Error& e) {
        if (e.python_type() == "NotImplementedError") throw;
    }
    dialog = normalize_word_hyphen_digit(dialog);
    dialog = normalize_units(dialog, full_lang);
    optional<rbnf::RbnfEngine> engine;
    try {
        engine = bridged([&] { return rbnf::RbnfEngine::for_language(code); });
    } catch (const Error& e) {
        if (e.python_type() != "ValueError" && e.python_type() != "KeyError") throw;
    }
    std::vector<U32> words;
    for (const U32& word : split_ws(dialog)) words.push_back(normalize_word(word, full_lang, engine));
    dialog = join(words, U" ");
    if (code == "ar") dialog = verbalize_residual_digits_ar(dialog);
    return s8(dialog);
}

}  // namespace arbtok::util
