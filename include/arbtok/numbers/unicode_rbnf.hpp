// The part of unicode-rbnf that arbtok calls. arbtok asks an engine for the
// cardinal of a word only when the word is all digits by str.isdigit() and not
// a number by float(); such a word holds a digit that is not a decimal digit,
// which decimal.Decimal rejects. That error is the whole reachable behaviour,
// so the rules themselves are not ported.
#pragma once

#include <string>
#include <string_view>

namespace arbtok::numbers::unicode_rbnf {

enum class FormatPurpose { CARDINAL, ORDINAL, YEAR };

class RbnfEngine {
public:
    // RbnfEngine.for_language(language). ValueError for a language with no
    // rules file.
    static RbnfEngine for_language(std::string_view language);

    // engine.format_number(number, purpose).text. InvalidOperation for a text
    // decimal.Decimal rejects; NotImplementedError for any other text, whose
    // reading needs the rules.
    std::string format_number(std::string_view number, FormatPurpose purpose = FormatPurpose::CARDINAL) const;

    const std::string& language() const { return language_; }

private:
    explicit RbnfEngine(std::string language) : language_(std::move(language)) {}
    std::string language_;
};

// The unicode-rbnf release this port holds parity with.
inline constexpr const char* kReferenceVersion = "2.4.1";

}  // namespace arbtok::numbers::unicode_rbnf
