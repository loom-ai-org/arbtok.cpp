// arbtok.orthography: what the writing already decided, and what it left to
// the diacritizer. A mark is a one-code-point UTF-8 string, a class is the
// UTF-8 string of a whole combining-mark sequence, and a set of marks is a
// std::set, which iterates in code-point order.
#pragma once

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace arbtok::orthography {

using MarkSet = std::set<std::string>;

// Hamza above, hamza below, madda: marks that are part of the spelling.
inline const MarkSet ORTHOGRAPHIC_MARKS = {"ٔ", "ٕ", "ٓ"};

// The madda, which makes its alif vowel-complete.
inline const std::string MADDA = "ٓ";

// The tanwin, the short vowels, shadda, sukun and the dagger alif: the marks
// that the writing omits.
inline const MarkSet VOCALIC_MARKS = {"ً", "ٌ", "ٍ", "َ", "ُ",
                                      "ِ", "ّ", "ْ", "ٰ"};

// The base characters of normalize(text), and the set of nonspacing marks
// (category Mn) on each. A mark before the first base is dropped.
std::pair<std::string, std::vector<MarkSet>> source_marks(
    const std::string& text, const std::function<std::string(const std::string&)>& normalize);

// The class ids that a letter carrying marks_here may take.
std::vector<int> allowed_classes(const std::vector<std::string>& classes, const MarkSet& marks_here,
                                 bool allow_restoration = true);

// The shortest class whose mark set is marks_here, or -1 when none is.
int pinned_class(const std::vector<std::string>& classes, const MarkSet& marks_here);

// The predicted class with its orthographic marks replaced by those of the
// source. classes.at(predicted) throws std::out_of_range where Python raises
// IndexError.
int project_class(const std::vector<std::string>& classes, int predicted, const MarkSet& marks_here,
                  bool allow_restoration = true);

}  // namespace arbtok::orthography
