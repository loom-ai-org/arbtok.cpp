// arbtok.plugin: the Arabic G2P engine on the orthography2ipa lattice. This
// is the class that the phoonnx arbtok phonemizer (scriptconv
// ArbtokPhonemizer) calls: ArbtokG2PPlugin(lang=..., diacritize=True)
// .transcribe(text).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <orthography2ipa/sentence.hpp>

#include "arbtok/dialects.hpp"
#include "arbtok/lexicon.hpp"

namespace arbtok::plugin {

using WordContext = orthography2ipa::sentence::WordContext;

// The characters that transcribe strips from the ends of a foreign word.
extern const std::string PUNCT_STRIP;

// The constructor keywords of ArbtokG2PPlugin, with their Python defaults.
// register_name is the keyword `register`.
struct Options {
    // Any tag; spec_for_lang resolves it, and std::nullopt gives DEFAULT_LANG.
    std::optional<std::string> lang = dialects::DEFAULT_LANG;
    bool diacritize = true;
    bool stress = true;
    std::optional<std::string> lexicon = lexicon::DEFAULT_LEXICON;
    bool dialect_lexicon = true;
    bool nativize = true;
    bool arabizi = true;
    std::optional<bool> pausal;
    std::string register_name = "pausal";
    bool fusion = true;
};

// Arabic G2P: Modern Standard Arabic unless lang names a variety. ValueError
// when register_name is neither "pausal" nor "full". An instance keeps its
// diacritizer, which is not safe to share between threads.
class ArbtokG2PPlugin {
public:
    explicit ArbtokG2PPlugin(Options options = {});
    ~ArbtokG2PPlugin();
    ArbtokG2PPlugin(ArbtokG2PPlugin&&) noexcept;
    ArbtokG2PPlugin& operator=(ArbtokG2PPlugin&&) noexcept;

    // ["ar", "arb"].
    std::vector<std::string> language_codes() const;

    // normalize_for_tts under "ar", then, with diacritize, the marks restored
    // by the fusion or the lattice diacritizer. A diacritizer that cannot load
    // or fails leaves the text as it is.
    std::string normalize(const std::string& text);

    // The text with its Arabic runs transcribed together and each Latin word
    // read as Arabizi or as a loanword. arabizi forces (true) or forbids
    // (false) the Arabizi reading; std::nullopt lets the digit gutturals
    // decide.
    std::string transcribe(const std::string& text, std::optional<bool> arabizi = std::nullopt);

    // One word, alone or with the orthographic neighbours of context.
    std::string transcribe_word(const std::string& word, const WordContext* context = nullptr);

    // The resolved spec code.
    std::string lang;
    bool fusion;
    bool diacritize;
    bool stress;
    std::optional<std::string> lexicon;
    bool dialect_lexicon;
    bool nativize;
    bool arabizi;
    bool pausal;

private:
    struct Diacritizer;
    std::string resolve_lang(const WordContext* context) const;
    std::string restore_marks(const std::string& text);
    std::unique_ptr<Diacritizer> diacritizer_;
    bool diacritizer_failed_ = false;
};

namespace detail {

// _donor_knows(tok): the donor G2P gives a reading that is not the letters of
// tok again.
bool donor_knows(const std::string& token);
// The runs of one whitespace token that take different paths.
std::vector<std::string> runs(const std::string& token);

}  // namespace detail

}  // namespace arbtok::plugin
