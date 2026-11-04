#include "arbtok/tokenizer.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace tk = arbtok::tokenizer;

namespace {

std::string char_record(const tk::CharToken& t) {
    return list({J(t.surface()), J(t.char_idx()), J(t.is_first_char()), J(t.is_first_word()), J(t.is_last_char()),
                 J(t.is_last_word()), J(t.is_punct()), J(t.lang()), J(t.normalized()), J(t.is_vowel()),
                 J(t.has_diacritic()), J(t.has_shada()), J(t.has_sukun()), J(t.has_vowel_mark()), J(t.is_sun()),
                 J(t.is_moon()), J(t.is_silent()), J(t.is_tanwin()), J(t.ipa())});
}

std::string word_record(const tk::WordToken& w) {
    std::vector<std::string> chars;
    for (const auto& t : w.tokens()) chars.push_back(char_record(t));
    return list({J(w.surface()), J(w.word_idx()), J(w.lang()), J(w.is_punct()), J(w.is_first_word()),
                 J(w.is_last_word()), J(w.end_with_vowel()), J(w.normalized()), J(w.has_definite_article()),
                 J(w.is_proclitic()), J(w.is_sun()), J(w.is_reference_register()), J(w.ipa()), list(chars)});
}

}  // namespace

std::string record_tokenizer(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "normalize_unicode") return head(v) + J(tk::normalize_unicode(v[1].str)) + "]";
    if (fn == "elide_silent_alif") return head(v) + J(tk::elide_silent_alif(v[1].str)) + "]";
    if (fn == "_reorder_diacritics") return head(v) + J(tk::detail::reorder_diacritics(v[1].str)) + "]";
    if (fn == "Sentence") {
        const tk::Sentence s(v[1].str, v[2].str, v[3].boolean, v[4].boolean);
        std::vector<std::string> surfaces;
        for (const auto& w : s.tokens()) surfaces.push_back(w.surface());
        return head(v) + list({J(s.normalized()), J(surfaces), guard([&] { return J(s.ipa()); })}) + "]";
    }
    if (fn == "Sentence.tokens") {
        return head(v) + guard([&] {
                   const tk::Sentence s(v[1].str, v[2].str);
                   std::vector<std::string> words;
                   for (const auto& w : s.tokens()) words.push_back(word_record(w));
                   return list(words);
               }) +
               "]";
    }
    throw std::runtime_error("unknown tokenizer record " + fn);
}

}  // namespace parity
