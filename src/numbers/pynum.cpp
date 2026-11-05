#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

#include "arbtok/numbers/errors.hpp"
#include "arbtok/numbers/pynum.hpp"
#include "text.hpp"

namespace arbtok::numbers {


namespace {

bool ascii_digit(char c) { return c >= '0' && c <= '9'; }
bool ascii_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

std::string strip_leading_zeros(const std::string& digits) {
    const size_t first = digits.find_first_not_of('0');
    return first == std::string::npos ? "0" : digits.substr(first);
}

// digits * 2, for a string of decimal digits.
void double_digits(std::string& digits) {
    int carry = 0;
    for (size_t i = digits.size(); i-- > 0;) {
        const int d = (digits[i] - '0') * 2 + carry;
        digits[i] = static_cast<char>('0' + d % 10);
        carry = d / 10;
    }
    if (carry) digits.insert(digits.begin(), static_cast<char>('0' + carry));
}

// _PyUnicode_TransformDecimalAndSpaceToASCII: an ASCII code point stays, other
// whitespace becomes ' ', a decimal digit becomes its ASCII digit, and any
// other code point makes the text invalid.
std::optional<std::string> to_ascii(std::string_view text) {
    std::string ascii;
    for (char32_t c : text::from_utf8(text)) {
        if (c < 0x80) {
            ascii.push_back(static_cast<char>(c));
        } else if (text::is_space(c)) {
            ascii.push_back(' ');
        } else if (int d = text::decimal_value(c); d >= 0) {
            ascii.push_back(static_cast<char>('0' + d));
        } else {
            return std::nullopt;
        }
    }
    return ascii;
}

// Python's _Py_dg_strtod grammar: does the whole text parse as a decimal float?
bool decimal_float_syntax(const std::string& s) {
    size_t i = 0, n = s.size();
    if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
    size_t digits = 0;
    while (i < n && ascii_digit(s[i])) ++i, ++digits;
    if (i < n && s[i] == '.') {
        ++i;
        while (i < n && ascii_digit(s[i])) ++i, ++digits;
    }
    if (digits == 0) return false;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        size_t j = i + 1;
        if (j < n && (s[j] == '+' || s[j] == '-')) ++j;
        size_t exp_digits = 0;
        while (j < n && ascii_digit(s[j])) ++j, ++exp_digits;
        if (exp_digits > 0) i = j;
    }
    return i == n;
}

// The decimal exponent of the leading non-zero digit of a valid decimal
// float text without a sign, saturated; used only to tell an overflow from an
// underflow.
long long leading_exponent(const std::string& s) {
    long long position = 0;  // exponent of the next integer-part digit, counted later
    size_t i = 0;
    std::string mantissa;
    long long int_digits = 0;
    while (i < s.size() && ascii_digit(s[i])) mantissa.push_back(s[i++]), ++int_digits;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && ascii_digit(s[i])) mantissa.push_back(s[i++]);
    }
    long long exp = 0;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        bool neg = false;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) neg = s[i++] == '-';
        for (; i < s.size(); ++i) {
            if (exp < 1000000000LL) exp = exp * 10 + (s[i] - '0');
        }
        if (neg) exp = -exp;
    }
    const size_t first = mantissa.find_first_not_of('0');
    position = int_digits - 1 - static_cast<long long>(first);
    return position + exp;
}

}  // namespace

BigInt::BigInt(long long value) {
    negative = value < 0;
    const unsigned long long magnitude =
        negative ? 0ULL - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    digits = std::to_string(magnitude);
}

BigInt BigInt::from_string(std::string_view text) {
    BigInt out;
    size_t i = 0;
    if (!text.empty() && text[0] == '-') {
        out.negative = true;
        i = 1;
    }
    if (i == text.size()) throw std::invalid_argument("BigInt: no digits");
    for (size_t k = i; k < text.size(); ++k) {
        if (!ascii_digit(text[k])) throw std::invalid_argument("BigInt: not a digit");
    }
    out.digits = strip_leading_zeros(std::string(text.substr(i)));
    if (out.is_zero()) out.negative = false;
    return out;
}

BigInt BigInt::from_double(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("BigInt: not a finite double");
    const double t = std::trunc(value);
    BigInt out;
    if (t == 0) return out;
    int exp = 0;
    const double m = std::frexp(std::fabs(t), &exp);  // |t| = m * 2^exp, m in [0.5, 1)
    std::uint64_t mantissa = static_cast<std::uint64_t>(std::ldexp(m, 53));
    exp -= 53;
    if (exp >= 0) {
        out.digits = std::to_string(mantissa);
        for (int k = 0; k < exp; ++k) double_digits(out.digits);
    } else {
        out.digits = std::to_string(mantissa >> (-exp));
    }
    out.negative = t < 0;
    return out;
}

std::optional<BigInt> py_int(std::string_view text) {
    const auto ascii = to_ascii(text);
    if (!ascii) return std::nullopt;
    const std::string& s = *ascii;
    size_t i = 0, n = s.size();
    while (i < n && ascii_space(s[i])) ++i;
    bool negative = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) negative = s[i++] == '-';
    std::string digits;
    bool prev_digit = false;
    while (i < n && (ascii_digit(s[i]) || s[i] == '_')) {
        if (s[i] == '_') {
            // An underscore sits between two digits.
            if (!prev_digit || i + 1 >= n || !ascii_digit(s[i + 1])) return std::nullopt;
            prev_digit = false;
        } else {
            digits.push_back(s[i]);
            prev_digit = true;
        }
        ++i;
    }
    while (i < n && ascii_space(s[i])) ++i;
    if (i != n || digits.empty()) return std::nullopt;
    // sys.get_int_max_str_digits(): Python refuses more than 4300 digits.
    if (digits.size() > 4300) return std::nullopt;
    BigInt out;
    out.digits = strip_leading_zeros(digits);
    out.negative = negative && !out.is_zero();
    return out;
}

std::optional<double> py_float(std::string_view text) {
    auto ascii = to_ascii(text);
    if (!ascii) return std::nullopt;
    // _Py_string_to_number_with_underscores: each '_' must sit between digits.
    if (ascii->find('_') != std::string::npos) {
        std::string kept;
        char prev = '\0';
        for (char c : *ascii) {
            if (c == '_') {
                if (!ascii_digit(prev)) return std::nullopt;
            } else {
                if (prev == '_' && !ascii_digit(c)) return std::nullopt;
                kept.push_back(c);
            }
            prev = c;
        }
        if (prev == '_') return std::nullopt;
        *ascii = kept;
    }
    // float_from_string_inner: strip ASCII whitespace, then parse it all.
    size_t b = 0, e = ascii->size();
    while (b < e && ascii_space((*ascii)[b])) ++b;
    while (e > b && ascii_space((*ascii)[e - 1])) --e;
    std::string s = ascii->substr(b, e - b);
    bool negative = false;
    std::string body = s;
    if (!body.empty() && (body[0] == '+' || body[0] == '-')) {
        negative = body[0] == '-';
        body.erase(0, 1);
    }
    if (decimal_float_syntax(s)) {
        double value = 0;
        const auto res = std::from_chars(body.data(), body.data() + body.size(), value, std::chars_format::general);
        if (res.ec == std::errc::result_out_of_range) {
            value = leading_exponent(body) > 0 ? std::numeric_limits<double>::infinity() : 0.0;
        } else if (res.ec != std::errc() || res.ptr != body.data() + body.size()) {
            return std::nullopt;
        }
        return negative ? -value : value;
    }
    std::string lowered;
    for (char c : body) lowered.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    if (lowered == "inf" || lowered == "infinity") {
        const double inf = std::numeric_limits<double>::infinity();
        return negative ? -inf : inf;
    }
    if (lowered == "nan") {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return negative ? -nan : nan;
    }
    return std::nullopt;
}

std::string float_repr(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::scientific);
    std::string s(buf, res.ptr);
    std::string sign;
    if (s[0] == '-') {
        sign = "-";
        s.erase(0, 1);
    }
    const size_t e = s.find('e');
    const int exp = std::stoi(s.substr(e + 1));
    std::string digits;
    for (char c : s.substr(0, e)) {
        if (c != '.') digits.push_back(c);
    }
    if (exp >= -4 && exp < 16) {
        if (exp < 0) return sign + "0." + std::string(static_cast<size_t>(-exp - 1), '0') + digits;
        const size_t int_len = static_cast<size_t>(exp) + 1;
        if (digits.size() <= int_len) return sign + digits + std::string(int_len - digits.size(), '0') + ".0";
        return sign + digits.substr(0, int_len) + "." + digits.substr(int_len);
    }
    std::string mantissa = digits.substr(0, 1);
    if (digits.size() > 1) mantissa += "." + digits.substr(1);
    const int a = std::abs(exp);
    return sign + mantissa + "e" + (exp < 0 ? "-" : "+") + (a < 10 ? "0" : "") + std::to_string(a);
}

namespace {

// Magnitude arithmetic on decimal digit strings without leading zeros.
int compare_magnitude(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    const int c = a.compare(b);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

std::string add_magnitude(const std::string& a, const std::string& b) {
    std::string out;
    int carry = 0;
    size_t i = a.size(), j = b.size();
    while (i > 0 || j > 0 || carry) {
        const int d = (i > 0 ? a[--i] - '0' : 0) + (j > 0 ? b[--j] - '0' : 0) + carry;
        out.push_back(static_cast<char>('0' + d % 10));
        carry = d / 10;
    }
    return strip_leading_zeros(std::string(out.rbegin(), out.rend()));
}

// a - b for a >= b.
std::string sub_magnitude(const std::string& a, const std::string& b) {
    std::string out;
    int borrow = 0;
    size_t i = a.size(), j = b.size();
    while (i > 0) {
        int d = (a[--i] - '0') - (j > 0 ? b[--j] - '0' : 0) - borrow;
        borrow = d < 0;
        if (borrow) d += 10;
        out.push_back(static_cast<char>('0' + d));
    }
    return strip_leading_zeros(std::string(out.rbegin(), out.rend()));
}

std::string mul_magnitude(const std::string& a, const std::string& b) {
    std::vector<int> acc(a.size() + b.size(), 0);
    for (size_t i = a.size(); i-- > 0;) {
        for (size_t j = b.size(); j-- > 0;) acc[i + j + 1] += (a[i] - '0') * (b[j] - '0');
    }
    for (size_t k = acc.size(); k-- > 1;) {
        acc[k - 1] += acc[k] / 10;
        acc[k] %= 10;
    }
    std::string out;
    for (int d : acc) out.push_back(static_cast<char>('0' + d));
    return strip_leading_zeros(out);
}

BigInt make(bool negative, std::string digits) {
    BigInt out;
    out.digits = std::move(digits);
    out.negative = negative && !out.is_zero();
    return out;
}

// The decimal text of |a| / |b| with enough digits that strtod rounds it as
// the exact quotient rounds; a remainder is kept as a final sticky 1.
std::string quotient_text(const std::string& a, const std::string& b) {
    std::string integer, rem = "0";
    auto step = [&](int digit) {
        rem = strip_leading_zeros(rem + static_cast<char>('0' + digit));
        int count = 0;
        while (compare_magnitude(rem, b) >= 0) {
            rem = sub_magnitude(rem, b);
            ++count;
        }
        return count;
    };
    for (char c : a) integer.push_back(static_cast<char>('0' + step(c - '0')));
    integer = strip_leading_zeros(integer);
    size_t significant = integer == "0" ? 0 : integer.size();
    std::string fraction;
    while (rem != "0" && significant < 800) {
        const int d = step(0);
        fraction.push_back(static_cast<char>('0' + d));
        if (significant || d) ++significant;
        // Below 1e-400 a quotient rounds to zero.
        if (!significant && fraction.size() > 400) return "0";
    }
    if (rem != "0") fraction.push_back('1');
    return fraction.empty() ? integer : integer + "." + fraction;
}

}  // namespace

double BigInt::to_double() const {
    const double magnitude = std::strtod(digits.c_str(), nullptr);
    if (std::isinf(magnitude)) throw OverflowError("int too large to convert to float");
    return negative ? -magnitude : magnitude;
}

std::optional<long long> BigInt::to_ll() const {
    if (digits.size() > 18) return std::nullopt;
    const long long magnitude = std::stoll(digits);
    return negative ? -magnitude : magnitude;
}

int compare(const BigInt& a, const BigInt& b) {
    if (a.negative != b.negative) return a.negative ? -1 : 1;
    const int c = compare_magnitude(a.digits, b.digits);
    return a.negative ? -c : c;
}

BigInt operator+(const BigInt& a, const BigInt& b) {
    if (a.negative == b.negative) return make(a.negative, add_magnitude(a.digits, b.digits));
    const int c = compare_magnitude(a.digits, b.digits);
    if (c == 0) return BigInt();
    if (c > 0) return make(a.negative, sub_magnitude(a.digits, b.digits));
    return make(b.negative, sub_magnitude(b.digits, a.digits));
}

BigInt operator-(const BigInt& a) { return make(!a.negative, a.digits); }
BigInt operator-(const BigInt& a, const BigInt& b) { return a + (-b); }
BigInt operator*(const BigInt& a, const BigInt& b) {
    return make(a.negative != b.negative, mul_magnitude(a.digits, b.digits));
}
BigInt abs(const BigInt& a) { return make(false, a.digits); }

bool is_int(const Number& n) { return std::holds_alternative<BigInt>(n); }

double to_double(const Number& n) {
    if (const auto* i = std::get_if<BigInt>(&n)) return i->to_double();
    return std::get<double>(n);
}

Number add(const Number& a, const Number& b) {
    if (is_int(a) && is_int(b)) return std::get<BigInt>(a) + std::get<BigInt>(b);
    return to_double(a) + to_double(b);
}

Number mul(const Number& a, const Number& b) {
    if (is_int(a) && is_int(b)) return std::get<BigInt>(a) * std::get<BigInt>(b);
    return to_double(a) * to_double(b);
}

Number neg(const Number& a) {
    if (const auto* i = std::get_if<BigInt>(&a)) return -*i;
    return -std::get<double>(a);
}

Number abs(const Number& a) {
    if (const auto* i = std::get_if<BigInt>(&a)) return abs(*i);
    return std::fabs(std::get<double>(a));
}

namespace {

// An int against a finite or infinite double, exactly.
int compare_int_double(const BigInt& i, double d) {
    if (std::isinf(d)) return d > 0 ? -1 : 1;
    const BigInt floor_d = BigInt::from_double(std::floor(d));
    const int c = compare(i, floor_d);
    if (c != 0) return c;
    return std::floor(d) == d ? 0 : -1;
}

}  // namespace

int compare(const Number& a, const Number& b) {
    const auto* ai = std::get_if<BigInt>(&a);
    const auto* bi = std::get_if<BigInt>(&b);
    if (ai && bi) return compare(*ai, *bi);
    if (ai) return compare_int_double(*ai, std::get<double>(b));
    if (bi) return -compare_int_double(*bi, std::get<double>(a));
    const double x = std::get<double>(a), y = std::get<double>(b);
    return x < y ? -1 : (x > y ? 1 : 0);
}

bool is_integer(const Number& n) {
    if (is_int(n)) return true;
    const double d = std::get<double>(n);
    return std::isfinite(d) && std::trunc(d) == d;
}

BigInt to_int(const Number& n) {
    if (const auto* i = std::get_if<BigInt>(&n)) return *i;
    const double d = std::get<double>(n);
    if (std::isinf(d)) throw OverflowError("cannot convert float infinity to integer");
    if (std::isnan(d)) throw ValueError("cannot convert float NaN to integer");
    return BigInt::from_double(d);
}

std::string str(const Number& n) {
    if (const auto* i = std::get_if<BigInt>(&n)) return i->str();
    return float_repr(std::get<double>(n));
}

BigInt round_half_even(double value) {
    if (std::isinf(value)) throw OverflowError("cannot convert float infinity to integer");
    if (std::isnan(value)) throw ValueError("cannot convert float NaN to integer");
    return BigInt::from_double(std::nearbyint(value));
}

double true_divide(const BigInt& a, const BigInt& b) {
    if (b.is_zero()) throw ZeroDivisionError("division by zero");
    const bool negative = a.negative != b.negative;
    const double magnitude = std::strtod(quotient_text(a.digits, b.digits).c_str(), nullptr);
    if (std::isinf(magnitude)) throw OverflowError("integer division result too large for a float");
    return negative ? -magnitude : magnitude;
}

void FloatSum::add(double x) {
    if (!started_) {
        // sum() starts from the int 0, and 0 + x is x for every x but -0.0.
        sum_ = 0 + x;
        started_ = true;
        return;
    }
    const double t = sum_ + x;
    if (std::fabs(sum_) >= std::fabs(x)) {
        c_ += (sum_ - t) + x;
    } else {
        c_ += (x - t) + sum_;
    }
    sum_ = t;
}

double FloatSum::result() const {
    if (c_ != 0 && std::isfinite(c_)) return sum_ + c_;
    return sum_;
}

}  // namespace arbtok::numbers
