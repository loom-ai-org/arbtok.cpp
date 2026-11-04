#include "arbtok/unicode_symbol2label.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace u = arbtok::unicode_symbol2label;

std::string record_unicode_symbol2label(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "item") {
        const auto& item = u::items().at(static_cast<size_t>(v[1].integer));
        return head(v) + list({J(item.first), J(item.second)}) + "]";
    }
    if (fn == "len") return head(v) + J(static_cast<unsigned long>(u::items().size())) + "]";
    if (fn == "get") return head(v) + J(u::get(v[1].str)) + "]";
    throw std::runtime_error("unknown unicode_symbol2label record " + fn);
}

}  // namespace parity
