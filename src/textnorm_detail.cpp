#include "textnorm_detail.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#include "json.hpp"
#include "sha256.hpp"

namespace arbtok::textnorm::detail {

namespace ad = arbtok::detail;
namespace onp = numbers::ovos_number_parser;
namespace odp = numbers::ovos_date_parser;

namespace {
#include "pyre_tables.inc"

template <size_t N>
bool in_ranges(const char32_t (&table)[N][2], char32_t c) {
    size_t lo = 0, hi = N;
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (c < table[mid][0]) {
            hi = mid;
        } else if (c > table[mid][1]) {
            lo = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

bool is_lower_char(char32_t c) { return in_ranges(kLowerCase, c); }
bool is_title_char(char32_t c) { return in_ranges(kTitle, c); }
bool is_printable(char32_t c) { return in_ranges(kPrintable, c); }
}  // namespace

U32 translate(U32View s, const Table& table) {
    U32 out;
    out.reserve(s.size());
    for (const char32_t c : s) {
        const auto it = table.find(c);
        if (it == table.end()) {
            out += c;
        } else if (it->second) {
            out += *it->second;
        }
    }
    return out;
}

Table maketrans(U32View from, U32View to) {
    Table t;
    for (size_t i = 0; i < from.size(); ++i) t[from[i]] = U32(1, to[i]);
    return t;
}

std::vector<U32> split_ws(U32View s) {
    std::vector<U32> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && ad::is_space(s[i])) ++i;
        if (i >= s.size()) break;
        size_t j = i;
        while (j < s.size() && !ad::is_space(s[j])) ++j;
        out.emplace_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

std::vector<U32> split(U32View s, U32View sep) {
    std::vector<U32> out;
    size_t start = 0;
    while (true) {
        const size_t at = s.find(sep, start);
        if (at == U32View::npos) break;
        out.emplace_back(s.substr(start, at - start));
        start = at + sep.size();
    }
    out.emplace_back(s.substr(start));
    return out;
}

U32 lstrip(U32View s) {
    size_t i = 0;
    while (i < s.size() && ad::is_space(s[i])) ++i;
    return U32(s.substr(i));
}

U32 rstrip(U32View s) {
    size_t j = s.size();
    while (j > 0 && ad::is_space(s[j - 1])) --j;
    return U32(s.substr(0, j));
}

U32 strip(U32View s) { return lstrip(rstrip(s)); }

U32 rstrip_chars(U32View s, U32View chars) {
    size_t j = s.size();
    while (j > 0 && chars.find(s[j - 1]) != U32View::npos) --j;
    return U32(s.substr(0, j));
}

U32 lstrip_chars(U32View s, U32View chars) {
    size_t i = 0;
    while (i < s.size() && chars.find(s[i]) != U32View::npos) ++i;
    return U32(s.substr(i));
}

U32 join(const std::vector<U32>& parts, U32View sep) {
    U32 out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

U32 replace(U32View s, U32View from, U32View to) { return ad::replace(s, from, to); }
bool starts(U32View s, U32View prefix) { return s.substr(0, prefix.size()) == prefix; }
bool ends(U32View s, U32View suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}
bool has(U32View s, char32_t c) { return s.find(c) != U32View::npos; }
bool contains(U32View s, U32View part) { return s.find(part) != U32View::npos; }

size_t count(U32View s, U32View part) {
    if (part.empty()) return s.size() + 1;
    size_t n = 0, at = 0;
    while ((at = s.find(part, at)) != U32View::npos) {
        ++n;
        at += part.size();
    }
    return n;
}

bool isdigit(char32_t c) { return ad::is_digit(c); }
bool isdigit(U32View s) { return ad::all_digit(s); }
bool isalpha(char32_t c) { return ad::is_alpha(c); }
bool isalnum(U32View s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char32_t c) { return ad::is_alnum(c); }); }
bool isascii(U32View s) { return std::all_of(s.begin(), s.end(), [](char32_t c) { return c < 0x80; }); }
bool isupper(char32_t c) { return in_ranges(kUpper, c); }

bool isupper(U32View s) {
    bool cased = false;
    for (const char32_t c : s) {
        if (is_lower_char(c) || is_title_char(c)) return false;
        if (isupper(c)) cased = true;
    }
    return cased;
}

U32 lower(U32View s) { return ad::lower(s); }
U32 upper(U32View s) { return ad::upper(s); }

U32 slice(U32View s, long start, long stop) {
    const long n = static_cast<long>(s.size());
    auto clamp = [n](long i) {
        if (i < 0) i += n;
        return std::max(0L, std::min(n, i));
    };
    const long a = clamp(start), b = clamp(stop);
    return a < b ? U32(s.substr(static_cast<size_t>(a), static_cast<size_t>(b - a))) : U32();
}

std::string repr(U32View s) {
    const bool squote = has(s, U'\''), dquote = has(s, U'"');
    const char32_t quote = (squote && !dquote) ? U'"' : U'\'';
    U32 out(1, quote);
    char buf[16];
    for (const char32_t c : s) {
        if (c == quote || c == U'\\') {
            out += U'\\';
            out += c;
        } else if (c == U'\t') {
            out += U"\\t";
        } else if (c == U'\n') {
            out += U"\\n";
        } else if (c == U'\r') {
            out += U"\\r";
        } else if (c < 0x20 || c == 0x7F) {
            std::snprintf(buf, sizeof buf, "\\x%02x", static_cast<unsigned>(c));
            out += u(buf);
        } else if (c < 0x7F || is_printable(c)) {
            out += c;
        } else if (c <= 0xFF) {
            std::snprintf(buf, sizeof buf, "\\x%02x", static_cast<unsigned>(c));
            out += u(buf);
        } else if (c <= 0xFFFF) {
            std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
            out += u(buf);
        } else {
            std::snprintf(buf, sizeof buf, "\\U%08x", static_cast<unsigned>(c));
            out += u(buf);
        }
    }
    out += quote;
    return s8(out);
}

std::string sha256_hex(std::string_view bytes, size_t n) { return ad::sha256_hex(bytes).substr(0, n); }

std::vector<U32> words_and_gaps(U32View text) {
    std::vector<U32> parts;
    size_t i = 0, start = 0;
    while (i < text.size()) {
        if (ad::is_space(text[i])) {
            size_t j = i;
            while (j < text.size() && ad::is_space(text[j])) ++j;
            parts.emplace_back(text.substr(start, i - start));
            parts.emplace_back(text.substr(i, j - i));
            start = i = j;
        } else {
            ++i;
        }
    }
    parts.emplace_back(text.substr(start));
    return parts;
}

namespace {
// What may stand at the edge of a word without being part of it: anything
// but a word character, whitespace or a combining mark of _NOT_EDGE.
bool edge_char(char32_t c) {
    if (pyre::is_word(c) || ad::is_space(c)) return false;
    return !((c >= 0x0300 && c <= 0x036F) || (c >= 0x0610 && c <= 0x061A) || (c >= 0x064B && c <= 0x065F) ||
             c == 0x0670 || (c >= 0x06D6 && c <= 0x06ED));
}
}  // namespace

Edges edges(U32View word) {
    size_t a = 0;
    while (a < word.size() && edge_char(word[a])) ++a;
    size_t b = word.size();
    while (b > a && edge_char(word[b - 1])) --b;
    // `$` also matches before a final "\n", which the lazy core then leaves out.
    if (b == word.size() && b > a && word[b - 1] == U'\n') {
        size_t c = b - 1;
        while (c > a && edge_char(word[c - 1])) --c;
        return {U32(word.substr(0, a)), U32(word.substr(a, c - a)), U32(word.substr(c, b - 1 - c))};
    }
    return {U32(word.substr(0, a)), U32(word.substr(a, b - a)), U32(word.substr(b))};
}

BigInt to_bigint(U32View text) {
    const auto v = numbers::py_int(s8(text));
    if (!v) throw ValueError("invalid literal for int() with base 10: " + repr(text));
    return *v;
}

long long to_int(U32View text) {
    const auto v = to_bigint(text).to_ll();
    if (!v) throw OverflowError("int too large");
    return *v;
}

U32 itos(long long n) { return u(std::to_string(n)); }

U32 itos2(long long n) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02lld", n);
    return u(buf);
}

int cmp(const Number& a, long long b) { return numbers::compare(a, Number(BigInt(b))); }

BigInt int_of(const Number& v) {
    if (const auto* d = std::get_if<double>(&v)) {
        if (*d != *d) throw ValueError("cannot convert float NaN to integer");
        if (*d == 1.0 / 0.0 || *d == -1.0 / 0.0) throw OverflowError("cannot convert float infinity to integer");
    }
    return numbers::to_int(v);
}

bool integral(const Number& v) { return numbers::compare(v, Number(int_of(v))) == 0; }

long long ll_of(const Number& v) {
    const auto n = int_of(v).to_ll();
    if (!n) throw OverflowError("int too large");
    return *n;
}

// ── The number and date libraries ───────────────────────────────────────────

std::optional<Number> extract_number(U32View text, const std::string& lang, bool ordinals) {
    return bridged([&] { return onp::extract_number(s8(text), lang, ordinals); });
}

U32 numbers_to_digits(U32View text, const std::string& lang) {
    return u(bridged([&] { return onp::numbers_to_digits(s8(text), lang); }));
}

U32 pronounce_number(const Number& value, const std::string& lang, onp::GrammaticalGender gender,
                     const std::optional<std::string>& grammatical_case, int places) {
    return u(bridged([&] { return onp::pronounce_number(value, lang, places, gender, grammatical_case); }));
}

U32 pronounce_ordinal(const Number& value, const std::string& lang, onp::GrammaticalGender gender,
                      const std::optional<std::string>& grammatical_case) {
    return u(bridged([&] { return onp::pronounce_ordinal(value, lang, gender, grammatical_case); }));
}

U32 pronounce_fraction(U32View text, const std::string& lang) {
    return u(bridged([&] { return onp::pronounce_fraction(s8(text), lang); }));
}

bool is_numeric(U32View text) {
    return bridged([&] { return onp::is_numeric(s8(text)); });
}

U32 nice_time(const odp::DateTime& dt, const std::string& lang, bool speech, bool use_24hour, bool use_ampm) {
    return u(bridged([&] { return odp::nice_time(dt, lang, speech, use_24hour, use_ampm); }));
}

U32 nice_date(const odp::DateTime& dt, const std::string& lang) {
    return u(bridged([&] { return odp::nice_date(dt, lang); }));
}

std::optional<odp::Extracted> extract_datetime(U32View text, const std::string& lang) {
    odp::DateTime anchor;
    anchor.year = 2000;
    return bridged([&] { return odp::extract_datetime(s8(text), lang, anchor); });
}

// ── Bundled tables ──────────────────────────────────────────────────────────


// str.splitlines() without the line ends.
std::vector<std::string> splitlines(const std::string& text) {
    const U32 s = u(text);
    std::vector<std::string> out;
    size_t start = 0, i = 0;
    auto is_break = [](char32_t c) {
        return c == U'\n' || c == U'\r' || c == 0x0B || c == 0x0C || c == 0x1C || c == 0x1D || c == 0x1E ||
               c == 0x85 || c == 0x2028 || c == 0x2029;
    };
    while (i < s.size()) {
        if (is_break(s[i])) {
            out.push_back(s8(s.substr(start, i - start)));
            if (s[i] == U'\r' && i + 1 < s.size() && s[i + 1] == U'\n') ++i;
            start = ++i;
        } else {
            ++i;
        }
    }
    if (start < s.size()) out.push_back(s8(s.substr(start)));
    return out;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t at = line.find('\t', start);
        if (at == std::string::npos) break;
        out.push_back(line.substr(start, at - start));
        start = at + 1;
    }
    out.push_back(line.substr(start));
    return out;
}

bool file_exists(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fclose(f);
    return true;
}

namespace {

const std::vector<std::string>& term_names() {
    static const std::vector<std::string> names = {"cars-sa",           "common-en",         "currencies-cldr",
                                                   "languages-cldr",    "spoken-sa",         "territories-cldr",
                                                   "units-cldr"};
    return names;
}

}  // namespace

const std::vector<Row>& term_rows(const std::string& name) {
    static std::mutex lock;
    static std::map<std::string, std::vector<Row>> cache;
    std::lock_guard<std::mutex> guard(lock);
    const auto hit = cache.find(name);
    if (hit != cache.end()) return hit->second;
    const std::string path = ad::data_path("term_lexicons/" + name + ".tsv");
    if (name.find('/') != std::string::npos || !file_exists(path)) {
        std::string have = "[";
        for (size_t i = 0; i < term_names().size(); ++i) have += (i ? ", '" : "'") + term_names()[i] + "'";
        throw ValueError("no bundled term lexicon '" + name + "'; bundled: " + have + "]");
    }
    const auto lines = splitlines(ad::read_file(path));
    std::string header;
    for (const auto& line : lines) {
        if (line.rfind("# latin\t", 0) == 0 || line.rfind("latin\t", 0) == 0) {
            header = line;
            break;
        }
    }
    // header.lstrip("# ")
    size_t skip = 0;
    while (skip < header.size() && (header[skip] == '#' || header[skip] == ' ')) ++skip;
    const auto columns = split_tab(header.substr(skip));
    std::vector<Row> rows;
    for (const auto& line : lines) {
        if (line.empty() || line[0] == '#' || line == header) continue;
        const auto cells = split_tab(line);
        Row row;
        for (size_t i = 0; i < columns.size() && i < cells.size(); ++i) {
            auto it = std::find_if(row.begin(), row.end(), [&](const auto& kv) { return kv.first == columns[i]; });
            if (it == row.end()) {
                row.emplace_back(columns[i], cells[i]);
            } else {
                it->second = cells[i];
            }
        }
        rows.push_back(std::move(row));
    }
    return cache.emplace(name, std::move(rows)).first->second;
}

const std::string* cell(const Row& row, const std::string& column) {
    for (const auto& [k, v] : row)
        if (k == column) return &v;
    return nullptr;
}

const std::vector<std::pair<U32, std::string>>& rank_nouns() {
    static const std::vector<std::pair<U32, std::string>> rows = [] {
        std::vector<std::pair<U32, std::string>> out;
        for (const auto& line : splitlines(ad::read_file(ad::data_path("rank_ordinals.tsv")))) {
            if (line.empty() || line[0] == '#') continue;
            const auto cells = split_tab(line);
            if (cells.size() < 2) throw ValueError("not enough values to unpack");
            const U32 noun = slice_from(u(cells[0]), 2);
            auto it = std::find_if(out.begin(), out.end(), [&](const auto& kv) { return kv.first == noun; });
            if (it == out.end()) {
                out.emplace_back(noun, cells[1]);
            } else {
                it->second = cells[1];
            }
        }
        return out;
    }();
    return rows;
}

const std::vector<U32>& feminine_counted_nouns() {
    static const std::vector<U32> rows = [] {
        std::vector<U32> out;
        for (const auto& line : splitlines(ad::read_file(ad::data_path("feminine_counted_nouns.tsv")))) {
            if (line.empty() || line[0] == '#') continue;
            out.push_back(u(split_tab(line)[0]));
        }
        return out;
    }();
    return rows;
}

const Pattern& controls() { return re(py::CONTROLS); }

const U32 kTatweel = U"ـ";

namespace {
const std::vector<std::pair<char32_t, char32_t>>& alef_pairs() {
    static const std::vector<std::pair<char32_t, char32_t>> v = {{U'آ', U'ا'}, {U'أ', U'ا'}, {U'إ', U'ا'}, {U'ٱ', U'ا'}};
    return v;
}
}  // namespace

const Table& clock_fold() {
    static const Table t = [] {
        Table out;
        for (const auto& [a, b] : alef_pairs()) out[a] = U32(1, b);
        out[U'ة'] = U"ه";
        out[U'ى'] = U"ي";
        return out;
    }();
    return t;
}

const Table& clock_marks() {
    static const Table t = [] {
        Table out;
        out[0x0640] = std::nullopt;
        for (char32_t c = 0x064B; c < 0x0653; ++c) out[c] = std::nullopt;
        return out;
    }();
    return t;
}

const Table& eastern_digits() {
    static const Table t = maketrans(U"٠١٢٣٤٥٦٧٨٩", U"0123456789");
    return t;
}

U32 fold_key(U32View word) { return translate(translate(word, clock_marks()), clock_fold()); }

const std::vector<std::pair<char32_t, U32>>& letter_names() {
    static const std::vector<std::pair<char32_t, U32>> v = {
        {U'أ', U"ألف"}, {U'إ', U"ألف"}, {U'آ', U"ألف"}, {U'ا', U"ألف"}, {U'ى', U"ياء"}, {U'ب', U"باء"},
        {U'ت', U"تاء"}, {U'ث', U"ثاء"}, {U'ج', U"جيم"}, {U'ح', U"حاء"}, {U'خ', U"خاء"}, {U'د', U"دال"},
        {U'ذ', U"ذال"}, {U'ر', U"راء"}, {U'ز', U"زاي"}, {U'س', U"سين"}, {U'ش', U"شين"}, {U'ص', U"صاد"},
        {U'ض', U"ضاد"}, {U'ط', U"طاء"}, {U'ظ', U"ظاء"}, {U'ع', U"عين"}, {U'غ', U"غين"}, {U'ف', U"فاء"},
        {U'ق', U"قاف"}, {U'ك', U"كاف"}, {U'ل', U"لام"}, {U'م', U"ميم"}, {U'ن', U"نون"}, {U'ه', U"هاء"},
        {U'و', U"واو"}, {U'ي', U"ياء"},
    };
    return v;
}

const std::vector<U32>& month_names() {
    static const std::vector<U32> v = {U"يناير", U"فبراير", U"مارس",  U"أبريل",  U"مايو",   U"يونيو",
                                       U"يوليو", U"أغسطس",  U"سبتمبر", U"أكتوبر", U"نوفمبر", U"ديسمبر"};
    return v;
}

const std::vector<U32>& hijri_month_names() {
    static const std::vector<U32> v = {U"محرم",  U"صفر",        U"ربيع الأول",   U"ربيع الآخر",
                                       U"جمادى الأولى", U"جمادى الآخرة", U"رجب",          U"شعبان",
                                       U"رمضان", U"شوال",       U"ذو القعدة",    U"ذو الحجة"};
    return v;
}

std::map<U32, U32> spelled_codes_u(const std::string& lang) {
    std::map<U32, U32> out;
    for (const auto& [k, v] : spelled_codes(lang)) out[u(k)] = u(v);
    return out;
}

const std::vector<std::string>& capitalised_units() {
    static const std::vector<std::string> v = {"hp", "kg", "km", "kw"};
    return v;
}

}  // namespace arbtok::textnorm::detail
