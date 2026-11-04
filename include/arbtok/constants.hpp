// arbtok.constants: the Arabic letters, marks and letter classes, as UTF-8.
#pragma once

#include <set>
#include <string>

namespace arbtok::constants {

// Diacritics (tashkeel).
inline const std::string FATHA = "َ";
inline const std::string DAMMA = "ُ";
inline const std::string KASRA = "ِ";
inline const std::string SHADDA = "ّ";
inline const std::string SUKUN = "ْ";
inline const std::string TANWIN_FATH = "ً";
inline const std::string TANWIN_DAMM = "ٌ";
inline const std::string TANWIN_KASR = "ٍ";
inline const std::string DAGGER_ALIF = "ٰ";
inline const std::string MADD = "ٓ";

// Letters and variants.
inline const std::string HAMZA = "ء";
inline const std::string ALEF_MADDA = "آ";
inline const std::string ALEF_HAMZA_ABOVE = "أ";
inline const std::string WAW_HAMZA = "ؤ";
inline const std::string ALEF_HAMZA_BELOW = "إ";
inline const std::string YA_HAMZA = "ئ";
inline const std::string ALIF = "ا";
inline const std::string TA_MARBUTA = "ة";
inline const std::string WAW = "و";
inline const std::string ALIF_MAKSURA = "ى";
inline const std::string YA = "ي";
inline const std::string LAM = "ل";
inline const std::string HAMZAT_AL_WASL = "ٱ";

inline const std::set<std::string> ALEF_VARIANTS = {ALIF, ALEF_MADDA, ALEF_HAMZA_ABOVE, ALEF_HAMZA_BELOW,
                                                     HAMZAT_AL_WASL};

// Punctuation: string.punctuation, then the Arabic comma, semicolon, question
// mark, tatweel and ellipsis.
inline const std::string ARABIC_PUNCT = "،؛؟ـ…";
inline const std::string PUNCT = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" + ARABIC_PUNCT;

// The sun letters (shamsiyya), which assimilate the lam of the article.
inline const std::set<std::string> SUN_LETTERS = {
    "ت", "ث", "د", "ذ", "ر", "ز", "س",
    "ش", "ص", "ض", "ط", "ظ", "ل", "ن",
};

// The proclitic letters: and, so, by, like, for, the future marker.
inline const std::set<std::string> CLITIC_BASES = {WAW, "ف", "ب", "ك", LAM, "س"};

// Letters named by the phoneme they spell.
inline const std::string B = "ب";
inline const std::string R = "ر";
inline const std::string Z = "ز";
inline const std::string S = "س";
inline const std::string F = "ف";
inline const std::string Q = "ق";
inline const std::string K = "ك";
inline const std::string M = "م";
inline const std::string N = "ن";
inline const std::string H = "ه";
inline const std::string T = "ت";
inline const std::string DJ = "ج";
inline const std::string X = "خ";
inline const std::string D = "د";

}  // namespace arbtok::constants
