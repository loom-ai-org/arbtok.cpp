#include "arbtok/dialect_lexicon.hpp"

#include <map>

#include "common.hpp"
#include "data_path.hpp"
#include "modules.hpp"

namespace parity {

std::string record_env(const Value& v);

namespace dl = arbtok::dialect_lexicon;

namespace {

std::string entries_json(const arbtok::OrderedDict<std::string>& entries) {
    std::vector<std::string> items;
    for (const auto& [k, v] : entries.items()) items.push_back(list({J(k), J(v)}));
    return list(items);
}

dl::DialectLexicon& lexicon(const std::string& lect) {
    static std::map<std::string, dl::DialectLexicon> lexicons;
    auto it = lexicons.find(lect);
    if (it == lexicons.end()) it = lexicons.emplace(lect, dl::DialectLexicon(lect)).first;
    return it->second;
}

}  // namespace

std::string record_dialect_lexicon(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "lexicon_dir") return head(v) + J(rel_data(dl::lexicon_dir())) + "]";
    if (fn == "available_lects") return head(v) + J(dl::available_lects()) + "]";
    if (fn == "lexicon_file") return head(v) + guard([&] { return J(rel_data(dl::lexicon_file(v[1].str))); }) + "]";
    if (fn == "parent_chain") return head(v) + J(dl::parent_chain(v[1].str)) + "]";
    if (fn == "entries") return head(v) + guard([&] { return entries_json(lexicon(v[1].str).entries()); }) + "]";
    if (fn == "len") return head(v) + guard([&] { return J(static_cast<unsigned long>(lexicon(v[1].str).size())); }) + "]";
    if (fn == "get") return head(v) + J(lexicon(v[1].str).get(v[2].str)) + "]";
    if (fn == "contains") return head(v) + J(lexicon(v[1].str).contains(v[2].str)) + "]";
    if (fn == "parse") return head(v) + entries_json(dl::parse(v[1].str)) + "]";
    if (fn == "env") return record_env(v);
    throw std::runtime_error("unknown dialect_lexicon record " + fn);
}

}  // namespace parity
