#include "arbtok/constants.hpp"

#include <map>

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace c = arbtok::constants;

namespace {

const std::map<std::string, std::string>& strings() {
    static const std::map<std::string, std::string> m = {
        {"FATHA", c::FATHA}, {"DAMMA", c::DAMMA}, {"KASRA", c::KASRA}, {"SHADDA", c::SHADDA},
        {"SUKUN", c::SUKUN}, {"TANWIN_FATH", c::TANWIN_FATH}, {"TANWIN_DAMM", c::TANWIN_DAMM},
        {"TANWIN_KASR", c::TANWIN_KASR}, {"DAGGER_ALIF", c::DAGGER_ALIF}, {"MADD", c::MADD},
        {"HAMZA", c::HAMZA}, {"ALEF_MADDA", c::ALEF_MADDA}, {"ALEF_HAMZA_ABOVE", c::ALEF_HAMZA_ABOVE},
        {"WAW_HAMZA", c::WAW_HAMZA}, {"ALEF_HAMZA_BELOW", c::ALEF_HAMZA_BELOW}, {"YA_HAMZA", c::YA_HAMZA},
        {"ALIF", c::ALIF}, {"TA_MARBUTA", c::TA_MARBUTA}, {"WAW", c::WAW}, {"ALIF_MAKSURA", c::ALIF_MAKSURA},
        {"YA", c::YA}, {"LAM", c::LAM}, {"HAMZAT_AL_WASL", c::HAMZAT_AL_WASL}, {"ARABIC_PUNCT", c::ARABIC_PUNCT},
        {"PUNCT", c::PUNCT}, {"B", c::B}, {"R", c::R}, {"Z", c::Z}, {"S", c::S}, {"F", c::F}, {"Q", c::Q},
        {"K", c::K}, {"M", c::M}, {"N", c::N}, {"H", c::H}, {"T", c::T}, {"DJ", c::DJ}, {"X", c::X},
        {"D", c::D},
    };
    return m;
}

const std::map<std::string, const std::set<std::string>*>& sets() {
    static const std::map<std::string, const std::set<std::string>*> m = {
        {"ALEF_VARIANTS", &c::ALEF_VARIANTS}, {"SUN_LETTERS", &c::SUN_LETTERS}, {"CLITIC_BASES", &c::CLITIC_BASES},
    };
    return m;
}

}  // namespace

std::string record_constants(const Value& v) {
    const std::string& name = v[1].str;
    if (auto it = strings().find(name); it != strings().end()) return head(v) + J(it->second) + "]";
    if (auto it = sets().find(name); it != sets().end()) return head(v) + J(*it->second) + "]";
    throw std::runtime_error("unknown constant " + name);
}

}  // namespace parity
