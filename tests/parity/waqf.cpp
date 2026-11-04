#include "arbtok/waqf.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace w = arbtok::waqf;

std::string record_waqf(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") {
        const std::string& name = v[1].str;
        if (name == "LEXICAL_FINAL_VOWEL") return head(v) + J(w::LEXICAL_FINAL_VOWEL) + "]";
        if (name == "SHORT_VOWELS") return head(v) + J(w::SHORT_VOWELS) + "]";
        if (name == "DROPPED_TANWIN") return head(v) + J(w::DROPPED_TANWIN) + "]";
        if (name == "KEPT") return head(v) + J(w::KEPT) + "]";
        const std::pair<const char*, const std::string*> marks[] = {
            {"FATHA", &w::FATHA}, {"DAMMA", &w::DAMMA}, {"KASRA", &w::KASRA}, {"SUKUN", &w::SUKUN},
            {"SHADDA", &w::SHADDA}, {"TANWIN_FATH", &w::TANWIN_FATH}, {"TANWIN_DAMM", &w::TANWIN_DAMM},
            {"TANWIN_KASR", &w::TANWIN_KASR}, {"TA_MARBUTA", &w::TA_MARBUTA}};
        for (const auto& [mark, value] : marks)
            if (name == mark) return head(v) + J(*value) + "]";
    }
    if (fn == "pausal") return head(v) + J(w::pausal(v[1].str, v[2].boolean)) + "]";
    throw std::runtime_error("unknown waqf record " + fn);
}

}  // namespace parity
