// The few regular-expression shapes the modules use, as code. Each function
// gives the match that Python's re gives for the pattern it names.
#pragma once

#include <cstddef>
#include <optional>
#include <utility>

#include "detail.hpp"

namespace arbtok::detail {

// re.search("[<class>]+$", s): the run of class members that ends at the end
// of s, or at a final "\n", as $ matches there too. [start, end) in code points.
template <class Pred>
std::optional<std::pair<size_t, size_t>> trailing_run(U32View s, Pred in_class) {
    size_t end = s.size();
    if (end > 0 && !in_class(s[end - 1]) && s[end - 1] == U'\n') --end;
    size_t start = end;
    while (start > 0 && in_class(s[start - 1])) --start;
    if (start == end) return std::nullopt;
    return std::make_pair(start, end);
}

}  // namespace arbtok::detail
