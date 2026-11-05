#include "numbers_ar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "arbtok/numbers/errors.hpp"
#include "tables.hpp"

namespace arbtok::numbers::numbers_ar {

namespace {

using tables::find;
using tables::has;

const tables::NumbersAr& T() { return tables::numbers_ar(); }

const U32 kAnd = U"و";
const U32 kArticle = U"ال";
const U32 kSeparator = U" و";

bool starts(U32View s, U32View prefix) { return text::starts_with(s, prefix); }
U32 sub(U32View s, size_t from) { return from >= s.size() ? U32() : U32(s.substr(from)); }
// Python's s[:n] for n >= 0 and for n < 0.
U32 head(U32View s, long n) {
    if (n < 0) n = std::max(0L, static_cast<long>(s.size()) + n);
    return U32(s.substr(0, std::min(static_cast<size_t>(n), s.size())));
}
// Python's `part in whole` for two strings.
bool in_text(U32View whole, U32View part) { return whole.find(part) != U32View::npos; }

// Python's tokens[k], with a negative k counted from the end.
const U32& at(const Tokens& tokens, long k) {
    if (k < 0) k += static_cast<long>(tokens.size());
    if (k < 0 || k >= static_cast<long>(tokens.size())) throw IndexError("list index out of range");
    return tokens[static_cast<size_t>(k)];
}

std::string resolve_case(const std::optional<std::string>& c) {
    if (!c || *c == "nominative") return "nominative";
    if (*c == "oblique" || *c == "genitive" || *c == "accusative") return "oblique";
    throw ValueError("Arabic case must be one of None, 'nominative', 'oblique', 'genitive', 'accusative'; got '" +
                     *c + "'");
}

bool is_feminine(Gender gender) {
    if (!gender || *gender == GrammaticalGender::MASCULINE) return false;
    if (*gender == GrammaticalGender::FEMININE) return true;
    throw ValueError("Arabic gender must be masculine or feminine; got <GrammaticalGender.NEUTRAL: 'neutral'>");
}

U32 oblique(U32View word) { return head(word, -2) + U"ين"; }

// ---------------------------------------------------------------------------
// pronunciation
// ---------------------------------------------------------------------------

U32 cardinal_two_digit(long long number, const std::string& c, bool feminine) {
    const auto& ones = feminine ? T().ones_fem : T().ones;
    if (number == 0) return T().ones[0];
    if (number <= 10) {
        if (number == 2 && c == "oblique") return oblique(ones[2]);
        return ones[static_cast<size_t>(number)];
    }
    if (feminine && number < 20) {
        if (number == 12 && c == "oblique") return T().twelve_fem_oblique;
        return T().teens_fem.at(number);
    }
    if (number == 11) return U"أحد عشر";
    if (number == 12) return c == "oblique" ? U"اثني عشر" : U"اثنا عشر";
    if (number < 20) return T().ones[static_cast<size_t>(number - 10)] + U" عشر";
    const long long tens = number / 10, unit = number % 10;
    U32 tens_word = T().tens.at(tens * 10);
    if (c == "oblique") tens_word = oblique(tens_word);
    if (unit == 0) return tens_word;
    const U32 unit_word = unit == 2 && c == "oblique" ? oblique(ones[2]) : ones[static_cast<size_t>(unit)];
    return unit_word + kSeparator + tens_word;
}

U32 cardinal_three_digit(long long number, const std::string& c, bool feminine) {
    if (number < 100) return cardinal_two_digit(number, c, feminine);
    const long long hundreds = number / 100, rest = number % 100;
    U32 result = T().hundreds.at(hundreds * 100);
    if (hundreds == 2 && c == "oblique") result = oblique(result);
    if (rest) result += kSeparator + cardinal_two_digit(rest, c, feminine);
    return result;
}

// The quotient and remainder of a non-negative int by 10**power.
std::pair<BigInt, long long> divmod_pow10(const BigInt& n, size_t power) {
    const std::string& d = n.digits;
    if (d.size() <= power) return {BigInt(), std::stoll(d)};
    return {BigInt::from_string(d.substr(0, d.size() - power)), std::stoll(d.substr(d.size() - power))};
}

U32 cardinal(const BigInt& number, const std::string& c, bool feminine = false, bool construct = true) {
    if (compare(number, BigInt(1000)) < 0) return cardinal_three_digit(*number.to_ll(), c, feminine);
    std::vector<U32> parts;
    BigInt remainder = number;
    for (auto it = T().scales.rbegin(); it != T().scales.rend(); ++it) {
        const size_t power = std::to_string(it->value).size() - 1;
        auto [count, rest] = divmod_pow10(remainder, power);
        remainder = BigInt(rest);
        if (count.is_zero()) continue;
        const auto small = count.to_ll();
        if (small && *small == 1) {
            parts.push_back(it->singular);
        } else if (small && *small == 2) {
            parts.push_back(c == "oblique" ? oblique(it->dual) : it->dual);
        } else if (small && *small >= 3 && *small <= 10) {
            parts.push_back(T().ones[static_cast<size_t>(*small)] + U" " + it->plural);
        } else {
            U32 words = cardinal(count, c, false, construct);
            if (construct && divmod_pow10(count, 3).second == 200) words = head(words, -1);
            parts.push_back(words + U" " + it->singular);
        }
    }
    if (!remainder.is_zero()) parts.push_back(cardinal_three_digit(*remainder.to_ll(), c, feminine));
    return text::join(parts, kSeparator);
}

bool is_space_at(U32View s, size_t i) { return text::is_space(s[i]); }

// re.sub(r"(?<!\S)(و?)(alternatives)(?!\S)", lambda m: m.group(1) + said[m.group(2)], words)
U32 substitute_words(U32View words, const std::vector<std::pair<U32, U32>>& alternatives) {
    U32 out;
    size_t p = 0;
    while (p <= words.size()) {
        bool matched = false;
        if (p == 0 || is_space_at(words, p - 1)) {
            for (int waw = 1; waw >= 0 && !matched; --waw) {
                if (waw && (p >= words.size() || words[p] != U'و')) continue;
                const size_t start = p + static_cast<size_t>(waw);
                for (const auto& [literary, form] : alternatives) {
                    if (words.compare(start, literary.size(), literary) != 0) continue;
                    const size_t end = start + literary.size();
                    if (end < words.size() && !is_space_at(words, end)) continue;
                    if (waw) out.push_back(U'و');
                    out += form;
                    p = end;
                    matched = true;
                    break;
                }
            }
        }
        if (matched) continue;
        if (p == words.size()) break;
        out.push_back(words[p++]);
    }
    return out;
}

U32 lect_cardinal(const U32& words, const std::optional<BigInt>& number, const std::string& c, const U32& lect,
                  bool feminine) {
    const tables::LectForms* table = nullptr;
    for (const auto& lf : T().lect_forms) {
        if (lf.lect == lect) table = &lf;
    }
    if (!table || table->forms.empty()) return words;
    std::vector<std::pair<long long, U32>> forms;
    for (const auto& f : table->forms) {
        if (!feminine || f.first >= 20) forms.push_back(f);
    }
    if (forms.empty()) return words;
    if (number) {
        for (const auto& [value, form] : forms) {
            if (BigInt(value) == *number) return form;
        }
    }
    std::vector<std::pair<U32, U32>> said;  // a dict: a repeated key keeps its first place
    for (const auto& [value, form] : forms) {
        const U32 literary = cardinal(BigInt(value), c);
        if (literary.empty() || literary == form) continue;
        auto it = std::find_if(said.begin(), said.end(), [&](const auto& e) { return e.first == literary; });
        if (it != said.end()) {
            it->second = form;
        } else {
            said.emplace_back(literary, form);
        }
    }
    if (said.empty()) return words;
    std::stable_sort(said.begin(), said.end(),
                     [](const auto& x, const auto& y) { return x.first.size() > y.first.size(); });
    return substitute_words(words, said);
}

// "%.<places>f" % value.
std::string fixed(double value, int places) {
    const int n = std::snprintf(nullptr, 0, "%.*f", places, value);
    std::string out(static_cast<size_t>(n) + 1, '\0');
    std::snprintf(out.data(), out.size(), "%.*f", places, value);
    out.resize(static_cast<size_t>(n));
    return out;
}

}  // namespace

std::optional<std::string> resolve_ar_lang(std::string_view lang) {
    std::string code;
    for (char ch : lang) {
        if (ch == '-') break;
        code.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
    }
    const auto it = T().dialect_default_case.find(code);
    if (it == T().dialect_default_case.end()) return std::nullopt;
    return it->second;
}

std::optional<std::string> resolve_ar_lect(std::string_view lang) {
    std::string code;
    for (char ch : lang) {
        if (ch == '-' || ch == '_') break;
        code.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
    }
    for (const auto& lf : T().lect_forms) {
        if (text::s8(lf.lect) == code) return code;
    }
    return std::nullopt;
}

U32 pronounce_ordinal_ar(const Number& number, Gender gender, const std::optional<std::string>& grammatical_case) {
    const std::string c = resolve_case(grammatical_case);
    const bool feminine = is_feminine(gender);
    const BigInt n = to_int(number);
    if (compare(n, BigInt(0)) <= 0) throw ValueError("Arabic ordinals start at 1");
    const auto& stems = feminine ? T().ordinal_stems_fem : T().ordinal_stems;
    const auto& units = feminine ? T().ordinal_compound_unit_fem : T().ordinal_compound_unit;
    const auto small = n.to_ll();
    if (small && *small <= 10) return kArticle + stems.at(*small);
    if (small && *small < 20) return kArticle + units.at(*small - 10) + (feminine ? U" عشرة" : U" عشر");
    if (small && *small < 100) {
        const long long tens = *small / 10, unit = *small % 10;
        U32 tens_word = T().tens.at(tens * 10);
        if (c == "oblique") tens_word = oblique(tens_word);
        tens_word = kArticle + tens_word;
        if (unit == 0) return tens_word;
        return kArticle + units.at(unit) + U" و" + tens_word;
    }
    if ((feminine || c == "oblique") && !(small && (*small == 100 || *small == 1000))) {
        throw NotImplementedError("the feminine and oblique Arabic ordinals above 99 are implemented for 100 and 1000 only");
    }
    return kArticle + cardinal(n, "nominative", false, false);
}

U32 pronounce_number_ar(const Number& number, int places, const std::optional<std::string>& grammatical_case,
                        const std::optional<std::string>& lect, Gender gender) {
    const std::string c = resolve_case(grammatical_case);
    const bool feminine = is_feminine(gender);
    if (const auto* d = std::get_if<double>(&number)) {
        if (std::isinf(*d)) return *d > 0 ? T().infinity : T().minus + U" " + T().infinity;
    }
    if (compare(number, Number(BigInt(0))) < 0) {
        return T().minus + U" " + pronounce_number_ar(abs(number), places, c, lect, gender);
    }
    const BigInt whole = to_int(number);
    U32 result = cardinal(whole, c, feminine, !lect);
    if (lect) {
        const bool exact = eq(number, Number(whole));
        result = lect_cardinal(result, exact ? std::optional<BigInt>(whole) : std::nullopt, c, text::u(*lect), feminine);
    }
    if (const auto* d = std::get_if<double>(&number)) {
        if (!eq(number, Number(whole)) && places > 0) {
            const std::string formatted = fixed(*d - whole.to_double(), places);
            std::string digits = formatted.substr(formatted.find('.') + 1);
            while (!digits.empty() && digits.back() == '0') digits.pop_back();
            if (!digits.empty()) {
                result += U" " + T().decimal + U" ";
                for (size_t k = 0; k < digits.size(); ++k) {
                    if (k) result += U" ";
                    result += T().ones[static_cast<size_t>(digits[k] - '0')];
                }
            }
        }
    }
    return result;
}

U32 nice_number_ar(double number, long long denominator) {
    // convert_to_mixed_fraction(number, [denominator])
    const BigInt int_number = to_int(Number(number));
    BigInt whole;
    long long num = 0, den = 1;
    if (eq(Number(int_number), Number(number))) {
        whole = int_number;
    } else {
        const double frac_number = std::fabs(number - int_number.to_double());
        const double numerator = std::fabs(frac_number) * static_cast<double>(denominator);
        if (std::fabs(numerator - std::nearbyint(numerator)) >= 0.01) {
            // str(round(number, 3))
            return text::u(float_repr(std::strtod(fixed(number, 3).c_str(), nullptr)));
        }
        whole = int_number;
        num = *round_half_even(numerator).to_ll();
        den = denominator;
    }
    if (num == 0) return text::u(whole.str());
    U32 frac;
    const auto it = T().fractions.find(den);
    if (it != T().fractions.end()) {
        const auto& [singular, dual, plural] = it->second;
        if (num == 1) {
            frac = singular;
        } else if (num == 2) {
            frac = dual;
        } else {
            frac = text::u(std::to_string(num)) + U" " + plural;
        }
    } else if (num == 1) {
        frac = U"جزء من " + cardinal(BigInt(den), "nominative");
    } else {
        frac = text::u(std::to_string(num)) + U" أجزاء من " + cardinal(BigInt(den), "nominative");
    }
    if (whole.is_zero()) return frac;
    return text::u(whole.str()) + U" و" + frac;
}

// ---------------------------------------------------------------------------
// extraction
// ---------------------------------------------------------------------------

U32 normalize_ar(U32View text) {
    U32 t;
    t.reserve(text.size());
    for (char32_t c : text) {
        if (c >= 0x0660 && c <= 0x0669) {
            t.push_back(U'0' + (c - 0x0660));
        } else if (c >= 0x06F0 && c <= 0x06F9) {
            t.push_back(U'0' + (c - 0x06F0));
        } else if (c == 0x066B) {
            t.push_back(U'.');
        } else if (c == 0x066C || c == 0x0640 || (c >= 0x064B && c <= 0x0652) || c == 0x0670) {
            continue;
        } else if (c == 0x0623 || c == 0x0625 || c == 0x0622) {
            t.push_back(0x0627);
        } else if (c == 0x0649) {
            t.push_back(0x064A);
        } else {
            t.push_back(c);
        }
    }
    auto after_mi = [](U32View s, size_t i) { return i >= 2 && s[i - 2] == U'م' && s[i - 1] == U'ي'; };
    U32 a = t;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == U'ة' && !after_mi(t, i)) a[i] = U'ه';
    }
    U32 b = a;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == U'ه' && after_mi(a, i) && !(i + 1 < a.size() && text::is_word(a[i + 1]))) b[i] = U'ة';
    }
    return b;
}

namespace {

bool in_vocab(const U32& token) {
    const auto& t = T();
    return t.units_lookup.count(token) || t.tens_lookup.count(token) || t.hundreds_lookup.count(token) ||
           t.scales_lookup.count(token) || t.scale_duals_lookup.count(token) || t.fractions_lookup.count(token) ||
           t.teen_first_lookup.count(token) || t.ordinal_units_lookup.count(token) || t.fused_teens_lookup.count(token);
}

std::optional<std::pair<U32, U32>> split_proclitic(const U32& token) {
    const auto& t = T();
    if (token.size() < 3 || t.proclitics.find(token[0]) == U32::npos || has(t.proclitic_not_numbers, token) ||
        has(t.number_words, token)) {
        return std::nullopt;
    }
    const U32 rest = sub(token, 1);
    if (has(t.proclitic_not_hosts, rest)) return std::nullopt;
    if (has(t.proclitic_hosts, rest) || (starts(rest, kArticle) && has(t.proclitic_hosts, sub(rest, 2)))) {
        return std::make_pair(U32(1, token[0]), rest);
    }
    if (token[0] == U'ل' && starts(rest, U"ل") && has(t.proclitic_hosts, sub(rest, 1))) {
        return std::make_pair(U32(1, token[0]), U"ا" + rest);
    }
    return std::nullopt;
}

U32 bare(const U32& token) {
    if (starts(token, kArticle) && has(T().number_words, sub(token, 2))) return sub(token, 2);
    return token;
}

bool is_number(const U32& s) {
    const auto v = py_float(text::s8(s));
    return v && std::isfinite(*v);
}

bool proclitic_or_waw(const U32& t) { return in_text(T().proclitics + kAnd, t); }

bool is_whole_text(const Tokens& tokens, size_t j) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i != j && !proclitic_or_waw(tokens[i])) return false;
    }
    return true;
}

enum Slot { NONE = 0, UNIT = 1, TEN = 2, HUNDRED = 4, SCALE = 8, FRAC = 16 };

bool out_of_context(const Tokens& tokens, size_t j);
Slot group_slot(const Tokens& tokens, size_t j);

bool licenses(const Tokens& tokens, size_t k) { return !has(T().homographs, tokens[k]) || !out_of_context(tokens, k); }

bool counted_word_follows(const Tokens& tokens, size_t j) {
    const auto& t = T();
    if (j + 1 >= tokens.size()) return false;
    const U32& nxt = tokens[j + 1];
    const U32 b = bare(nxt);
    if (t.scales_lookup.count(b) || t.scale_duals_lookup.count(b)) return licenses(tokens, j + 1);
    if (has(t.hundred_mult_lookup, b)) return false;
    return has(t.unit_words, nxt) || has(t.per_cent_words, nxt) ||
           (nxt == U"في" && j + 2 < tokens.size() && has(t.per_cent_after_fi, tokens[j + 2]));
}

bool joined_to_a_number(const Tokens& tokens, size_t j) {
    const auto& t = T();
    if (j + 2 < tokens.size() && tokens[j + 1] == t.and_one && has(t.teen_second_lookup, tokens[j + 2])) return true;
    return j + 2 < tokens.size() && tokens[j + 1] == kAnd &&
           (is_number(tokens[j + 2]) || (group_slot(tokens, j + 2) != NONE && licenses(tokens, j + 2)));
}

bool hundred_after_a_day_or_lady(const Tokens& tokens, size_t j) {
    const auto& t = T();
    return has(t.hundred_mult_lookup, bare(tokens[j])) && j > 0 && has(t.hundred_unit_homographs, tokens[j - 1]) &&
           !(j > 1 && has(t.day_words, tokens[j - 2]));
}

bool out_of_context(const Tokens& tokens, size_t j) {
    const auto& t = T();
    const U32& tok = tokens[j];
    if (hundred_after_a_day_or_lady(tokens, j)) return true;
    if (!has(t.homographs, tok)) return false;
    if (has(t.day_names, tok) && j > 0 && has(t.day_words, tokens[j - 1])) {
        if (j + 1 >= tokens.size()) return true;
        const U32 nxt = bare(tokens[j + 1]);
        return !(t.scales_lookup.count(nxt) || t.scale_duals_lookup.count(nxt));
    }
    if (counted_word_follows(tokens, j)) return false;
    if (tok == t.construct_hundred) return true;
    if (joined_to_a_number(tokens, j)) return false;
    return !(is_whole_text(tokens, j) && tok != kArticle + t.water_hundred && tok != kArticle + t.dead_two_hundred &&
             tok != kArticle + t.price_or_eighth);
}

Slot group_slot(const Tokens& tokens, size_t j) {
    const auto& t = T();
    const U32 tok = bare(tokens[j]);
    const std::optional<U32> nxt = j + 1 < tokens.size() ? std::optional<U32>(bare(tokens[j + 1])) : std::nullopt;
    if (t.construct_hundred_lookup.count(tok)) return j > 0 && tokens[j - 1] == kAnd ? HUNDRED : NONE;
    if (t.teen_first_lookup.count(tok) && nxt && has(t.teen_second_lookup, *nxt)) return UNIT;
    if (t.fused_teens_lookup.count(tok)) return UNIT;
    if (const long long* v = find(t.units_lookup, tok)) {
        if (nxt && has(t.hundred_mult_lookup, *nxt) && 1 <= *v && *v <= 9) return HUNDRED;
        return UNIT;
    }
    if (t.tens_lookup.count(tok)) return TEN;
    if (t.hundreds_lookup.count(tok)) return HUNDRED;
    if (t.scales_lookup.count(tok) || t.scale_duals_lookup.count(tok)) return SCALE;
    if (t.fractions_lookup.count(tok)) return FRAC;
    return NONE;
}

bool is_clock_hour(const Tokens& tokens, size_t k) {
    if (k > 0 && has(T().teen_second_lookup, tokens[k]) && group_slot(tokens, k - 1) == UNIT) --k;
    while (k > 1 && tokens[k - 1] == kAnd && group_slot(tokens, k - 2) != NONE) k -= 2;
    return k > 0 && tokens[k - 1] == T().clock_word;
}

bool is_spoken_half(const Tokens& tokens, size_t j) {
    const auto& t = T();
    if (!t.spoken_half_lookup.count(tokens[j]) || j < 2 || tokens[j - 1] != kAnd) return false;
    const U32 before = bare(tokens[j - 2]);
    if (!(t.units_lookup.count(before) || t.tens_lookup.count(before) || t.fused_teens_lookup.count(before) ||
          has(t.teen_second_lookup, before))) {
        return false;
    }
    if (j + 1 < tokens.size()) return has(t.unit_words, tokens[j + 1]);
    return !is_clock_hour(tokens, j - 2);
}

bool is_round(const U32& token) {
    const auto& t = T();
    const U32 tok = bare(token);
    return t.hundreds_lookup.count(tok) || t.scales_lookup.count(tok) || t.scale_duals_lookup.count(tok) ||
           t.construct_hundred_lookup.count(tok);
}

bool is_unread_fraction(const Tokens& tokens, size_t j) {
    return j > 1 && T().fractions_lookup.count(bare(tokens[j])) && tokens[j - 1] == kAnd && is_round(tokens[j - 2]);
}

bool is_construct_dual(const Tokens& tokens, size_t j) {
    return T().accusative_or_construct_dual.count(tokens[j]) && j + 1 < tokens.size() &&
           has(T().unit_words, tokens[j + 1]);
}

bool closes_a_hundred(const U32& token) {
    const U32 tok = bare(token);
    const long long* v = find(T().units_lookup, tok);
    return v && (*v <= 2 || text::ends_with(tok, U"ه"));
}

bool is_plural_head(const U32& stem) {
    return text::ends_with(stem, U"ات") || text::ends_with(stem, U"ين") || text::ends_with(stem, U"ون") ||
           has(T().broken_plural_heads, stem);
}

std::optional<size_t> ordinal_end(const Tokens& tokens, size_t i) {
    const auto& t = T();
    const U32& tok = tokens[i];
    const bool definite = starts(tok, kArticle);
    const U32 stem = definite ? sub(tok, 2) : tok;
    if (definite && t.tens_lookup.count(stem)) {
        U32 before = i ? tokens[i - 1] : U32();
        if (proclitic_or_waw(head(before, 1)) && before.substr(std::min<size_t>(1, before.size()), 2) == kArticle) {
            before = sub(before, 1);
        } else if (starts(before, U"لل")) {
            before = U"ا" + sub(before, 1);
        }
        if (starts(before, kArticle) && !has(t.number_words, sub(before, 2)) && !has(t.homographs, before) &&
            !is_plural_head(sub(before, 2))) {
            return i + 1;
        }
        return std::nullopt;
    }
    if (!t.ordinal_units_lookup.count(stem) || has(t.first_ordinals, stem)) return std::nullopt;
    if (i + 1 < tokens.size() && has(t.teen_second_lookup, tokens[i + 1])) return i + 2;
    if (i + 2 < tokens.size() && tokens[i + 1] == kAnd) {
        const U32& ten = tokens[i + 2];
        if (starts(ten, kArticle) == definite && t.tens_lookup.count(definite ? sub(ten, 2) : ten)) return i + 3;
    }
    return std::nullopt;
}

bool is_time_second(const Tokens& tokens, size_t i, std::optional<size_t> last_end, bool halves) {
    if (tokens[i] != T().second_or_a_second) return false;
    return (last_end && *last_end == i) ||
           (i + 1 < tokens.size() && parse_number_span(tokens, i + 1, halves).first.has_value());
}

std::pair<std::optional<double>, size_t> parse_decimal_part(const Tokens& tokens, size_t i) {
    const auto& t = T();
    const size_t n = tokens.size();
    std::vector<long long> digits;
    bool none = false;
    size_t j = i;
    while (j < n) {
        const long long* v = find(t.units_lookup, tokens[j]);
        if (!v || *v > 9 || (j + 1 < n && has(t.teen_second_lookup, tokens[j + 1])) ||
            (j + 1 < n && has(t.hundred_mult_lookup, tokens[j + 1]))) {
            break;
        }
        digits.push_back(*v);
        ++j;
        if (j < n && tokens[j] == kAnd) {
            none = true;
            break;
        }
    }
    if (!none && !digits.empty()) {
        FloatSum total;
        for (size_t k = 0; k < digits.size(); ++k) {
            // d / 10 ** (k + 1), correctly rounded
            total.add(std::strtod((std::to_string(digits[k]) + "e-" + std::to_string(k + 1)).c_str(), nullptr));
        }
        return {total.result(), j};
    }
    const auto [value, end] = parse_number_span(tokens, i);
    if (!value || !eq(*value, Number(to_int(*value)))) return {std::nullopt, i};
    // value / 10 ** len(str(int(value))); the '-' of a negative value counts as a digit
    const BigInt whole = to_int(*value);
    const std::string quotient = (whole.negative ? "-0.0" : "0.") + whole.digits;
    return {std::strtod(quotient.c_str(), nullptr), end};
}

struct Span {
    size_t start, end;
    Number value;
};

std::vector<Span> number_spans(const Tokens& tokens, bool ordinals, bool halves) {
    std::vector<Span> out;
    const auto& t = T();
    size_t i = 0;
    const size_t n = tokens.size();
    std::optional<size_t> last_end;
    while (i < n) {
        const size_t start = i;
        bool negative = false;
        if (has(t.minus_lookup, tokens[i]) && i + 1 < n) {
            negative = true;
            ++i;
        }
        if (is_time_second(tokens, i, last_end, halves)) {
            ++i;
            continue;
        }
        if (ordinals) {
            const auto [value, j] = parse_ordinal_span(tokens, i);
            if (value) {
                out.push_back({start, j, negative ? Number(BigInt(-*value)) : Number(BigInt(*value))});
                i = j;
                last_end = j;
                continue;
            }
        } else {
            if (const auto j = ordinal_end(tokens, i)) {
                i = *j;
                continue;
            }
        }
        auto [value, j] = parse_number_span(tokens, i, halves);
        if (!value) {
            ++i;
            continue;
        }
        if (j < n && has(t.decimal_lookup, tokens[j])) {
            const auto [frac, j2] = parse_decimal_part(tokens, j + 1);
            if (frac) {
                value = add(*value, Number(*frac));
                j = j2;
            }
        }
        out.push_back({start, j, negative ? neg(*value) : *value});
        i = j;
        last_end = j;
    }
    return out;
}

}  // namespace

Tokens tokenize_ar(U32View text) {
    const auto& t = T();
    Tokens tokens;
    for (const U32& written : text::split_ws(text)) {
        U32 token = text::strip(normalize_ar(written), t.punct);
        if (token.empty()) continue;
        const bool whole_word = in_vocab(token) || has(t.teen_second_lookup, token);
        if (!whole_word && token.size() > 1 && token[0] == U'و') {
            const U32 rest = sub(token, 1);
            if (in_vocab(rest) || has(t.teen_second_lookup, rest) || t.construct_hundred_lookup.count(rest) ||
                t.spoken_half_lookup.count(rest) || (starts(rest, kArticle) && in_vocab(sub(token, 3)))) {
                tokens.push_back(kAnd);
                token = rest;
            } else if (const auto split = split_proclitic(token)) {
                tokens.push_back(split->first);
                token = split->second;
            }
        } else if (!whole_word) {
            if (const auto split = split_proclitic(token)) {
                tokens.push_back(split->first);
                token = split->second;
            }
        }
        if (t.accusative_or_construct_dual.count(token) && written.find(0x064B) != U32::npos) token = head(token, -1);
        tokens.push_back(token);
    }
    return tokens;
}

std::pair<std::optional<long long>, size_t> parse_ordinal_span(const Tokens& tokens, size_t i) {
    const auto& t = T();
    auto strip_article = [](const U32& tok) { return starts(tok, kArticle) ? sub(tok, 2) : tok; };
    const U32 tok = strip_article(tokens.at(i));
    if (const long long* unit = find(t.ordinal_units_lookup, tok)) {
        const size_t j = i + 1;
        if (j < tokens.size() && has(t.teen_second_lookup, strip_article(tokens[j]))) return {10 + *unit, j + 1};
        if (j + 1 < tokens.size() && tokens[j] == kAnd) {
            if (const long long* ten = find(t.tens_lookup, strip_article(tokens[j + 1]))) return {*unit + *ten, j + 2};
        }
        return {*unit, j};
    }
    if (const long long* ten = find(t.tens_lookup, tok)) {
        if (starts(tokens[i], kArticle)) return {*ten, i + 1};
    }
    return {std::nullopt, i};
}

std::pair<std::optional<Number>, size_t> parse_number_span(const Tokens& tokens, size_t i, bool halves) {
    const auto& t = T();
    Number total = BigInt(0), current = BigInt(0);
    bool started = false;
    int filled = 0;
    const size_t n = tokens.size();
    size_t j = i;
    auto prev = [&](size_t k) -> const U32& { return at(tokens, static_cast<long>(k) - 1); };
    while (j < n) {
        const U32& raw = tokens[j];
        if (raw == kAnd && started && j + 1 < n) {
            if (halves && is_spoken_half(tokens, j + 1)) {
                ++j;
                continue;
            }
            Slot slot = group_slot(tokens, j + 1);
            if (slot == HUNDRED && (filled & HUNDRED) && closes_a_hundred(tokens[j + 1])) slot = UNIT;
            if (is_unread_fraction(tokens, j + 1)) break;
            if (slot == SCALE || slot == FRAC || ((slot == UNIT || slot == TEN || slot == HUNDRED) && !(filled & slot))) {
                ++j;
                continue;
            }
            break;
        }
        if (is_number(raw)) {
            if (started) break;
            const double f = *py_float(text::s8(raw));
            Number value = std::trunc(f) == f ? Number(BigInt::from_double(f)) : Number(f);
            size_t j2 = j + 1;
            if (j2 < n) {
                if (const long long* scale = find(t.scales_lookup, bare(tokens[j2]))) {
                    value = mul(value, Number(BigInt(*scale)));
                    ++j2;
                }
            }
            return {value, j2};
        }
        if (!started && (out_of_context(tokens, j) || is_unread_fraction(tokens, j))) break;
        const U32 tok = bare(raw);
        const std::optional<U32> nxt = j + 1 < n ? std::optional<U32>(bare(tokens[j + 1])) : std::nullopt;
        const bool nxt_teen_second = nxt && has(t.teen_second_lookup, *nxt);
        const bool nxt_hundred = nxt && has(t.hundred_mult_lookup, *nxt);
        if (const long long* v = find(t.fused_teens_lookup, tok)) {
            if (filled & (UNIT | TEN)) break;
            current = add(current, Number(BigInt(*v)));
            filled |= UNIT | TEN;
            started = true;
            ++j;
            continue;
        }
        const long long* unit_value = find(t.units_lookup, tok);
        const long long* teen_first = find(t.teen_first_lookup, tok);
        if ((teen_first || (unit_value && 1 <= *unit_value && *unit_value <= 9)) && nxt_teen_second) {
            if (filled & (UNIT | TEN)) break;
            const long long unit = teen_first && *teen_first ? *teen_first : t.units_lookup.at(tok);
            current = add(current, Number(BigInt(10 + unit)));
            filled |= UNIT | TEN;
            started = true;
            j += 2;
            continue;
        }
        if (unit_value) {
            if (nxt_hundred && 1 <= *unit_value && *unit_value <= 9 &&
                !((filled & HUNDRED) && prev(j) == kAnd && closes_a_hundred(raw))) {
                if ((filled & HUNDRED) || ((filled & (UNIT | TEN)) && prev(j) != kAnd)) break;
                current = add(current, Number(BigInt(*unit_value * 100)));
                filled |= HUNDRED;
                started = true;
                j += 2;
                continue;
            }
            if (filled & UNIT) break;
            current = add(current, Number(BigInt(*unit_value)));
            filled |= UNIT;
            started = true;
            ++j;
            continue;
        }
        if (const long long* v = find(t.tens_lookup, tok)) {
            if (filled & TEN) break;
            current = add(current, Number(BigInt(*v)));
            filled |= TEN;
            started = true;
            ++j;
            continue;
        }
        const long long* hundred = find(t.hundreds_lookup, tok);
        const long long* construct = find(t.construct_hundred_lookup, tok);
        if (hundred || (construct && (!started || (j > i && tokens[j - 1] == kAnd)))) {
            if ((filled & HUNDRED) || ((filled & TEN) && prev(j) != kAnd)) break;
            current = add(current, Number(BigInt(hundred && *hundred ? *hundred : t.construct_hundred_lookup.at(tok))));
            filled |= HUNDRED;
            started = true;
            ++j;
            continue;
        }
        if (const long long* scale = find(t.scales_lookup, tok)) {
            const Number count = !eq(current, Number(BigInt(0))) ? current
                                 : Number(BigInt(is_construct_dual(tokens, j) ? 2 : 1));
            total = add(total, mul(count, Number(BigInt(*scale))));
            current = BigInt(0);
            filled = 0;
            started = true;
            ++j;
            continue;
        }
        if (const long long* v = find(t.scale_duals_lookup, tok)) {
            total = add(total, Number(BigInt(*v)));
            current = BigInt(0);
            filled = 0;
            started = true;
            ++j;
            continue;
        }
        const double* frac = find(t.fractions_lookup, tok);
        if (frac || (halves && started && is_spoken_half(tokens, j))) {
            current = add(current, Number(frac && *frac != 0 ? *frac : t.spoken_half_lookup.at(tok)));
            started = true;
            ++j;
            break;
        }
        break;
    }
    if (!started) return {std::nullopt, i};
    return {add(total, current), j};
}

std::vector<Number> extract_numbers_ar(U32View text, bool ordinals, const std::optional<std::string>& lect) {
    std::vector<Number> out;
    const bool halves = lect && T().lects_with_spoken_half.count(*lect);
    for (const auto& span : number_spans(tokenize_ar(text), ordinals, halves)) out.push_back(span.value);
    return out;
}

std::optional<Number> extract_number_ar(U32View text, bool ordinals, const std::optional<std::string>& lect) {
    const auto numbers = extract_numbers_ar(text, ordinals, lect);
    if (numbers.empty()) return std::nullopt;
    const Number& value = numbers[0];
    if (!is_int(value) && is_integer(value)) return Number(to_int(value));
    return value;
}

// ---------------------------------------------------------------------------
// numbers_to_digits
// ---------------------------------------------------------------------------

namespace {

bool clitic(char32_t c) { return c == U'و' || c == U'ف' || c == U'ب' || c == U'ل'; }
bool clitic_digit(char32_t c) {
    return (c >= U'0' && c <= U'9') || (c >= 0x0660 && c <= 0x0669) || (c >= 0x06F0 && c <= 0x06F9);
}
bool space_before(U32View s, size_t p) { return p == 0 || text::is_space(s[p - 1]); }

// re.sub(r'\S+', fn, s)
template <class Fn>
U32 sub_words(U32View s, Fn fn) {
    U32 out;
    size_t i = 0;
    while (i < s.size()) {
        if (text::is_space(s[i])) {
            out.push_back(s[i++]);
            continue;
        }
        size_t j = i;
        while (j < s.size() && !text::is_space(s[j])) ++j;
        out += fn(U32(s.substr(i, j - i)));
        i = j;
    }
    return out;
}

// re.sub("<mark>(.)<mark>", lambda m: words[ord(m.group(1)) - 0xE000], s)
U32 unmask(U32View s, char32_t mark, const std::vector<U32>& words) {
    U32 out;
    size_t p = 0;
    while (p < s.size()) {
        if (s[p] == mark && p + 2 < s.size() && s[p + 1] != U'\n' && s[p + 2] == mark) {
            long k = static_cast<long>(s[p + 1]) - 0xE000;
            if (k < 0) k += static_cast<long>(words.size());
            if (k < 0 || k >= static_cast<long>(words.size())) throw IndexError("list index out of range");
            out += words[static_cast<size_t>(k)];
            p += 3;
        } else {
            out.push_back(s[p++]);
        }
    }
    return out;
}

char32_t mask_char(size_t k) {
    const size_t c = 0xE000 + k;
    if (c > 0x10FFFF) throw ValueError("chr() arg not in range(0x110000)");
    return static_cast<char32_t>(c);
}

U32 detach_proclitic(const U32& word) {
    const auto& t = T();
    const U32 b = normalize_ar(text::strip(word, t.punct));
    if (word[0] == U'و' && b.size() > 1) {
        const Tokens tokens = tokenize_ar(b);
        if (at(tokens, 0) == kAnd && !extract_numbers_ar(sub(b, 1)).empty()) return U"و " + sub(word, 1);
    }
    const auto split = split_proclitic(b);
    if (split && t.proclitics.find(word[0]) != U32::npos && !extract_numbers_ar(split->second).empty()) {
        return U32(1, word[0]) + U" " + sub(word, 1);
    }
    return word;
}

bool continues_a_number(const U32& word) {
    U32 b = normalize_ar(word);
    if (starts(b, kAnd)) b = sub(b, 1);
    return T().construct_hundred_lookup.count(b) || T().spoken_half_lookup.count(b);
}

std::optional<size_t> first_number_words(const std::vector<U32>& words, bool halves) {
    Tokens tokens;
    std::vector<size_t> owner;
    for (size_t w = 0; w < words.size(); ++w) {
        for (const U32& token : tokenize_ar(words[w])) {
            tokens.push_back(token);
            owner.push_back(w);
        }
    }
    const auto spans = number_spans(tokens, false, halves);
    if (spans.empty()) return std::nullopt;
    if (owner[spans[0].start] != 0) return std::nullopt;
    return owner[spans[0].end - 1] + 1;
}

std::pair<U32, std::vector<U32>> resolve_context_words(U32View utterance, bool halves) {
    const auto& t = T();
    const std::vector<U32> written = text::split_ws(utterance);
    std::vector<U32> words = written;
    Tokens tokens;
    std::vector<size_t> owner;
    for (size_t w = 0; w < words.size(); ++w) {
        for (const U32& token : tokenize_ar(words[w])) {
            tokens.push_back(token);
            owner.push_back(w);
        }
    }
    std::vector<bool> in_number(tokens.size(), false), starts_at(tokens.size(), false), unread(tokens.size(), false);
    for (const auto& span : number_spans(tokens, false, halves)) {
        for (size_t k = span.start; k < span.end; ++k) in_number[k] = true;
        starts_at[span.start] = true;
    }
    for (size_t k = 0; k < tokens.size(); ++k) {
        if (const auto end = ordinal_end(tokens, k)) {
            for (size_t m = k; m < *end; ++m) unread[m] = true;
        } else if (is_unread_fraction(tokens, k)) {
            unread[k] = true;
        }
    }
    for (size_t k = 0; k < tokens.size(); ++k) {
        if (in_number[k]) unread[k] = false;
    }
    std::vector<U32> masked;
    for (size_t k = 0; k < tokens.size(); ++k) {
        const U32& token = tokens[k];
        const bool half = halves && t.spoken_half_lookup.count(token);
        const bool dual = in_number[k] && is_construct_dual(tokens, k) &&
                          (starts_at[k] || (tokens[k - 1] == kAnd && k > 1 &&
                                            (t.scales_lookup.count(bare(tokens[k - 2])) ||
                                             t.scale_duals_lookup.count(bare(tokens[k - 2])))));
        if (!half && !dual && !t.context_canonical.count(token) && !unread[k] &&
            !(!in_number[k] && hundred_after_a_day_or_lady(tokens, k))) {
            continue;
        }
        const size_t w = owner[k];
        const U32& word = written[w];
        if (in_number[k]) {
            if (half) continue;
            const U32 lead = head(word, static_cast<long>(word.size() - text::lstrip(word, t.punct).size()));
            const U32 trail = sub(word, text::rstrip(word, t.punct).size());
            U32 prefix;
            for (size_t m = 0; m < tokens.size(); ++m) {
                if (owner[m] == w && m < k) prefix += tokens[m];
            }
            words[w] = lead + prefix + (dual ? t.accusative_or_construct_dual.at(token) : t.context_canonical.at(token)) +
                       trail;
        } else {
            words[w] = U32{0xF8FF, mask_char(masked.size()), 0xF8FF};
            masked.push_back(word);
        }
    }
    if (words == written) return {U32(utterance), masked};
    return {text::join(words, U" "), masked};
}

U32 join_literary_teens(U32View s) {
    const auto& t = T();
    std::vector<U32> out;
    auto is_punct = [&](char32_t c) { return t.punct.find(c) != U32::npos; };
    for (const U32& word : text::split_ws(s)) {
        if (!out.empty() && !is_punct(out.back().front()) && !is_punct(out.back().back()) &&
            has(t.teen_second_lookup, normalize_ar(text::rstrip(word, t.punct)))) {
            const Tokens tokens = tokenize_ar(out.back());
            const U32& last = at(tokens, -1);
            bool clitics = true;
            for (size_t k = 0; k + 1 < tokens.size(); ++k) clitics = clitics && proclitic_or_waw(tokens[k]);
            if (const long long* first = find(t.teen_first_lookup, last); first && clitics) {
                out.back() = head(out.back(), static_cast<long>(tokens.size()) - 1) + t.fused_literary_teens.at(*first) +
                             sub(word, text::rstrip(word, t.punct).size());
                continue;
            }
        }
        out.push_back(word);
    }
    return text::join(out, U" ");
}

std::pair<U32, std::vector<U32>> mask_dual_nouns(U32View s) {
    std::vector<U32> masked;
    U32 out = sub_words(s, [&](const U32& word) {
        bool dual = false;
        for (const U32& token : tokenize_ar(word)) dual = dual || has(T().dual_noun_tokens, token);
        if (!dual) return word;
        masked.push_back(word);
        return U32{0xF8FE, mask_char(masked.size() - 1), 0xF8FE};
    });
    return {out, masked};
}

}  // namespace

U32 numbers_to_digits_ar(U32View utterance, std::string_view lang) {
    const auto lect = resolve_ar_lect(lang);
    const bool halves = lect && T().lects_with_spoken_half.count(*lect);
    auto [resolved, masked] = resolve_context_words(utterance, halves);
    auto [dual_masked, duals] = mask_dual_nouns(join_literary_teens(resolved));
    // re.sub(r'(?<!\S)([وفبل])([0-9٠-٩۰-۹])', r'\1 \2', ...)
    U32 spaced;
    for (size_t p = 0; p < dual_masked.size(); ++p) {
        spaced.push_back(dual_masked[p]);
        if (clitic(dual_masked[p]) && space_before(dual_masked, p) && p + 1 < dual_masked.size() &&
            clitic_digit(dual_masked[p + 1])) {
            spaced.push_back(U' ');
            spaced.push_back(dual_masked[++p]);
        }
    }
    spaced = sub_words(spaced, detach_proclitic);
    U32 converted = ovos_number_parser::detail::numbers_to_digits_generic(
        spaced, lang, continues_a_number, [&](const std::vector<U32>& words) { return first_number_words(words, halves); });
    // re.sub(r'(?<!\S)([وفبل]) (?=[0-9٠-٩۰-۹])', r'\1', ...)
    U32 joined;
    for (size_t p = 0; p < converted.size(); ++p) {
        joined.push_back(converted[p]);
        if (clitic(converted[p]) && space_before(converted, p) && p + 2 < converted.size() && converted[p + 1] == U' ' &&
            clitic_digit(converted[p + 2])) {
            ++p;
        }
    }
    return unmask(unmask(joined, 0xF8FE, duals), 0xF8FF, masked);
}

}  // namespace arbtok::numbers::numbers_ar
