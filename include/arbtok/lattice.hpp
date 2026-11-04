// arbtok.lattice: one word's phonology on the orthography2ipa lattice, with
// the Arabic rules that the shared spec table cannot express added as
// rescorers.
#pragma once

// rescorer.hpp uses AllophoneRule without declaring it; orthography2ipa.hpp
// declares it.
#include <orthography2ipa/orthography2ipa.hpp>
#include <orthography2ipa/rescorer.hpp>

#include <optional>
#include <string>
#include <vector>

namespace arbtok::lattice {

namespace rescorer = orthography2ipa::rescorer;
using orthography2ipa::Candidate;
using rescorer::SegmentSlot;

inline const std::string DEFAULT_LANG = "ar";

// The lam of the article assimilates into a following sun letter.
class SunLetterRescorer : public rescorer::LatticeRescorer {
public:
    std::vector<Candidate> rescore(const SegmentSlot& slot, const rescorer::RescoreContext& context) const override;
};

// A word-initial bare alif is the silent support of hamzat al-wasl.
class WaslRescorer : public rescorer::LatticeRescorer {
public:
    std::vector<Candidate> rescore(const SegmentSlot& slot, const rescorer::RescoreContext& context) const override;
};

// A hamza carrier before a sukun or at the word end is a bare /ʔ/.
class HamzaCarrierRescorer : public rescorer::LatticeRescorer {
public:
    std::vector<Candidate> rescore(const SegmentSlot& slot, const rescorer::RescoreContext& context) const override;
};

// A glide before a vowel or before its own geminate is a consonant.
class ConsonantalGlideRescorer : public rescorer::LatticeRescorer {
public:
    std::vector<Candidate> rescore(const SegmentSlot& slot, const rescorer::RescoreContext& context) const override;
};

// The alif after tanwin al-fath is silent.
class AccusativeAlifRescorer : public rescorer::LatticeRescorer {
public:
    std::vector<Candidate> rescore(const SegmentSlot& slot, const rescorer::RescoreContext& context) const override;
};

// DEFAULT_RESCORERS, in the order they run.
const std::vector<const rescorer::LatticeRescorer*>& default_rescorers();
// The Python class names of default_rescorers(), in the same order.
const std::vector<std::string>& default_rescorer_names();

// The rescored lattice of one diacritized word under the lect lang.
std::vector<SegmentSlot> word_lattice(const std::string& word, const std::string& lang = "ar");

// The IPA of one diacritized word: the spec's word exception, else the best
// candidate of each slot; stressed unless stress is false or the spec lists
// the word as a clitic.
std::string word_ipa(const std::string& word, const std::string& lang = "ar", bool stress = true);

// The spec's whole-word IPA for word, or nullopt.
std::optional<std::string> spec_word_exception(const std::string& word, const std::string& lang = "ar");

// True when the word needs the character cascade: it holds punctuation, or a
// proclitic stands before a wasl alif that is not the article.
bool defers_to_cascade(const std::string& word);

}  // namespace arbtok::lattice
