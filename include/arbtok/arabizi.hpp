// arbtok.arabizi: Arabic written in Latin letters and digits, read back to an
// unpointed Arabic skeleton.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arbtok::arabizi {

// The digit gutturals, in their Python dict order.
extern const std::vector<std::pair<std::string, std::string>> ARABIZI_DIGITS;

// True when word is to be read as Arabizi: the hint when one is given,
// otherwise whether the word carries a digit guttural.
bool is_arabizi(const std::string& word, std::optional<bool> hint = std::nullopt);

// True when every letter and digit of word has a reverse-transliteration.
bool arabizi_covers(const std::string& word);

// word reverse-transliterated, longest rule first, with short vowels dropped.
std::string to_arabic_skeleton(const std::string& word);

}  // namespace arbtok::arabizi
