// arbtok.sandhi: the rules that act between words. Nun assimilation, the
// pausal form at a written pause, the elision of the article's vowel and the
// glottal onset of a hamzat al-wasl word.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace arbtok::sandhi {

// What a final /n/ of min and man becomes before each onset, in dict order.
extern const std::vector<std::pair<std::string, std::string>> NUN_ASSIMILATION;

// One word as the tokenizer assembles it.
struct CrossWord {
    std::string ipa;
    std::string surface;
    bool is_punct = false;
};

// The IPA of each spoken word after the cross-word rules. Punctuation is not
// in the result; it marks the pause before it.
std::vector<std::string> apply_cross_word(const std::vector<CrossWord>& words, bool pausal = true,
                                          const std::string& lang = "ar");

namespace detail {
// The private helpers that the goldens call.
std::string pausal_form(const std::string& ipa, const std::string& surface);
bool has_article(const std::string& surface);
bool is_bare_wasl_alif(const std::string& surface);
}  // namespace detail

}  // namespace arbtok::sandhi
