#include "arbtok/nisba.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

std::string record_nisba(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + J(arbtok::nisba::NOT_NISBA) + "]";
    if (fn == "restore_nisba") return head(v) + J(arbtok::nisba::restore_nisba(v[1].str)) + "]";
    throw std::runtime_error("unknown nisba record " + fn);
}

}  // namespace parity
