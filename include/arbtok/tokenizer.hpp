// arbtok.tokenizer: Unicode normalization, the character cascade, and the
// sentence that joins the word lattice, the cascade and the cross-word rules.
//
// A WordToken owns its CharTokens and a Sentence owns its WordTokens. The
// neighbour pointers stay valid while the owner lives, and a copy of the owner
// points into the copy.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace arbtok::tokenizer {

// Tatweel removed, NFC, shadda before the short vowel, and the silent alif of
// the hundred morpheme removed.
std::string normalize_unicode(const std::string& text);

// The unpronounced alif of the hundred morpheme removed.
std::string elide_silent_alif(const std::string& text);

class WordToken;
class Sentence;

// One character of a word: a letter, a diacritic or punctuation.
class CharToken {
public:
    const std::string& surface() const { return surface_; }
    int char_idx() const { return char_idx_; }
    const CharToken* prev_token() const;
    const CharToken* next_token() const;
    const WordToken* word() const { return word_; }

    bool is_first_char() const;
    bool is_first_word() const;
    bool is_last_char() const;
    bool is_last_word() const;
    bool is_punct() const;
    const std::string& lang() const;
    std::string normalized() const;
    bool is_vowel() const;
    bool has_diacritic() const;
    bool has_shada() const;
    bool has_sukun() const;
    bool has_vowel_mark() const;
    bool is_sun() const;
    bool is_moon() const;
    bool is_silent() const;
    bool is_tanwin() const;
    // The IPA fragment of this character in its context.
    std::string ipa() const;

private:
    friend class WordToken;
    using Known = std::unordered_map<const CharToken*, std::string>;

    CharToken(std::string surface, int char_idx) : surface_(std::move(surface)), char_idx_(char_idx) {}
    // The fragment given the fragments already known, or false when the
    // previous token's fragment is needed first.
    bool resolve(Known& known) const;

    std::string surface_;
    int char_idx_;
    const WordToken* word_ = nullptr;
};

// One word of a sentence.
class WordToken {
public:
    WordToken(std::string surface, int word_idx, std::string lang = "ar");
    WordToken(const WordToken& other);
    WordToken(WordToken&& other) noexcept;
    WordToken& operator=(const WordToken& other);
    WordToken& operator=(WordToken&& other) noexcept;

    const std::string& surface() const { return surface_; }
    int word_idx() const { return word_idx_; }
    const std::string& lang() const { return lang_; }
    const WordToken* prev_word() const { return prev_word_; }
    const WordToken* next_word() const { return next_word_; }

    bool is_punct() const;
    bool is_first_word() const;
    bool is_last_word() const;
    bool end_with_vowel() const;
    std::string normalized() const;
    const std::vector<CharToken>& tokens() const { return tokens_; }
    bool has_definite_article() const;
    bool is_proclitic() const;
    bool is_sun() const;
    bool is_reference_register() const;
    // The word's IPA: a word exception, the lattice, or the character cascade.
    std::string ipa() const;

private:
    friend class Sentence;
    void bind();

    std::string surface_;
    int word_idx_;
    std::string lang_;
    const WordToken* prev_word_ = nullptr;
    const WordToken* next_word_ = nullptr;
    std::vector<CharToken> tokens_;
};

// A sentence: its words, and its IPA with the cross-word rules and the stress
// applied.
class Sentence {
public:
    explicit Sentence(std::string surface, std::string lang = "ar", bool stress = true, bool pausal = true);
    Sentence(const Sentence& other);
    Sentence(Sentence&& other) noexcept;
    Sentence& operator=(const Sentence& other);
    Sentence& operator=(Sentence&& other) noexcept;

    const std::string& surface() const { return surface_; }
    const std::string& lang() const { return lang_; }
    bool stress() const { return stress_; }
    bool pausal() const { return pausal_; }
    std::string normalized() const;
    // The words and the punctuation marks, split on whitespace and punctuation.
    const std::vector<WordToken>& tokens() const { return tokens_; }
    std::string ipa() const;

private:
    void link();

    std::string surface_;
    std::string lang_;
    bool stress_;
    bool pausal_;
    std::vector<WordToken> tokens_;
};

namespace detail {
// _reorder_diacritics: a short vowel before a shadda moves after it.
std::string reorder_diacritics(const std::string& text);
}  // namespace detail

}  // namespace arbtok::tokenizer
