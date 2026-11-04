#include "unicode.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>

namespace arbtok::ensemble::detail {

namespace {

struct CategoryRun {
    char32_t first;
    char32_t last;
    unsigned char category;
};

struct CccRun {
    char32_t first;
    char32_t last;
    unsigned char ccc;
};

struct Decomposition {
    char32_t cp;
    std::uint32_t offset;
    std::uint8_t length;
};

struct Composition {
    char32_t first;
    char32_t second;
    char32_t composite;
};

#include "unicode_tables.inc"

constexpr char32_t kSBase = 0xAC00;
constexpr char32_t kLBase = 0x1100;
constexpr char32_t kVBase = 0x1161;
constexpr char32_t kTBase = 0x11A7;
constexpr char32_t kLCount = 19;
constexpr char32_t kVCount = 21;
constexpr char32_t kTCount = 28;
constexpr char32_t kNCount = kVCount * kTCount;
constexpr char32_t kSCount = kLCount * kNCount;

template <typename Run>
const Run* find_run(const Run* begin, const Run* end, char32_t cp) {
    const Run* it = std::upper_bound(begin, end, cp, [](char32_t c, const Run& r) { return c < r.first; });
    if (it == begin) return nullptr;
    --it;
    return cp <= it->last ? it : nullptr;
}

void decompose_into(char32_t cp, std::u32string& out) {
    if (cp >= kSBase && cp < kSBase + kSCount) {
        const char32_t s = cp - kSBase;
        out.push_back(kLBase + s / kNCount);
        out.push_back(kVBase + (s % kNCount) / kTCount);
        if (s % kTCount) out.push_back(kTBase + s % kTCount);
        return;
    }
    const auto* end = std::end(kDecompositions);
    const auto* it = std::lower_bound(std::begin(kDecompositions), end, cp,
                                      [](const Decomposition& d, char32_t c) { return d.cp < c; });
    if (it != end && it->cp == cp) {
        out.append(kDecompPool + it->offset, it->length);
    } else {
        out.push_back(cp);
    }
}

bool compose_pair(char32_t a, char32_t b, char32_t& out) {
    if (a >= kLBase && a < kLBase + kLCount && b >= kVBase && b < kVBase + kVCount) {
        out = kSBase + ((a - kLBase) * kVCount + (b - kVBase)) * kTCount;
        return true;
    }
    if (a >= kSBase && a < kSBase + kSCount && (a - kSBase) % kTCount == 0 && b > kTBase &&
        b < kTBase + kTCount) {
        out = a + (b - kTBase);
        return true;
    }
    const auto* end = std::end(kCompositions);
    const auto* it = std::lower_bound(std::begin(kCompositions), end, std::make_pair(a, b),
                                      [](const Composition& c, const std::pair<char32_t, char32_t>& k) {
                                          return c.first != k.first ? c.first < k.first : c.second < k.second;
                                      });
    if (it != end && it->first == a && it->second == b) {
        out = it->composite;
        return true;
    }
    return false;
}

}  // namespace

const char* unidata_version() { return ARBTOK_ENSEMBLE_UNIDATA_VERSION; }

Category category(char32_t cp) {
    const auto* r = find_run(std::begin(kCategoryRuns), std::end(kCategoryRuns), cp);
    return r ? static_cast<Category>(r->category) : Category::Cn;
}

bool is_letter(char32_t cp) {
    switch (category(cp)) {
        case Category::Lu:
        case Category::Ll:
        case Category::Lt:
        case Category::Lm:
        case Category::Lo:
            return true;
        default:
            return false;
    }
}

unsigned char combining_class(char32_t cp) {
    const auto* r = find_run(std::begin(kCccRuns), std::end(kCccRuns), cp);
    return r ? r->ccc : 0;
}

std::u32string decode_utf8(std::string_view bytes) {
    std::u32string out;
    out.reserve(bytes.size());
    const auto* s = reinterpret_cast<const unsigned char*>(bytes.data());
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char b0 = s[i];
        if (b0 < 0x80) {
            out.push_back(b0);
            ++i;
            continue;
        }
        int need;
        char32_t cp;
        unsigned char lo = 0x80, hi = 0xBF;
        if (b0 >= 0xC2 && b0 <= 0xDF) {
            need = 1;
            cp = b0 & 0x1F;
        } else if (b0 >= 0xE0 && b0 <= 0xEF) {
            need = 2;
            cp = b0 & 0x0F;
            if (b0 == 0xE0) lo = 0xA0;
            if (b0 == 0xED) hi = 0x9F;
        } else if (b0 >= 0xF0 && b0 <= 0xF4) {
            need = 3;
            cp = b0 & 0x07;
            if (b0 == 0xF0) lo = 0x90;
            if (b0 == 0xF4) hi = 0x8F;
        } else {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        bool ok = true;
        for (int k = 0; k < need; ++k, ++j) {
            const unsigned char lo_k = k == 0 ? lo : 0x80;
            const unsigned char hi_k = k == 0 ? hi : 0xBF;
            if (j >= n || s[j] < lo_k || s[j] > hi_k) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (s[j] & 0x3F);
        }
        out.push_back(ok ? cp : 0xFFFD);
        i = j;
    }
    return out;
}

std::string encode_utf8(std::u32string_view text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (char32_t cp : text) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u32string nfd(std::u32string_view text) {
    std::u32string out;
    out.reserve(text.size());
    for (char32_t cp : text) decompose_into(cp, out);
    // Canonical ordering: a stable sort of each run of non-starters by class.
    std::size_t i = 0;
    while (i < out.size()) {
        if (combining_class(out[i]) == 0) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < out.size() && combining_class(out[j]) != 0) ++j;
        if (j - i > 1) {
            std::stable_sort(out.begin() + static_cast<std::ptrdiff_t>(i), out.begin() + static_cast<std::ptrdiff_t>(j),
                             [](char32_t a, char32_t b) { return combining_class(a) < combining_class(b); });
        }
        i = j;
    }
    return out;
}

std::u32string nfc(std::u32string_view text) {
    std::u32string s = nfd(text);
    if (s.empty()) return s;
    std::size_t starter = 0;
    int last_class = combining_class(s[0]);
    if (last_class != 0) last_class = 256;
    std::size_t write = 1;
    for (std::size_t read = 1; read < s.size(); ++read) {
        const char32_t ch = s[read];
        const int ch_class = combining_class(ch);
        char32_t composite;
        if ((last_class < ch_class || last_class == 0) && compose_pair(s[starter], ch, composite)) {
            s[starter] = composite;
            continue;
        }
        if (ch_class == 0) starter = write;
        last_class = ch_class;
        s[write++] = ch;
    }
    s.resize(write);
    return s;
}

}  // namespace arbtok::ensemble::detail
