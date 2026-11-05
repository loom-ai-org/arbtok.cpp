#include "arbtok/util.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace u = arbtok::util;

namespace {

std::string tables(const u::Tables& t) {
    std::vector<std::string> langs;
    for (const auto& [lang, table] : t) {
        std::vector<std::string> pairs;
        for (const auto& [a, b] : table) pairs.push_back("[" + J(a) + "," + J(b) + "]");
        langs.push_back("[" + J(lang) + "," + list(pairs) + "]");
    }
    return list(langs);
}

std::string constant(const std::string& name) {
    if (name == "CONTRACTIONS") return tables(u::CONTRACTIONS);
    if (name == "TITLES") return tables(u::TITLES);
    if (name == "UNITS") return tables(u::UNITS);
    throw std::runtime_error("unknown util constant " + name);
}

}  // namespace

std::string record_util(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + constant(v[1].str) + "]";
    if (fn == "normalize") return head(v) + guard([&] { return J(u::normalize(v[1].str, v[2].str)); }) + "]";
    if (fn == "match_lang") {
        // A str for valid_langs is the list of that one tag.
        const std::vector<std::string> valid = v[2].kind == Value::String ? std::vector<std::string>{v[2].str}
                                                                          : v[2].strings();
        return head(v) + guard([&] {
                   const auto [lang, distance] = u::match_lang(v[1].str, valid);
                   return "[" + J(lang) + "," + J(distance) + "]";
               }) +
               "]";
    }
    if (fn == "resolve_thousands_or_decimal")
        return head(v) + guard([&] { return J(u::resolve_thousands_or_decimal(v[1].str, v[2].str)); }) + "]";
    if (fn == "is_fraction") return head(v) + J(u::is_fraction(v[1].str)) + "]";
    if (fn == "pronounce_date") {
        arbtok::numbers::ovos_date_parser::DateTime date;
        date.year = static_cast<int>(v[1][0].integer);
        date.month = static_cast<int>(v[1][1].integer);
        date.day = static_cast<int>(v[1][2].integer);
        return head(v) + guard([&] { return J(u::pronounce_date(date, v[2].str)); }) + "]";
    }
    if (fn == "pronounce_time") return head(v) + guard([&] { return J(u::pronounce_time(v[1].str, v[2].str)); }) + "]";
    throw std::runtime_error("unknown util record " + fn);
}

}  // namespace parity
