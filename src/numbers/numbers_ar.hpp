// ovos_number_parser.numbers_ar, the functions the rest of the port calls.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "arbtok/numbers/ovos_number_parser.hpp"
#include "arbtok/numbers/pynum.hpp"
#include "text.hpp"

namespace arbtok::numbers::numbers_ar {

using text::U32;
using text::U32View;
using Tokens = std::vector<U32>;
using ovos_number_parser::GrammaticalGender;

// The gender argument of the reference: None, or a GrammaticalGender.
using Gender = std::optional<GrammaticalGender>;

U32 normalize_ar(U32View text);
Tokens tokenize_ar(U32View text);

// resolve_ar_lang: "nominative", "oblique", or nullopt.
std::optional<std::string> resolve_ar_lang(std::string_view lang);
// resolve_ar_lect: the ISO 639-3 code of a lect with its own table, or nullopt.
std::optional<std::string> resolve_ar_lect(std::string_view lang);

// pronounce_number_ar(number, places, case=case, lect=lect, gender=gender);
// case is None or a name of a case.
U32 pronounce_number_ar(const Number& number, int places = 2,
                        const std::optional<std::string>& grammatical_case = std::string("nominative"),
                        const std::optional<std::string>& lect = std::nullopt,
                        Gender gender = GrammaticalGender::MASCULINE);
U32 pronounce_ordinal_ar(const Number& number, Gender gender = GrammaticalGender::MASCULINE,
                         const std::optional<std::string>& grammatical_case = std::string("nominative"));
// nice_number_ar(number, speech=True, denominators=[denominator]).
U32 nice_number_ar(double number, long long denominator);

std::pair<std::optional<Number>, size_t> parse_number_span(const Tokens& tokens, size_t i, bool halves = false);
std::pair<std::optional<long long>, size_t> parse_ordinal_span(const Tokens& tokens, size_t i);

std::vector<Number> extract_numbers_ar(U32View text, bool ordinals = false,
                                       const std::optional<std::string>& lect = std::nullopt);
std::optional<Number> extract_number_ar(U32View text, bool ordinals = false,
                                        const std::optional<std::string>& lect = std::nullopt);
U32 numbers_to_digits_ar(U32View utterance, std::string_view lang);

}  // namespace arbtok::numbers::numbers_ar

namespace arbtok::numbers::ovos_number_parser::detail {

// _numbers_to_digits_generic(utterance, lang, continues, first_number_words).
text::U32 numbers_to_digits_generic(
    text::U32View utterance, std::string_view lang, const std::function<bool(const text::U32&)>& continues,
    const std::function<std::optional<size_t>(const std::vector<text::U32>&)>& first_number_words);

// extract_number on code points.
std::optional<Number> extract_number(text::U32View text, std::string_view lang, bool ordinals = false);

}  // namespace arbtok::numbers::ovos_number_parser::detail
