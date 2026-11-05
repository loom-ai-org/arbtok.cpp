// arbtok.o2i_plugins: arbtok's steps as orthography2ipa plugins. Python
// registers them as the entry points orthography2ipa.normalize,
// orthography2ipa.rescore and orthography2ipa.sandhi, each named "arbtok";
// register_plugins() registers them with orthography2ipa.cpp under the same
// name. A plugin changes a transcription only where a caller or a spec names
// it, as in G2P("ar", {{"normalize", {"arbtok"}}}).
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <orthography2ipa/orthography2ipa.hpp>

namespace arbtok::diacritize {
class LatticeDiacritizer;
}
namespace arbtok::fusion {
class FusionDiacritizer;
}

namespace arbtok::o2i_plugins {

namespace o2i = orthography2ipa;

// The normalize step: the tashkeel restored by the lattice diacritizer, or by
// the fusion diacritizer when ARBTOK_DIACRITIZER is "fusion". ValueError for
// any other value of ARBTOK_DIACRITIZER.
class ArbtokDiacritizer : public o2i::NormalizePlugin {
public:
    // The values of ARBTOK_DIACRITIZER, the default first.
    static const std::vector<std::string> PATHS;
    ArbtokDiacritizer();
    ~ArbtokDiacritizer() override;
    // One diacritizer per (lang, path), made on first use.
    std::string normalize(const std::string& text, const std::string& lang) const override;
    // The codes of supported_lects().
    std::vector<std::string> language_codes() const override;

private:
    struct Reader;
    mutable std::mutex mutex_;
    mutable std::map<std::pair<std::string, std::string>, std::unique_ptr<Reader>> by_lang_;
};

// The rescore step: the lattice rescorers of arbtok.lattice, for every lect.
class ArbtokRescorers : public o2i::RescorerPlugin {
public:
    // DEFAULT_RESCORERS, in the order they run.
    std::vector<const o2i::rescorer::LatticeRescorer*> rescorers(const std::string& lang) const;
    std::vector<const o2i::rescorer::LatticeRescorer*> lattice_rescorers(const std::string& lang) const override;
    // The paths unchanged: the Python plugin contributes lattice rescorers
    // only, and orthography2ipa.cpp's whole-word seam has no Python
    // counterpart.
    std::vector<o2i::IPAPath> rescore(const std::string& word, const std::string& lang,
                                      const std::vector<o2i::IPAPath>& paths) const override;
    std::vector<std::string> language_codes() const override;
};

// The sandhi step: arbtok.sandhi.apply_cross_word over the words, with each
// pause flag as a punctuation token after its word, and each word's stress
// mark put back where it was.
class ArbtokSandhi : public o2i::SandhiPlugin {
public:
    std::vector<std::string> apply(const std::vector<std::string>& words, const std::vector<std::string>& surfaces,
                                   const std::vector<bool>& pausal, const std::string& lang) const override;
    std::vector<std::string> language_codes() const override;
};

// Registers the three plugins with orthography2ipa.cpp under the name
// "arbtok", as the Python entry points do. Each call registers new instances.
void register_plugins();

}  // namespace arbtok::o2i_plugins
