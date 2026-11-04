// arbtok (the package): the names that the Python package re-exports at its
// top level and that this port holds. AsrNorm, TtsNorm, normalize_asr,
// normalize_for_tts and vocalize are not ported.
#pragma once

#include "arbtok/arabizi.hpp"
#include "arbtok/constants.hpp"
#include "arbtok/dialects.hpp"
#include "arbtok/errors.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/nisba.hpp"
#include "arbtok/orthography.hpp"
#include "arbtok/sandhi.hpp"
#include "arbtok/stress.hpp"
#include "arbtok/tashkeel.hpp"
#include "arbtok/textnorm.hpp"
#include "arbtok/tokenizer.hpp"
#include "arbtok/unicode_symbol2label.hpp"
#include "arbtok/version.hpp"
#include "arbtok/waqf.hpp"

namespace arbtok {

// The re-exports of arbtok/__init__.py. __version__ is version::VERSION_STR.
using dialects::Lect;
using dialects::spec_for_lang;
using dialects::supported_lects;
using textnorm::is_arabic_lang;

}  // namespace arbtok
