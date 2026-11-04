// The skeleton guards of arbtok.diacritize that check a proposal of the
// ensemble: strip_marks, repair_skeleton, _skeleton_is_preserved and
// _has_arabic. Text is UTF-8 and is compared code point by code point.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace arbtok::diacritize {

// strip_marks(text): text without the marks a diacritizer may add (_MARKS).
std::string strip_marks(std::string_view text);

// repair_skeleton(original, proposed): the marks of proposed on the letters
// of original, or std::nullopt where Python returns None.
std::optional<std::string> repair_skeleton(std::string_view original, std::string_view proposed);

// _skeleton_is_preserved(original, diacritized).
bool skeleton_is_preserved(std::string_view original, std::string_view diacritized);

// _has_arabic(word).
bool has_arabic(std::string_view word);

}  // namespace arbtok::diacritize
