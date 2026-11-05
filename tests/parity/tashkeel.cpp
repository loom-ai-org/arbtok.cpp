#include "arbtok/tashkeel.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

// A TashkeelError record is [str(error), the class name, the names of its
// Python base classes]; the bases are those of the class that python_type()
// names. A TashkeelDiacritizer record is the diacritized text.
std::string record_tashkeel(const Value& v) {
    if (v[0].str == "TashkeelDiacritizer.diacritize") {
        static const arbtok::tashkeel::TashkeelDiacritizer reader;
        return head(v) + guard([&] { return J(reader.diacritize(v[1].str, v[2].boolean)); }) + "]";
    }
    const arbtok::tashkeel::TashkeelError e(v[1].str);
    const arbtok::Error& base = e;
    return head(v) + list({J(base.message()), J(base.python_type()),
                           J(std::vector<std::string>{"Exception", "BaseException", "object"})}) +
           "]";
}

}  // namespace parity
