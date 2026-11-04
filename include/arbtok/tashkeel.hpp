// arbtok.tashkeel: the error that diacritization raises. TashkeelDiacritizer
// runs the bundled ensemble model, which this port does not hold.
#pragma once

#include <string>

#include "arbtok/errors.hpp"

namespace arbtok::tashkeel {

// TashkeelError(Exception): what() is str(error).
struct TashkeelError : Error {
    explicit TashkeelError(const std::string& message) : Error("TashkeelError", message) {}
};

}  // namespace arbtok::tashkeel
