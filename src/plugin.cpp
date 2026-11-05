#include "arbtok/plugin.hpp"

#include <orthography2ipa/orthography2ipa.hpp>

#include "arbtok/arabizi.hpp"
#include "arbtok/diacritize.hpp"
#include "arbtok/donor_lexicon.hpp"
#include "arbtok/fusion.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/textnorm.hpp"
#include "arbtok/tokenizer.hpp"
#include "arbtok/translit.hpp"
#include "detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::plugin {

namespace o2i = orthography2ipa;
using arbtok::detail::s8;
using arbtok::detail::u;
using arbtok::detail::U32;

const std::string PUNCT_STRIP = ".,;:!?()[]\"'،؛؟";

namespace {

// The Arabic blocks of _ARABIC_RUN, _SCRIPT_RUN and _MIXED_RUN.
bool is_arabic_block(char32_t c) {
    return (c >= 0x0600 && c <= 0x06FF) || (c >= 0x0750 && c <= 0x077F) || (c >= 0xFB50 && c <= 0xFDFF) ||
           (c >= 0xFE70 && c <= 0xFEFF);
}

bool is_ascii_letter(char32_t c) { return (c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z'); }

// str.split() with no separator: runs of non-whitespace.
std::vector<std::string> split_whitespace(const std::string& text) {
    std::vector<std::string> out;
    U32 current;
    for (char32_t c : u(text)) {
        if (arbtok::detail::is_space(c)) {
            if (!current.empty()) out.push_back(s8(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) out.push_back(s8(current));
    return out;
}

// _SCRIPT_RUN.findall: an Arabic run, or a run of what is neither Arabic nor
// whitespace.
std::vector<std::string> script_runs(const std::string& token) {
    const U32 s = u(token);
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        if (arbtok::detail::is_space(s[i])) {
            ++i;
            continue;
        }
        const bool arabic = is_arabic_block(s[i]);
        std::size_t j = i;
        while (j < s.size() && !arbtok::detail::is_space(s[j]) && is_arabic_block(s[j]) == arabic) ++j;
        out.push_back(s8(s.substr(i, j - i)));
        i = j;
    }
    return out;
}

// _MIXED_RUN.finditer: an Arabic run, a Latin word with its apostrophes and
// hyphens, or a run of what is none of these and not whitespace.
std::vector<std::string> mixed_runs(const std::string& run) {
    const U32 s = u(run);
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const char32_t c = s[i];
        std::size_t j = i + 1;
        if (is_arabic_block(c)) {
            while (j < s.size() && is_arabic_block(s[j])) ++j;
        } else if (is_ascii_letter(c)) {
            while (j < s.size() && (is_ascii_letter(s[j]) || s[j] == U'\'' || s[j] == U'’' || s[j] == U'-')) ++j;
        } else if (!arbtok::detail::is_space(c)) {
            while (j < s.size() && !arbtok::detail::is_space(s[j]) && !is_arabic_block(s[j]) &&
                   !is_ascii_letter(s[j]))
                ++j;
        } else {
            ++i;
            continue;
        }
        out.push_back(s8(s.substr(i, j - i)));
        i = j;
    }
    return out;
}

bool has_arabic_block(const std::string& s) {
    for (char32_t c : u(s))
        if (is_arabic_block(c)) return true;
    return false;
}

// tok.isascii() and tok.isalpha() and tok.isupper() and len(tok) > 1: an
// all-caps ASCII word.
bool is_ascii_caps_word(const std::string& token) {
    if (token.size() < 2) return false;
    for (char c : token)
        if (c < 'A' || c > 'Z') return false;
    return true;
}

// _spelled_out(tok): an all-caps word the donor does not know is a spelling.
bool spelled_out(const std::string& token) { return is_ascii_caps_word(token) && !detail::donor_knows(token); }

// str.strip(chars) with PUNCT_STRIP.
std::string strip_punct(const std::string& text) {
    return s8(arbtok::detail::strip(u(text), u(PUNCT_STRIP)));
}

}  // namespace

namespace detail {

bool donor_knows(const std::string& token) {
    try {
        donor_lexicon::ensure_registered(translit::DONOR_LANG);
        const std::string lowered = s8(arbtok::detail::lower(u(token)));
        const std::string got = o2i::G2P(translit::DONOR_LANG).transcribe_word(lowered);
        // Stress off, and the IPA symbols that only look like their ASCII
        // letter folded back to it.
        U32 bare;
        for (char32_t c : u(got)) {
            if (c == U'ˈ' || c == U'ˌ') continue;
            switch (c) {
                case U'ɡ': bare += U'g'; break;
                case U'ɹ': bare += U'r'; break;
                case U'ɪ': bare += U'i'; break;
                case U'ʊ': bare += U'u'; break;
                default: bare += c;
            }
        }
        return !got.empty() && s8(bare) != lowered;
    } catch (const NotPortedError&) {
        throw;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> runs(const std::string& token) {
    std::vector<std::string> out;
    for (const auto& run : script_runs(token)) {
        if (has_arabic_block(run)) {
            out.push_back(run);
        } else if (arabizi::is_arabizi(run) && arabizi::arabizi_covers(run)) {
            out.push_back(run);  // Arabizi is never split
        } else {
            auto pieces = mixed_runs(run);
            if (pieces.empty()) pieces = {run};
            for (const auto& piece : pieces) {
                if (spelled_out(piece)) {
                    for (char32_t c : u(piece)) out.push_back(s8(c));
                } else {
                    out.push_back(piece);
                }
            }
        }
    }
    if (out.empty()) out.push_back(token);
    return out;
}

}  // namespace detail

// The diacritizer that normalize restores the marks with: fusion or lattice.
struct ArbtokG2PPlugin::Diacritizer {
    std::unique_ptr<fusion::FusionDiacritizer> fusion;
    std::unique_ptr<::arbtok::diacritize::LatticeDiacritizer> lattice;
    std::string diacritize(const std::string& text) {
        return fusion ? fusion->diacritize(text) : lattice->diacritize(text);
    }
};

ArbtokG2PPlugin::ArbtokG2PPlugin(Options options)
    : lang(dialects::spec_for_lang(options.lang)),
      fusion(options.fusion),
      diacritize(options.diacritize),
      stress(options.stress),
      lexicon(std::move(options.lexicon)),
      dialect_lexicon(options.dialect_lexicon),
      nativize(options.nativize),
      arabizi(options.arabizi),
      pausal(true) {
    if (options.register_name != "pausal" && options.register_name != "full")
        throw ValueError("register must be 'pausal' or 'full', got " +
                         arbtok::detail::py_str_repr(u(options.register_name)));
    pausal = options.pausal ? *options.pausal : options.register_name == "pausal";
}

ArbtokG2PPlugin::~ArbtokG2PPlugin() = default;
ArbtokG2PPlugin::ArbtokG2PPlugin(ArbtokG2PPlugin&&) noexcept = default;
ArbtokG2PPlugin& ArbtokG2PPlugin::operator=(ArbtokG2PPlugin&&) noexcept = default;

std::vector<std::string> ArbtokG2PPlugin::language_codes() const { return {"ar", "arb"}; }

std::string ArbtokG2PPlugin::resolve_lang(const WordContext* context) const {
    if (context != nullptr && context->lang && !context->lang->empty()) {
        const std::string code = dialects::spec_for_lang(context->lang);
        if (code != dialects::DEFAULT_LANG) return code;
    }
    return lang;
}

std::string ArbtokG2PPlugin::normalize(const std::string& text) {
    std::string out = textnorm::normalize_for_tts(text, "ar");
    if (diacritize) out = restore_marks(out);
    return out;
}

std::string ArbtokG2PPlugin::restore_marks(const std::string& text) {
    // A model that cannot load, or a call that fails, is a degraded mode: the
    // text passes through. NotPortedError is not a Python exception and stays
    // visible.
    if (diacritizer_failed_) return text;
    if (!diacritizer_) {
        try {
            auto d = std::make_unique<Diacritizer>();
            if (fusion) {
                fusion::Options o;
                o.waqf = pausal;
                o.lexicon = lexicon;
                o.dialect_lexicon = dialect_lexicon;
                d->fusion = std::make_unique<fusion::FusionDiacritizer>(lang, o);
            } else {
                ::arbtok::diacritize::Options o;
                o.waqf = pausal;
                o.lexicon = lexicon;
                o.dialect_lexicon = dialect_lexicon;
                d->lattice = std::make_unique<::arbtok::diacritize::LatticeDiacritizer>(lang, o);
            }
            diacritizer_ = std::move(d);
        } catch (const NotPortedError&) {
            throw;
        } catch (const std::exception&) {
            diacritizer_failed_ = true;
            return text;
        }
    }
    try {
        return diacritizer_->diacritize(text);
    } catch (const NotPortedError&) {
        throw;
    } catch (const std::exception&) {
        return text;
    }
}

std::string ArbtokG2PPlugin::transcribe(const std::string& text, std::optional<bool> arabizi_hint) {
    std::vector<std::string> parts, buffer;
    const auto tokens = split_whitespace(text);

    bool arabizi_mode = false;
    if (!arabizi) {
        arabizi_mode = false;
    } else if (arabizi_hint) {
        arabizi_mode = *arabizi_hint;
    } else {
        // Decided over the runs of each token, so that a glued الـ7abibi counts.
        for (const auto& t : tokens) {
            for (const auto& r : script_runs(t))
                if (arabizi::is_arabizi(r) && arabizi::arabizi_covers(r)) arabizi_mode = true;
            if (arabizi_mode) break;
        }
    }

    auto join = [](const std::vector<std::string>& items) {
        std::string out;
        for (std::size_t i = 0; i < items.size(); ++i) out += (i ? " " : "") + items[i];
        return out;
    };
    auto flush_arabic = [&] {
        if (buffer.empty()) return;
        parts.push_back(tokenizer::Sentence(normalize(join(buffer)), lang, stress, pausal).ipa());
        buffer.clear();
    };

    std::vector<std::string> pieces;
    for (const auto& t : tokens)
        for (auto& r : detail::runs(t)) pieces.push_back(std::move(r));
    for (const auto& token : pieces) {
        if (translit::is_latin(token)) {
            if (arabizi_mode) {
                // Arabic in Latin letters: a skeleton for the Arabic pipeline.
                buffer.push_back(arabizi::to_arabic_skeleton(token));
                continue;
            }
            flush_arabic();
            if (!nativize) {
                parts.push_back(token);
                continue;
            }
            const auto guest = translit::transliterate(strip_punct(token), lang);
            if (guest && !guest->empty()) parts.push_back(*guest);
        } else {
            buffer.push_back(token);
        }
    }
    flush_arabic();
    std::vector<std::string> kept;
    for (const auto& p : parts)
        if (!p.empty()) kept.push_back(p);
    return join(kept);
}

std::string ArbtokG2PPlugin::transcribe_word(const std::string& word, const WordContext* context) {
    const std::string code = resolve_lang(context);
    if (translit::is_latin(word)) {
        // A Latin word has no Arabic graphemes: read it as a loanword, or,
        // with nativize off, give it back untouched.
        if (!nativize) return word;
        const auto guest = translit::transliterate(word, code);
        return guest ? *guest : "";
    }
    if (context == nullptr || (!context->prev_word && !context->next_word)) {
        // An isolated word runs on the lattice unless it is a word exception
        // or needs the cascade.
        if (!dialects::find(dialects::WORD_EXCEPTIONS, tokenizer::normalize_unicode(word)) &&
            !lattice::defers_to_cascade(word))
            return lattice::word_ipa(word, code, stress);
        return tokenizer::Sentence(word, code, stress, pausal).ipa();
    }
    // The word with its neighbours, so that the cross-word rules see them.
    std::vector<std::string> parts;
    if (context->prev_word) parts.push_back(*context->prev_word);
    parts.push_back(word);
    if (context->next_word) parts.push_back(*context->next_word);
    std::string joined;
    for (std::size_t i = 0; i < parts.size(); ++i) joined += (i ? " " : "") + parts[i];
    const std::size_t target = context->prev_word ? 1 : 0;
    const tokenizer::Sentence sentence(joined, code, stress, pausal);
    std::vector<const tokenizer::WordToken*> words;
    for (const auto& t : sentence.tokens())
        if (!t.surface().empty()) words.push_back(&t);
    if (target < words.size()) return words[target]->ipa();
    return tokenizer::Sentence(word, code, stress, pausal).ipa();
}

}  // namespace arbtok::plugin
