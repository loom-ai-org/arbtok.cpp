// arbtok.util: the text normalization for speech that arbtok takes from
// phoonnx. normalize() expands contractions and titles and speaks dates,
// times, numbers, units and fractions, and for Arabic it speaks every digit
// that is left.
//
// The spoken forms come from the ported number and date calls, which read
// Arabic only. Where the reference speaks a form with the module of another
// language, the call raises NotPortedError. Where the reference rejects the
// language, the call gives what the reference gives, which is mostly the text
// as written. The contractions, the titles and the steps that only move text
// work for every language.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "arbtok/numbers/ovos_date_parser.hpp"

namespace arbtok::util {

// Each language's table: (written form, spoken form) pairs in insertion order.
using Table = std::vector<std::pair<std::string, std::string>>;
// The tables by language, in insertion order.
using Tables = std::vector<std::pair<std::string, Table>>;

// The closest language of valid_langs to target_lang by the CLDR language
// distance, with its distance; ("und", 10000) when none is within 10.
std::pair<std::string, int> match_lang(const std::string& target_lang, const std::vector<std::string>& valid_langs);

extern const Tables CONTRACTIONS;
extern const Tables TITLES;
// UNITS["ar"] is textnorm::cldr_units("ar").
extern const Tables UNITS;

// The number word reads as when its thousands separators resolve to one
// shape, else nullopt.
std::optional<std::string> resolve_thousands_or_decimal(const std::string& word,
                                                        const std::string& thousands_separator = ",");

// nice_date of a date: dt carries the year, the month and the day.
std::string pronounce_date(const numbers::ovos_date_parser::DateTime& date, const std::string& full_lang);
// A time written "15h01", spoken; a text that is no such time comes back with
// its "h" as a space.
std::string pronounce_time(const std::string& time_string, const std::string& full_lang);

// Whether word is two runs of digits around one "/".
bool is_fraction(const std::string& word);

std::string normalize(const std::string& text, const std::string& lang);

}  // namespace arbtok::util
