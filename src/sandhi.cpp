#include "arbtok/sandhi.hpp"

#include <optional>

#include "arbtok/stress.hpp"
#include "arbtok/waqf.hpp"
#include "detail.hpp"
#include "text_match.hpp"

namespace arbtok::sandhi {

const std::vector<std::pair<std::string, std::string>> NUN_ASSIMILATION = {
    {"r", "r"}, {"j", "j"}, {"l", "l"}, {"b", "m"},
};

namespace {

using arbtok::detail::U32;
using arbtok::detail::U32View;
namespace d = arbtok::detail;

constexpr char32_t kAlif = 0x0627, kWasl = 0x0671, kLam = 0x0644;
constexpr char32_t kTanwinFath = 0x064B, kTanwinDamm = 0x064C, kTanwinKasr = 0x064D, kTaMarbuta = 0x0629;

// _TRAILING_MARKS, and the harakat that _has_article and _is_bare_wasl_alif skip.
bool is_haraka(char32_t c) { return (c >= 0x064B && c <= 0x0652) || c == 0x0670; }

// _IPA_VOWELS
bool is_ipa_vowel(char32_t c) {
    switch (c) {
        case U'a': case U'e': case U'i': case U'o': case U'u':
        case 0x0251: case 0x00E6: case 0x0259: case 0x025B: case 0x0254:
            return true;
        default:
            return false;
    }
}

bool ends_in_vowel(U32View ipa) { return !ipa.empty() && (is_ipa_vowel(ipa.back()) || ipa.back() == 0x02D0); }

U32 letters_of(U32View surface) {
    U32 out;
    for (char32_t c : surface)
        if (!is_haraka(c)) out.push_back(c);
    return out;
}

bool opens_on_article(U32View surface) {
    const U32 letters = letters_of(surface);
    return letters.size() >= 2 && (letters[0] == kAlif || letters[0] == kWasl) && letters[1] == kLam;
}

U32 restore_wasl_onset(U32View ipa) {
    if (!ipa.empty() && (ipa[0] == U'i' || ipa[0] == U'u')) return U"ʔ" + U32(ipa);
    return U32(ipa);
}

U32 elide_article(U32View ipa) {
    if (!ipa.empty() && (ipa[0] == U'a' || ipa[0] == 0x0251)) return U32(ipa.substr(1));
    return U32(ipa);
}

U32 assimilate_nun(U32View ipa, U32View next_ipa) {
    if (!d::ends_with(ipa, U"n") || (ipa != U"min" && ipa != U"man")) return U32(ipa);
    if (next_ipa.empty()) return U32(ipa);
    const std::string onset = d::s8(next_ipa[0]);
    for (const auto& [from, to] : NUN_ASSIMILATION)
        if (from == onset) return U32(ipa.substr(0, ipa.size() - 1)) + d::u(to);
    return U32(ipa);
}

bool article_vowel_is_stressed(const std::string& ipa, const std::string& lang) {
    return d::starts_with(d::u(stress::stress_ipa(ipa, lang)), U"ˈ");
}

}  // namespace

namespace detail {

std::string pausal_form(const std::string& ipa_utf8, const std::string& surface_utf8) {
    if (waqf::LEXICAL_FINAL_VOWEL.count(surface_utf8)) return ipa_utf8;
    const U32 surface = d::u(surface_utf8);
    U32 ipa = d::u(ipa_utf8);
    const bool fath = d::contains(surface, kTanwinFath), damm = d::contains(surface, kTanwinDamm),
               kasr = d::contains(surface, kTanwinKasr);
    if ((fath || damm || kasr) && d::contains(surface, kTaMarbuta)) {
        for (const U32View ending : {U32View(U"atun"), U32View(U"atin"), U32View(U"atan")})
            if (d::ends_with(ipa, ending)) return d::s8(U32View(ipa).substr(0, ipa.size() - ending.size())) + "a";
    }
    if (fath && d::ends_with(ipa, U"an")) return d::s8(U32View(ipa).substr(0, ipa.size() - 2)) + "aː";
    if ((damm && d::ends_with(ipa, U"un")) || (kasr && d::ends_with(ipa, U"in")))
        return d::s8(U32View(ipa).substr(0, ipa.size() - 2));
    if (const auto trailing = d::trailing_run(surface, is_haraka)) {
        const U32View tail = U32View(surface).substr(trailing->first, trailing->second - trailing->first);
        // _SHORT_VOWEL_IPA in dict order: fatha, damma, kasra.
        std::optional<char32_t> vowel;
        for (const auto& [mark, v] : {std::pair{U'َ', U'a'}, std::pair{U'ُ', U'u'}, std::pair{U'ِ', U'i'}})
            if (d::contains(tail, mark)) {
                vowel = v;
                break;
            }
        if (vowel && !ipa.empty() && ipa.back() == *vowel) {
            ipa.pop_back();
            const U32View before = U32View(surface).substr(0, trailing->first);
            if (!before.empty() && before.back() == kTaMarbuta && !ipa.empty() && ipa.back() == U't') ipa.pop_back();
        }
    }
    return d::s8(ipa);
}

bool has_article(const std::string& surface) { return opens_on_article(d::u(surface)); }

bool is_bare_wasl_alif(const std::string& surface_utf8) {
    const U32 surface = d::u(surface_utf8);
    if (surface.empty()) return false;
    if (surface[0] != kAlif && surface[0] != kWasl) return false;
    return !opens_on_article(surface);
}

}  // namespace detail

std::vector<std::string> apply_cross_word(const std::vector<CrossWord>& words, bool pausal, const std::string& lang) {
    std::vector<std::string> out;
    for (const auto& w : words) out.push_back(w.ipa);
    const bool reference_register = lang == "ar" || lang == "arb";

    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i].is_punct || words[i].ipa.empty()) continue;
        std::optional<std::string> nxt;
        bool before_pause = false;
        for (size_t j = i + 1; j < words.size(); ++j) {
            if (words[j].is_punct) {
                before_pause = true;
                break;
            }
            if (!words[j].ipa.empty()) {
                nxt = words[j].ipa;
                break;
            }
        }
        if (before_pause) {
            if (pausal && reference_register) out[i] = detail::pausal_form(words[i].ipa, words[i].surface);
        } else if (nxt && reference_register) {
            out[i] = d::s8(assimilate_nun(d::u(words[i].ipa), d::u(*nxt)));
        }
    }

    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i].is_punct || out[i].empty()) continue;
        std::optional<std::string> prev_spoken;
        bool prev_was_pause = false;
        for (size_t j = i; j-- > 0;) {
            if (words[j].is_punct) {
                prev_was_pause = true;
                break;
            }
            if (!out[j].empty()) {
                prev_spoken = out[j];
                break;
            }
        }
        const bool after_vowel = prev_spoken && !prev_was_pause && ends_in_vowel(d::u(*prev_spoken));
        if (detail::has_article(words[i].surface)) {
            if (after_vowel && !article_vowel_is_stressed(out[i], lang)) out[i] = d::s8(elide_article(d::u(out[i])));
        } else if (detail::is_bare_wasl_alif(words[i].surface)) {
            out[i] = d::s8(restore_wasl_onset(d::u(out[i])));
        }
    }

    std::vector<std::string> spoken;
    for (size_t i = 0; i < words.size(); ++i)
        if (!words[i].is_punct) spoken.push_back(out[i]);
    return spoken;
}

}  // namespace arbtok::sandhi
