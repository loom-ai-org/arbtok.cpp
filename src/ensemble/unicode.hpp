// Code points, UTF-8 and the Unicode properties that _ensemble.py reads
// through unicodedata. The tables come from the reference interpreter
// (tools/goldens/ensemble_unicode.py), not from ICU.
#pragma once

#include <string>
#include <string_view>

namespace arbtok::ensemble::detail {

// The general categories in the order of the generated tables.
enum class Category : unsigned char {
    Cn, Lu, Ll, Lt, Lm, Lo, Mn, Mc, Me, Nd, Nl, No, Pc, Pd, Ps, Pe, Pi, Pf, Po,
    Sm, Sc, Sk, So, Zs, Zl, Zp, Cc, Cf, Cs, Co,
};

// The Unicode version of the tables.
const char* unidata_version();

Category category(char32_t cp);

// unicodedata.category(ch).startswith("L").
bool is_letter(char32_t cp);

unsigned char combining_class(char32_t cp);

// bytes.decode("utf-8", "replace"): each maximal ill-formed subpart becomes
// one U+FFFD.
std::u32string decode_utf8(std::string_view bytes);

std::string encode_utf8(std::u32string_view text);

std::u32string nfd(std::u32string_view text);

std::u32string nfc(std::u32string_view text);

}  // namespace arbtok::ensemble::detail
