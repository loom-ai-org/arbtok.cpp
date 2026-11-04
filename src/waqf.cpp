#include "arbtok/waqf.hpp"

#include <algorithm>

#include "detail.hpp"
#include "text_match.hpp"

namespace arbtok::waqf {

const std::set<std::string> LEXICAL_FINAL_VOWEL = {
    "هُوَ", "هِيَ", "هُمَا",
    "هُمْ", "هُنَّ",
    "أَنْتَ", "أَنْتِ",
    "أَنْتُمَا", "أَنْتُمْ",
    "أَنْتُنَّ",
    "أَنَا", "نَحْنُ",
    "الَّذِي", "الَّتِي",
    "الَّذِينَ", "اللَّاتِي",
    "هَذَا", "هَذِهِ", "ذَلِكَ",
    "تِلْكَ",
    "مُنْذُ", "حَيْثُ", "قَبْلُ",
    "بَعْدُ",
};

const std::vector<std::string> SHORT_VOWELS = {"َ", "ُ", "ِ"};
const std::vector<std::string> DROPPED_TANWIN = {"ٌ", "ٍ"};
const std::vector<std::string> KEPT = {"ْ", "ّ"};

namespace {

using detail::U32;
using detail::U32View;

constexpr char32_t kFatha = 0x064E, kDamma = 0x064F, kKasra = 0x0650, kSukun = 0x0652, kShadda = 0x0651;
constexpr char32_t kTanwinFath = 0x064B, kTanwinDamm = 0x064C, kTanwinKasr = 0x064D, kTaMarbuta = 0x0629;

// _WORD: [ء-ٰٟٱ-ۓ]
bool in_word(char32_t c) { return (c >= 0x0621 && c <= 0x065F) || (c >= 0x0670 && c <= 0x06D3); }
// _TRAILING_MARKS: [ً-ْٰ]
bool is_trailing_mark(char32_t c) { return (c >= 0x064B && c <= 0x0652) || c == 0x0670; }
// _PHRASE and _PUNCT: . ! ? ، ؛ ؟ … and newline.
bool is_phrase_punct(char32_t c) {
    return c == U'.' || c == U'!' || c == U'?' || c == 0x060C || c == 0x061B || c == 0x061F || c == 0x2026 ||
           c == U'\n';
}

U32 pausal_word(U32View word) {
    if (detail::contains(word, kTanwinFath)) {
        const U32 tm_tf = {kTaMarbuta, kTanwinFath}, tf_tm = {kTanwinFath, kTaMarbuta};
        if (word.find(tm_tf) != U32View::npos || word.find(tf_tm) != U32View::npos)
            return detail::replace(word, U32(1, kTanwinFath), U32());
        return detail::replace(word, U32(1, kTanwinFath), U32(1, kFatha));
    }
    const auto marks = detail::trailing_run(word, is_trailing_mark);
    if (!marks) return U32(word);
    const U32View tail = word.substr(marks->first, marks->second - marks->first);
    const U32View stem = word.substr(0, marks->first);
    if (LEXICAL_FINAL_VOWEL.count(detail::s8(word))) return U32(word);
    const bool drop = detail::contains(tail, kTanwinDamm) || detail::contains(tail, kTanwinKasr) ||
                      detail::contains(tail, kFatha) || detail::contains(tail, kDamma) ||
                      detail::contains(tail, kKasra);
    if (!drop) return U32(word);
    U32 out(stem);
    for (char32_t c : tail)
        if (c == kSukun || c == kShadda) out.push_back(c);
    return out;
}

// _WORD.sub(pausal_word, text) over [begin, end): each maximal run of word
// characters is one match.
U32 pausal_all(U32View text) {
    U32 out;
    size_t i = 0;
    while (i < text.size()) {
        if (!in_word(text[i])) {
            out.push_back(text[i++]);
            continue;
        }
        size_t j = i;
        while (j < text.size() && in_word(text[j])) ++j;
        out += pausal_word(text.substr(i, j - i));
        i = j;
    }
    return out;
}

}  // namespace

std::string pausal(const std::string& text_utf8, bool phrase_final_only) {
    const U32 text = detail::u(text_utf8);
    if (!phrase_final_only) return detail::s8(pausal_all(text));
    U32 out;
    size_t i = 0;
    while (i < text.size()) {
        const bool punct = is_phrase_punct(text[i]);
        size_t j = i;
        while (j < text.size() && is_phrase_punct(text[j]) == punct) ++j;
        const U32View chunk = U32View(text).substr(i, j - i);
        i = j;
        if (punct) {
            out += chunk;
            continue;
        }
        // Only the last Arabic word of the phrase stands at the pause.
        size_t last_start = U32View::npos, last_end = 0;
        for (size_t k = 0; k < chunk.size();) {
            if (!in_word(chunk[k])) {
                ++k;
                continue;
            }
            size_t e = k;
            while (e < chunk.size() && in_word(chunk[e])) ++e;
            last_start = k;
            last_end = e;
            k = e;
        }
        if (last_start == U32View::npos) {
            out += chunk;
            continue;
        }
        out += chunk.substr(0, last_start);
        out += pausal_word(chunk.substr(last_start, last_end - last_start));
        out += chunk.substr(last_end);
    }
    return detail::s8(out);
}

}  // namespace arbtok::waqf
