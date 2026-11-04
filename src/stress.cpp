#include "arbtok/stress.hpp"

#include <orthography2ipa/orthography2ipa.hpp>
#include <orthography2ipa/stress.hpp>
#include <orthography2ipa/vowels.hpp>

#include <algorithm>

#include "detail.hpp"
#include "stress_detail.hpp"

namespace arbtok::stress {

namespace o2i = orthography2ipa;
using detail::U32;
using detail::U32View;

namespace {

// _SEGMENT_MODIFIERS
bool is_segment_modifier(char32_t c) {
    switch (c) {
        case 0x02E4: case 0x02B7: case 0x02B2: case 0x02B0: case 0x02E0: case 0x02C0: case 0x02BC:
        case 0x032A: case 0x032C: case 0x0325: case 0x0320: case 0x0303: case 0x02D0:
            return true;
        default:
            return false;
    }
}

std::string stress_core(const std::string& ipa, const std::string& lang) {
    const U32 cps = detail::u(ipa);
    if (cps.empty() || std::none_of(cps.begin(), cps.end(), [](char32_t c) { return o2i::vowels::is_ipa_vowel(c); }))
        return ipa;
    const o2i::LanguageSpec& spec = o2i::get(lang);
    if (!spec.stress_defined) return ipa;
    if (spec.quantity_sensitive) {
        const int idx = o2i::stress::detect_stress_by_weight(ipa, spec);
        const std::vector<std::string> syllables = o2i::stress::syllabify_ipa(ipa, spec.max_onset);
        return o2i::stress::apply_stress_mark(ipa, spec, idx, nullptr, &syllables);
    }
    return o2i::stress::apply_stress_mark(ipa, spec, o2i::stress::detect_stress(ipa, spec));
}

}  // namespace

int first_segment_len(const std::string& ipa) {
    const U32 cps = detail::u(ipa);
    if (cps.empty()) return 0;
    size_t n = 1;
    while (n < cps.size() && is_segment_modifier(cps[n])) ++n;
    return static_cast<int>(n);
}

std::string stress_ipa(const std::string& ipa, const std::string& lang, int proclitic_onset) {
    if (proclitic_onset) {
        // ipa[:n] and ipa[n:], with Python's slice rules for a negative n.
        const U32 cps = detail::u(ipa);
        const long size = static_cast<long>(cps.size());
        long at = proclitic_onset < 0 ? std::max(0L, size + proclitic_onset) : std::min<long>(proclitic_onset, size);
        return detail::s8(U32View(cps).substr(0, at)) + stress_core(detail::s8(U32View(cps).substr(at)), lang);
    }
    return stress_core(ipa, lang);
}

std::string stress_words(const std::string& ipa, const std::string& lang) {
    const std::vector<U32> words = detail::split(detail::u(ipa), U" ");
    std::vector<U32> out;
    for (const auto& w : words) out.push_back(detail::u(stress_ipa(detail::s8(w), lang)));
    return detail::s8(detail::join(out, U" "));
}

}  // namespace arbtok::stress
