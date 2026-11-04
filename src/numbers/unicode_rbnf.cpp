#include "arbtok/numbers/unicode_rbnf.hpp"

#include <algorithm>

#include "arbtok/numbers/errors.hpp"
#include "tables.hpp"
#include "text.hpp"

namespace arbtok::numbers::unicode_rbnf {

namespace {

// Whether decimal.Decimal(text) accepts text: surrounding whitespace, any
// underscores, any Unicode decimal digit, a sign, then a finite number with an
// optional exponent, an infinity or a NaN.
bool decimal_syntax(std::string_view number) {
    std::string s;
    for (char32_t c : text::strip_ws(text::u(number))) {
        if (c == U'_') continue;
        if (c < 0x80) {
            s.push_back(static_cast<char>(c >= U'A' && c <= U'Z' ? c - U'A' + U'a' : c));
        } else if (text::is_decimal(c)) {
            s.push_back(static_cast<char>('0' + text::decimal_value(c)));
        } else {
            return false;
        }
    }
    size_t p = 0;
    if (p < s.size() && (s[p] == '+' || s[p] == '-')) ++p;
    const std::string_view body = std::string_view(s).substr(p);
    if (body == "inf" || body == "infinity") return true;
    auto digits = [&](size_t& q) {
        const size_t from = q;
        while (q < body.size() && body[q] >= '0' && body[q] <= '9') ++q;
        return q - from;
    };
    size_t q = 0;
    if (body.substr(0, 4) == "snan" || body.substr(0, 3) == "nan") {
        q = body[0] == 's' ? 4 : 3;
        digits(q);
        return q == body.size();
    }
    size_t mantissa = digits(q);
    if (q < body.size() && body[q] == '.') {
        ++q;
        mantissa += digits(q);
    }
    if (mantissa == 0) return false;
    if (q < body.size() && body[q] == 'e') {
        ++q;
        if (q < body.size() && (body[q] == '+' || body[q] == '-')) ++q;
        if (digits(q) == 0) return false;
    }
    return q == body.size();
}

}  // namespace

RbnfEngine RbnfEngine::for_language(std::string_view language) {
    const auto& known = tables::rbnf_languages();
    if (std::find(known.begin(), known.end(), language) == known.end()) {
        throw ValueError(std::string(language) + " is not supported");
    }
    return RbnfEngine(std::string(language));
}

std::string RbnfEngine::format_number(std::string_view number, FormatPurpose) const {
    if (language_ != "ar") throw NotImplementedError("unicode-rbnf is ported for ar only");
    if (!decimal_syntax(number)) throw InvalidOperation("[<class 'decimal.ConversionSyntax'>]");
    throw NotImplementedError("unicode-rbnf rules are not ported");
}

}  // namespace arbtok::numbers::unicode_rbnf
