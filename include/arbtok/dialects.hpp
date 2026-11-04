// arbtok.dialects: the Arabic lexical exceptions, and the bridge from a
// language tag to the orthography2ipa spec of an Arabic lect.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arbtok::dialects {

// An ordered str -> str dict.
using OrderedMap = std::vector<std::pair<std::string, std::string>>;

// The interdental letters.
extern const std::string THEH, THAL, ZAH;

// The reference consonant realizations, in dict order.
extern const OrderedMap ARABIC_TO_IPA_CONSONANTS;
extern const OrderedMap DIACRITIC_TO_IPA;
extern const OrderedMap TANWIN_TO_IPA;
// The diacritics, the tanwin, the madda as a length mark and the alif maksura.
extern const OrderedMap VOWEL_MAP;
// The words whose spelling does not decide their reading, with their IPA.
extern const OrderedMap WORD_EXCEPTIONS;

// The spec code when a caller names no variety.
extern const std::string DEFAULT_LANG;

// The value of key in an ordered map, or nullptr.
const std::string* find(const OrderedMap& map, const std::string& key);

// The first reading of each consonant grapheme that lang's spec declares,
// leaving out a reading with a vowel. Ordered by grapheme.
const std::map<std::string, std::string>& consonant_map(const std::string& lang);

// The spec's reading of a consonant grapheme, or default_ipa.
std::string consonant_ipa(const std::string& grapheme, const std::string& lang, const std::string& default_ipa);

// The orthography2ipa Arabic spec code for a language tag. An empty or absent
// tag, and a tag with no Arabic match, give DEFAULT_LANG. A tag that names a
// non-Arabic spec exactly gives DEFAULT_LANG with a UserWarning when the tag
// is an Arabic one.
std::string spec_for_lang(const std::optional<std::string>& lang);

// The ISO 639-3 code of the lect lang names, through the spec's parents, or
// nullopt.
std::optional<std::string> lect_code(const std::string& lang);

// A resolvable Arabic variety and the quality tier of its spec.
struct Lect {
    std::string code;
    std::string tier;
};

// Every Arabic lect of the orthography2ipa registry, sorted by code.
std::vector<Lect> supported_lects();

// The private helpers that the goldens call.
namespace detail {
// The ordered dict and tuple constants of the module.
extern const std::vector<std::string> REGION_DEFAULTS;
extern const OrderedMap DIALECT_NAME_ALIASES;
extern const OrderedMap SPEC_LANGUAGE_CODES;
extern const std::vector<std::string> VOWEL_CHARS;

// The Arabic spec codes: MSA first, then the region defaults, then by code.
const std::vector<std::string>& arabic_codes();
// A dialect token -> its spec code.
std::map<std::string, std::string> dialect_index(const std::vector<std::string>& codes);
bool is_arabic_code(const std::string& code);
// The language, extlang, script and region subtags that open tag.
std::vector<std::string> core_subtags(const std::string& tag);

// orthography2ipa.resolves_exactly: true when code names a spec without the
// nearest-language guess.
bool resolves_exactly(const std::string& code);

// langcodes.closest_match(desired, supported) with max_distance 25. Throws
// Error("LanguageTagError") where langcodes raises.
std::pair<std::string, int> closest_match(const std::string& desired, const std::vector<std::string>& supported);
}  // namespace detail

}  // namespace arbtok::dialects
