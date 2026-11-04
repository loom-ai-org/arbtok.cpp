// arbtok.textnorm, the part that dialects depends on: is_arabic_lang.
#pragma once

#include <set>
#include <string>

namespace arbtok::textnorm {

// The ISO 639-3 codes of the individual languages of the Arabic macrolanguage.
extern const std::set<std::string> ARABIC_LANGUAGE_CODES;

// True when the primary subtag of lang names an Arabic variety. Case and an
// underscore in place of a hyphen do not matter.
bool is_arabic_lang(const std::string& lang);

}  // namespace arbtok::textnorm
