// The code-point layer that mirrors Python's str methods, for the number and
// date modules. Character properties come from unicode_tables.inc, which is
// generated from the reference interpreter.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace arbtok::numbers::text {

using U32 = std::u32string;
using U32View = std::u32string_view;

// UTF-8 conversion. Ill-formed input decodes as Python's
// bytes.decode("utf-8", "replace"): one U+FFFD per maximal subpart.
U32 from_utf8(std::string_view text);
std::string to_utf8(U32View text);

inline U32 u(std::string_view s) { return from_utf8(s); }
inline std::string s8(U32View s) { return to_utf8(s); }

bool is_space(char32_t c);      // str.isspace(), re's \s
bool is_word(char32_t c);       // re's \w: str.isalnum() or '_'
bool is_digit(char32_t c);      // str.isdigit()
bool is_decimal(char32_t c);    // str.isdecimal(), re's \d
int decimal_value(char32_t c);  // unicodedata.decimal(c, -1)

// Whole strings, as Python's str methods: true only for a non-empty string.
bool all_digit(U32View s);
bool all_decimal(U32View s);

// str.lower(), full case mappings, Greek final sigma included.
U32 lower(U32View s);

// str.split() with no argument, and str.split(sep).
std::vector<U32> split_ws(U32View s);
std::vector<U32> split(U32View s, U32View sep);
// str.strip(chars), str.lstrip(chars), str.rstrip(chars), str.strip().
U32 strip(U32View s, U32View chars);
U32 lstrip(U32View s, U32View chars);
U32 rstrip(U32View s, U32View chars);
U32 strip_ws(U32View s);
// sep.join(parts).
U32 join(const std::vector<U32>& parts, U32View sep);
// str.replace(old, new).
U32 replace(U32View s, U32View from, U32View to);

bool starts_with(U32View s, U32View prefix);
bool ends_with(U32View s, U32View suffix);
bool contains(U32View s, U32View part);

// An ASCII string as code points.
U32 a(std::string_view ascii);

}  // namespace arbtok::numbers::text
