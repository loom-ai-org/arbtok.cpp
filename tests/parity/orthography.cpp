#include "arbtok/orthography.hpp"

#include <map>

#include "common.hpp"
#include "detail.hpp"
#include "json.hpp"
#include "modules.hpp"
#include "unicode_norm.hpp"

namespace parity {

namespace o = arbtok::orthography;

namespace {

// The rawi vocabulary in class-id order, read from tests/data, and the
// synthetic inventory of tools/goldens/orthography.py.
const std::vector<std::string>& inventory(const std::string& name) {
    static const std::map<std::string, std::vector<std::string>> inventories = [] {
        std::map<std::string, std::vector<std::string>> out;
        const auto vocab = arbtok::json::loads(arbtok::detail::read_file(ARBTOK_TEST_DATA_DIR "/rawi_v2.vocab.json"));
        const auto& diac = vocab.at("diac_to_idx");
        std::vector<std::string> rawi(diac.members.size());
        for (const auto& [symbol, index] : diac.members) rawi.at(std::stoul(index.text)) = symbol;
        out["rawi"] = rawi;
        out["synthetic"] = {"", "\u0654", "\u0655", "\u0653", "\u064E", "\u0654\u064E", "\u064E\u0654",
                            "\u0650\u0650\u0651", "\u0650\u0651", "\u0651\u0650", "\u0653\u064E", "x", "\u0654\u0654"};
        return out;
    }();
    return inventories.at(name);
}

o::MarkSet marks_of(const Value& v) {
    const auto items = v.strings();
    return o::MarkSet(items.begin(), items.end());
}

std::string normalized(const std::string& name, const std::string& text) {
    namespace d = arbtok::detail;
    if (name == "NFC") return d::s8(d::nfc(d::u(text)));
    if (name == "NFD") return d::s8(d::nfd(d::u(text)));
    return text;
}

}  // namespace

std::string record_orthography(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") {
        const std::string& name = v[1].str;
        if (name == "ORTHOGRAPHIC_MARKS") return head(v) + J(o::ORTHOGRAPHIC_MARKS) + "]";
        if (name == "VOCALIC_MARKS") return head(v) + J(o::VOCALIC_MARKS) + "]";
        if (name == "MADDA") return head(v) + J(o::MADDA) + "]";
    }
    if (fn == "classes") return head(v) + J(inventory(v[1].str)) + "]";
    if (fn == "allowed_classes")
        return head(v) + J(o::allowed_classes(inventory(v[1].str), marks_of(v[2]), v[3].boolean)) + "]";
    if (fn == "pinned_class") {
        const int pinned = o::pinned_class(inventory(v[1].str), marks_of(v[2]));
        return head(v) + J(pinned) + "]";
    }
    if (fn == "project_class")
        return head(v) +
               J(o::project_class(inventory(v[1].str), static_cast<int>(v[2].integer), marks_of(v[3]), v[4].boolean)) +
               "]";
    if (fn == "source_marks") {
        const std::string norm = v[2].str;
        const auto [bases, marks] =
            o::source_marks(v[1].str, [&](const std::string& s) { return normalized(norm, s); });
        std::vector<std::string> parts;
        for (const auto& m : marks) parts.push_back(J(m));
        return head(v) + list({J(bases), list(parts)}) + "]";
    }
    throw std::runtime_error("unknown orthography record " + fn);
}

}  // namespace parity
