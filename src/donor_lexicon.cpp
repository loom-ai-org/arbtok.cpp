#include "arbtok/donor_lexicon.hpp"

#include <sys/stat.h>

#include <orthography2ipa/orthography2ipa.hpp>

#include "arbtok/errors.hpp"
#include "pyio.hpp"

namespace arbtok::donor_lexicon {

std::optional<std::string> bundled_path(const std::string& code) {
    const std::string path = detail::path_join(detail::path_join(data_dir(), "donor_lexicons"), code + ".tsv");
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return std::nullopt;
    return path;
}

bool ensure_registered(const std::string& code) {
    try {
        if (orthography2ipa::lexicon_path(code)) return false;
    } catch (const orthography2ipa::FileNotFoundError& e) {
        throw Error("FileNotFoundError", e.what());
    }
    const auto path = bundled_path(code);
    if (!path) return false;
    orthography2ipa::register_lexicon(code, *path);
    return true;
}

}  // namespace arbtok::donor_lexicon
