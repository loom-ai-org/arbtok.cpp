#include "pyio.hpp"

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "unicode_norm.hpp"

namespace arbtok::detail {

namespace {

struct Utf8Error {
    size_t start, end;
    const char* reason;
};

// The first error of bytes.decode("utf-8"), with CPython's range and reason.
std::optional<Utf8Error> first_utf8_error(std::string_view b) {
    const size_t n = b.size();
    auto at = [&](size_t k) { return static_cast<unsigned char>(b[k]); };
    size_t i = 0;
    while (i < n) {
        const unsigned char c = at(i);
        if (c < 0x80) {
            ++i;
            continue;
        }
        size_t len;
        unsigned char lo = 0x80, hi = 0xBF;  // the range of the second byte
        if (c >= 0xC2 && c <= 0xDF) {
            len = 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            len = 3;
            if (c == 0xE0) lo = 0xA0;
            if (c == 0xED) hi = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            len = 4;
            if (c == 0xF0) lo = 0x90;
            if (c == 0xF4) hi = 0x8F;
        } else {
            return Utf8Error{i, i + 1, "invalid start byte"};
        }
        for (size_t k = 1; k < len; ++k) {
            if (i + k >= n) return Utf8Error{i, n, "unexpected end of data"};
            const unsigned char d = at(i + k);
            const bool ok = k == 1 ? (d >= lo && d <= hi) : (d >= 0x80 && d <= 0xBF);
            if (!ok) return Utf8Error{i, i + k, "invalid continuation byte"};
        }
        i += len;
    }
    return std::nullopt;
}

bool ascii_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

bool line_break(char32_t c) {
    return c == '\n' || c == '\r' || c == 0x0B || c == 0x0C || c == 0x1C || c == 0x1D || c == 0x1E || c == 0x85 ||
           c == 0x2028 || c == 0x2029;
}

}  // namespace

Error os_error(int err, const std::string& filename) {
    std::string type = "OSError";
    if (err == ENOENT) type = "FileNotFoundError";
    else if (err == EISDIR) type = "IsADirectoryError";
    else if (err == ENOTDIR) type = "NotADirectoryError";
    else if (err == EACCES || err == EPERM) type = "PermissionError";
    return Error(type, "[Errno " + std::to_string(err) + "] " + std::strerror(err) + ": " + py_str_repr(u(filename)));
}

std::string read_text(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) throw os_error(errno, path);
    struct stat st {};
    if (::fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) {
        ::close(fd);
        throw os_error(EISDIR, path);
    }
    std::string bytes;
    char buffer[65536];
    for (;;) {
        const ssize_t got = ::read(fd, buffer, sizeof buffer);
        if (got < 0) {
            const int err = errno;
            if (err == EINTR) continue;
            ::close(fd);
            throw os_error(err, path);
        }
        if (got == 0) break;
        bytes.append(buffer, static_cast<size_t>(got));
    }
    ::close(fd);
    if (const auto e = first_utf8_error(bytes)) {
        char message[160];
        if (e->end - e->start == 1)
            std::snprintf(message, sizeof message, "'utf-8' codec can't decode byte 0x%02x in position %zu: %s",
                          static_cast<unsigned char>(bytes[e->start]), e->start, e->reason);
        else
            std::snprintf(message, sizeof message, "'utf-8' codec can't decode bytes in position %zu-%zu: %s",
                          e->start, e->end - 1, e->reason);
        throw Error("UnicodeDecodeError", message);
    }
    std::string text;
    text.reserve(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\r') {
            text.push_back('\n');
            if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
        } else {
            text.push_back(bytes[i]);
        }
    }
    return text;
}

std::vector<U32> splitlines(U32View text) {
    std::vector<U32> out;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (!line_break(text[i])) continue;
        out.emplace_back(text.substr(start, i - start));
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
        start = i + 1;
    }
    if (start < text.size()) out.emplace_back(text.substr(start));
    return out;
}

std::optional<long long> py_int(U32View text) {
    // _PyUnicode_TransformDecimalAndSpaceToASCII, then PyLong_FromString in base 10.
    std::string s;
    for (char32_t c : text) {
        if (is_space(c)) {
            s.push_back(' ');
        } else if (const int d = decimal_value(c); d >= 0) {
            s.push_back(static_cast<char>('0' + d));
        } else if (c > 0 && c < 0x80) {
            s.push_back(static_cast<char>(c));
        } else {
            return std::nullopt;
        }
    }
    size_t i = 0;
    while (i < s.size() && ascii_space(s[i])) ++i;
    bool negative = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) negative = s[i++] == '-';
    if (i >= s.size() || s[i] < '0' || s[i] > '9') return std::nullopt;
    unsigned long long value = 0;
    bool overflow = false;
    char prev = 0;
    for (; i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '_'); prev = s[i++]) {
        if (s[i] == '_') {
            if (prev == '_') return std::nullopt;
            continue;
        }
        const unsigned d = static_cast<unsigned>(s[i] - '0');
        if (value > (~0ULL - d) / 10) overflow = true;
        value = value * 10 + d;
    }
    if (prev == '_') return std::nullopt;
    while (i < s.size() && ascii_space(s[i])) ++i;
    if (i != s.size()) return std::nullopt;
    const unsigned long long limit = negative ? (1ULL << 63) : (1ULL << 63) - 1;
    if (overflow || value > limit) return std::nullopt;
    if (negative) return value == (1ULL << 63) ? std::numeric_limits<long long>::min() : -static_cast<long long>(value);
    return static_cast<long long>(value);
}

double py_true_divide(long long a, long long b) {
    constexpr long long exact = 1LL << 53;
    if (a >= -exact && a <= exact && b >= -exact && b <= exact) return static_cast<double>(a) / static_cast<double>(b);
    return static_cast<double>(static_cast<long double>(a) / static_cast<long double>(b));
}

std::string path_str(std::string_view path) {
    size_t slashes = 0;
    while (slashes < path.size() && path[slashes] == '/') ++slashes;
    const std::string root = slashes == 0 ? "" : slashes == 2 ? "//" : "/";
    std::string out = root;
    bool first = true;
    size_t i = slashes;
    while (i <= path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string_view::npos) j = path.size();
        const std::string_view part = path.substr(i, j - i);
        if (!part.empty() && part != ".") {
            if (!first) out += '/';
            out += part;
            first = false;
        }
        i = j + 1;
    }
    return out.empty() ? "." : out;
}

std::string path_join(std::string_view a, std::string_view b) {
    if (!b.empty() && b[0] == '/') return path_str(b);
    if (a.empty()) return path_str(b);
    return path_str(std::string(a) + "/" + std::string(b));
}

std::optional<std::string> getenv(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr) return std::nullopt;
    return std::string(v);
}

std::string expanduser(std::string_view raw) {
    const std::string path = path_str(raw);
    if (path.empty() || path[0] != '~') return path;
    const size_t slash = path.find('/');
    const std::string first = path.substr(0, slash);
    const std::string rest = slash == std::string::npos ? "" : path.substr(slash + 1);
    std::optional<std::string> home;
    if (first == "~") {
        home = getenv("HOME");
        if (!home) {
            if (const passwd* pw = ::getpwuid(::getuid())) home = pw->pw_dir;
        }
    } else if (const passwd* pw = ::getpwnam(first.substr(1).c_str())) {
        home = pw->pw_dir;
    }
    if (!home) throw Error("RuntimeError", "Could not determine home directory.");
    std::string userhome = *home;
    while (!userhome.empty() && userhome.back() == '/') userhome.pop_back();
    if (userhome.empty()) userhome = "/";
    return rest.empty() ? path_str(userhome) : path_join(userhome, rest);
}

}  // namespace arbtok::detail
