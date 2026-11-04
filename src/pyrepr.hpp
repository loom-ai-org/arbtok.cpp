// repr() of a Python float.
#pragma once

#include <string>

namespace arbtok::detail {

// The shortest digits that read back as v, laid out as Python's repr: fixed
// notation for a decimal exponent from -4 to 15, otherwise d.ddde±XX.
std::string float_repr(double v);

}  // namespace arbtok::detail
