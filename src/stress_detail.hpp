// arbtok.stress._first_segment_len, which the tokenizer and the goldens call.
#pragma once

#include <string>

namespace arbtok::stress {

// The length in code points of the first segment of ipa: a base character and
// the modifiers on it.
int first_segment_len(const std::string& ipa);

}  // namespace arbtok::stress
