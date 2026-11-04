#include "gemination.hpp"

namespace arbtok::detail {

namespace {

constexpr char32_t kShadda = 0x0651;

// _AR_HARAKAT: U+064B to U+0650, U+0652 and U+0670.
bool is_haraka(char32_t c) { return (c >= 0x064B && c <= 0x0650) || c == 0x0652 || c == 0x0670; }

// One leftmost, non-overlapping substitution pass over a three-symbol pattern.
template <class Match, class Emit>
U32 pass(U32View text, size_t width, Match match, Emit emit) {
    U32 out;
    size_t i = 0;
    while (i < text.size()) {
        if (i + width <= text.size() && match(text.substr(i, width))) {
            emit(out, text.substr(i, width));
            i += width;
        } else {
            out.push_back(text[i++]);
        }
    }
    return out;
}

}  // namespace

U32 expand_arabic_gemination(U32View text) {
    if (!contains(text, kShadda)) return U32(text);
    const U32 one = pass(
        text, 3, [](U32View m) { return is_arabic_script_letter(m[0]) && m[1] == kShadda && is_haraka(m[2]); },
        [](U32& out, U32View m) { out += {m[0], m[0], m[2]}; });
    const U32 two = pass(
        one, 3, [](U32View m) { return is_arabic_script_letter(m[0]) && is_haraka(m[1]) && m[2] == kShadda; },
        [](U32& out, U32View m) { out += {m[0], m[0], m[1]}; });
    return pass(
        two, 2, [](U32View m) { return is_arabic_script_letter(m[0]) && m[1] == kShadda; },
        [](U32& out, U32View m) { out += {m[0], m[0]}; });
}

}  // namespace arbtok::detail
