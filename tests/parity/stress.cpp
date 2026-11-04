#include "arbtok/stress.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace s = arbtok::stress;

std::string record_stress(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "_first_segment_len") return head(v) + J(s::first_segment_len(v[1].str)) + "]";
    if (fn == "stress_ipa")
        return head(v) + guard([&] { return J(s::stress_ipa(v[1].str, v[2].str, static_cast<int>(v[3].integer))); }) +
               "]";
    if (fn == "stress_words") return head(v) + guard([&] { return J(s::stress_words(v[1].str, v[2].str)); }) + "]";
    throw std::runtime_error("unknown stress record " + fn);
}

}  // namespace parity
