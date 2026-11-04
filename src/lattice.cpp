#include "arbtok/lattice.hpp"

#include <orthography2ipa/orthography2ipa.hpp>
#include <orthography2ipa/phonetok.hpp>
#include <orthography2ipa/stress.hpp>

#include <algorithm>
#include <map>
#include <memory>

#include "arbtok/constants.hpp"
#include "arbtok/stress.hpp"
#include "arbtok/tokenizer.hpp"
#include "detail.hpp"
#include "gemination.hpp"
#include "unicode_norm.hpp"

namespace arbtok::lattice {

namespace o2i = orthography2ipa;
namespace c = arbtok::constants;
using detail::U32;
using detail::U32View;
using rescorer::RescoreContext;

namespace {

const std::string kArticle = c::ALIF + c::LAM;

bool is_short_haraka(const std::string& g) { return g == c::FATHA || g == c::DAMMA || g == c::KASRA; }
// _HARAKAT: the short vowels, the tanwin and sukun.
bool is_haraka_char(char32_t ch) { return (ch >= 0x064B && ch <= 0x0650) || ch == 0x0652; }

bool is_hamza_carrier(const std::string& g) {
    return g == c::ALEF_HAMZA_ABOVE || g == c::ALEF_HAMZA_BELOW || g == c::WAW_HAMZA || g == c::YA_HAMZA;
}

// _is_consonant_ipa
bool is_consonant_ipa(const std::string& ipa) {
    const U32 cps = detail::u(ipa);
    return !cps.empty() && U32View(U"aiueoɑəː").find(cps[0]) == U32View::npos;
}

// _ARTICLE_PROCLITICS: و ف ب ك ل س
bool is_article_proclitic(char32_t ch) {
    return ch == 0x0648 || ch == 0x0641 || ch == 0x0628 || ch == 0x0643 || ch == 0x0644 || ch == 0x0633;
}

bool is_article_position(const RescoreContext& context) {
    for (size_t i = 0; i < context.index; ++i)
        for (char32_t ch : detail::u((*context.slots)[i].grapheme))
            if (!is_article_proclitic(ch)) return false;
    return true;
}

template <class Keep>
std::vector<Candidate> prefer_only(const SegmentSlot& slot, const std::string& ipa, Keep keep) {
    std::vector<Candidate> kept{{ipa, 0.0}};
    for (const auto& cand : slot.candidates)
        if (cand.ipa != ipa && keep(cand.ipa)) kept.push_back({cand.ipa, std::max(cand.score, 1.0)});
    return kept;
}

std::vector<Candidate> prefer(const SegmentSlot& slot, const std::string& ipa) {
    return prefer_only(slot, ipa, [](const std::string&) { return true; });
}

struct Engine {
    std::unique_ptr<o2i::PhonetokTokenizer> tokenizer;
    std::unique_ptr<rescorer::LatticeRescorer> allophones;
    std::vector<const rescorer::LatticeRescorer*> rescorers;
};

// _engine: one tokenizer and rescorer chain per spec code, built on first use.
const Engine& engine(const std::string& lang) {
    static std::map<std::string, Engine> engines;
    auto it = engines.find(lang);
    if (it == engines.end()) {
        const o2i::LanguageSpec& spec = o2i::get(lang);
        Engine e;
        e.tokenizer = std::make_unique<o2i::PhonetokTokenizer>(spec);
        e.rescorers = default_rescorers();
        e.allophones = rescorer::compile_allophone_rescorer(spec.allophone_rules);
        if (e.allophones) e.rescorers.push_back(e.allophones.get());
        it = engines.emplace(lang, std::move(e)).first;
    }
    return it->second;
}

std::optional<std::string> lookup(const std::map<std::string, std::string>& table, const std::string& key) {
    auto it = table.find(key);
    if (it == table.end()) return std::nullopt;
    return it->second;
}

}  // namespace

std::vector<Candidate> SunLetterRescorer::rescore(const SegmentSlot& slot, const RescoreContext& context) const {
    if (slot.grapheme != kArticle) return slot.candidates;
    const SegmentSlot* nxt = context.next_slot();
    if (nxt == nullptr) return slot.candidates;
    if (!is_article_position(context)) return slot.candidates;
    if (nxt->grapheme == c::LAM) return prefer(slot, "al");
    if (c::SUN_LETTERS.count(nxt->grapheme)) {
        bool pointed = false;
        for (const auto& s : *context.slots)
            for (char32_t ch : detail::u(s.grapheme))
                if (is_haraka_char(ch) || ch == 0x0651) pointed = true;
        if (!pointed) return prefer(slot, "a");
    }
    return prefer(slot, "al");
}

std::vector<Candidate> WaslRescorer::rescore(const SegmentSlot& slot, const RescoreContext& context) const {
    if (slot.grapheme != c::ALIF || !context.is_word_initial()) return slot.candidates;
    const SegmentSlot* nxt = context.next_slot();
    if (nxt != nullptr && is_short_haraka(nxt->grapheme)) return {};
    if (nxt != nullptr && is_consonant_ipa(nxt->top().ipa))
        return prefer_only(slot, "i", [](const std::string& ipa) { return detail::starts_with(detail::u(ipa), U"ʔ"); });
    return slot.candidates;
}

std::vector<Candidate> HamzaCarrierRescorer::rescore(const SegmentSlot& slot, const RescoreContext& context) const {
    if (!is_hamza_carrier(slot.grapheme)) return slot.candidates;
    const U32 top = detail::u(slot.top().ipa);
    if (!(detail::starts_with(top, U"ʔ") && top.size() > 1)) return slot.candidates;
    const SegmentSlot* nxt = context.next_slot();
    if (nxt == nullptr || nxt->grapheme == c::SUKUN) return {{"ʔ", 0.0}};
    return slot.candidates;
}

std::vector<Candidate> ConsonantalGlideRescorer::rescore(const SegmentSlot& slot,
                                                         const RescoreContext& context) const {
    static const std::vector<std::string> ya_onsets = {"ي", "يَ", "يِ", "يُ"};
    static const std::vector<std::string> waw_onsets = {"و", "وَ", "وِ", "وُ",
                                                        "وَا"};
    auto in = [](const std::vector<std::string>& set, const std::string& g) {
        return std::find(set.begin(), set.end(), g) != set.end();
    };
    const SegmentSlot* nxt = context.next_slot();
    bool nxt_vowel_onset = false;
    if (nxt != nullptr && !nxt->top().ipa.empty()) {
        const char32_t first = detail::u(nxt->top().ipa)[0];
        nxt_vowel_onset = U32View(U"aiueoɑə").find(first) != U32View::npos && nxt->grapheme != c::WAW &&
                          nxt->grapheme != c::YA;
    }
    if (slot.grapheme == c::KASRA + c::YA && nxt != nullptr && (in(ya_onsets, nxt->grapheme) || nxt_vowel_onset))
        return {{"ij", 0.0}};
    if (slot.grapheme == c::DAMMA + c::WAW && nxt != nullptr && (in(waw_onsets, nxt->grapheme) || nxt_vowel_onset))
        return {{"uw", 0.0}};
    if (const SegmentSlot* prev = context.prev_slot()) {
        if (slot.grapheme == c::YA && prev->grapheme == c::KASRA + c::YA) return {{"j", 0.0}};
        if (slot.grapheme == c::WAW && prev->grapheme == c::DAMMA + c::WAW) return {{"w", 0.0}};
    }
    return slot.candidates;
}

std::vector<Candidate> AccusativeAlifRescorer::rescore(const SegmentSlot& slot, const RescoreContext& context) const {
    if (slot.grapheme != c::ALIF && slot.grapheme != c::ALIF_MAKSURA) return slot.candidates;
    const SegmentSlot* prev = context.prev_slot();
    if (prev != nullptr && prev->grapheme == c::TANWIN_FATH) return {{"", 0.0}};
    return slot.candidates;
}

const std::vector<const rescorer::LatticeRescorer*>& default_rescorers() {
    static const AccusativeAlifRescorer accusative;
    static const SunLetterRescorer sun;
    static const WaslRescorer wasl;
    static const HamzaCarrierRescorer hamza;
    static const ConsonantalGlideRescorer glide;
    static const std::vector<const rescorer::LatticeRescorer*> chain = {&accusative, &sun, &wasl, &hamza, &glide};
    return chain;
}

const std::vector<std::string>& default_rescorer_names() {
    static const std::vector<std::string> names = {"AccusativeAlifRescorer", "SunLetterRescorer", "WaslRescorer",
                                                   "HamzaCarrierRescorer", "ConsonantalGlideRescorer"};
    return names;
}

std::vector<SegmentSlot> word_lattice(const std::string& word, const std::string& lang) {
    const std::string text = tokenizer::normalize_unicode(word);
    const Engine& e = engine(lang);
    return e.tokenizer->ipa_lattice(text, 8, false, &e.rescorers);
}

std::string word_ipa(const std::string& word, const std::string& lang, bool stress) {
    std::optional<std::string> ipa = spec_word_exception(word, lang);
    if (!ipa) {
        ipa.emplace();
        for (const auto& slot : word_lattice(word, lang)) *ipa += slot.top().ipa;
    }
    if (!stress || o2i::stress::is_cliticless(word, o2i::get(lang))) return *ipa;
    return stress::stress_ipa(*ipa, lang);
}

std::optional<std::string> spec_word_exception(const std::string& word, const std::string& lang) {
    const auto& exceptions = o2i::get(lang).word_exceptions;
    if (exceptions.empty()) return std::nullopt;
    if (auto hit = lookup(exceptions, detail::s8(detail::nfc(detail::u(word))))) return hit;
    const std::string normalized = tokenizer::normalize_unicode(word);
    if (auto hit = lookup(exceptions, normalized)) return hit;
    return lookup(exceptions, detail::s8(detail::expand_arabic_gemination(detail::u(normalized))));
}

bool defers_to_cascade(const std::string& word) {
    const U32 norm = detail::u(tokenizer::normalize_unicode(word));
    const U32 punct = detail::u(c::PUNCT);
    for (char32_t ch : norm)
        if (punct.find(ch) != U32::npos) return true;
    const auto is_clitic_base = [](char32_t ch) { return c::CLITIC_BASES.count(detail::s8(ch)) > 0; };
    const auto is_short = [](char32_t ch) { return ch == 0x064E || ch == 0x064F || ch == 0x0650; };
    size_t i = 0;
    bool peeled = false;
    std::optional<char32_t> last_harakah;
    while (i < norm.size() && is_clitic_base(norm[i])) {
        size_t j = i + 1;
        if (j < norm.size() && norm[j] == 0x0651) break;
        last_harakah.reset();
        if (j < norm.size() && is_short(norm[j])) {
            last_harakah = norm[j];
            ++j;
        }
        i = j;
        peeled = true;
    }
    if (peeled && last_harakah != U'َ') {
        // _ALL_DIACRITICS: _HARAKAT, shadda, the dagger alif and the madda.
        U32 stem;
        for (size_t k = i; k < norm.size(); ++k) {
            const char32_t ch = norm[k];
            if (!(is_haraka_char(ch) || ch == 0x0651 || ch == 0x0670 || ch == 0x0653)) stem.push_back(ch);
        }
        if (!stem.empty() && stem[0] == 0x0627 && !(stem.size() > 1 && stem[1] == 0x0644)) return true;
    }
    return false;
}

}  // namespace arbtok::lattice
