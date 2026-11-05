// arbtok.diacritize: the lattice-guarded diacritizer. Each word takes the
// first answer that holds, in this order: the writing itself, the lect's
// closed-class lexicon, the stem lexicon, a bare leading proclitic, and the
// ensemble's proposal, which must keep the word's letters and tokenize
// against the lect's grapheme table. Text is UTF-8 and is compared code point
// by code point.
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "arbtok/dialect_lexicon.hpp"
#include "arbtok/dialects.hpp"
#include "arbtok/lexicon.hpp"

namespace arbtok::ensemble {
class EnsembleDiacritizer;
}  // namespace arbtok::ensemble

namespace orthography2ipa {
struct LanguageSpec;
class PhonetokTokenizer;
}  // namespace orthography2ipa

namespace arbtok::diacritize {

// Where the reading of a word came from, as DiacritizedWord.provenance.
inline const std::vector<std::string> PROVENANCE = {"author", "closed-class", "stem-lexicon", "proclitic",
                                                    "model",  "model-repaired", "refused",    "not-arabic"};

struct DiacritizedWord {
    std::string surface;
    std::string output;
    std::string provenance;

    // The reading is the lect's own: closed-class, author or proclitic.
    bool lect_constrained() const;
    bool is_failure() const { return provenance == "refused"; }
};

struct DiacritizedText {
    std::string text;
    std::vector<DiacritizedWord> words;

    const std::string& str() const { return text; }
    // The surfaces of the words under each provenance, in first-seen order.
    std::vector<std::pair<std::string, std::vector<std::string>>> by_provenance() const;
};

// _author_complete(word, positions): the writing already fixes every
// position that is not in the definite article. Positions are code point
// indices.
bool author_complete(std::string_view word, const std::vector<std::size_t>& positions);

// _proclitic_complete(word, positions): only a bare leading proclitic is
// left open.
bool proclitic_complete(std::string_view word, const std::vector<std::size_t>& positions);

struct Options {
    bool waqf = true;
    // An empty string or std::nullopt runs the model without a stem lexicon.
    std::optional<std::string> lexicon = lexicon::DEFAULT_LEXICON;
    bool dialect_lexicon = true;
    std::optional<std::string> model_path;
    std::optional<std::string> vocab_path;
};

// One lect's diacritizer. KeyError when orthography2ipa has no spec for
// lang. The record lists grow as words go through; an instance is not safe
// to share between threads.
class LatticeDiacritizer {
public:
    explicit LatticeDiacritizer(const std::string& lang = dialects::DEFAULT_LANG, Options options = {});
    ~LatticeDiacritizer();
    LatticeDiacritizer(LatticeDiacritizer&&) noexcept;
    LatticeDiacritizer& operator=(LatticeDiacritizer&&) noexcept;

    const std::string& lang() const { return lang_; }
    bool waqf() const { return options_.waqf; }
    const std::optional<lexicon::StemLexicon>& stem_lexicon() const { return lexicon_; }
    const std::optional<dialect_lexicon::DialectLexicon>& closed_class_lexicon() const { return dialect_lexicon_; }
    // The ensemble of model_path and vocab_path, loaded on first use.
    const ensemble::EnsembleDiacritizer& diacritizer() const;

    std::string diacritize_word(const std::string& word);
    DiacritizedWord diacritize_word_record(const std::string& word);
    std::string diacritize(const std::string& text) { return diacritize_text(text).text; }
    // The text split on " ": each word that is not all whitespace gets a
    // record, and the rest stay as written.
    DiacritizedText diacritize_text(const std::string& text);

    std::vector<std::string> rejected;
    std::vector<std::string> repaired;
    std::vector<std::string> looked_up;
    std::vector<std::string> dialect_looked_up;

private:
    std::string propose(const std::string& word) const;
    std::optional<std::string> lookup(const std::string& word) const;
    std::optional<std::string> dialect_lookup(const std::string& word) const;
    bool is_licensed(const std::string& word) const;

    std::string lang_;
    Options options_;
    const orthography2ipa::LanguageSpec* spec_ = nullptr;
    std::unique_ptr<orthography2ipa::PhonetokTokenizer> tokenizer_;
    std::optional<lexicon::StemLexicon> lexicon_;
    std::optional<dialect_lexicon::DialectLexicon> dialect_lexicon_;
};

// The module function diacritize(text, lect): one cached LatticeDiacritizer
// per lect string, guarded by a mutex.
DiacritizedText diacritize(const std::string& text, const std::string& lect = dialects::DEFAULT_LANG);
// diacritize(text, lect, **kwargs) with options: a fresh diacritizer.
DiacritizedText diacritize(const std::string& text, const std::string& lect, const Options& options);

// strip_marks(text): text without the marks a diacritizer may add (_MARKS).
std::string strip_marks(std::string_view text);

// repair_skeleton(original, proposed): the marks of proposed on the letters
// of original, or std::nullopt where Python returns None.
std::optional<std::string> repair_skeleton(std::string_view original, std::string_view proposed);

// _skeleton_is_preserved(original, diacritized).
bool skeleton_is_preserved(std::string_view original, std::string_view diacritized);

// _has_arabic(word).
bool has_arabic(std::string_view word);

}  // namespace arbtok::diacritize
