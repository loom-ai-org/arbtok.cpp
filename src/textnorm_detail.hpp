// Internal pieces shared by textnorm and util: Python's str methods on code
// points, the number and date calls with their caches, and the bundled tables.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "arbtok/numbers/errors.hpp"
#include "arbtok/numbers/ovos_date_parser.hpp"
#include "arbtok/numbers/ovos_number_parser.hpp"
#include "arbtok/numbers/pynum.hpp"
#include "arbtok/textnorm.hpp"
#include "detail.hpp"
#include "pyre.hpp"

// The module-level values of arbtok.textnorm and arbtok.util, as tools/gen_textnorm_tables.py
// writes them.
namespace arbtok::textnorm::py {
struct Re {
    std::u32string_view pattern;
    int flags;
};
// One entry of a str.maketrans table: a code point to a string, or to nothing.
struct Tr {
    char32_t from;
    std::u32string_view to;
    bool keep;
};
// A dict keyed by a pair of words.
struct Pair2 {
    std::u32string_view a, b, to;
};
struct NamedRe {
    const char* name;
    const Re* re;
};
// One language's table of a dict keyed by language.
struct LangTable {
    std::u32string_view lang;
    const std::pair<std::u32string_view, std::u32string_view>* items;
    size_t size;
};
struct Range {
    long long start, stop;
    bool has(long long v) const { return start <= v && v < stop; }
};
}  // namespace arbtok::textnorm::py
namespace arbtok::textnorm::py_util {
using py::LangTable;
using py::NamedRe;
using py::Pair2;
using py::Range;
using py::Re;
using py::Tr;
}  // namespace arbtok::textnorm::py_util
#include "textnorm_tables.inc"
#include "util_tables.inc"

namespace arbtok::textnorm::detail {

using arbtok::detail::U32;
using arbtok::detail::U32View;
using numbers::BigInt;
using numbers::Number;
using pyre::Match;
using pyre::Pattern;

inline U32 u(std::string_view s) { return arbtok::detail::from_utf8(s); }
inline std::string s8(U32View s) { return arbtok::detail::to_utf8(s); }

// ── str methods ─────────────────────────────────────────────────────────────

// str.translate with a table: a code point maps to a string, or to nothing.
using Table = std::map<char32_t, std::optional<U32>>;
U32 translate(U32View s, const Table& table);
// A table of one code point per code point: str.maketrans(a, b).
Table maketrans(U32View from, U32View to);

// ── The generated tables ────────────────────────────────────────────────────

inline const Pattern& re(const py::Re& r) { return pyre::compiled(r.pattern, r.flags); }

template <size_t N>
bool in(U32View word, const std::u32string_view (&words)[N]) {
    for (const auto& w : words)
        if (w == word) return true;
    return false;
}

// The value of a dict entry, or nullptr.
template <class V, size_t N>
const V* lookup(U32View key, const std::pair<std::u32string_view, V> (&dict)[N]) {
    for (const auto& kv : dict)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

template <size_t N>
Table table(const py::Tr (&entries)[N]) {
    Table t;
    for (const auto& e : entries) t[e.from] = e.keep ? std::optional<U32>(U32(e.to)) : std::nullopt;
    return t;
}

// `word[:1] in chars`, which is true for an empty word, as in Python.
inline bool first_in(U32View word, U32View chars) { return word.empty() || chars.find(word[0]) != U32View::npos; }

std::vector<U32> split_ws(U32View s);                  // str.split()
std::vector<U32> split(U32View s, U32View sep);        // str.split(sep)
U32 strip(U32View s);                                  // str.strip()
U32 lstrip(U32View s);
U32 rstrip(U32View s);
U32 rstrip_chars(U32View s, U32View chars);            // str.rstrip(chars)
U32 lstrip_chars(U32View s, U32View chars);            // str.lstrip(chars)
U32 join(const std::vector<U32>& parts, U32View sep);
U32 replace(U32View s, U32View from, U32View to);
bool starts(U32View s, U32View prefix);
bool ends(U32View s, U32View suffix);
bool has(U32View s, char32_t c);
bool contains(U32View s, U32View part);
size_t count(U32View s, U32View part);
bool isdigit(U32View s);    // str.isdigit()
bool isdigit(char32_t c);
bool isalpha(char32_t c);
bool isalnum(U32View s);    // str.isalnum()
bool isascii(U32View s);
bool isupper(U32View s);    // str.isupper()
bool isupper(char32_t c);
U32 lower(U32View s);       // str.lower()
U32 upper(U32View s);       // str.upper()
U32 slice(U32View s, long start, long stop);  // s[start:stop] with Python's index rules
inline U32 slice_from(U32View s, long start) { return slice(s, start, static_cast<long>(s.size())); }
// repr(str) and repr of the tuples TtsNorm.describe() hashes.
std::string repr(U32View s);
// The first n hex digits of sha256(bytes).
std::string sha256_hex(std::string_view bytes, size_t n);

// The words of _WORDS_AND_GAPS.split(text): words at even positions, the
// whitespace runs between them at odd positions.
std::vector<U32> words_and_gaps(U32View text);
// _EDGE_PUNCTUATION.match(word).groups(): the punctuation before, the core,
// the punctuation after.
struct Edges {
    U32 lead, core, trail;
};
Edges edges(U32View word);

// int(text) for the digit strings the rules read; ValueError otherwise.
long long to_int(U32View text);
BigInt to_bigint(U32View text);
// f"{n}" and f"{n:02d}".
U32 itos(long long n);
U32 itos2(long long n);

// ── Python numbers ──────────────────────────────────────────────────────────

int cmp(const Number& a, long long b);
// value == int(value); int() raises as Python does for inf and nan.
bool integral(const Number& v);
// int(value).
BigInt int_of(const Number& v);
long long ll_of(const Number& v);

// ── The number and date libraries ───────────────────────────────────────────
// Their exceptions reach the caller as arbtok::Error with the same Python class.

std::optional<Number> extract_number(U32View text, const std::string& lang, bool ordinals = false);
U32 numbers_to_digits(U32View text, const std::string& lang);
U32 pronounce_number(const Number& value, const std::string& lang,
                     numbers::ovos_number_parser::GrammaticalGender gender =
                         numbers::ovos_number_parser::GrammaticalGender::MASCULINE,
                     const std::optional<std::string>& grammatical_case = std::nullopt, int places = 3);
U32 pronounce_ordinal(const Number& value, const std::string& lang,
                      numbers::ovos_number_parser::GrammaticalGender gender,
                      const std::optional<std::string>& grammatical_case);
U32 pronounce_fraction(U32View text, const std::string& lang);
bool is_numeric(U32View text);
U32 nice_time(const numbers::ovos_date_parser::DateTime& dt, const std::string& lang, bool speech, bool use_24hour,
              bool use_ampm);
U32 nice_date(const numbers::ovos_date_parser::DateTime& dt, const std::string& lang);
std::optional<numbers::ovos_date_parser::Extracted> extract_datetime(U32View text, const std::string& lang);

// Runs fn and turns an exception of the number libraries into the arbtok class
// of the same Python name, or into arbtok::Error when arbtok has no such class.
template <class F>
auto bridged(F&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const numbers::ValueError& e) {
        throw ValueError(e.what());
    } catch (const numbers::IndexError& e) {
        throw IndexError(e.what());
    } catch (const numbers::OverflowError& e) {
        throw OverflowError(e.what());
    } catch (const numbers::NotImplementedError& e) {
        throw NotImplementedError(e.what());
    } catch (const numbers::NotPortedError& e) {
        throw NotPortedError(e.what());
    } catch (const numbers::Error& e) {
        throw Error(e.python_type(), e.what());
    }
}

// ── Bundled tables ──────────────────────────────────────────────────────────

// str.splitlines() of UTF-8 text, line.split("\t"), and whether a file can be read.
std::vector<std::string> splitlines(const std::string& text);
std::vector<std::string> split_tab(const std::string& line);
bool file_exists(const std::string& path);

using Row = std::vector<std::pair<std::string, std::string>>;  // column -> value, in header order
const std::vector<Row>& term_rows(const std::string& name);
const std::string* cell(const Row& row, const std::string& column);
// _rank_nouns(): noun without its article -> gender, in file order.
const std::vector<std::pair<U32, std::string>>& rank_nouns();
const std::vector<U32>& feminine_counted_nouns();

// The pattern sets shared by the two directions.
const Pattern& controls();
extern const U32 kTatweel;
// _ALEF, _TA_MARBUTA, _ALEF_MAQSURA as one table, and _CLOCK_MARKS.
const Table& clock_fold();
const Table& clock_marks();
const Table& eastern_digits();
U32 fold_key(U32View word);
// The letter names of _LETTER_NAMES, in its order.
const std::vector<std::pair<char32_t, U32>>& letter_names();
const std::vector<U32>& month_names();        // the second _MONTH_NAMES, January first
const std::vector<U32>& hijri_month_names();
std::map<U32, U32> spelled_codes_u(const std::string& lang);  // as spelled_codes, in code points
const std::vector<std::string>& capitalised_units();

}  // namespace arbtok::textnorm::detail
