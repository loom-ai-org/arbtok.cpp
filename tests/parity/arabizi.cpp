#include "arbtok/arabizi.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace a = arbtok::arabizi;

std::string record_arabizi(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") {
        std::vector<std::string> items;
        for (const auto& [digit, letter] : a::ARABIZI_DIGITS) items.push_back(list({J(digit), J(letter)}));
        return head(v) + list(items) + "]";
    }
    if (fn == "is_arabizi") {
        const std::optional<bool> hint = v[2].is_null() ? std::nullopt : std::optional<bool>(v[2].boolean);
        return head(v) + J(a::is_arabizi(v[1].str, hint)) + "]";
    }
    if (fn == "arabizi_covers") return head(v) + J(a::arabizi_covers(v[1].str)) + "]";
    if (fn == "to_arabic_skeleton") return head(v) + J(a::to_arabic_skeleton(v[1].str)) + "]";
    throw std::runtime_error("unknown arabizi record " + fn);
}

}  // namespace parity
