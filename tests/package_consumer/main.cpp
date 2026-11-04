// Built against an installed arbtok to prove the CMake package and the
// installed data directories work: the rule modules, the bundled model, the
// closed-class lexicons and the bundled onnxruntime. The stem lexicon is
// left out, because it comes from the network.
#include <arbtok/arbtok.hpp>

#include <iostream>
#include <string>

int main() {
    const arbtok::tokenizer::Sentence sentence("كَتَبَ الْوَلَدُ", "ar");
    const std::string ipa = sentence.ipa();
    arbtok::diacritize::Options no_stems;
    no_stems.lexicon.reset();
    const auto msa = arbtok::vocalize("ذهب الولد إلى المدرسة", "ar", no_stems);
    const auto egyptian = arbtok::vocalize("مش عارف", "arz-EG", no_stems);
    arbtok::fusion::Options fusion_options;
    fusion_options.lexicon.reset();
    arbtok::fusion::FusionDiacritizer fusion("ar", fusion_options);
    const std::string fused = fusion.diacritize("ذهب الولد إلى المدرسة");
    const std::string pausal = arbtok::tashkeel::TashkeelDiacritizer().diacritize("ذهب الولد", true);
    const bool ok = arbtok::spec_for_lang(std::string("ar-EG")) == "arz-EG" && arbtok::is_arabic_lang("ar_SA") &&
                    arbtok::unicode_symbol2label::get("%").has_value() && !arbtok::supported_lects().empty() &&
                    ipa == "ˈkataba lˈwaladu" && std::string(arbtok::version::VERSION_STR) == "0.0.2a71" &&
                    msa.text == "ذَهَب الْوَلَد إِلَى الْمُدَرَّسَة" && egyptian.text == "مِش عَارِف" &&
                    egyptian.words.at(0).provenance == "closed-class" && fused == "ذَهَب الْوَلَد إِلَى الْمُدْرَسَة" &&
                    pausal == "ذَهَب الوَلَد";
    std::cout << ipa << "\n" << msa.text << "\n" << egyptian.text << "\n" << fused << "\n" << pausal << "\n";
    return ok ? 0 : 1;
}
