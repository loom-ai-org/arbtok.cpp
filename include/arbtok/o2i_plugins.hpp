// arbtok.o2i_plugins: arbtok's steps as orthography2ipa plugins, and the
// orthography2ipa Pipeline that arbtok builds. Python registers the plugins as
// the entry points orthography2ipa.normalize, orthography2ipa.rescore and
// orthography2ipa.sandhi, each named "arbtok"; register_plugins() registers
// them with orthography2ipa.cpp under the same name. A plugin changes a
// transcription only where a caller or a spec names it, as in
// G2P("ar", {{"normalize", {"arbtok"}}}). pipeline() gives the same steps as
// one Pipeline, for the pipeline argument of G2P.
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
// the fusion diacritizer when path is "fusion".
class ArbtokDiacritizer : public o2i::NormalizePlugin {
public:
    // The values of path, the default first.
    static const std::vector<std::string> PATHS;
    // ValueError when path is not one of PATHS.
    explicit ArbtokDiacritizer(std::string path = "lattice");
    ~ArbtokDiacritizer() override;
    const std::string path;
    // One diacritizer per lang, made on first use.
    std::string normalize(const std::string& text, const std::string& lang) const override;
    // The codes of supported_lects().
    std::vector<std::string> language_codes() const override;

private:
    struct Reader;
    mutable std::mutex mutex_;
    mutable std::map<std::string, std::unique_ptr<Reader>> by_lang_;
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

// The Pipeline arbtok builds for lang. expand is normalize_for_tts. mark
// restores the tashkeel through an ArbtokDiacritizer of path and keeps the
// written letters. rescore holds the lattice rescorers of arbtok.lattice, and
// sandhi is ArbtokSandhi. ValueError when path is not one of PATHS.
o2i::Pipeline pipeline(const std::string& lang = "ar", const std::string& path = "lattice");

namespace detail {
// _keep_letters(original, proposed): proposed with the letters of original, so
// that only marks are added. A letter of proposed stays when its NFD base is
// the written letter. A word whose letters do not align keeps original.
std::string keep_letters(const std::string& original, const std::string& proposed);
}  // namespace detail

// Registers the three plugins with orthography2ipa.cpp under the name
// "arbtok", as the Python entry points do. Each call registers new instances.
void register_plugins();

}  // namespace arbtok::o2i_plugins
