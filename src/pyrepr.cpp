#include "pyrepr.hpp"

#include <charconv>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <system_error>

namespace arbtok::detail {

std::string float_repr(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::scientific);
    std::string sci(buf, res.ptr);
    std::string sign;
    if (sci[0] == '-') {
        sign = "-";
        sci.erase(0, 1);
    }
    const size_t e = sci.find('e');
    std::string digits = sci.substr(0, e);
    digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
    const int exp = std::atoi(sci.c_str() + e + 1);
    std::string out;
    if (exp >= -4 && exp <= 15) {
        if (exp < 0) {
            out = "0." + std::string(-exp - 1, '0') + digits;
        } else if (static_cast<size_t>(exp) + 1 >= digits.size()) {
            out = digits + std::string(exp + 1 - digits.size(), '0') + ".0";
        } else {
            out = digits.substr(0, exp + 1) + "." + digits.substr(exp + 1);
        }
    } else {
        out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        const int a = std::abs(exp);
        out += std::string(exp < 0 ? "e-" : "e+") + (a < 10 ? "0" : "") + std::to_string(a);
    }
    return sign + out;
}

}  // namespace arbtok::detail
