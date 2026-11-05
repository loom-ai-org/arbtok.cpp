// Internal helpers shared by the modules: the code-point layer that mirrors
// Python's str methods, and file access.
#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "arbtok/errors.hpp"

namespace arbtok::detail {

using U32 = std::u32string;
using U32View = std::u32string_view;

// UTF-8 conversion. Ill-formed input decodes as Python's
// bytes.decode("utf-8", "replace"): one U+FFFD per maximal subpart.
U32 from_utf8(std::string_view text);
std::string to_utf8(U32View text);

// The Unicode version of the character tables: unicodedata.unidata_version.
const char* unidata_version();

// One code point.
bool is_space(char32_t c);           // str.isspace()
bool is_alnum(char32_t c);           // str.isalnum()
bool is_alpha(char32_t c);           // str.isalpha()
bool is_digit(char32_t c);           // str.isdigit()
bool is_nonspacing_mark(char32_t c);  // unicodedata.category(c) == "Mn"
bool is_other_symbol(char32_t c);     // unicodedata.category(c) == "So"
// Category L* and a unicodedata.name that starts with "ARABIC".
bool is_arabic_named_letter(char32_t c);
// The script that the unicodedata.name of a letter (category L*) starts with,
// among the four that translit.guest_script reads.
enum class NamedScript { Other, Latin, Cyrillic, Greek, Arabic };
NamedScript named_script(char32_t c);
// unicodedata.decimal(c), the digit value that int() reads; -1 for none.
int decimal_value(char32_t c);
// Category Lo in the Arabic, Arabic Supplement, Arabic Extended-A and -B blocks.
bool is_arabic_script_letter(char32_t c);

// Whole strings, as Python's str methods: true only for a non-empty string.
bool all_alpha(U32View s);
bool all_digit(U32View s);
bool all_space(U32View s);

// Full case mappings, Greek final sigma included.
U32 lower(U32View s);
U32 upper(U32View s);

// str.strip(chars).
U32 strip(U32View s, U32View chars);
// sep.join(parts).
U32 join(const std::vector<U32>& parts, U32View sep);
// str.split(sep) with a non-empty separator.
std::vector<U32> split(U32View s, U32View sep);
// str.replace(old, new).
U32 replace(U32View s, U32View from, U32View to);

bool contains(U32View s, char32_t c);
bool starts_with(U32View s, U32View prefix);
bool ends_with(U32View s, U32View suffix);

// Python's text: decode once at the API edge, encode on the way out.
inline U32 u(std::string_view s) { return from_utf8(s); }
inline std::string s8(U32View s) { return to_utf8(s); }
inline std::string s8(char32_t c) { return to_utf8(U32View(&c, 1)); }

// The code points of a Python set of one-character strings.
std::set<char32_t> chars_of(const std::set<std::string>& strings);

// Files under the data directory.
std::string data_path(const std::string& relative);
// The bytes of a file; FileNotFoundError when it cannot be opened.
std::string read_file(const std::string& path);

// warnings.warn(message, category).
void warn(const std::string& category, const std::string& message);

}  // namespace arbtok::detail
