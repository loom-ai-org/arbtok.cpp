// Python exceptions as C++ exceptions. Each one names the Python class that
// the reference raises in the same case.
#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace arbtok::numbers {

class Error : public std::runtime_error {
public:
    Error(std::string python_type, const std::string& message)
        : std::runtime_error(message), python_type_(std::move(python_type)) {}
    // "ValueError", "IndexError" and so on.
    const std::string& python_type() const { return python_type_; }

private:
    std::string python_type_;
};

struct ValueError : Error {
    explicit ValueError(const std::string& message) : Error("ValueError", message) {}
};
struct IndexError : Error {
    explicit IndexError(const std::string& message) : Error("IndexError", message) {}
};
struct OverflowError : Error {
    explicit OverflowError(const std::string& message) : Error("OverflowError", message) {}
};
struct ZeroDivisionError : Error {
    explicit ZeroDivisionError(const std::string& message) : Error("ZeroDivisionError", message) {}
};
struct NotImplementedError : Error {
    explicit NotImplementedError(const std::string& message) : Error("NotImplementedError", message) {}
};
// decimal.InvalidOperation.
struct InvalidOperation : Error {
    explicit InvalidOperation(const std::string& message) : Error("InvalidOperation", message) {}
};

}  // namespace arbtok::numbers
