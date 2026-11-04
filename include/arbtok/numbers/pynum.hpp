// Python's int and float, as far as the number and date modules need them:
// an integer of any size, mixed arithmetic with Python's rules, and the text
// forms that int(), float(), str() and repr() read and write.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace arbtok::numbers {

// A Python int of any size: a sign and the decimal digits of the magnitude.
// The digits have no leading zero, and zero is "0" with no sign.
struct BigInt {
    bool negative = false;
    std::string digits = "0";

    BigInt() = default;
    BigInt(long long value);  // NOLINT: an int literal is a Python int
    // Decimal digits with an optional leading '-'; std::invalid_argument otherwise.
    static BigInt from_string(std::string_view text);
    // The exact integer part of a finite double, as Python's int(x) gives it.
    static BigInt from_double(double value);

    bool is_zero() const { return digits == "0"; }
    // Python's str(n).
    std::string str() const { return negative ? "-" + digits : digits; }
    // float(n), rounded to nearest even; OverflowError past the double range.
    double to_double() const;
    // The value, when it fits a long long.
    std::optional<long long> to_ll() const;

    friend bool operator==(const BigInt& a, const BigInt& b) {
        return a.negative == b.negative && a.digits == b.digits;
    }
    friend bool operator!=(const BigInt& a, const BigInt& b) { return !(a == b); }
};

int compare(const BigInt& a, const BigInt& b);
BigInt operator+(const BigInt& a, const BigInt& b);
BigInt operator-(const BigInt& a, const BigInt& b);
BigInt operator-(const BigInt& a);
BigInt operator*(const BigInt& a, const BigInt& b);
BigInt abs(const BigInt& a);
inline bool operator<(const BigInt& a, const BigInt& b) { return compare(a, b) < 0; }

// A Python int or float.
using Number = std::variant<BigInt, double>;

bool is_int(const Number& n);
// Python's a + b, a * b, -a and abs(a): int with int stays exact, and an int
// meets a float as float(int), which raises OverflowError past the double range.
Number add(const Number& a, const Number& b);
Number mul(const Number& a, const Number& b);
Number neg(const Number& a);
Number abs(const Number& a);
// Python's comparison of two numbers, exact across int and float. Neither is NaN.
int compare(const Number& a, const Number& b);
inline bool eq(const Number& a, const Number& b) { return compare(a, b) == 0; }
// float.is_integer(); true for every int.
bool is_integer(const Number& n);
// int(n) for a finite number.
BigInt to_int(const Number& n);
// float(n).
double to_double(const Number& n);
// Python's str(n).
std::string str(const Number& n);

// Python's int(text) for base 10, with the same leniency: surrounding Unicode
// whitespace, a sign, underscores between digits, and any Unicode decimal
// digit. nullopt where Python raises ValueError, which includes a value of more
// than 4300 digits.
std::optional<BigInt> py_int(std::string_view text);

// Python's float(text): the same leniency as py_int, a fraction and an
// exponent, and inf, infinity and nan in any case. nullopt where Python
// raises ValueError.
std::optional<double> py_float(std::string_view text);

// Python's repr(x) of a float.
std::string float_repr(double value);

// Python's round(x) of a float to an int, half to even.
BigInt round_half_even(double value);

// Python's a / b for two ints, correctly rounded; ZeroDivisionError for a zero
// b, OverflowError past the double range.
double true_divide(const BigInt& a, const BigInt& b);

// sum() of floats as CPython 3.12 adds them, with Neumaier compensation.
class FloatSum {
public:
    void add(double x);
    double result() const;

private:
    bool started_ = false;
    double sum_ = 0.0, c_ = 0.0;
};

}  // namespace arbtok::numbers
