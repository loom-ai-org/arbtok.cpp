#include "arbtok/o2i_plugins.hpp"

#include <algorithm>
#include <optional>

#include "arbtok/diacritize.hpp"
#include "arbtok/dialects.hpp"
#include "arbtok/errors.hpp"
#include "arbtok/fusion.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/sandhi.hpp"
#include "arbtok/textnorm.hpp"
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

// _base(text): text in NFD, without its combining marks.
U32 base(const U32& text) {
    U32 out;
    for (char32_t c : arbtok::detail::nfd(text))
        if (!arbtok::detail::is_nonspacing_mark(c)) out += c;
    return out;
}

// c is in diacritize._MARKS.
bool is_mark(char32_t c) { return (c >= 0x064B && c <= 0x0653) || c == 0x0670; }

}  // namespace

const std::vector<std::string> ArbtokDiacritizer::PATHS = {"lattice", "fusion"};

struct ArbtokDiacritizer::Reader {
    std::unique_ptr<fusion::FusionDiacritizer> fusion;
    std::unique_ptr<diacritize::LatticeDiacritizer> lattice;
};

ArbtokDiacritizer::ArbtokDiacritizer(std::string path_) : path(std::move(path_)) {
    if (std::find(PATHS.begin(), PATHS.end(), path) == PATHS.end())
        throw ValueError("path=" + arbtok::detail::py_str_repr(u(path)) +
                         " is not one of ('lattice', 'fusion'). Refusing rather than falling back: a typo that "
                         "silently kept the default would read exactly like the setting working.");
}

ArbtokDiacritizer::~ArbtokDiacritizer() = default;

std::string ArbtokDiacritizer::normalize(const std::string& text, const std::string& lang) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_lang_.find(lang);
    if (it == by_lang_.end()) {
        auto reader = std::make_unique<Reader>();
        if (path == "fusion")
            reader->fusion = std::make_unique<fusion::FusionDiacritizer>(lang);
        else
            reader->lattice = std::make_unique<diacritize::LatticeDiacritizer>(lang);
        it = by_lang_.emplace(lang, std::move(reader)).first;
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
    const std::size_t n = std::min({stripped.size(), surfaces.size(), pausal.size()});
    for (std::size_t i = 0; i < n; ++i) {
        rows.push_back({stripped[i], surfaces[i], false});
        if (pausal[i]) rows.push_back({"", "", true});
    }
    // apply_cross_word gives one word for each row that is not a pause.
    const auto rewritten = sandhi::apply_cross_word(rows, true, lang);
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

namespace detail {

std::string keep_letters(const std::string& original, const std::string& proposed) {
    const U32 orig = u(original), prop = u(proposed);
    if (base(orig) == base(prop)) return proposed;
    const U32 letters = u(diacritize::strip_marks(original));
    if (letters.size() != u(diacritize::strip_marks(proposed)).size()) return original;
    U32 kept;
    std::size_t i = 0;
    for (char32_t c : prop) {
        if (is_mark(c)) {
            kept += c;
            continue;
        }
        kept += base(U32(1, c)) == base(U32(1, letters[i])) ? c : letters[i];
        ++i;
    }
    return base(kept) == base(orig) ? s8(kept) : original;
}

}  // namespace detail

o2i::Pipeline pipeline(const std::string& lang, const std::string& path) {
    const auto diacritizer = std::make_shared<const ArbtokDiacritizer>(path);
    // The words of the proposal must align with the written words: when the
    // diacritizer changes their count, each written word is marked alone.
    auto mark = [diacritizer, lang](const std::string& text) {
        const U32 space = U" ";
        const auto written = arbtok::detail::split(u(text), space);
        auto marked = arbtok::detail::split(u(diacritizer->normalize(text, lang)), space);
        if (marked.size() != written.size()) {
            marked.clear();
            for (const auto& w : written) marked.push_back(w.empty() ? w : u(diacritizer->normalize(s8(w), lang)));
        }
        std::vector<U32> out;
        for (std::size_t i = 0; i < written.size(); ++i)
            out.push_back(u(detail::keep_letters(s8(written[i]), s8(marked[i]))));
        return s8(arbtok::detail::join(out, space));
    };
    auto expand = [lang](const std::string& text) { return textnorm::normalize_for_tts(text, lang); };
    // The members in order: lexicon, syllabifier, expand, mark, rescore, sandhi.
    return o2i::Pipeline{std::nullopt, nullptr, expand, mark, lattice::default_rescorers(),
                         std::make_shared<const ArbtokSandhi>()};
}

void register_plugins() {
    o2i::register_normalize_plugin("arbtok", std::make_shared<ArbtokDiacritizer>());
    o2i::register_rescorer_plugin("arbtok", std::make_shared<ArbtokRescorers>());
    o2i::register_sandhi_plugin("arbtok", std::make_shared<ArbtokSandhi>());
}

}  // namespace arbtok::o2i_plugins
