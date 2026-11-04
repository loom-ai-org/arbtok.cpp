#include "arbtok/version.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace ver = arbtok::version;

std::string record_version(const Value& v) {
    const std::string& name = v[1].str;
    if (name == "VERSION_MAJOR") return head(v) + J(ver::VERSION_MAJOR) + "]";
    if (name == "VERSION_MINOR") return head(v) + J(ver::VERSION_MINOR) + "]";
    if (name == "VERSION_BUILD") return head(v) + J(ver::VERSION_BUILD) + "]";
    if (name == "VERSION_ALPHA") return head(v) + J(ver::VERSION_ALPHA) + "]";
    if (name == "VERSION_STR") return head(v) + J(ver::VERSION_STR) + "]";
    throw std::runtime_error("unknown version constant " + name);
}

}  // namespace parity
