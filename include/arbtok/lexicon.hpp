// arbtok.lexicon: the diacritized-stem lexicon that the diacritizer consults
// before the model. Nothing is bundled: the default lexicon is a Hugging Face
// dataset, read from the Hugging Face cache and fetched with curl when the
// cache lacks it and HF_HUB_OFFLINE is not set.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "arbtok/errors.hpp"
#include "arbtok/ordered_dict.hpp"

namespace arbtok::lexicon {

// A lexicon was asked for and could not be read (a RuntimeError in Python).
struct LexiconUnavailable : Error {
    explicit LexiconUnavailable(const std::string& message) : Error("LexiconUnavailable", message) {}
};

// The lexicon arbtok reaches for when the caller names none.
inline const std::string DEFAULT_LEXICON = "hf://TigreGotico/arabic-stem-lexicon/ar-stems.tsv";

// CACHE_DIR: $XDG_CACHE_HOME/arbtok/lexicons, or ~/.cache/arbtok/lexicons.
// Python reads the environment once at import; this reads it at each call.
std::string cache_dir();

// A path, an http(s):// URL or an hf://org/name/file id as a local file. A
// URL is fetched into cache_dir() under its last path segment unless that
// file exists. An hf:// id is the file of the dataset's main revision in the
// Hugging Face cache (HF_HUB_CACHE, HUGGINGFACE_HUB_CACHE, HF_HOME/hub or
// $XDG_CACHE_HOME/huggingface/hub, else ~/.cache/huggingface/hub); with
// HF_HUB_OFFLINE set, a file missing from the cache raises
// LocalEntryNotFoundError, else it is downloaded into the cache. Unlike
// huggingface_hub, a cached file is used without asking the Hub for a newer
// revision.
std::string resolve_source(const std::string& source);

// The runner-up share at which a spelling counts as ambiguous.
inline constexpr double AMBIGUOUS_SHARE = 0.10;

// The shortest sentence in which an ambiguous spelling is left to the model.
inline constexpr long MIN_CONTEXT_WORDS = 3;

// A key<TAB>stem[<TAB>count<TAB>runner_up] TSV: the first entry for a key wins.
OrderedDict<std::string> parse(const std::string& text);

// The runner-up share of each entry that carries its counts.
OrderedDict<double> parse_shares(const std::string& text);

// Undiacritized surface form -> its most frequent diacritized stem. Loading
// is lazy. Instances that read the same unchanged file share one parse.
class StemLexicon {
public:
    explicit StemLexicon(std::string source = DEFAULT_LEXICON,
                         std::optional<double> ambiguous_share = AMBIGUOUS_SHARE);

    const std::string& source() const { return source_; }
    std::optional<double> ambiguous_share() const { return ambiguous_share_; }

    // The entries; LexiconUnavailable when the lexicon cannot be read.
    const OrderedDict<std::string>& entries() const;
    // The runner-up shares (_shares): empty before the lexicon is loaded.
    const OrderedDict<double>& shares() const;

    // The stem for word. In a sentence of at least MIN_CONTEXT_WORDS words an
    // ambiguous spelling gives std::nullopt.
    std::optional<std::string> get(const std::string& word, std::optional<long> context_words = std::nullopt) const;
    bool ambiguous(const std::string& word) const;
    size_t size() const;  // len()
    bool contains(const std::string& word) const;

    struct Data;

private:
    void load() const;

    std::string source_;
    std::optional<double> ambiguous_share_;
    mutable std::shared_ptr<const Data> data_;
};

}  // namespace arbtok::lexicon
