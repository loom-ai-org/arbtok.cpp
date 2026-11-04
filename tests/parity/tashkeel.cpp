#include "arbtok/tashkeel.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

// [str(error), the class name, the names of its Python base classes]. The
// bases are those of the class that python_type() names.
std::string record_tashkeel(const Value& v) {
    const arbtok::tashkeel::TashkeelError e(v[1].str);
    const arbtok::Error& base = e;
    return head(v) + list({J(base.message()), J(base.python_type()),
                           J(std::vector<std::string>{"Exception", "BaseException", "object"})}) +
           "]";
}

}  // namespace parity
