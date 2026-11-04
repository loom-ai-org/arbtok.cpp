#include "arbtok/diacritize.hpp"

#include "detail.hpp"

namespace arbtok::diacritize {

namespace {

using detail::U32;
using detail::U32View;

bool is_mark(char32_t c) { return (c >= 0x064B && c <= 0x0653) || c == 0x0670; }

U32 strip32(U32View text) {
    U32 out;
    for (char32_t c : text)
        if (!is_mark(c)) out.push_back(c);
    return out;
}

// b is in _RESTORABLE.get(a, ()).
bool restorable(char32_t a, char32_t b) {
    switch (a) {
        case 0x0627: return b == 0x0622 || b == 0x0623 || b == 0x0625 || b == 0x0671;
        case 0x0647: return b == 0x0629;
        case 0x064A: return b == 0x0649;
        default: return false;
    }
}

}  // namespace

std::string strip_marks(std::string_view text) { return detail::to_utf8(strip32(detail::from_utf8(text))); }

std::optional<std::string> repair_skeleton(std::string_view original, std::string_view proposed) {
    const U32 letters = strip32(detail::from_utf8(original));
    const U32 prop = detail::from_utf8(proposed);
    if (letters.size() != strip32(prop).size()) return std::nullopt;
    U32 out;
    std::size_t i = 0;
    for (char32_t c : prop) out.push_back(is_mark(c) ? c : letters[i++]);
    return detail::to_utf8(out);
}

bool skeleton_is_preserved(std::string_view original, std::string_view diacritized) {
    const U32 before = strip32(detail::from_utf8(original));
    const U32 after = strip32(detail::from_utf8(diacritized));
    if (before == after) return true;
    if (before.size() != after.size()) return false;
    for (std::size_t i = 0; i < before.size(); ++i)
        if (before[i] != after[i] && !restorable(before[i], after[i])) return false;
    return true;
}

bool has_arabic(std::string_view word) {
    for (char32_t c : detail::from_utf8(word))
        if ((c >= 0x0600 && c <= 0x06FF) || (c >= 0x0750 && c <= 0x077F) || (c >= 0xFB50 && c <= 0xFDFF) ||
            (c >= 0xFE70 && c <= 0xFEFF))
            return true;
    return false;
}

}  // namespace arbtok::diacritize
