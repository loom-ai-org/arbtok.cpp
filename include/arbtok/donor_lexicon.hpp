// arbtok.donor_lexicon: the bundled donor lexicons (data/donor_lexicons),
// registered with orthography2ipa's lexicon registry on arbtok's behalf. A
// caller's own registration always wins.
#pragma once

#include <optional>
#include <string>

namespace arbtok::donor_lexicon {

// The donor whose lexicon is bundled.
inline const std::string BUNDLED_DONOR = "en-GB";

// The shipped data/donor_lexicons/<code>.tsv, or std::nullopt when it is not
// a file.
std::optional<std::string> bundled_path(const std::string& code = BUNDLED_DONOR);

// Registers the bundled lexicon for code unless orthography2ipa has one for
// it. True when this call registered it. Reads nothing.
bool ensure_registered(const std::string& code = BUNDLED_DONOR);

}  // namespace arbtok::donor_lexicon
