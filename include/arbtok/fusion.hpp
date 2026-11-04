// arbtok.fusion: diacritization as a search over the ensemble's distribution.
// A beam over each letter's top classes gives the n-best readings of a word;
// the readings that tokenize against the lect's grapheme table are ranked by
// their log-probability less a weight times their lattice cost.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

namespace arbtok::fusion {

// _has_arabic_letter(word): a letter (category L*) whose Unicode name starts
// with ARABIC.
bool has_arabic_letter(std::string_view word);

// logprobs(row): the log-softmax of a float32 row, with numpy's float32
// arithmetic: its SIMD exp, its pairwise sum, and the log in double.
std::vector<float> logprobs(const float* row, std::size_t size);

struct Hypothesis {
    std::vector<std::int64_t> class_ids;
    double logprob = 0.0;
    std::string text;
    double lattice_cost = 0.0;

    // repr(hypothesis).
    std::string repr() const;
};

struct Options {
    bool waqf = true;
    // An empty string or std::nullopt runs without a stem lexicon.
    std::optional<std::string> lexicon = lexicon::DEFAULT_LEXICON;
    std::optional<double> ambiguous_share = lexicon::AMBIGUOUS_SHARE;
    bool dialect_lexicon = true;
    // Python slice bounds: a negative value counts from the end.
    long beam = 8;
    long topk = 4;
    double lattice_weight = 0.5;
};

// One lect's fusion diacritizer. KeyError when orthography2ipa has no spec
// for lang. The record lists and last_nbest change as text goes through; an
// instance is not safe to share between threads.
class FusionDiacritizer {
public:
    explicit FusionDiacritizer(const std::string& lang = dialects::DEFAULT_LANG, Options options = {});
    ~FusionDiacritizer();
    FusionDiacritizer(FusionDiacritizer&&) noexcept;
    FusionDiacritizer& operator=(FusionDiacritizer&&) noexcept;

    const std::string& lang() const { return lang_; }
    // The bundled ensemble, loaded on first use.
    const ensemble::EnsembleDiacritizer& diacritizer() const;

    // The text split on " ". When the ensemble's view of the text does not
    // line up word for word with it, a LatticeDiacritizer reads it instead.
    std::string diacritize(const std::string& text);

    std::vector<std::string> recovered;
    std::vector<std::string> looked_up;
    std::vector<std::string> dialect_looked_up;
    std::vector<std::string> rejected;
    // The ranked licensed readings of the last word that reached the beam.
    std::vector<Hypothesis> last_nbest;

private:
    struct Pin;
    bool is_licensed(const std::string& word) const;
    double lattice_cost(const std::string& word) const;
    std::optional<std::string> dialect_lookup(const std::string& word) const;
    std::optional<std::string> lookup(const std::string& word) const;
    std::optional<std::vector<Pin>> pins(const std::u32string& word, const std::u32string& bare_word) const;
    std::vector<Hypothesis> beam_search(const std::u32string& bare_word, const float* logits, std::size_t cols,
                                        const std::optional<std::vector<Pin>>& pins) const;
    std::string generate(const std::string& orig_word, const std::string& normalized, bool recovery = false);
    std::string fuse_word(const std::string& orig_word, const std::u32string& bare_word, const float* logits,
                          std::size_t cols);

    std::string lang_;
    Options options_;
    const orthography2ipa::LanguageSpec* spec_ = nullptr;
    std::unique_ptr<orthography2ipa::PhonetokTokenizer> tokenizer_;
    std::optional<lexicon::StemLexicon> lexicon_;
    std::optional<dialect_lexicon::DialectLexicon> dialect_lexicon_;
    std::optional<long> context_words_;
};

}  // namespace arbtok::fusion
