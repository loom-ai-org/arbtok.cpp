// A path under arbtok::data_dir() as the goldens write it: relative to the
// Python package directory, whose data/ directory data_dir() stands for.
#pragma once

#include <optional>
#include <string>

#include "arbtok/errors.hpp"
#include "pyio.hpp"

namespace parity {

inline std::optional<std::string> rel_data(const std::optional<std::string>& path) {
    if (!path) return std::nullopt;
    const std::string base = arbtok::detail::path_str(arbtok::data_dir());
    const std::string p = arbtok::detail::path_str(*path);
    if (p == base) return std::string("data");
    if (p.rfind(base + "/", 0) == 0) return "data/" + p.substr(base.size() + 1);
    throw arbtok::ValueError(p + " is not in the subpath of " + base);
}

}  // namespace parity
