// arbtok.nisba: the shadda of the nisba suffix -ijj, which the writing omits.
#pragma once

#include <set>
#include <string>

namespace arbtok::nisba {

// The bare words whose final ya is a long vowel, not the nisba suffix.
extern const std::set<std::string> NOT_NISBA;

// word with its final ya marked as the nisba (kasra, ya, shadda) where it is one.
std::string restore_nisba(const std::string& word);

}  // namespace arbtok::nisba
