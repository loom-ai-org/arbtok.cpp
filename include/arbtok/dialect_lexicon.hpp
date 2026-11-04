// arbtok.dialect_lexicon: the per-lect closed-class lexicons, bundled as
// data/lexicons/<lect>.tsv. A lect reads its own file over the files of its
// parent-typed ancestors, and the more specific file wins.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "arbtok/ordered_dict.hpp"

namespace arbtok::dialect_lexicon {

// The bundled directory: data_dir()/lexicons.
std::string lexicon_dir();

// The file bundled for lect, or std::nullopt. A file is named for the code
// its lect was cited under and answers to orthography2ipa's resolve() of
// that name.
std::optional<std::string> lexicon_file(const std::string& lect);

// Every resolved lect code that ships a file, sorted.
std::vector<std::string> available_lects();

// A key<TAB>vocalization[<TAB>source[<TAB>gloss]] TSV. The first entry for a
// key wins; the header row, blank lines and "#" lines are skipped.
OrderedDict<std::string> parse(const std::string& text);

// _parent_chain(lang): lang and its parent-typed ancestors, most specific
// first. The chain stops where orthography2ipa does not know a code.
std::vector<std::string> parent_chain(const std::string& lang);

// Undiacritized surface form -> the lect's diacritized vocalization. Loading
// is lazy.
class DialectLexicon {
public:
    explicit DialectLexicon(std::string lang, std::optional<std::string> data_dir = std::nullopt);

    const std::string& lang() const { return lang_; }
    const OrderedDict<std::string>& entries() const;
    std::optional<std::string> get(const std::string& word) const;
    size_t size() const { return entries().size(); }
    bool contains(const std::string& word) const { return get(word).has_value(); }

private:
    std::string lang_;
    std::optional<std::string> data_dir_;
    mutable std::optional<OrderedDict<std::string>> entries_;
};

}  // namespace arbtok::dialect_lexicon
