// Python behaviour of text files, paths and int() that the lexicons need:
// Path.read_text(encoding="utf-8"), str.splitlines(), int(str), the string
// form of a pathlib path and Path.expanduser().
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "detail.hpp"

namespace arbtok::detail {

// Path(path).read_text(encoding="utf-8"): strict UTF-8 and universal
// newlines. A failure raises the Error that Python raises, with its message:
// FileNotFoundError, IsADirectoryError, PermissionError, another OSError, or
// UnicodeDecodeError.
std::string read_text(const std::string& path);

// str.splitlines() without keepends.
std::vector<U32> splitlines(U32View text);

// int(text) for a str, or std::nullopt where Python raises ValueError. A
// value outside the range of long long is std::nullopt too.
std::optional<long long> py_int(U32View text);

// a / b for two Python ints, as true division rounds it.
double py_true_divide(long long a, long long b);

// str(PurePosixPath(path)): repeated and trailing slashes and "." parts
// removed, exactly two leading slashes kept, "." for an empty path.
std::string path_str(std::string_view path);

// str(PurePosixPath(a) / b).
std::string path_join(std::string_view a, std::string_view b);

// str(Path(path).expanduser()). RuntimeError when the home directory of a
// "~user" prefix is unknown.
std::string expanduser(std::string_view path);

// os.environ.get(name), with std::nullopt for an unset variable.
std::optional<std::string> getenv(const char* name);

// The Error that Python raises for an OSError with this errno and filename.
Error os_error(int err, const std::string& filename);

}  // namespace arbtok::detail
