// Python exceptions and warnings as C++ objects. Each exception names the
// Python class that the Python package raises in the same case.
#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace arbtok {

class Error : public std::runtime_error {
public:
    Error(std::string python_type, const std::string& message)
        : std::runtime_error(message), python_type_(std::move(python_type)), message_(message) {}
    // "ValueError", "KeyError" and so on.
    const std::string& python_type() const { return python_type_; }
    // str(error). what() stops at the first NUL; this does not.
    const std::string& message() const { return message_; }

private:
    std::string python_type_;
    std::string message_;
};

struct ValueError : Error {
    explicit ValueError(const std::string& message) : Error("ValueError", message) {}
};
struct KeyError : Error {
    explicit KeyError(const std::string& message) : Error("KeyError", message) {}
};
struct TypeError : Error {
    explicit TypeError(const std::string& message) : Error("TypeError", message) {}
};
struct IndexError : Error {
    explicit IndexError(const std::string& message) : Error("IndexError", message) {}
};
struct OverflowError : Error {
    explicit OverflowError(const std::string& message) : Error("OverflowError", message) {}
};
struct NotImplementedError : Error {
    explicit NotImplementedError(const std::string& message) : Error("NotImplementedError", message) {}
};

// A path the Python package computes and the port does not hold, such as a
// number spoken in a language other than Arabic. It stands for no Python
// exception, so it does not derive from Error.
struct NotPortedError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The directory that holds the data files. The first of these that is set
// wins: set_data_dir(), the ARBTOK_DATA_DIR environment variable, the data
// directory of the install that holds the library (found from the path of the
// shared library or of the program), the data directory of the installed
// package that the program was built against with find_package(arbtok), the
// install location compiled into the library.
std::string data_dir();
void set_data_dir(std::string path);

// Where the warnings that Python issues with warnings.warn go: the handler
// receives the category ("UserWarning") and the message. With no handler, as
// at start, a warning is dropped. The handler is process-wide.
using WarningHandler = std::function<void(const std::string& category, const std::string& message)>;
void set_warning_handler(WarningHandler handler);

}  // namespace arbtok
