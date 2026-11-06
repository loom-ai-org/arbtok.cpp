// arbtok (the package): the names that the Python package re-exports at its
// top level, and the headers of the ported modules.
#pragma once

#include "arbtok/arabizi.hpp"
#include "arbtok/constants.hpp"
#include "arbtok/diacritize.hpp"
#include "arbtok/dialect_lexicon.hpp"
#include "arbtok/dialects.hpp"
#include "arbtok/donor_lexicon.hpp"
#include "arbtok/ensemble.hpp"
#include "arbtok/errors.hpp"
#include "arbtok/fusion.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/lexicon.hpp"
#include "arbtok/nisba.hpp"
#include "arbtok/o2i_plugins.hpp"
#include "arbtok/orthography.hpp"
#include "arbtok/plugin.hpp"
#include "arbtok/sandhi.hpp"
#include "arbtok/stress.hpp"
#include "arbtok/tashkeel.hpp"
#include "arbtok/textnorm.hpp"
#include "arbtok/tokenizer.hpp"
#include "arbtok/translit.hpp"
#include "arbtok/unicode_symbol2label.hpp"
#include "arbtok/version.hpp"
#include "arbtok/waqf.hpp"

namespace arbtok {

// The re-exports of arbtok/__init__.py. __version__ is version::VERSION_STR.
using dialects::Lect;
using dialects::spec_for_lang;
using dialects::supported_lects;
using textnorm::AsrNorm;
using textnorm::is_arabic_lang;
using textnorm::normalize_asr;
using textnorm::normalize_for_tts;
using textnorm::TtsNorm;

// vocalize(text, lect): text diacritized as lect speaks it, with a record per
// word (diacritize::diacritize).
inline diacritize::DiacritizedText vocalize(const std::string& text, const std::string& lect = "ar") {
    return diacritize::diacritize(text, lect);
}
inline diacritize::DiacritizedText vocalize(const std::string& text, const std::string& lect,
                                            const diacritize::Options& options) {
    return diacritize::diacritize(text, lect, options);
}

}  // namespace arbtok
