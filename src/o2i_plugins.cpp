#include "arbtok/o2i_plugins.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>

#include "arbtok/diacritize.hpp"
#include "arbtok/dialects.hpp"
#include "arbtok/errors.hpp"
#include "arbtok/fusion.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/sandhi.hpp"
#include "detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::o2i_plugins {

using arbtok::detail::s8;
using arbtok::detail::u;
using arbtok::detail::U32;

namespace {

// The codes the plugins speak for: supported_lects(), not a list kept here.
std::vector<std::string> arabic_codes() {
    std::vector<std::string> out;
    for (const auto& lect : dialects::supported_lects()) out.push_back(lect.code);
    return out;
}

// _path(): ARBTOK_DIACRITIZER stripped and lowercased, "lattice" when unset.
std::string diacritizer_path() {
    const char* env = std::getenv("ARBTOK_DIACRITIZER");
    U32 value = u(env ? env : "lattice");
    while (!value.empty() && arbtok::detail::is_space(value.back())) value.pop_back();
    std::size_t start = 0;
    while (start < value.size() && arbtok::detail::is_space(value[start])) ++start;
    const std::string want = s8(arbtok::detail::lower(value.substr(start)));
    for (const auto& path : ArbtokDiacritizer::PATHS)
        if (want == path) return want;
    throw ValueError("ARBTOK_DIACRITIZER=" + arbtok::detail::py_str_repr(u(want)) +
                     " is not one of ('lattice', 'fusion'). Refusing rather than falling back: a typo that "
                     "silently kept the default would read exactly like the setting working.");
}

}  // namespace

const std::vector<std::string> ArbtokDiacritizer::PATHS = {"lattice", "fusion"};

struct ArbtokDiacritizer::Reader {
    std::unique_ptr<fusion::FusionDiacritizer> fusion;
    std::unique_ptr<diacritize::LatticeDiacritizer> lattice;
};

ArbtokDiacritizer::ArbtokDiacritizer() = default;
ArbtokDiacritizer::~ArbtokDiacritizer() = default;

std::string ArbtokDiacritizer::normalize(const std::string& text, const std::string& lang) const {
    const auto key = std::make_pair(lang, diacritizer_path());
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_lang_.find(key);
    if (it == by_lang_.end()) {
        auto reader = std::make_unique<Reader>();
        if (key.second == "fusion")
            reader->fusion = std::make_unique<fusion::FusionDiacritizer>(lang);
        else
            reader->lattice = std::make_unique<diacritize::LatticeDiacritizer>(lang);
        it = by_lang_.emplace(key, std::move(reader)).first;
    }
    Reader& r = *it->second;
    return r.fusion ? r.fusion->diacritize(text) : r.lattice->diacritize(text);
}

std::vector<std::string> ArbtokDiacritizer::language_codes() const { return arabic_codes(); }

std::vector<const o2i::rescorer::LatticeRescorer*> ArbtokRescorers::rescorers(const std::string& /*lang*/) const {
    return lattice::default_rescorers();
}

std::vector<const o2i::rescorer::LatticeRescorer*> ArbtokRescorers::lattice_rescorers(const std::string& lang) const {
    return rescorers(lang);
}

std::vector<o2i::IPAPath> ArbtokRescorers::rescore(const std::string& /*word*/, const std::string& /*lang*/,
                                                   const std::vector<o2i::IPAPath>& paths) const {
    return paths;
}

std::vector<std::string> ArbtokRescorers::language_codes() const { return arabic_codes(); }

std::vector<std::string> ArbtokSandhi::apply(const std::vector<std::string>& words,
                                             const std::vector<std::string>& surfaces,
                                             const std::vector<bool>& pausal, const std::string& lang) const {
    // orthography2ipa stresses each word before sandhi runs. The marks come
    // off for the match and go back at their index after it: sandhi rewrites
    // only a word's end.
    struct Mark {
        std::size_t index;
        char32_t mark;
    };
    std::vector<std::optional<Mark>> marks;
    std::vector<std::string> stripped;
    for (const auto& w : words) {
        const U32 s = u(w);
        std::optional<Mark> mark;
        U32 bare;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == U'ˈ' || s[i] == U'ˌ') {
                if (!mark) mark = Mark{i, s[i]};
            } else {
                bare += s[i];
            }
        }
        marks.push_back(mark);
        stripped.push_back(s8(bare));
    }
    // A pause is a punctuation token in arbtok and a flag in orthography2ipa.
    std::vector<sandhi::CrossWord> rows;
    std::vector<bool> is_pause;
    const std::size_t n = std::min({stripped.size(), surfaces.size(), pausal.size()});
    for (std::size_t i = 0; i < n; ++i) {
        rows.push_back({stripped[i], surfaces[i], false});
        is_pause.push_back(false);
        if (pausal[i]) {
            rows.push_back({"", "", true});
            is_pause.push_back(true);
        }
    }
    // apply_cross_word gives the words without the pause rows, and Python
    // indexes that list by row: a pause before a word raises IndexError.
    const auto out = sandhi::apply_cross_word(rows, true, lang);
    std::vector<std::string> rewritten;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (is_pause[i]) continue;
        if (i >= out.size()) throw IndexError("list index out of range");
        rewritten.push_back(out[i]);
    }
    std::vector<std::string> restored;
    for (std::size_t w = 0; w < rewritten.size() && w < marks.size(); ++w) {
        const U32 word = u(rewritten[w]);
        const auto& mark = marks[w];
        if (!mark || mark->index > word.size()) {
            restored.push_back(rewritten[w]);
        } else {
            restored.push_back(s8(word.substr(0, mark->index) + mark->mark + word.substr(mark->index)));
        }
    }
    return restored;
}

std::vector<std::string> ArbtokSandhi::language_codes() const { return arabic_codes(); }

void register_plugins() {
    o2i::register_normalize_plugin("arbtok", std::make_shared<ArbtokDiacritizer>());
    o2i::register_rescorer_plugin("arbtok", std::make_shared<ArbtokRescorers>());
    o2i::register_sandhi_plugin("arbtok", std::make_shared<ArbtokSandhi>());
}

}  // namespace arbtok::o2i_plugins
