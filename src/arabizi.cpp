#include "arbtok/arabizi.hpp"

#include <map>
#include <set>

#include "detail.hpp"

namespace arbtok::arabizi {

const std::vector<std::pair<std::string, std::string>> ARABIZI_DIGITS = {
    {"2", "ء"}, {"3", "ع"}, {"5", "خ"}, {"6", "ط"},
    {"7", "ح"}, {"8", "غ"}, {"9", "ق"},
};

namespace {

using detail::U32;
using detail::U32View;
using Table = std::map<U32, U32>;

const Table kPrimeDigits = {
    {U"3'", U"غ"}, {U"7'", U"خ"}, {U"6'", U"ظ"}, {U"9'", U"ق"},
};

const Table kDigraphs = {
    {U"kh", U"خ"}, {U"gh", U"غ"}, {U"sh", U"ش"}, {U"ch", U"ش"}, {U"th", U"ث"},
    {U"dh", U"ذ"}, {U"aa", U"ا"}, {U"ee", U"ي"}, {U"ii", U"ي"}, {U"oo", U"و"},
    {U"uu", U"و"}, {U"ou", U"و"}, {U"ai", U"ي"}, {U"ay", U"ي"}, {U"ei", U"ي"},
    {U"ey", U"ي"}, {U"oi", U"و"}, {U"au", U"و"}, {U"aw", U"و"},
};

const Table kConsonants = {
    {U"b", U"ب"}, {U"t", U"ت"}, {U"j", U"ج"}, {U"7", U"ح"}, {U"5", U"خ"},
    {U"d", U"د"}, {U"r", U"ر"}, {U"z", U"ز"}, {U"s", U"س"}, {U"9", U"ق"},
    {U"3", U"ع"}, {U"8", U"غ"}, {U"f", U"ف"}, {U"q", U"ق"}, {U"k", U"ك"},
    {U"g", U"ق"}, {U"l", U"ل"}, {U"m", U"م"}, {U"n", U"ن"}, {U"h", U"ه"},
    {U"w", U"و"}, {U"y", U"ي"}, {U"2", U"ء"}, {U"v", U"ف"}, {U"p", U"ب"},
    {U"c", U"ك"}, {U"6", U"ط"}, {U"S", U"ص"}, {U"D", U"ض"}, {U"T", U"ط"},
    {U"Z", U"ظ"},
};

const Table kVowelsSingle = {
    {U"a", U"ا"}, {U"e", U"ي"}, {U"i", U"ي"}, {U"o", U"و"}, {U"u", U"و"},
};

const U32 kStrip = U".,;:!?()[]\"'،؛؟-";

const std::set<U32>& known_keys() {
    static const std::set<U32> keys = [] {
        std::set<U32> out;
        for (const Table* table : {&kPrimeDigits, &kDigraphs, &kConsonants, &kVowelsSingle})
            for (const auto& [key, value] : *table) {
                out.insert(key);
                out.insert(detail::lower(key));
                out.insert(detail::upper(key));
            }
        return out;
    }();
    return keys;
}

}  // namespace

bool is_arabizi(const std::string& word, std::optional<bool> hint) {
    if (hint) return *hint;
    // _HAS_DIGIT_GUTTURAL: each alternative starts with one of these digits.
    return word.find_first_of("2356789") != std::string::npos;
}

bool arabizi_covers(const std::string& word_utf8) {
    const U32 w = detail::strip(detail::u(word_utf8), kStrip);
    if (w.empty()) return false;
    const auto& known = known_keys();
    for (char32_t c : w) {
        if (!detail::is_alnum(c)) continue;
        if (!known.count(U32(1, c)) && !known.count(detail::lower(U32View(&c, 1)))) return false;
    }
    return true;
}

std::string to_arabic_skeleton(const std::string& word_utf8) {
    const U32 w = detail::strip(detail::u(word_utf8), kStrip);
    if (w.empty()) return word_utf8;
    U32 out;
    const size_t n = w.size();
    for (size_t i = 0; i < n;) {
        const U32 two = w.substr(i, 2);
        if (auto it = kPrimeDigits.find(two); it != kPrimeDigits.end()) {
            out += it->second;
            i += 2;
            continue;
        }
        if (auto it = kDigraphs.find(detail::lower(two)); it != kDigraphs.end()) {
            out += it->second;
            i += 2;
            continue;
        }
        const U32 ch(1, w[i]);
        if (auto it = kConsonants.find(ch); it != kConsonants.end()) {
            out += it->second;
            ++i;
            continue;
        }
        const U32 low = detail::lower(ch);
        if (auto it = kConsonants.find(low); it != kConsonants.end()) {
            out += it->second;
            ++i;
            continue;
        }
        if (auto it = kVowelsSingle.find(low); it != kVowelsSingle.end()) {
            if (i == 0)
                out += U'ا';
            else if (!(i == n - 1 && low != U"a"))
                out += it->second;
        }
        ++i;
    }
    return detail::s8(out);
}

}  // namespace arbtok::arabizi
