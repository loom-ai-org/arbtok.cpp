#include "arbtok/textnorm.hpp"

#include "detail.hpp"

namespace arbtok::textnorm {

const std::set<std::string> ARABIC_LANGUAGE_CODES = {
    "arb", "aao", "abh", "abv", "acm", "acq", "acw", "acx", "acy", "adf", "aeb", "aec", "afb", "ajp", "apc",
    "apd", "arq", "ars", "ary", "arz", "auz", "avl", "ayh", "ayl", "ayn", "ayp", "bbz", "pga", "shu", "ssh",
};

namespace {
const std::set<std::string> kArabicLanguagesOutsideAra = {"mey"};
}

bool is_arabic_lang(const std::string& lang) {
    const detail::U32 folded = detail::replace(detail::lower(detail::u(lang)), U"_", U"-");
    const std::string language = detail::s8(detail::split(folded, U"-").front());
    return language == "ar" || ARABIC_LANGUAGE_CODES.count(language) || kArabicLanguagesOutsideAra.count(language);
}

}  // namespace arbtok::textnorm
