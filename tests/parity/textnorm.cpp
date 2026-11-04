#include "arbtok/textnorm.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace t = arbtok::textnorm;

std::string record_textnorm(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + J(t::ARABIC_LANGUAGE_CODES) + "]";
    if (fn == "is_arabic_lang") return head(v) + guard([&] { return J(t::is_arabic_lang(v[1].str)); }) + "]";
    throw std::runtime_error("unknown textnorm record " + fn);
}

}  // namespace parity
