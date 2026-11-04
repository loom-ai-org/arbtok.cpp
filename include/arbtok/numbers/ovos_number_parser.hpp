// The part of ovos-number-parser that arbtok calls: the Arabic cardinals,
// ordinals and fractions it speaks, and the Arabic number words it reads.
// A language code that names an Arabic variety for ovos-number-parser
// (numbers_ar.resolve_ar_lang) is ported. Any other code raises
// NotImplementedError, as the reference does for the arbtok lect codes it has
// no rules for; numbers_to_digits gives those codes the text with its
// whitespace collapsed, as the reference does.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "arbtok/numbers/pynum.hpp"

namespace arbtok::numbers::ovos_number_parser {

// ovos_number_parser.util.GrammaticalGender.
enum class GrammaticalGender { MASCULINE, FEMININE, NEUTRAL };

// pronounce_number(number, lang, places, gender=gender, case=case). An absent
// case gives the default register of the lect lang names. ValueError for NaN,
// an unknown case and a gender Arabic does not have.
std::string pronounce_number(const Number& number, std::string_view lang, int places = 3,
                             GrammaticalGender gender = GrammaticalGender::MASCULINE,
                             const std::optional<std::string>& grammatical_case = std::nullopt);

// pronounce_ordinal(number, lang, gender=gender, case=case).
std::string pronounce_ordinal(const Number& number, std::string_view lang,
                              GrammaticalGender gender = GrammaticalGender::MASCULINE,
                              const std::optional<std::string>& grammatical_case = std::nullopt);

// pronounce_fraction(fraction_word, lang) for a text "n/m".
std::string pronounce_fraction(std::string_view fraction_word, std::string_view lang);

// extract_number(text, lang, ordinals=ordinals); nullopt where the reference
// returns False.
std::optional<Number> extract_number(std::string_view text, std::string_view lang, bool ordinals = false);

// numbers_to_digits(utterance, lang).
std::string numbers_to_digits(std::string_view utterance, std::string_view lang);

// ovos_number_parser.util.is_numeric(input_str).
bool is_numeric(std::string_view input_str);

// The ovos-number-parser release this port holds parity with.
inline constexpr const char* kReferenceVersion = "0.24.3a1";

}  // namespace arbtok::numbers::ovos_number_parser
