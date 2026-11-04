#include "arbtok/diacritize.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <set>

#include <orthography2ipa/orthography2ipa.hpp>
#include <orthography2ipa/phonetok.hpp>
#include <orthography2ipa/underspecification.hpp>

#include "arbtok/ensemble.hpp"
#include "arbtok/nisba.hpp"
#include "arbtok/tokenizer.hpp"
#include "arbtok/waqf.hpp"
#include "detail.hpp"

namespace arbtok::diacritize {

namespace {

using detail::U32;
using detail::U32View;

bool is_mark(char32_t c) { return (c >= 0x064B && c <= 0x0653) || c == 0x0670; }

U32 strip32(U32View text) {
    U32 out;
    for (char32_t c : text)
        if (!is_mark(c)) out.push_back(c);
    return out;
}

// b is in _RESTORABLE.get(a, ()).
bool restorable(char32_t a, char32_t b) {
    switch (a) {
        case 0x0627: return b == 0x0622 || b == 0x0623 || b == 0x0625 || b == 0x0671;
        case 0x0647: return b == 0x0629;
        case 0x064A: return b == 0x0649;
        default: return false;
    }
}

bool all_space(const U32& s) {
    for (char32_t c : s)
        if (!detail::is_space(c)) return false;
    return true;
}

}  // namespace

std::string strip_marks(std::string_view text) { return detail::to_utf8(strip32(detail::from_utf8(text))); }

std::optional<std::string> repair_skeleton(std::string_view original, std::string_view proposed) {
    const U32 letters = strip32(detail::from_utf8(original));
    const U32 prop = detail::from_utf8(proposed);
    if (letters.size() != strip32(prop).size()) return std::nullopt;
    U32 out;
    std::size_t i = 0;
    for (char32_t c : prop) out.push_back(is_mark(c) ? c : letters[i++]);
    return detail::to_utf8(out);
}

bool skeleton_is_preserved(std::string_view original, std::string_view diacritized) {
    const U32 before = strip32(detail::from_utf8(original));
    const U32 after = strip32(detail::from_utf8(diacritized));
    if (before == after) return true;
    if (before.size() != after.size()) return false;
    for (std::size_t i = 0; i < before.size(); ++i)
        if (before[i] != after[i] && !restorable(before[i], after[i])) return false;
    return true;
}

bool has_arabic(std::string_view word) {
    for (char32_t c : detail::from_utf8(word))
        if ((c >= 0x0600 && c <= 0x06FF) || (c >= 0x0750 && c <= 0x077F) || (c >= 0xFB50 && c <= 0xFDFF) ||
            (c >= 0xFE70 && c <= 0xFEFF))
            return true;
    return false;
}

bool DiacritizedWord::lect_constrained() const {
    return provenance == "closed-class" || provenance == "author" || provenance == "proclitic";
}

std::vector<std::pair<std::string, std::vector<std::string>>> DiacritizedText::by_provenance() const {
    std::vector<std::pair<std::string, std::vector<std::string>>> out;
    for (const auto& w : words) {
        auto it = std::find_if(out.begin(), out.end(), [&](const auto& item) { return item.first == w.provenance; });
        if (it == out.end()) it = out.insert(out.end(), {w.provenance, {}});
        it->second.push_back(w.surface);
    }
    return out;
}

bool author_complete(std::string_view word, const std::vector<std::size_t>& positions) {
    if (positions.empty()) return true;
    // _ARTICLE_RE: ^[وفبكل]?[َُِ]?(?:ا|ٱ|أ)ل. Neither optional part can be a
    // character that the parts after it match, so a greedy scan is the match.
    const U32 w = detail::from_utf8(word);
    std::size_t i = 0;
    if (i < w.size() && U32View(U"وفبكل").find(w[i]) != U32View::npos) ++i;
    if (i < w.size() && U32View(U"َُِ").find(w[i]) != U32View::npos) ++i;
    if (!(i + 1 < w.size() && U32View(U"اٱأ").find(w[i]) != U32View::npos && w[i + 1] == U'ل')) return false;
    const std::size_t end = i + 2;
    for (std::size_t p : positions)
        if (p != end - 2 && p != end - 1) return false;
    return true;
}

bool proclitic_complete(std::string_view word, const std::vector<std::size_t>& positions) {
    if (positions.empty()) return false;
    for (std::size_t p : positions)
        if (p != 0) return false;
    // _BARE_PROCLITIC_RE: ^[وفبكلس](?=[^\sًٌٍَُِّْٰٓ]), and more than two code points.
    const U32 w = detail::from_utf8(word);
    return w.size() > 2 && U32View(U"وفبكلس").find(w[0]) != U32View::npos && !detail::is_space(w[1]) &&
           !is_mark(w[1]);
}

LatticeDiacritizer::LatticeDiacritizer(const std::string& lang, Options options)
    : lang_(lang), options_(std::move(options)) {
    try {
        spec_ = &orthography2ipa::get(lang_);
    } catch (const std::exception& e) {
        throw KeyError(e.what());
    }
    tokenizer_ = std::make_unique<orthography2ipa::PhonetokTokenizer>(*spec_);
    if (options_.lexicon && !options_.lexicon->empty()) lexicon_.emplace(*options_.lexicon);
    if (options_.dialect_lexicon) dialect_lexicon_.emplace(lang_);
}

LatticeDiacritizer::~LatticeDiacritizer() = default;
LatticeDiacritizer::LatticeDiacritizer(LatticeDiacritizer&&) noexcept = default;
LatticeDiacritizer& LatticeDiacritizer::operator=(LatticeDiacritizer&&) noexcept = default;

const ensemble::EnsembleDiacritizer& LatticeDiacritizer::diacritizer() const {
    return ensemble::get_ensemble(options_.model_path, options_.vocab_path);
}

std::string LatticeDiacritizer::propose(const std::string& word) const {
    std::string proposed = diacritizer().diacritize(word);
    if (options_.waqf) proposed = waqf::pausal(proposed);
    return proposed;
}

std::optional<std::string> LatticeDiacritizer::lookup(const std::string& word) const {
    if (!lexicon_ || !options_.waqf) return std::nullopt;
    auto stem = lexicon_->get(word);
    if (!stem) return std::nullopt;
    stem = nisba::restore_nisba(*stem);
    if (strip_marks(*stem) != strip_marks(word) || !is_licensed(*stem)) return std::nullopt;
    return stem;
}

std::optional<std::string> LatticeDiacritizer::dialect_lookup(const std::string& word) const {
    if (!dialect_lexicon_) return std::nullopt;
    auto voc = dialect_lexicon_->get(word);
    if (!voc) return std::nullopt;
    if (strip_marks(*voc) != strip_marks(word) || !is_licensed(*voc)) return std::nullopt;
    return voc;
}

bool LatticeDiacritizer::is_licensed(const std::string& word) const {
    std::vector<orthography2ipa::Token> tokens;
    try {
        tokens = tokenizer_->tokenize(tokenizer::normalize_unicode(word));
    } catch (const std::exception&) {
        return false;
    }
    for (const auto& t : tokens)
        if (t.kind == orthography2ipa::TokenKind::UNKNOWN) return false;
    return true;
}

std::string LatticeDiacritizer::diacritize_word(const std::string& word) { return diacritize_word_record(word).output; }

DiacritizedWord LatticeDiacritizer::diacritize_word_record(const std::string& word) {
    if (!has_arabic(word)) return {word, word, "not-arabic"};
    const std::string normalized = tokenizer::normalize_unicode(word);
    const auto positions = orthography2ipa::underspecification::underdetermined_positions(normalized, *spec_);
    if (author_complete(normalized, positions)) return {word, word, "author"};
    if (auto dialect = dialect_lookup(normalized)) {
        dialect_looked_up.push_back(word);
        return {word, *dialect, "closed-class"};
    }
    if (auto entry = lookup(normalized)) {
        looked_up.push_back(word);
        return {word, *entry, "stem-lexicon"};
    }
    if (proclitic_complete(normalized, positions)) return {word, word, "proclitic"};
    std::string proposed = propose(normalized);
    std::string how = "model";
    if (!skeleton_is_preserved(normalized, proposed)) {
        auto fixed = repair_skeleton(normalized, proposed);
        if (!fixed) {
            rejected.push_back(word);
            return {word, word, "refused"};
        }
        repaired.push_back(word);
        proposed = *fixed;
        how = "model-repaired";
    }
    proposed = nisba::restore_nisba(proposed);
    if (!is_licensed(proposed)) {
        rejected.push_back(word);
        return {word, word, "refused"};
    }
    return {word, proposed, how};
}

DiacritizedText LatticeDiacritizer::diacritize_text(const std::string& text) {
    DiacritizedText out;
    std::string joined;
    std::size_t start = 0;
    for (bool first = true;; first = false) {
        const std::size_t space = text.find(' ', start);
        const std::string w = text.substr(start, space == std::string::npos ? std::string::npos : space - start);
        if (!first) joined += ' ';
        if (all_space(detail::from_utf8(w))) {
            joined += w;
        } else {
            out.words.push_back(diacritize_word_record(w));
            joined += out.words.back().output;
        }
        if (space == std::string::npos) break;
        start = space + 1;
    }
    out.text = std::move(joined);
    return out;
}

DiacritizedText diacritize(const std::string& text, const std::string& lect) {
    static std::mutex mu;
    static std::map<std::string, LatticeDiacritizer> by_lect;
    std::lock_guard<std::mutex> lock(mu);
    auto it = by_lect.find(lect);
    if (it == by_lect.end()) it = by_lect.emplace(lect, LatticeDiacritizer(lect)).first;
    return it->second.diacritize_text(text);
}

DiacritizedText diacritize(const std::string& text, const std::string& lect, const Options& options) {
    return LatticeDiacritizer(lect, options).diacritize_text(text);
}

}  // namespace arbtok::diacritize
