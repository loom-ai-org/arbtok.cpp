// Parity test for arbtok_numbers. Each line of a golden file is one JSON
// record [kind, arguments..., result] written by tools/goldens/numbers_ar.py or
// tools/goldens/dates_ar.py from the Python reference. The test recomputes the
// result and compares the JSON text. A "_block" record holds the SHA-256 of the
// results of a whole range, one JSON text per line, in place of the results.
//
//   numbers_parity <golden.jsonl.gz>...

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "arbtok/numbers/errors.hpp"
#include "arbtok/numbers/ovos_date_parser.hpp"
#include "arbtok/numbers/ovos_number_parser.hpp"
#include "arbtok/numbers/unicode_rbnf.hpp"
#include "json.hpp"
#include "numbers_sha256.hpp"

namespace {

using namespace arbtok::numbers;
namespace onp = arbtok::numbers::ovos_number_parser;
namespace odp = arbtok::numbers::ovos_date_parser;
using json::Value;

std::vector<std::string> read_lines(const std::string& path) {
    gzFile f = gzopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open " + path);
    std::string data;
    char buf[1 << 16];
    int n = 0;
    while ((n = gzread(f, buf, sizeof buf)) > 0) data.append(buf, static_cast<size_t>(n));
    gzclose(f);
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= data.size(); ++i) {
        if (i == data.size() || data[i] == '\n') {
            if (i > start) lines.push_back(data.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

std::string q(const std::string& s) { return json::quote(s); }

Number number(const Value& v) {
    if (v.kind == Value::Kind::Object && !v.members.empty() && v.members[0].first == "int") {
        return BigInt::from_string(v.members[0].second.str);
    }
    return std::strtod(v.at("float").str.c_str(), nullptr);
}

std::string tag(const Number& n) {
    if (std::holds_alternative<BigInt>(n)) return "{\"int\":" + q(std::get<BigInt>(n).str()) + "}";
    return "{\"float\":" + q(float_repr(std::get<double>(n))) + "}";
}

onp::GrammaticalGender gender(const Value& v) {
    if (v.str == "FEMININE") return onp::GrammaticalGender::FEMININE;
    if (v.str == "NEUTRAL") return onp::GrammaticalGender::NEUTRAL;
    return onp::GrammaticalGender::MASCULINE;
}

std::optional<std::string> opt_str(const Value& v) {
    if (v.is_null()) return std::nullopt;
    return v.str;
}

odp::DateTime date_time(const Value& v) {
    odp::DateTime dt;
    int* fields[] = {&dt.year, &dt.month, &dt.day, &dt.hour, &dt.minute, &dt.second, &dt.microsecond};
    for (size_t k = 0; k < v.size() && k < 7; ++k) *fields[k] = static_cast<int>(v[k].integer());
    return dt;
}

// What a call gives when the reference has a module for the language and the
// port does not.
const std::string kNotPorted = "<not ported>";

// The JSON text of a result, {"err": "<Python class>"}, or kNotPorted.
std::string result_of(const std::function<std::string()>& fn) {
    try {
        return fn();
    } catch (const Error& e) {
        return "{\"err\":" + q(e.python_type()) + "}";
    } catch (const NotPortedError&) {
        return kNotPorted;
    } catch (const std::exception& e) {
        return "{\"err\":" + q(std::string("C++ ") + e.what()) + "}";
    }
}

std::string pronounce_number(const Number& n, const std::string& lang, const Value& g, const Value& c, int places = 3) {
    return result_of([&] { return q(onp::pronounce_number(n, lang, places, gender(g), opt_str(c))); });
}

std::string extract_number(const std::string& text, const std::string& lang, bool ordinals) {
    return result_of([&] {
        const auto v = onp::extract_number(text, lang, ordinals);
        return v ? tag(*v) : std::string("false");
    });
}

// The result of one record, computed by the port.
std::string compute(const std::string& kind, const Value& r) {
    if (kind == "pronounce_number") {
        return pronounce_number(number(r[1]), r[2].str, r[4], r[5], static_cast<int>(r[3].integer()));
    }
    if (kind == "pronounce_ordinal") {
        return result_of(
            [&] { return q(onp::pronounce_ordinal(number(r[1]), r[2].str, gender(r[3]), opt_str(r[4]))); });
    }
    if (kind == "pronounce_fraction") return result_of([&] { return q(onp::pronounce_fraction(r[1].str, r[2].str)); });
    if (kind == "extract_number") return extract_number(r[1].str, r[2].str, r[3].boolean);
    if (kind == "numbers_to_digits") return result_of([&] { return q(onp::numbers_to_digits(r[1].str, r[2].str)); });
    if (kind == "is_numeric") return onp::is_numeric(r[1].str) ? "true" : "false";
    if (kind == "rbnf") {
        return result_of([&] {
            const auto engine = unicode_rbnf::RbnfEngine::for_language(r[1].str);
            if (r[2].is_null()) return std::string("true");
            return q(engine.format_number(r[2].str));
        });
    }
    if (kind == "nice_time") {
        return result_of([&] {
            return q(odp::nice_time(date_time(r[1]), r[2].str, r[3].boolean, r[4].boolean, r[5].boolean));
        });
    }
    if (kind == "nice_date") return result_of([&] { return q(odp::nice_date(date_time(r[1]), r[2].str)); });
    if (kind == "extract_datetime") {
        return result_of([&] {
            const auto found = odp::extract_datetime(r[1].str, r[2].str, date_time(r[3]));
            if (!found) return std::string("null");
            return "[" + q(odp::isoformat(found->moment)) + "," + q(found->remainder) + "]";
        });
    }
    throw std::runtime_error("unknown record kind " + kind);
}

bool is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

// The results of a block record, one per line, and the last argument index.
std::vector<std::string> compute_block(const std::string& kind, const Value& r) {
    std::vector<std::string> out;
    if (kind == "nice_date_block") {
        static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        for (long long y = r[1].integer(); y <= r[2].integer(); ++y) {
            for (int m = 1; m <= 12; ++m) {
                const int last = m == 2 && is_leap(static_cast<int>(y)) ? 29 : days[m - 1];
                for (int d = 1; d <= last; ++d) {
                    const odp::DateTime dt{static_cast<int>(y), m, d};
                    out.push_back(result_of([&] { return q(odp::nice_date(dt, r[3].str)); }));
                }
            }
        }
        return out;
    }
    const BigInt start = BigInt::from_string(r[1].str);
    const long long count = r[2].integer();
    for (long long k = 0; k < count; ++k) {
        const Number n = start + BigInt(k);
        if (kind == "pronounce_number_block") {
            out.push_back(pronounce_number(n, r[3].str, r[4], r[5]));
        } else if (kind == "pronounce_ordinal_block") {
            out.push_back(result_of(
                [&] { return q(onp::pronounce_ordinal(n, r[3].str, gender(r[4]), opt_str(r[5]))); }));
        } else if (kind == "extract_number_block") {
            // extract_number(pronounce_number(n, lang, gender, case), lang)
            try {
                out.push_back(extract_number(onp::pronounce_number(n, r[3].str, 3, gender(r[4]), opt_str(r[5])),
                                             r[3].str, false));
            } catch (const Error& e) {
                out.push_back("{\"err\":" + q(e.python_type()) + "}");
            }
        } else {
            throw std::runtime_error("unknown block kind " + kind);
        }
    }
    return out;
}

}  // namespace

// usage: numbers_parity [--stride <n>] <golden.jsonl.gz>...
// --stride <n> checks only every n-th record, for the slow sanitizer builds.
// The record count of the header is checked against every record of the file.
int main(int argc, char** argv) {
    size_t stride = 1;
    std::vector<std::string> files;
    for (int a = 1; a < argc; ++a) {
        if (std::string(argv[a]) == "--stride" && a + 1 < argc) {
            stride = std::stoul(argv[++a]);
        } else {
            files.emplace_back(argv[a]);
        }
    }
    if (files.empty() || stride == 0) {
        std::cerr << "usage: numbers_parity [--stride <n>] <golden.jsonl.gz>...\n";
        return 2;
    }
    std::map<std::string, std::pair<long long, long long>> counts;  // kind -> (records, failures)
    long long records = 0, failures = 0;
    for (const std::string& file : files) {
        const std::vector<std::string> lines = read_lines(file);
        // The first line is the header that tools/goldens/common.py writes: the
        // file must hold the record count it gives.
        const Value header = lines.empty() ? Value() : json::parse(lines[0]);
        const bool has_count = std::any_of(header.members.begin(), header.members.end(),
                                           [](const auto& m) { return m.first == "count"; });
        if (header.kind != Value::Kind::Object || !has_count) {
            std::cout << "FAIL " << file << ": no header line with a record count\n";
            ++failures;
            continue;
        }
        const long long expected = header.at("count").integer();
        const long long read = static_cast<long long>(lines.size()) - 1;
        if (read != expected) {
            std::cout << "FAIL " << file << ": the header gives " << expected << " records, the file holds " << read
                      << "\n";
            ++failures;
        }
        for (size_t k = 1; k < lines.size(); k += stride) {
            const std::string& line = lines[k];
            const Value r = json::parse(line);
            const std::string kind = r[0].str;
            ++records;
            auto& c = counts[kind];
            ++c.first;
            bool ok = false;
            std::string got, want;
            const bool block = kind.size() > 6 && kind.compare(kind.size() - 6, 6, "_block") == 0;
            if (kind == "dispatch") {
                // ["dispatch", route, record]: a "module" record gives kNotPorted,
                // any other the reference result.
                const Value& inner = r[2];
                got = compute(inner[0].str, inner);
                want = r[1].str == "module" ? kNotPorted : json::dump(inner[inner.size() - 1]);
                ok = got == want;
            } else if (block) {
                const auto results = compute_block(kind, r);
                std::string joined;
                for (size_t k = 0; k < results.size(); ++k) joined += (k ? "\n" : "") + results[k];
                got = numbers_test::sha256_hex(joined);
                want = r[r.size() - 1].str;
                ok = got == want;
                if (!ok) got += " first results: " + joined.substr(0, 600);
            } else {
                got = compute(kind, r);
                want = json::dump(r[r.size() - 1]);
                ok = got == want;
            }
            if (!ok) {
                ++failures;
                ++c.second;
                if (c.second <= 10) std::cout << "FAIL " << line << "\n  got  " << got << "\n";
            }
        }
    }
    for (const auto& [kind, c] : counts) {
        std::cout << kind << ": " << c.first << " records, " << c.first - c.second << " pass, " << c.second
                  << " fail\n";
    }
    std::cout << records << " records, " << failures << " failures\n";
    return failures ? 1 : 0;
}
