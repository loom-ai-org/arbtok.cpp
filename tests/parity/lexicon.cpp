#include "arbtok/lexicon.hpp"

#include <map>

#include "../sha256.hpp"
#include "arbtok/dialect_lexicon.hpp"
#include "common.hpp"
#include "envcase.hpp"
#include "modules.hpp"
#include "pyio.hpp"

namespace parity {

namespace lx = arbtok::lexicon;

namespace {

std::string entries_json(const arbtok::OrderedDict<std::string>& entries) {
    std::vector<std::string> items;
    for (const auto& [k, v] : entries.items()) items.push_back(list({J(k), J(v)}));
    return list(items);
}

std::string shares_json(const arbtok::OrderedDict<double>& shares) {
    std::vector<std::string> items;
    for (const auto& [k, v] : shares.items()) items.push_back(list({J(k), F(v)}));
    return list(items);
}

std::optional<double> share_of(const Value& v) {
    if (v.is_null()) return std::nullopt;
    return std::stod(v.str);
}

const lx::StemLexicon& shared_lexicon(std::optional<double> share) {
    static std::map<std::optional<double>, lx::StemLexicon> lexicons;
    auto it = lexicons.find(share);
    if (it == lexicons.end()) it = lexicons.emplace(share, lx::StemLexicon(lx::DEFAULT_LEXICON, share)).first;
    return it->second;
}

std::string source_record() {
    const std::string path = lx::resolve_source(lx::DEFAULT_LEXICON);
    const std::string data = arbtok::detail::read_file(path);
    const lx::StemLexicon lex;
    const std::string entries = entries_json(lex.entries());
    const std::string shares = shares_json(lex.shares());
    const std::string parent = path.substr(0, path.rfind('/'));
    return "{\"revision\":" + J(parent.substr(parent.rfind('/') + 1)) + ",\"sha256\":" + J(sha256::hex(data)) +
           ",\"entries\":" + J(static_cast<unsigned long>(lex.entries().size())) +
           ",\"entries_sha256\":" + J(sha256::hex(entries)) +
           ",\"shares\":" + J(static_cast<unsigned long>(lex.shares().size())) +
           ",\"shares_sha256\":" + J(sha256::hex(shares)) + "}";
}

std::string env_call(const Value& c) {
    return run_env_case(c, [&](const std::string& root) -> std::string {
        const Value& call = *member(c, "call");
        const std::string& fn = call[0].str;
        auto arg = [&](size_t i) { return replace_all(call[i].str, "<ROOT>", root); };
        if (fn == "cache_dir") return J(lx::cache_dir());
        if (fn == "resolve_source") return J(lx::resolve_source(arg(1)));
        if (fn == "load") {
            const lx::StemLexicon lex(arg(1));
            const std::string entries = entries_json(lex.entries());
            return list({entries, shares_json(lex.shares())});
        }
        if (fn == "dialect_entries")
            return entries_json(arbtok::dialect_lexicon::DialectLexicon(call[1].str, arg(2)).entries());
        throw std::runtime_error("unknown env call " + fn);
    });
}

}  // namespace

std::string record_env(const Value& v) { return head(v) + env_call(v[1]) + "]"; }

std::string record_lexicon(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") {
        const std::string& name = v[1].str;
        if (name == "DEFAULT_LEXICON") return head(v) + J(lx::DEFAULT_LEXICON) + "]";
        if (name == "AMBIGUOUS_SHARE") return head(v) + J(arbtok::detail::float_repr(lx::AMBIGUOUS_SHARE)) + "]";
        return head(v) + J(lx::MIN_CONTEXT_WORDS) + "]";
    }
    if (fn == "parse") return head(v) + entries_json(lx::parse(v[1].str)) + "]";
    if (fn == "parse_shares") return head(v) + shares_json(lx::parse_shares(v[1].str)) + "]";
    if (fn == "source") return head(v) + source_record() + "]";
    if (fn == "get") {
        const auto ctx = v[2].is_null() ? std::nullopt : std::optional<long>(v[2].integer);
        return head(v) + J(shared_lexicon(share_of(v[1])).get(v[3].str, ctx)) + "]";
    }
    if (fn == "ambiguous") return head(v) + J(shared_lexicon(share_of(v[1])).ambiguous(v[2].str)) + "]";
    if (fn == "contains") return head(v) + J(shared_lexicon(lx::AMBIGUOUS_SHARE).contains(v[1].str)) + "]";
    if (fn == "len") return head(v) + J(static_cast<unsigned long>(shared_lexicon(lx::AMBIGUOUS_SHARE).size())) + "]";
    if (fn == "env") return record_env(v);
    throw std::runtime_error("unknown lexicon record " + fn);
}

}  // namespace parity
