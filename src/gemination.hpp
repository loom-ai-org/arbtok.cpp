// orthography2ipa phonetok._expand_arabic_gemination, which the C++ port keeps
// internal: a shadda becomes a doubled letter.
#pragma once

#include "detail.hpp"

namespace arbtok::detail {

// The three re.sub passes of the reference, in order: letter + shadda +
// haraka, letter + haraka + shadda, then letter + shadda.
U32 expand_arabic_gemination(U32View text);

}  // namespace arbtok::detail
