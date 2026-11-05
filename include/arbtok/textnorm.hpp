// arbtok.textnorm: text normalization for speech recognition and synthesis.
//
// normalize_asr prepares a transcript for comparison against another; every
// rule is a flag of AsrNorm that is off unless asked for. normalize_for_tts
// prepares text to be spoken; its rules are the flags of TtsNorm. The rules,
// their order and their bytes are those of arbtok 0.0.2a71.
#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace arbtok::textnorm {

// The ISO 639-3 codes of the individual languages of the Arabic macrolanguage.
extern const std::set<std::string> ARABIC_LANGUAGE_CODES;

// True when the primary subtag of lang names an Arabic variety. Case and an
// underscore in place of a hyphen do not matter.
bool is_arabic_lang(const std::string& lang);

// A lexicon as a frozen config holds it: (spelling or term, what it stands
// for) pairs, sorted. Keep it in the form with_lexicon() gives.
using Lexicon = std::vector<std::pair<std::string, std::string>>;
// (value, word) pairs, sorted by value.
using NumberForms = std::vector<std::pair<long long, std::string>>;
// A mapping in its insertion order.
using Pairs = std::vector<std::pair<std::string, std::string>>;

// The rules normalize_asr applies, in the order they run. All off by default.
struct AsrNorm {
    bool nfc = false;
    bool strip_event_markup = false;
    bool strip_controls = false;
    bool punctuation_before_marks = false;
    bool strip_harakat = false;
    bool strip_extended_marks = false;
    bool strip_quranic_marks = false;
    bool strip_tatweel = false;
    bool split_term_proclitics = false;
    bool fix_asr_errors = false;
    bool unify_spoken_variants = false;
    bool unify_dialect_spellings = false;
    bool number_forms = false;
    bool spoken_numbers_to_digits = false;
    bool join_dictated_digits = false;
    bool spell_codes = false;
    bool unify_alef = false;
    bool unify_ta_marbuta = false;
    bool unify_alef_maqsura = false;
    bool unify_hamza_carriers = false;
    bool unify_loan_letters = false;
    bool fold_digits = false;
    bool fold_case = false;
    bool arabic_block_only = false;
    bool blank_punctuation = false;
    bool drop_word_final_hamza = false;
    bool join_clitics = false;
    bool collapse_whitespace = false;
    Lexicon lexicon;

    // This config with `entries` as its lexicon, sorted. ValueError for an
    // entry whose spelling is empty or only whitespace.
    AsrNorm with_lexicon(const Pairs& entries) const;
    // The rule set, the rules that are on, the lexicon by digest, and the
    // parser versions when numbers are read or recognizer errors repaired.
    std::string describe() const;

    // The bool fields in declaration order, for code that sets a rule by name.
    struct Field {
        const char* name;
        bool AsrNorm::*member;
    };
    static const std::vector<Field>& fields();
    friend bool operator==(const AsrNorm& a, const AsrNorm& b);
};

// The rules normalize_for_tts applies, in the order they run. spoken_forms,
// canonical_unicode and drop_false_starts are on by default.
struct TtsNorm {
    bool strip_controls = false;
    bool spell_out_plates = false;
    bool spell_out_codes = false;
    bool speak_percent = false;
    bool keep_code_digits = false;
    std::vector<std::string> phone_shapes;
    std::vector<std::string> phone_regions;
    bool long_digit_runs = false;
    std::vector<std::string> phone_prefixes;
    std::vector<std::string> identifier_words;
    bool cardinal_numbers = false;
    bool colloquial_clock = false;
    bool colloquial_years = false;
    bool colloquial_units = false;
    bool leave_unspeakable_numbers = false;
    bool oblique_numbers = false;
    bool space_fused_hundreds = false;
    bool dialect_numbers = false;
    NumberForms number_forms;
    // Empty, or a word for each of the twelve months, January first.
    std::vector<std::string> month_names;
    bool spoken_forms = true;
    bool canonical_unicode = true;
    bool drop_false_starts = true;
    bool plain_orthography = false;
    Lexicon lexicon;

    TtsNorm with_lexicon(const Pairs& entries) const;
    // TypeError for a negative value or an empty word. A later pair for the
    // same value wins; the words are stripped.
    TtsNorm with_number_forms(const NumberForms& forms) const;
    // ValueError when month_names is neither empty nor twelve words. Python
    // raises it when the config is made; normalize_for_tts checks it.
    void check() const;
    std::string describe() const;

    struct Field {
        const char* name;
        bool TtsNorm::*member;
    };
    static const std::vector<Field>& fields();
    friend bool operator==(const TtsNorm& a, const TtsNorm& b);
};

// The named bundles.
extern const AsrNorm TRUTH_CHECK;
extern const AsrNorm CER_STRIP;
extern const AsrNorm CER_NORM;
extern const AsrNorm CER_MARKS_FIRST;
extern const AsrNorm CER_NORM_MARKS_FIRST;
extern const AsrNorm INTELLIGIBILITY_GATE;
// CER_NORM_MARKS_FIRST with the canonical forms of numbers, codes and terms.
// Its lexicon is canonical_term_lexicon(); it is built at first use.
const AsrNorm& CER_CANONICAL();

extern const std::vector<std::string> IDENTIFIER_WORDS;
extern const std::vector<std::string> ARAB_PHONE_REGIONS;
// The digests that name the two rule sets.
extern const std::string ASR_NORM_VERSION;
extern const std::string TTS_NORM_VERSION;

// Normalize a transcript for comparison. lang is the language whose number
// words spoken_numbers_to_digits reads; the rules that read Arabic words run
// only for an Arabic lang.
std::string normalize_asr(const std::string& text, const AsrNorm& config = AsrNorm(), const std::string& lang = "ar");

// Prepare text to be spoken. ValueError when a rule that speaks Arabic is on
// and lang is not Arabic. The spoken forms of numbers, dates and times are
// ported for Arabic only: where the reference speaks them with the module of
// another language, the call raises NotPortedError.
std::string normalize_for_tts(const std::string& text, const std::string& lang = "ar",
                              const TtsNorm& config = TtsNorm());

// The bundled code spellings: each Latin capital and ASCII digit to its name.
// ValueError for a language with no table.
Pairs spelled_codes(const std::string& lang = "ar");
// Unit symbols to their names in lang, from CLDR. ValueError for a language
// other than Arabic.
Pairs cldr_units(const std::string& lang = "ar");
// The bundled term lexicons. ValueError for an unknown name and, in
// bundled_asr_lexicon, for a CLDR table.
Pairs bundled_asr_lexicon(const std::string& name = "cars-sa");
Pairs bundled_tts_lexicon(const std::string& name = "cars-sa", bool pointed = true);
Pairs canonical_term_lexicon(const std::vector<std::string>& names = {"cars-sa", "common-en", "spoken-sa"});

}  // namespace arbtok::textnorm
