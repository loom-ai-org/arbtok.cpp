// arbtok.translit: a Latin-script (or other foreign-script) word in an Arabic
// sentence, read as a loanword of the matrix lect. The donor G2P gives the
// donor's reading, and a per-lect nativisation table, then the matrix lect's
// own inventory, map it into the lect's phonology.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arbtok::translit {

// A Python dict of str to str, in its insertion order.
using Table = std::vector<std::pair<std::string, std::string>>;

// The value of key in table, or nullptr.
const std::string* table_get(const Table& table, const std::string& key);

// The donor assumed when the caller names none.
extern const std::string DONOR_LANG;

// The donor guessed from the script: Latin, Cyrillic, Greek.
extern const std::vector<std::pair<std::string, std::string>> DONOR_BY_SCRIPT;

// Alhoody (2019): the Najdi table, ar-SA-x-najd.
extern const Table SEGMENT_MAP;

// Loans that are already Arabic words, keyed by donor tag, then by the donor
// spelling lowercased.
extern const std::vector<std::pair<std::string, Table>> ESTABLISHED_LOANS;

// The nativisation table of the matrix lect: the first table on lang's spec
// and its parent chain, else the pan-Arabic default. The reference stays valid
// for the life of the program.
const Table& nativization_table(const std::string& lang);

// True when word holds an ASCII letter.
bool is_latin(const std::string& word);

// The IPA string cut into phonemes: a ligature of the maps longest first,
// else one code point with its trailing length, aspiration, palatal,
// pharyngeal and nasal marks.
std::vector<std::string> segment_ipa(const std::string& ipa);

// "Latin", "Cyrillic" or "Greek" for the first letter that names one of them,
// std::nullopt at an Arabic letter first or when no letter names a script.
std::optional<std::string> guest_script(const std::string& word);

// donor_ipa without its stress marks, each phoneme mapped into lang. The
// donor is not read; Python takes it for the call signature.
std::string nativize(const std::string& donor_ipa, const std::string& lang, const std::string& donor = DONOR_LANG);

// word as a speaker of the matrix lect lang says it, or std::nullopt when
// the donor G2P fails or gives nothing, and, with strict, when the adapted
// form needs a phoneme lang does not declare. The donor defaults to the guess
// from the script.
std::optional<std::string> transliterate(const std::string& word, const std::string& lang,
                                         const std::optional<std::string>& donor = std::nullopt,
                                         bool strict = false);

namespace detail {

// The private names the goldens call.
// _TABLES: (spec code, table name) in Python order.
const std::vector<std::pair<std::string, std::string>>& table_names();
// _BORROWABLE_TABLES, by name.
std::vector<std::string> borrowable_names();
// The named table: SEGMENT_MAP, _EGYPTIAN_MAP, _LEVANTINE_MAP, _DEFAULT_MAP,
// _MAGHREBI_MAP, _RAISED, or nullptr.
const Table* table_by_name(const std::string& name);
// The name of a table that table_by_name gives.
std::string name_of(const Table& table);
// _established(word, donor).
std::optional<std::string> established(const std::string& word, const std::string& donor);
// _targets(lang): the single-phoneme members of lang's inventory, sorted.
const std::vector<std::string>& targets(const std::string& lang);
// _project(segment, lang).
std::optional<std::string> project(const std::string& segment, const std::string& lang);
// _map_segments(segments, lang).
std::vector<std::string> map_segments(const std::vector<std::string>& segments, const std::string& lang);

}  // namespace detail

}  // namespace arbtok::translit
