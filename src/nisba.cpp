#include "arbtok/nisba.hpp"

#include "detail.hpp"

namespace arbtok::nisba {

const std::set<std::string> NOT_NISBA = {
    "الذي", "التي", "ذي", "اللذي",
    "اللتي", "عندي", "لدي", "إلي",
    "الي", "علي", "في", "لي", "بي", "معي",
    "مني", "عني", "هي", "الاولي",
};

namespace {

constexpr char32_t kYa = 0x064A, kShadda = 0x0651, kKasra = 0x0650, kAlif = 0x0627;

bool is_mark(char32_t c) { return (c >= 0x064B && c <= 0x0652) || c == 0x0670; }

detail::U32 strip_marks(detail::U32View word) {
    detail::U32 out;
    for (char32_t c : word)
        if (!is_mark(c)) out.push_back(c);
    return out;
}

// re.fullmatch("[^ا]ا[^ا]ي", skeleton): the faʿil pattern of a III-weak root.
bool is_defective_participle(detail::U32View s) {
    return s.size() == 4 && s[0] != kAlif && s[1] == kAlif && s[2] != kAlif && s[3] == kYa;
}

}  // namespace

std::string restore_nisba(const std::string& word_utf8) {
    const detail::U32 word = detail::u(word_utf8);
    const detail::U32 skeleton = strip_marks(word);
    if (word.empty() || word.back() != kYa || skeleton.size() < 3) return word_utf8;
    if (NOT_NISBA.count(detail::s8(skeleton)) || is_defective_participle(skeleton)) return word_utf8;
    if (detail::contains(word, kShadda)) return word_utf8;
    detail::U32 body = word.substr(0, word.size() - 1);
    while (!body.empty() && is_mark(body.back())) body.pop_back();
    if (body.empty()) return word_utf8;
    body.push_back(kKasra);
    body.push_back(kYa);
    body.push_back(kShadda);
    return detail::s8(body);
}

}  // namespace arbtok::nisba
