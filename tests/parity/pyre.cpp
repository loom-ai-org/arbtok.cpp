// The regex engine of textnorm and util against Python's re. A pattern that the
// two modules hold as a module global is taken from the generated tables, so
// its "pattern" record checks the table against the module. A pattern they
// build at call time is compiled from the source text of its "pattern" record:
// those records pass by construction, and the match records check the engine
// on them.
#include <map>
#include <mutex>

#include "common.hpp"
#include "modules.hpp"
#include "pyre.hpp"
#include "textnorm_detail.hpp"

namespace parity {

namespace {

namespace ad = arbtok::detail;
namespace tp = arbtok::textnorm::py;
namespace up = arbtok::textnorm::py_util;
using arbtok::pyre::Match;
using arbtok::pyre::Pattern;

constexpr int kUnicode = 32;

const tp::Re* module_pattern(const std::string& name) {
    auto find = [](const auto& table, const std::string& bare) -> const tp::Re* {
        for (const auto& p : table)
            if (bare == p.name) return p.re;
        return nullptr;
    };
    if (name.rfind("textnorm.", 0) == 0) return find(tp::PATTERNS, name.substr(9));
    if (name.rfind("util.", 0) == 0) return find(up::PATTERNS, name.substr(5));
    return nullptr;
}

// The "pattern" records of the golden file, by name.
const std::map<std::string, std::pair<std::string, int>>& golden_patterns() {
    static std::once_flag once;
    static std::map<std::string, std::pair<std::string, int>> table;
    std::call_once(once, [] {
        for (const auto& line : json_line::read_gz_lines(std::string(ARBTOK_GOLDENS_DIR) + "/pyre.jsonl.gz")) {
            if (line.empty()) continue;
            const Value v = json_line::Parser(line).parse();
            if (v.kind == Value::Array && v.size() == 4 && v[0].str == "pattern")
                table[v[1].str] = {v[2].str, static_cast<int>(v[3].integer)};
        }
    });
    return table;
}

const Pattern& pattern(const std::string& name) {
    if (const tp::Re* re = module_pattern(name)) return arbtok::pyre::compiled(re->pattern, re->flags);
    const auto& table = golden_patterns();
    const auto it = table.find(name);
    if (it == table.end()) throw std::runtime_error("no pattern record for " + name);
    return arbtok::pyre::compiled(std::string_view(it->second.first), it->second.second & ~kUnicode);
}

std::string match_value(const std::optional<Match>& m) {
    if (!m) return "null";
    const auto& spans = m->spans();
    std::vector<std::string> groups;
    for (size_t i = 2; i < spans.size(); ++i) groups.push_back(J(spans[i]));
    return "[" + J(static_cast<long>(m->start())) + "," + J(static_cast<long>(m->end())) + "," + list(groups) + "]";
}

}  // namespace

std::string record_pyre(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "pattern") {
        if (const tp::Re* re = module_pattern(v[1].str))
            return "[" + J(fn) + "," + J(v[1].str) + "," + J(ad::to_utf8(re->pattern)) + "," + J(re->flags | kUnicode) +
                   "]";
        return "[" + J(fn) + "," + J(v[1].str) + "," + J(v[2].str) + "," + J(v[3].integer) + "]";
    }
    const Pattern& p = pattern(v[1].str);
    const ad::U32 text = ad::from_utf8(v[2].str);
    if (fn == "finditer") {
        std::vector<std::string> out;
        for (const auto& m : p.finditer(text)) out.push_back(match_value(m));
        return head(v) + list(out) + "]";
    }
    if (fn == "fullmatch") return head(v) + match_value(p.fullmatch(text)) + "]";
    if (fn == "match") return head(v) + match_value(p.match(text, static_cast<size_t>(v[3].integer))) + "]";
    if (fn == "search")
        return head(v) +
               match_value(p.search(text, static_cast<size_t>(v[3].integer), static_cast<size_t>(v[4].integer))) + "]";
    throw std::runtime_error("unknown pyre record " + fn);
}

}  // namespace parity
