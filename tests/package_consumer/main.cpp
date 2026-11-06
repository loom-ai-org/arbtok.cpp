// Built against an installed arbtok to prove the CMake package and the
// installed data directories work: the rule modules, the bundled model, the
// closed-class lexicons, the bundled onnxruntime, the G2P plugin and the
// orthography2ipa plugins. The stem lexicon is left out, because it comes from
// the network. arbtok.hpp is the only arbtok header it names.
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
    arbtok::plugin::Options plugin_options;
    plugin_options.lang = "ar-EG";
    plugin_options.lexicon.reset();
    arbtok::plugin::ArbtokG2PPlugin g2p(plugin_options);
    const std::string spoken = g2p.transcribe("السلام عليكم");
    arbtok::o2i_plugins::register_plugins();
    orthography2ipa::G2P o2i("ar", {{"sandhi", {"arbtok"}}});
    const std::string sandhi = o2i.transcribe("السلام عليكم");
    const bool ok = g2p.lang == "arz-EG" && spoken == "assaˈlaːm ʕaˈleːkam" && sandhi == "alsˈlaːm ˈʕljkm" &&
                    arbtok::translit::transliterate("meeting", "ar") == "miːtink" &&
                    arbtok::spec_for_lang(std::string("ar-EG")) == "arz-EG" && arbtok::is_arabic_lang("ar_SA") &&
                    arbtok::unicode_symbol2label::get("%").has_value() && !arbtok::supported_lects().empty() &&
                    ipa == "ˈkataba lˈwaladu" && std::string(arbtok::version::VERSION_STR) == "0.0.2a71" &&
                    msa.text == "ذَهَب الْوَلَد إِلَى الْمُدَرَّسَة" && egyptian.text == "مِش عَارِف" &&
                    egyptian.words.at(0).provenance == "closed-class" && fused == "ذَهَب الْوَلَد إِلَى الْمُدْرَسَة" &&
                    pausal == "ذَهَب الوَلَد";
    std::cout << ipa << "\n" << msa.text << "\n" << egyptian.text << "\n" << fused << "\n" << pausal << "\n"
              << spoken << "\n" << sandhi << "\n";
    return ok ? 0 : 1;
}
