#include "arbtok/tokenizer.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include <orthography2ipa/orthography2ipa.hpp>
#include <orthography2ipa/stress.hpp>
#include <orthography2ipa/vowels.hpp>

#include "arbtok/constants.hpp"
#include "arbtok/dialects.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/sandhi.hpp"
#include "arbtok/stress.hpp"
#include "detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::tokenizer {

namespace c = arbtok::constants;
namespace d = arbtok::detail;
namespace dl = arbtok::dialects;
namespace o2i = orthography2ipa;
using d::U32;
using d::U32View;

namespace {

constexpr char32_t kFatha = 0x064E, kDamma = 0x064F, kKasra = 0x0650, kShadda = 0x0651;
constexpr char32_t kMeem = 0x0645, kAlif = 0x0627, kYaHamza = 0x0626, kTaMarbuta = 0x0629, kTeh = 0x062A,
                   kYa = 0x064A;

bool in_punct(const std::string& s) { return c::PUNCT.find(s) != std::string::npos; }

// str.strip() with no argument: whitespace at both ends.
std::string strip_space(const std::string& s) {
    const U32 cps = d::u(s);
    size_t a = 0, b = cps.size();
    while (a < b && d::is_space(cps[a])) ++a;
    while (b > a && d::is_space(cps[b - 1])) --b;
    return d::s8(U32View(cps).substr(a, b - a));
}

std::string replace(const std::string& s, const std::string& from, const std::string& to) {
    return d::s8(d::replace(d::u(s), d::u(from), d::u(to)));
}

struct IndexError : Error {
    explicit IndexError(const std::string& message) : Error("IndexError", message) {}
};

}  // namespace

namespace detail {

std::string reorder_diacritics(const std::string& text) {
    U32 chars = d::u(text);
    size_t i = 0;
    while (i + 1 < chars.size()) {
        if ((chars[i] == kFatha || chars[i] == kDamma || chars[i] == kKasra) && chars[i + 1] == kShadda) {
            std::swap(chars[i], chars[i + 1]);
            ++i;
        }
        ++i;
    }
    return d::s8(chars);
}

}  // namespace detail

// _SILENT_ALIF_MIA, replaced by M\1\2: meem, an optional kasra, alif, then ya
// with hamza, an optional fatha, and ta marbuta or a teh that a long vowel
// follows past any of U+064E..U+0652.
std::string elide_silent_alif(const std::string& text_utf8) {
    const U32 text = d::u(text_utf8);
    U32 out;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = i;
        bool matched = false;
        if (text[j] == kMeem) {
            ++j;
            const bool kasra = j < text.size() && text[j] == kKasra;
            if (kasra) ++j;
            if (j < text.size() && text[j] == kAlif && j + 1 < text.size() && text[j + 1] == kYaHamza) {
                size_t k = j + 2;
                if (k < text.size() && text[k] == kFatha) ++k;
                bool tail = false;
                if (k < text.size() && text[k] == kTaMarbuta) {
                    tail = true;
                    ++k;
                } else if (k < text.size() && text[k] == kTeh) {
                    size_t look = k + 1;
                    while (look < text.size() && text[look] >= 0x064E && text[look] <= 0x0652) ++look;
                    if (look < text.size() && (text[look] == kAlif || text[look] == kYa)) {
                        tail = true;
                        ++k;
                    }
                }
                if (tail) {
                    out.push_back(kMeem);
                    if (kasra) out.push_back(kKasra);
                    out.append(text, j + 1, k - (j + 1));
                    i = k;
                    matched = true;
                }
            }
        }
        if (!matched) out.push_back(text[i++]);
    }
    return d::s8(out);
}

std::string normalize_unicode(const std::string& text) {
    const U32 no_tatweel = d::replace(d::u(text), U"ـ", U"");
    return elide_silent_alif(detail::reorder_diacritics(d::s8(d::nfc(no_tatweel))));
}

// ── CharToken ───────────────────────────────────────────────────────────────

const CharToken* CharToken::prev_token() const {
    return char_idx_ > 0 ? &word_->tokens()[char_idx_ - 1] : nullptr;
}

const CharToken* CharToken::next_token() const {
    return static_cast<size_t>(char_idx_) + 1 < word_->tokens().size() ? &word_->tokens()[char_idx_ + 1] : nullptr;
}

bool CharToken::is_first_char() const { return prev_token() == nullptr; }
bool CharToken::is_first_word() const { return word_->prev_word() == nullptr && is_first_char(); }
bool CharToken::is_last_char() const { return next_token() == nullptr; }
bool CharToken::is_last_word() const { return word_->is_last_word() && is_last_char(); }
bool CharToken::is_punct() const { return in_punct(surface_); }
const std::string& CharToken::lang() const { return word_->lang(); }
std::string CharToken::normalized() const { return normalize_unicode(surface_); }
bool CharToken::is_vowel() const { return dl::find(dl::VOWEL_MAP, surface_) != nullptr; }
bool CharToken::has_diacritic() const { return has_shada() || has_sukun() || has_vowel_mark(); }

bool CharToken::has_shada() const {
    const CharToken* n = next_token();
    return n != nullptr && n->surface_ == c::SHADDA;
}

bool CharToken::has_sukun() const {
    const CharToken* n = next_token();
    if (n == nullptr) return false;
    if (n->surface_ == c::SUKUN) return true;
    const CharToken* nn = n->next_token();
    return nn != nullptr && nn->surface_ == c::SUKUN;
}

bool CharToken::has_vowel_mark() const {
    const CharToken* n = next_token();
    return n != nullptr && dl::find(dl::DIACRITIC_TO_IPA, n->surface_) != nullptr;
}

bool CharToken::is_sun() const { return c::SUN_LETTERS.count(surface_) > 0; }
bool CharToken::is_moon() const { return !is_sun(); }
bool CharToken::is_silent() const { return has_sukun() || ipa().empty(); }
bool CharToken::is_tanwin() const { return dl::find(dl::TANWIN_TO_IPA, surface_) != nullptr; }

std::string CharToken::ipa() const {
    Known known;
    std::vector<const CharToken*> pending{this};
    while (!pending.empty()) {
        const CharToken* tok = pending.back();
        if (tok->resolve(known))
            pending.pop_back();
        else
            pending.push_back(tok->prev_token());
    }
    return known.at(this);
}

namespace {

using Known = std::unordered_map<const CharToken*, std::string>;

// The previous token's fragment, or nullptr when it is not known yet.
const std::string* prev_ipa(const CharToken& tok, const Known& known) {
    auto it = known.find(tok.prev_token());
    return it == known.end() ? nullptr : &it->second;
}

// The two characters of the definite article; nullopt past them.
std::optional<std::string> definite_article_ipa(const CharToken& tok) {
    if (tok.is_first_char()) {
        const WordToken* prev = tok.word()->prev_word();
        if (prev != nullptr && prev->is_proclitic()) return "";
        if (prev != nullptr && prev->end_with_vowel()) return "";
        return "a";
    }
    if (tok.char_idx() == 1) {
        if (tok.next_token() != nullptr && tok.next_token()->is_moon()) return "l";
        return "";
    }
    return std::nullopt;
}

// A fragment, or nullopt when the previous fragment is needed first.
using Fragment = std::optional<std::string>;
const Fragment kNeedPrev = std::nullopt;

Fragment alif_ipa(const CharToken& tok, const Known& known) {
    const std::string& s = tok.surface();
    if (tok.is_first_word() && tok.is_first_char() && s == c::ALIF) return "i";
    if (tok.prev_token() != nullptr) {
        const std::string* prev = prev_ipa(tok, known);
        if (prev == nullptr) return kNeedPrev;
        if (*prev == "a") return "ː";
    }
    if (!tok.is_first_char() && tok.prev_token() != nullptr) {
        const std::string* prev = prev_ipa(tok, known);
        if (prev == nullptr) return kNeedPrev;
        if ((*prev == "i" || *prev == "u") && s == c::ALIF) return "";
    }
    if (tok.is_last_char()) return "";
    if (s == c::ALEF_MADDA) return "ʔaː";
    if (tok.is_first_char()) return "";
    return "aː";
}

Fragment waw_ipa(const CharToken& tok, const Known& known) {
    if (tok.is_first_char() && tok.next_token() != nullptr && tok.next_token()->surface() == c::ALIF) {
        const WordToken* prev = tok.word()->prev_word();
        if (prev != nullptr) {
            if (prev->tokens().empty()) throw IndexError("list index out of range");
            if (d::starts_with(d::u(prev->tokens().back().ipa()), U"a")) return "w";
        }
        return "aw";
    }
    if (tok.has_shada()) return "w";
    if (tok.prev_token() != nullptr) {
        const std::string* prev = prev_ipa(tok, known);
        if (prev == nullptr) return kNeedPrev;
        if (d::ends_with(d::u(*prev), U"u")) return "ː";
    }
    return "w";
}

Fragment ya_ipa(const CharToken& tok, const Known& known) {
    if (tok.has_shada()) return "j";
    if (tok.prev_token() != nullptr) {
        const std::string* prev = prev_ipa(tok, known);
        if (prev == nullptr) return kNeedPrev;
        if (d::ends_with(d::u(*prev), U"i")) return "ː";
    }
    return "j";
}

std::string consonant_letter_ipa(const CharToken& tok) {
    const std::string& s = tok.surface();
    const std::string cons = dl::consonant_ipa(s, tok.lang(), *dl::find(dl::ARABIC_TO_IPA_CONSONANTS, s));
    if (tok.char_idx() == 2 && tok.word()->has_definite_article() && tok.is_sun()) return cons + cons;
    const CharToken* prev = tok.prev_token();
    if (s == c::LAM && prev != nullptr && prev->surface() == c::KASRA && prev->prev_token() != nullptr &&
        prev->prev_token()->surface() == c::LAM) {
        const CharToken* nxt = tok.next_token();
        while (nxt != nullptr && (nxt->surface() == c::FATHA || nxt->surface() == c::DAMMA ||
                                  nxt->surface() == c::KASRA || nxt->surface() == c::SUKUN ||
                                  nxt->surface() == c::TANWIN_FATH || nxt->surface() == c::TANWIN_DAMM ||
                                  nxt->surface() == c::TANWIN_KASR))
            nxt = nxt->next_token();
        if (nxt != nullptr && nxt->is_sun() && nxt->has_shada()) return "";
        if (nxt != nullptr && nxt->is_moon()) return "l";
    }
    return cons;
}

Fragment shadda_ipa(const CharToken& tok, const Known& known) {
    if (tok.char_idx() == 3 && tok.word()->has_definite_article() && tok.prev_token()->is_sun()) return "";
    if (tok.prev_token() != nullptr) {
        const std::string* prev = prev_ipa(tok, known);
        if (prev == nullptr) return kNeedPrev;
        if (d::ends_with(d::u(*prev), U"ː")) return "";
        return *prev;
    }
    return "";
}

// s.isascii() and (s.isalnum() or s in string.punctuation or s.isspace())
bool ascii_passthrough(const std::string& s) {
    for (unsigned char ch : s)
        if (ch >= 0x80) return false;
    const U32 cps = d::u(s);
    const bool alnum = !cps.empty() && std::all_of(cps.begin(), cps.end(), d::is_alnum);
    const std::string punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    return alnum || punctuation.find(s) != std::string::npos || d::all_space(cps);
}

Fragment char_ipa(const CharToken& tok, const Known& known) {
    const std::string& s = tok.surface();
    if (tok.is_punct()) return s;
    if (tok.word()->has_definite_article()) {
        if (auto article = definite_article_ipa(tok)) return article;
    }
    if (s == c::HAMZAT_AL_WASL) {
        if (tok.is_first_char() && tok.is_first_word()) return "ʔ";
        return "";
    }
    if (s == c::ALIF || s == c::ALEF_MADDA || s == c::ALEF_HAMZA_BELOW) return alif_ipa(tok, known);
    if (s == c::TA_MARBUTA) {
        if (tok.next_token() != nullptr && tok.next_token()->is_vowel()) return "t";
        return "";
    }
    if (s == c::WAW) return waw_ipa(tok, known);
    if (s == c::YA) return ya_ipa(tok, known);
    if (s == c::ALIF_MAKSURA) {
        if (tok.next_token() != nullptr && dl::find(dl::VOWEL_MAP, tok.next_token()->surface())) return "a";
        if (tok.prev_token() != nullptr && tok.prev_token()->surface() == c::FATHA) return "ː";
        return "aː";
    }
    if (s == c::KASRA && tok.prev_token() != nullptr) {
        const std::string* prev = prev_ipa(tok, known);
        if (prev == nullptr) return kNeedPrev;
        if (*prev == "i") return "";
    }
    if (const std::string* vowel = dl::find(dl::VOWEL_MAP, s)) return *vowel;
    if (dl::find(dl::ARABIC_TO_IPA_CONSONANTS, s)) return consonant_letter_ipa(tok);
    if (s == c::SHADDA) return shadda_ipa(tok, known);
    if (ascii_passthrough(s)) return s;
    return "";
}

}  // namespace

bool CharToken::resolve(Known& known) const {
    Fragment fragment = char_ipa(*this, known);
    if (!fragment) return false;
    known[this] = std::move(*fragment);
    return true;
}

// ── WordToken ───────────────────────────────────────────────────────────────

WordToken::WordToken(std::string surface, int word_idx, std::string lang)
    : surface_(std::move(surface)), word_idx_(word_idx), lang_(std::move(lang)) {
    const U32 chars = d::u(normalize_unicode(surface_));
    for (size_t i = 0; i < chars.size(); ++i) tokens_.push_back(CharToken(d::s8(chars[i]), static_cast<int>(i)));
    bind();
}

WordToken::WordToken(const WordToken& other)
    : surface_(other.surface_), word_idx_(other.word_idx_), lang_(other.lang_), prev_word_(other.prev_word_),
      next_word_(other.next_word_), tokens_(other.tokens_) {
    bind();
}

WordToken::WordToken(WordToken&& other) noexcept
    : surface_(std::move(other.surface_)), word_idx_(other.word_idx_), lang_(std::move(other.lang_)),
      prev_word_(other.prev_word_), next_word_(other.next_word_), tokens_(std::move(other.tokens_)) {
    bind();
}

WordToken& WordToken::operator=(const WordToken& other) {
    WordToken copy(other);
    *this = std::move(copy);
    return *this;
}

WordToken& WordToken::operator=(WordToken&& other) noexcept {
    surface_ = std::move(other.surface_);
    word_idx_ = other.word_idx_;
    lang_ = std::move(other.lang_);
    prev_word_ = other.prev_word_;
    next_word_ = other.next_word_;
    tokens_ = std::move(other.tokens_);
    bind();
    return *this;
}

void WordToken::bind() {
    for (auto& t : tokens_) t.word_ = this;
}

bool WordToken::is_punct() const { return in_punct(surface_); }
bool WordToken::is_first_word() const { return prev_word_ == nullptr; }

bool WordToken::is_last_word() const {
    return next_word_ == nullptr || (next_word_->is_punct() && next_word_->next_word_ == nullptr);
}

bool WordToken::end_with_vowel() const {
    if (tokens_.empty()) throw IndexError("list index out of range");
    static const std::vector<std::string> endings = {"a", "u", "i", "aː", "uː", "iː",
                                                     "ː", "an", "un", "in"};
    const std::string last = tokens_.back().ipa();
    return std::find(endings.begin(), endings.end(), last) != endings.end();
}

std::string WordToken::normalized() const { return normalize_unicode(surface_); }

bool WordToken::has_definite_article() const {
    return tokens_.size() >= 2 && tokens_[0].surface() == c::ALIF && tokens_[1].surface() == c::LAM;
}

bool WordToken::is_proclitic() const {
    const U32 s = d::u(normalized());
    if (is_punct()) return false;
    if (s.size() > 2) return false;
    if (s.empty()) throw IndexError("string index out of range");
    return c::CLITIC_BASES.count(d::s8(s[0])) > 0;
}

bool WordToken::is_sun() const {
    if (has_definite_article() && tokens_.size() > 2) return tokens_[2].is_sun();
    if (!tokens_.empty()) return tokens_[0].is_sun();
    return false;
}

bool WordToken::is_reference_register() const { return lang_ == "ar" || lang_ == "arb"; }

std::string WordToken::ipa() const {
    if (is_reference_register()) {
        if (const std::string* hit = dl::find(dl::WORD_EXCEPTIONS, surface_)) return *hit;
    }
    if (!lattice::defers_to_cascade(surface_)) {
        const std::string ipa = lattice::word_ipa(surface_, lang_, false);
        if (!ipa.empty()) return replace(ipa, "idʒt", "ijt");
    }
    std::string ipa;
    for (const auto& t : tokens_) ipa += t.ipa();
    return strip_space(replace(ipa, "idʒt", "ijt"));
}

// ── Sentence ────────────────────────────────────────────────────────────────

Sentence::Sentence(std::string surface, std::string lang, bool stress, bool pausal)
    : surface_(std::move(surface)), lang_(std::move(lang)), stress_(stress), pausal_(pausal) {
    // re.split(r"(\s+|[<PUNCT>])", normalized), keeping the non-empty pieces
    // that are not whitespace.
    const U32 text = d::u(normalized());
    const U32 punct = d::u(c::PUNCT);
    std::vector<U32> pieces;
    U32 current;
    auto flush = [&] {
        if (!current.empty()) pieces.push_back(current);
        current.clear();
    };
    for (char32_t ch : text) {
        if (d::is_space(ch)) {
            flush();
        } else if (punct.find(ch) != U32::npos) {
            flush();
            pieces.push_back(U32(1, ch));
        } else {
            current.push_back(ch);
        }
    }
    flush();
    for (size_t i = 0; i < pieces.size(); ++i) tokens_.emplace_back(d::s8(pieces[i]), static_cast<int>(i), lang_);
    link();
}

Sentence::Sentence(const Sentence& other)
    : surface_(other.surface_), lang_(other.lang_), stress_(other.stress_), pausal_(other.pausal_),
      tokens_(other.tokens_) {
    link();
}

Sentence::Sentence(Sentence&& other) noexcept
    : surface_(std::move(other.surface_)), lang_(std::move(other.lang_)), stress_(other.stress_),
      pausal_(other.pausal_), tokens_(std::move(other.tokens_)) {
    link();
}

Sentence& Sentence::operator=(const Sentence& other) {
    Sentence copy(other);
    *this = std::move(copy);
    return *this;
}

Sentence& Sentence::operator=(Sentence&& other) noexcept {
    surface_ = std::move(other.surface_);
    lang_ = std::move(other.lang_);
    stress_ = other.stress_;
    pausal_ = other.pausal_;
    tokens_ = std::move(other.tokens_);
    link();
    return *this;
}

void Sentence::link() {
    for (size_t i = 0; i < tokens_.size(); ++i) {
        tokens_[i].prev_word_ = i > 0 ? &tokens_[i - 1] : nullptr;
        tokens_[i].next_word_ = i + 1 < tokens_.size() ? &tokens_[i + 1] : nullptr;
        tokens_[i].bind();
    }
}

std::string Sentence::normalized() const { return normalize_unicode(surface_); }

std::string Sentence::ipa() const {
    std::vector<sandhi::CrossWord> words;
    for (const auto& w : tokens_) words.push_back({w.ipa(), w.surface(), w.is_punct()});
    const std::vector<std::string> pieces = sandhi::apply_cross_word(words, pausal_, lang_);

    std::vector<const WordToken*> spoken;
    for (const auto& w : tokens_)
        if (!w.is_punct()) spoken.push_back(&w);
    std::vector<int> onsets;
    for (size_t i = 0; i < spoken.size() && i < pieces.size(); ++i) {
        const U32 p = d::u(pieces[i]);
        const bool held = sandhi::detail::has_article(spoken[i]->surface()) && !p.empty() &&
                          !o2i::vowels::is_ipa_vowel(p[0]);
        onsets.push_back(held ? stress::first_segment_len(pieces[i]) : 0);
    }

    std::string joined;
    for (size_t i = 0; i < pieces.size(); ++i) joined += (i ? " " : "") + pieces[i];
    const std::string hacked = strip_space(replace(joined, " ː", "ː "));
    if (!stress_) return hacked;

    const std::vector<U32> parts = d::split(d::u(hacked), U" ");
    if (parts.size() == onsets.size()) {
        const o2i::LanguageSpec& spec = o2i::get(lang_);
        std::vector<U32> out;
        for (size_t i = 0; i < parts.size(); ++i) {
            const std::string w = d::s8(parts[i]);
            out.push_back(o2i::stress::is_cliticless(spoken[i]->surface(), spec)
                              ? parts[i]
                              : d::u(stress::stress_ipa(w, lang_, onsets[i])));
        }
        return d::s8(d::join(out, U" "));
    }
    return stress::stress_words(hacked, lang_);
}

}  // namespace arbtok::tokenizer
