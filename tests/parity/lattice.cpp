#include "arbtok/lattice.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace lt = arbtok::lattice;

namespace {

// [grapheme, [start, end], [[ipa, repr(cost)], ...]]
std::string slot(const lt::SegmentSlot& s) {
    std::vector<std::string> candidates;
    for (const auto& c : s.candidates) candidates.push_back(list({J(c.ipa), F(c.score)}));
    return list({J(s.grapheme), list({J(static_cast<unsigned long>(s.span.first)), J(static_cast<unsigned long>(s.span.second))}),
                 list(candidates)});
}

}  // namespace

std::string record_lattice(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const")
        return head(v) + (v[1].str == "DEFAULT_LANG" ? J(lt::DEFAULT_LANG) : J(lt::default_rescorer_names())) + "]";
    if (fn == "defers_to_cascade") return head(v) + J(lt::defers_to_cascade(v[1].str)) + "]";
    if (fn == "word_ipa") return head(v) + guard([&] { return J(lt::word_ipa(v[1].str, v[2].str, v[3].boolean)); }) + "]";
    if (fn == "word_lattice") {
        std::vector<std::string> slots;
        for (const auto& s : lt::word_lattice(v[1].str, v[2].str)) slots.push_back(slot(s));
        return head(v) + list(slots) + "]";
    }
    if (fn == "spec_word_exception") return head(v) + J(lt::spec_word_exception(v[1].str, v[2].str)) + "]";
    throw std::runtime_error("unknown lattice record " + fn);
}

}  // namespace parity
