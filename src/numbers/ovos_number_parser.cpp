#include "arbtok/numbers/ovos_number_parser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>

#include "arbtok/numbers/errors.hpp"
#include "arbtok/numbers/unicode_rbnf.hpp"
#include "numbers_ar.hpp"

namespace arbtok::numbers::ovos_number_parser {

namespace {

using text::U32;
using text::U32View;

bool starts(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }

bool starts_any(std::string_view s, std::initializer_list<std::string_view> prefixes) {
    for (auto p : prefixes) {
        if (starts(s, p)) return true;
    }
    return false;
}

// lang.lower().split("-")[0]; the codes compared with it are ASCII.
std::string base_lang(std::string_view lang) {
    std::string out;
    for (char c : lang) {
        if (c == '-') break;
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

bool is_ar(std::string_view lang) { return numbers_ar::resolve_ar_lang(lang).has_value(); }

// A code the reference reads with the module of another language, which the
// port does not hold.
[[noreturn]] void not_ported(std::string_view lang) {
    throw NotPortedError("language not ported: '" + std::string(lang) + "'");
}

[[noreturn]] void unsupported(std::string_view lang) {
    throw NotImplementedError("Unsupported language: '" + std::string(lang) + "'");
}

// The fallback of pronounce_number and pronounce_ordinal to unicode-rbnf, for
// lang.split("-")[0]. A code with rules is read by those rules, which the port
// does not hold. Any error of the fallback is NotImplementedError.
[[noreturn]] void rbnf_fallback(std::string_view lang) {
    try {
        unicode_rbnf::RbnfEngine::for_language(lang.substr(0, lang.find('-')));
    } catch (const Error&) {
        unsupported(lang);
    }
    not_ported(lang);
}

// The prefixes _pronounce_number_dispatch has a module for, after en and az and
// the Arabic codes.
bool pronounce_number_module(std::string_view lang) {
    return starts_any(lang, {"en", "az", "bg", "ca", "ast", "oc", "an", "cs", "da", "de", "gl", "el", "es", "eu",
                             "et", "fa", "fi", "fr", "he", "hr", "hu", "id", "ms", "tr", "it", "kab", "fy", "nn",
                             "nb", "no", "nl", "pl", "pt", "mwl", "ro", "ru", "sk", "sl", "sv", "uk"});
}

// The prefixes pronounce_ordinal has a module for, other than the Arabic codes.
bool pronounce_ordinal_module(std::string_view lang) {
    return starts_any(lang, {"pt", "mwl", "ro", "ast", "ca", "oc", "an", "da", "de", "et", "fi", "hr", "el", "he",
                             "hu", "id", "ms", "tr", "fy", "nn", "nb", "no", "nl", "sv", "gl", "cs", "sk", "pl", "uk",
                             "eu", "fa", "bg", "sl", "kab"});
}

// The keys of _NICE_NUMBER_FNS.
bool nice_number_lang(std::string_view lang2) {
    static const std::array<std::string_view, 31> keys = {
        "ar", "az", "bg", "ca", "cs", "da", "de", "el", "es", "et", "eu", "fa", "fi", "fr", "fy", "he",
        "hr", "hu", "id", "it", "ms", "nb", "nl", "nn", "pl", "ru", "sk", "sl", "sv", "tr", "uk"};
    return std::find(keys.begin(), keys.end(), lang2) != keys.end();
}

const BigInt& ten_to_15() {
    static const BigInt value = BigInt::from_string("1000000000000000");
    return value;
}

// _round_for_speech(number, places).
Number round_for_speech(const Number& number, int places) {
    const auto* d = std::get_if<double>(&number);
    if (!d || !std::isfinite(*d)) return number;
    if (*d == 0) return 0.0;
    if (std::trunc(*d) == *d) return number;
    // Decimal(repr(number)).quantize(Decimal(1).scaleb(-places), ROUND_HALF_UP)
    const std::string r = float_repr(*d);
    const bool negative = r[0] == '-';
    std::string mantissa = r.substr(negative ? 1 : 0);
    long exponent = 0;
    if (const auto e = mantissa.find('e'); e != std::string::npos) {
        exponent = std::stol(mantissa.substr(e + 1));
        mantissa.resize(e);
    }
    std::string digits;
    for (size_t k = 0; k < mantissa.size(); ++k) {
        if (mantissa[k] == '.') {
            exponent -= static_cast<long>(mantissa.size() - k - 1);
        } else {
            digits.push_back(mantissa[k]);
        }
    }
    const long target = -static_cast<long>(places);
    std::string coefficient;
    if (exponent >= target) {
        coefficient = digits + std::string(static_cast<size_t>(exponent - target), '0');
    } else {
        const size_t drop = static_cast<size_t>(target - exponent);
        coefficient = drop >= digits.size() ? "0" : digits.substr(0, digits.size() - drop);
        const char first_dropped = drop > digits.size() ? '0' : digits[digits.size() - drop];
        if (first_dropped >= '5') {
            int k = static_cast<int>(coefficient.size()) - 1;
            while (k >= 0 && coefficient[static_cast<size_t>(k)] == '9') coefficient[static_cast<size_t>(k--)] = '0';
            if (k < 0) {
                coefficient.insert(0, "1");
            } else {
                ++coefficient[static_cast<size_t>(k)];
            }
        }
    }
    const size_t lead = coefficient.find_first_not_of('0');
    coefficient = lead == std::string::npos ? "0" : coefficient.substr(lead);
    if (coefficient.size() > 28) return number;  // InvalidOperation: past the context precision
    if (coefficient == "0") return number;
    bool integral = target >= 0;
    if (!integral) {
        const size_t frac = static_cast<size_t>(-target);
        integral = coefficient.size() > frac &&
                   coefficient.find_first_not_of('0', coefficient.size() - frac) == std::string::npos;
        if (integral) coefficient.resize(coefficient.size() - frac);
    } else {
        coefficient += std::string(static_cast<size_t>(target), '0');
    }
    if (integral) return BigInt::from_string((negative ? "-" : "") + coefficient);
    const std::string text = (negative ? "-" : "") + coefficient + "e" + std::to_string(target);
    return std::strtod(text.c_str(), nullptr);
}

std::string pronounce_infinity(double number, std::string_view lang) {
    static const std::map<std::string, std::array<const char*, 2>> words = {
        {"en", {"infinity", "negative infinity"}}, {"es", {"infinito", "infinito negativo"}},
        {"pt", {"infinito", "infinito negativo"}}, {"it", {"infinito", "meno infinito"}},
        {"fr", {"infini", "moins infini"}},        {"de", {"unendlich", "minus unendlich"}},
    };
    auto it = words.find(base_lang(lang));
    if (it == words.end()) it = words.find("en");
    return it->second[number < 0 ? 1 : 0];
}

const char* scientific_words(std::string_view lang) {
    static const std::map<std::string, const char*> words = {
        {"en", "times ten to the power of"}, {"es", "por diez elevado a"}, {"pt", "vezes dez elevado a"},
        {"it", "per dieci alla"},           {"fr", "fois dix puissance"}, {"de", "mal zehn hoch"},
        {"nl", "maal tien tot de macht"},   {"ca", "per deu elevat a"},   {"gl", "veces dez elevado a"},
        {"ro", "ori zece la puterea"},      {"da", "gange ti i"},         {"sv", "gånger tio upphöjt till"},
    };
    auto it = words.find(base_lang(lang));
    if (it == words.end()) it = words.find("en");
    return it->second;
}

bool has_padded_scale_words(const std::string& spoken) {
    const auto words = text::split_ws(text::u(spoken));
    for (size_t k = 1; k < words.size(); ++k) {
        if (words[k] == words[k - 1]) return true;
    }
    return false;
}

std::string bignum_fallback(const Number& number, std::string_view lang, int places) {
    const std::string digits = abs(to_int(number)).digits;
    const size_t exponent = digits.size() - 1;
    // digits[1:1 + places]
    long end = 1L + places;
    if (end < 0) end = std::max(0L, end + static_cast<long>(digits.size()));
    std::string tail = end > 1 ? digits.substr(1, static_cast<size_t>(end - 1)) : "";
    while (!tail.empty() && tail.back() == '0') tail.pop_back();
    double mantissa = std::strtod((digits.substr(0, 1) + (tail.empty() ? "" : "." + tail)).c_str(), nullptr);
    if (compare(number, Number(BigInt(0))) < 0) mantissa = -mantissa;
    return pronounce_number(mantissa, lang, places) + " " + scientific_words(lang) + " " +
           pronounce_number(BigInt(static_cast<long long>(exponent)), lang);
}

// The prefixes extract_number has a language module for.
bool extract_number_lang(std::string_view lang) {
    return starts_any(lang, {"en", "az", "bg", "ca", "cs", "da", "de", "el", "es", "gl", "eu", "et", "fa", "fi", "fr",
                             "id", "it", "ms", "tr", "kab", "fy", "nn", "nb", "no", "nl", "pl", "pt", "mwl", "ro",
                             "ast", "oc", "an", "ru", "sk", "sv", "uk", "hr", "he", "hu", "sl"});
}

}  // namespace

std::string pronounce_number(const Number& number, std::string_view lang, int places, GrammaticalGender gender,
                             const std::optional<std::string>& grammatical_case) {
    if (const auto* d = std::get_if<double>(&number)) {
        if (std::isnan(*d)) throw ValueError("cannot pronounce NaN (not a number)");
        if (std::isinf(*d)) return pronounce_infinity(*d, lang);
    }
    const Number n = round_for_speech(number, places);
    const bool big = compare(abs(n), Number(ten_to_15())) >= 0;
    std::string spoken;
    try {
        if (starts_any(lang, {"en", "az"})) not_ported(lang);
        if (!is_ar(lang)) {
            if (pronounce_number_module(lang)) not_ported(lang);
            rbnf_fallback(lang);
        }
        const auto c = grammatical_case ? grammatical_case : numbers_ar::resolve_ar_lang(lang);
        spoken = text::s8(numbers_ar::pronounce_number_ar(n, places, c, numbers_ar::resolve_ar_lect(lang),
                                                          gender));
    } catch (const Error& e) {
        const auto& type = e.python_type();
        if ((type == "IndexError" || type == "KeyError" || type == "OverflowError") && !starts(lang, "kab") && big) {
            return bignum_fallback(n, lang, places);
        }
        throw;
    }
    if (big && !starts(lang, "kab") && is_integer(n) && has_padded_scale_words(spoken)) {
        return bignum_fallback(n, lang, places);
    }
    return spoken;
}

std::string pronounce_ordinal(const Number& number, std::string_view lang, GrammaticalGender gender,
                              const std::optional<std::string>& grammatical_case) {
    if (starts_any(lang, {"pt", "mwl", "ro", "ast", "ca", "oc", "an"})) not_ported(lang);
    if (!is_ar(lang)) {
        if (pronounce_ordinal_module(lang)) not_ported(lang);
        rbnf_fallback(lang);
    }
    return text::s8(numbers_ar::pronounce_ordinal_ar(number, gender, grammatical_case));
}

std::string pronounce_fraction(std::string_view fraction_word, std::string_view lang) {
    if (starts_any(lang, {"pt", "ast", "ca", "oc", "an", "mwl", "gl", "ro"})) not_ported(lang);
    // _pronounce_fraction_generic
    const auto parts = text::split(text::u(fraction_word), U"/");
    if (parts.size() != 2) {
        throw ValueError(parts.size() > 2 ? "too many values to unpack (expected 2)"
                                          : "not enough values to unpack (expected 2, got 1)");
    }
    auto parse = [](const U32& s) {
        auto v = py_int(text::s8(s));
        if (!v) throw ValueError("invalid literal for int() with base 10: '" + text::s8(s) + "'");
        return *v;
    };
    BigInt n1 = parse(parts[0]), n2 = parse(parts[1]);
    if (n2.is_zero()) throw ZeroDivisionError("denominator is zero: " + std::string(fraction_word));
    const bool negative = n1.negative != n2.negative;
    n1 = abs(n1);
    n2 = abs(n2);
    const std::string lang2 = base_lang(lang);
    std::optional<std::string> result;
    // _NICE_NUMBER_FNS.get(lang2), else nice_number_ar for an Arabic code
    const bool nice_ar = lang2 == "ar" || (!nice_number_lang(lang2) && is_ar(lang));
    const bool nice_range = compare(n2, BigInt(2)) >= 0 && compare(n2, BigInt(20)) <= 0;
    if (lang2 == "en") {
        std::string denominator;
        if (compare(n2, BigInt(2)) == 0) {
            denominator = compare(n1, BigInt(1)) == 0 ? "half" : "halves";
        } else if (compare(n2, BigInt(4)) == 0) {
            denominator = compare(n1, BigInt(1)) == 0 ? "quarter" : "quarters";
        } else {
            denominator = pronounce_ordinal(n2, lang);
            if (compare(n1, BigInt(1)) != 0) denominator += "s";
        }
        result = pronounce_number(n1, lang) + " " + denominator;
    } else if (nice_range && nice_number_lang(lang2) && !nice_ar) {
        not_ported(lang);
    }
    if (!result && nice_ar && nice_range) {
        const U32 spoken = numbers_ar::nice_number_ar(true_divide(n1, n2), *n2.to_ll());
        const auto tokens = text::split_ws(spoken);
        bool all_numeric = true;
        for (const auto& t : tokens) {
            all_numeric = all_numeric && text::all_decimal(text::replace(text::replace(t, U",", U""), U".", U""));
        }
        if (spoken.find(U'/') == U32::npos && !all_numeric) {
            std::string joined;
            for (size_t k = 0; k < tokens.size(); ++k) {
                if (k) joined += " ";
                joined += text::all_decimal(tokens[k]) ? pronounce_number(*py_int(text::s8(tokens[k])), lang)
                                                       : text::s8(tokens[k]);
            }
            result = joined;
        }
    }
    if (!result) {
        // in Python's order: the module that speaks n1 is reached first
        const std::string numerator = pronounce_number(n1, lang);
        result = numerator + " " + pronounce_ordinal(n2, lang);
    }
    if (negative) {
        // _minus_word(lang)
        const std::string minus_one = pronounce_number(BigInt(-1), lang);
        const std::string minus =
            minus_one.find(' ') != std::string::npos ? text::s8(text::split_ws(text::u(minus_one))[0]) : "";
        result = minus.empty() ? "-" + *result : minus + " " + *result;
    }
    return *result;
}

namespace detail {

std::optional<Number> extract_number(U32View text, std::string_view lang, bool ordinals) {
    // _unglue_compound_numerals: [-_] between two letters becomes a space
    U32 unglued(text);
    if (base_lang(lang) != "hu") {
        auto letter = [](char32_t c) { return text::is_word(c) && !text::is_decimal(c) && c != U'_'; };
        for (size_t k = 1; k + 1 < text.size(); ++k) {
            if ((text[k] == U'-' || text[k] == U'_') && letter(text[k - 1]) && letter(text[k + 1])) unglued[k] = U' ';
        }
    }
    if (starts_any(lang, {"en", "az"}) || !is_ar(lang)) {
        if (extract_number_lang(lang)) not_ported(lang);
        unsupported(lang);
    }
    return numbers_ar::extract_number_ar(unglued, ordinals, numbers_ar::resolve_ar_lect(lang));
}

U32 numbers_to_digits_generic(U32View utterance, std::string_view lang,
                              const std::function<bool(const U32&)>& continues,
                              const std::function<std::optional<size_t>(const std::vector<U32>&)>& first_number_words) {
    const U32 connector = U"و";  // _NUMBER_CONNECTORS["ar"]
    const std::vector<U32> tokens = text::split_ws(utterance);
    const U32 punct = U".,!?;:؟،؛";
    auto clean = [&](const U32& t) { return text::lower(text::strip(t, punct)); };
    auto extract = [&](const U32& t) { return extract_number(t, lang); };
    auto is_connector = [&](const U32& t) { return clean(t) == connector; };

    std::map<U32, std::optional<Number>> value_cache;
    auto is_num = [&](const U32& t) -> bool {
        const U32 c = clean(t);
        if (c.empty()) return false;
        try {
            value_cache[t] = extract(c);
            if (!value_cache[t]) return false;
        } catch (const NotImplementedError&) {
            return false;
        }
        if (text::contains(text::strip(c, U"-"), U"-")) {
            for (const U32& part : text::split(c, U"-")) {
                if (part.empty() || part == connector) continue;
                try {
                    if (!extract(part)) return false;
                } catch (const NotImplementedError&) {
                    return false;
                }
            }
        }
        return true;
    };
    auto denominator = [&](const U32& t) -> std::optional<BigInt> {
        const U32 c = clean(t);
        if (!is_num(t)) return std::nullopt;
        for (char32_t ch : c) {
            if (text::is_digit(ch)) return std::nullopt;
        }
        const auto& val = value_cache[t];
        if (!val) return std::nullopt;
        if (!(compare(*val, Number(BigInt(0))) > 0 && compare(*val, Number(BigInt(1))) < 0)) return std::nullopt;
        const double inverse = 1.0 / to_double(*val);
        const BigInt n = round_half_even(inverse);
        if (compare(n, BigInt(2)) >= 0 && std::fabs(inverse - n.to_double()) < 1e-9) return n;
        return std::nullopt;
    };
    auto span_text = [&](size_t from, size_t to) {
        std::vector<U32> cleaned;
        for (size_t k = from; k <= to; ++k) cleaned.push_back(clean(tokens[k]));
        return text::join(cleaned, U" ");
    };
    auto is_denominator_of = [&](size_t i, size_t j, size_t next_j) {
        const auto n = denominator(tokens[next_j]);
        if (!n) return false;
        const auto current_val = extract(span_text(i, j));
        if (!current_val || !eq(*current_val, Number(to_int(*current_val))) ||
            !(compare(*current_val, Number(BigInt(0))) > 0 && compare(*current_val, Number(*n)) < 0)) {
            return false;
        }
        const auto extended_val = extract(span_text(i, next_j));
        if (!extended_val) return false;
        const double part = true_divide(to_int(*current_val), *n);
        return compare(abs(add(*extended_val, Number(-part))), Number(1e-9)) < 0;
    };

    std::vector<U32> out;
    size_t i = 0;
    while (i < tokens.size()) {
        const auto den = is_num(tokens[i]) ? denominator(tokens[i]) : std::nullopt;
        if (!is_num(tokens[i]) || (den && compare(*den, BigInt(3)) >= 0)) {
            out.push_back(tokens[i]);
            ++i;
            continue;
        }
        size_t j = i;
        auto continues_at = [&](size_t next_j, bool via_connector) {
            const auto extended_val = extract(span_text(i, next_j));
            if (!extended_val) return false;
            const auto current_val = extract(span_text(i, j));
            const auto next_val = extract(clean(tokens[next_j]));
            if (!current_val) return false;
            if (eq(*extended_val, *current_val) || compare(abs(*extended_val), abs(*current_val)) <= 0) return false;
            // a next_val of False compares as 0, which extended_val is not
            if (!next_val || !eq(*extended_val, *next_val)) return true;
            return !via_connector && eq(abs(*current_val), Number(BigInt(1))) &&
                   compare(abs(*next_val), Number(BigInt(100))) >= 0;
        };
        auto may_extend = [&](const U32& t) { return is_num(t) || (continues && continues(clean(t))); };
        size_t limit = tokens.size() - 1;
        if (first_number_words) {
            size_t end = i;
            while (end + 1 < tokens.size() &&
                   (may_extend(tokens[end + 1]) ||
                    (is_connector(tokens[end + 1]) && end + 2 < tokens.size() && may_extend(tokens[end + 2])))) {
                ++end;
            }
            std::vector<U32> words;
            for (size_t k = i; k <= end; ++k) words.push_back(clean(tokens[k]));
            if (const auto covered = first_number_words(words); covered && *covered) limit = i + *covered - 1;
        }
        while (j + 1 < tokens.size()) {
            if (j + 1 > limit) break;
            if (denominator(tokens[j + 1])) {
                if (is_denominator_of(i, j, j + 1)) ++j;
                break;
            }
            if (may_extend(tokens[j + 1]) && continues_at(j + 1, false)) {
                ++j;
            } else if (is_connector(tokens[j + 1]) && j + 2 <= limit && may_extend(tokens[j + 2]) &&
                       continues_at(j + 2, true)) {
                j += 2;
            } else {
                break;
            }
        }
        const U32 trail = tokens[j].substr(text::rstrip(tokens[j], punct).size());
        const U32 first = clean(tokens[i]);
        bool digit_run = !first.empty();
        for (char32_t c : first) {
            digit_run = digit_run && ((c >= U'0' && c <= U'9') || (c >= 0x0660 && c <= 0x0669) || (c >= 0x06F0 && c <= 0x06F9));
        }
        if (i == j && digit_run) {
            U32 western;
            for (char32_t c : first) {
                western.push_back(c >= 0x06F0 ? U'0' + (c - 0x06F0) : c >= 0x0660 ? U'0' + (c - 0x0660) : c);
            }
            out.push_back(western + trail);
        } else {
            const auto val = extract(span_text(i, j));
            std::string shown = "False";
            if (val) shown = str(!is_int(*val) && is_integer(*val) ? Number(to_int(*val)) : *val);
            out.push_back(text::u(shown) + trail);
        }
        i = j + 1;
    }
    return text::join(out, U" ");
}

}  // namespace detail

std::optional<Number> extract_number(std::string_view text, std::string_view lang, bool ordinals) {
    return detail::extract_number(text::u(text), lang, ordinals);
}

std::string numbers_to_digits(std::string_view utterance, std::string_view lang) {
    if (starts_any(lang, {"en", "ast", "oc", "an", "fy", "gl", "de", "pt", "mwl", "ro", "bg", "hr", "ru", "sk", "id",
                          "ms", "tr", "uk"})) {
        not_ported(lang);
    }
    if (is_ar(lang)) return text::s8(numbers_ar::numbers_to_digits_ar(text::u(utterance), lang));
    if (starts(lang, "kab")) not_ported(lang);
    // _numbers_to_digits_generic reads a number only where extract_number has a
    // language module, and asks that module about each token that is not empty
    // after its punctuation is stripped.
    const std::vector<U32> tokens = text::split_ws(text::u(utterance));
    if (extract_number_lang(lang)) {
        for (const U32& t : tokens) {
            if (!text::strip(t, U".,!?;:؟،؛").empty()) not_ported(lang);
        }
    }
    return text::s8(text::join(tokens, U" "));
}

bool is_numeric(std::string_view input_str) {
    if (const auto v = py_float(input_str)) {
        if (std::isfinite(*v)) return true;
        return text::all_digit(text::lstrip(text::strip_ws(text::u(input_str)), U"+-"));
    }
    return false;
}

}  // namespace arbtok::numbers::ovos_number_parser
