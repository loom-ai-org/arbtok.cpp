#include "arbtok/donor_lexicon.hpp"

#include <sys/stat.h>

#include <map>
#include <mutex>
#include <utility>

#include "arbtok/errors.hpp"
#include "pyio.hpp"

namespace arbtok::donor_lexicon {

namespace o2i = orthography2ipa;

std::optional<std::string> bundled_path(const std::string& code) {
    const std::string path = detail::path_join(detail::path_join(data_dir(), "donor_lexicons"), code + ".tsv");
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return std::nullopt;
    return path;
}

namespace {

// G2P(code, pipeline=pipeline). A code with no spec is KeyError, as in Python,
// where orthography2ipa.cpp throws std::runtime_error.
std::shared_ptr<const o2i::G2P> engine(const std::string& code, const o2i::Pipeline& pipeline = {}) {
    try {
        o2i::get(code);
    } catch (const std::exception& e) {
        throw KeyError(e.what());
    }
    return std::make_shared<const o2i::G2P>(code, std::map<std::string, std::vector<std::string>>{}, "", false, true,
                                            true, true, nullptr, nullptr, pipeline);
}

}  // namespace

std::shared_ptr<const o2i::G2P> donor_engine(const std::string& code) {
    const auto path = bundled_path(code);
    if (!path) return engine(code);
    try {
        if (o2i::lexicon_path(code)) return engine(code);
    } catch (const o2i::FileNotFoundError& e) {
        throw Error("FileNotFoundError", e.what());
    }
    // functools.lru_cache on (code, path).
    static std::mutex mutex;
    static std::map<std::pair<std::string, std::string>, std::shared_ptr<const o2i::G2P>> bundled;
    std::lock_guard<std::mutex> lock(mutex);
    auto& slot = bundled[{code, *path}];
    if (!slot) slot = engine(code, o2i::Pipeline{o2i::load_lexicon(*path)});
    return slot;
}

}  // namespace arbtok::donor_lexicon
