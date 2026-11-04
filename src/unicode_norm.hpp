// Unicode normalization and str.__repr__, from tables generated out of the
// reference Python (tools/gen_unicode_norm_tables.py).
#pragma once

#include "detail.hpp"

namespace arbtok::detail {

// unicodedata.normalize("NFD", s) and unicodedata.normalize("NFC", s).
U32 nfd(U32View s);
U32 nfc(U32View s);
// unicodedata.combining(c): the canonical combining class.
int combining(char32_t c);

// str.isprintable() of one code point, which repr() of a str uses.
bool is_printable(char32_t c);
// repr(s) of a Python str.
std::string py_str_repr(U32View s);

}  // namespace arbtok::detail
