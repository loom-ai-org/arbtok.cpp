// arbtok.stress: the stressed syllable of a prosodic word, placed from the IPA
// by the rules of the lect's orthography2ipa spec.
#pragma once

#include <string>

namespace arbtok::stress {

// One prosodic word of IPA with its stressed syllable marked. The first
// proclitic_onset code points are held out of the computation and put back
// unmarked. A stretch with no IPA vowel is returned unchanged.
std::string stress_ipa(const std::string& ipa, const std::string& lang = "ar", int proclitic_onset = 0);

// stress_ipa over each space-separated word of an utterance.
std::string stress_words(const std::string& ipa, const std::string& lang = "ar");

}  // namespace arbtok::stress
