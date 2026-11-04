// Built against an installed arbtok to prove the CMake package and the
// installed data directories work.
#include <arbtok/arbtok.hpp>

#include <iostream>
#include <string>

int main() {
    const arbtok::tokenizer::Sentence sentence("كَتَبَ الْوَلَدُ", "ar");
    const std::string ipa = sentence.ipa();
    const bool ok = arbtok::spec_for_lang(std::string("ar-EG")) == "arz-EG" && arbtok::is_arabic_lang("ar_SA") &&
                    arbtok::unicode_symbol2label::get("%").has_value() && !arbtok::supported_lects().empty() &&
                    ipa == "ˈkataba lˈwaladu" && std::string(arbtok::version::VERSION_STR) == "0.0.2a71";
    std::cout << ipa << "\n";
    return ok ? 0 : 1;
}
