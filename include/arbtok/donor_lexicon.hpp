// arbtok.donor_lexicon: the bundled donor lexicons (data/donor_lexicons), held
// by arbtok's own donor engine through Pipeline::lexicon. No other engine in
// the process sees them. A caller's own process-wide lexicon always wins.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include <orthography2ipa/orthography2ipa.hpp>

namespace arbtok::donor_lexicon {

// The donor whose lexicon is bundled.
inline const std::string BUNDLED_DONOR = "en-GB";

// The shipped data/donor_lexicons/<code>.tsv, or std::nullopt when it is not
// a file.
std::optional<std::string> bundled_path(const std::string& code = BUNDLED_DONOR);

// The engine that reads a word of the donor code. It holds the bundled
// lexicon for code, when one ships, and one such engine serves each code. A
// donor with no bundled lexicon, or one the caller gave a process-wide
// lexicon, gets a new plain engine. KeyError for a code with no spec, and
// FileNotFoundError when the caller's lexicon file is missing.
std::shared_ptr<const orthography2ipa::G2P> donor_engine(const std::string& code = BUNDLED_DONOR);

}  // namespace arbtok::donor_lexicon
