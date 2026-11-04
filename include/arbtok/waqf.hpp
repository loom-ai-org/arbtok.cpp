// arbtok.waqf: the pausal form, which drops the case and mood endings.
#pragma once

#include <set>
#include <string>
#include <vector>

namespace arbtok::waqf {

// The marks the pausal rules name.
inline const std::string FATHA = "\u064E", DAMMA = "\u064F", KASRA = "\u0650", SUKUN = "\u0652", SHADDA = "\u0651";
inline const std::string TANWIN_FATH = "\u064B", TANWIN_DAMM = "\u064C", TANWIN_KASR = "\u064D";
inline const std::string TA_MARBUTA = "\u0629";

// The words whose final short vowel is lexical and survives the pause.
extern const std::set<std::string> LEXICAL_FINAL_VOWEL;

// Fatha, damma, kasra: dropped word-finally.
extern const std::vector<std::string> SHORT_VOWELS;
// Tanwin damm and tanwin kasr: dropped with their /n/.
extern const std::vector<std::string> DROPPED_TANWIN;
// Sukun and shadda: kept.
extern const std::vector<std::string> KEPT;

// text in its pausal form: every Arabic word by default, or only the last word
// of each phrase with phrase_final_only.
std::string pausal(const std::string& text, bool phrase_final_only = false);

}  // namespace arbtok::waqf
