#include "arbtok/translit.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace tl = arbtok::translit;

namespace {

std::string items(const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::vector<std::string> out;
    for (const auto& [k, v] : pairs) out.push_back(list({J(k), J(v)}));
    return list(out);
}

std::string constant(const std::string& name) {
    if (name == "DONOR_LANG") return J(tl::DONOR_LANG);
    if (name == "DONOR_BY_SCRIPT") return items(tl::DONOR_BY_SCRIPT);
    if (name == "ESTABLISHED_LOANS") {
        std::vector<std::string> out;
        for (const auto& [donor, table] : tl::ESTABLISHED_LOANS) out.push_back(list({J(donor), items(table)}));
        return list(out);
    }
    if (name == "_TABLES") return items(tl::detail::table_names());
    if (name == "_BORROWABLE_TABLES") return J(tl::detail::borrowable_names());
    throw std::runtime_error("unknown translit constant " + name);
}

}  // namespace

std::string record_translit(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + constant(v[1].str) + "]";
    if (fn == "table") {
        const tl::Table* table = tl::detail::table_by_name(v[1].str);
        if (table == nullptr) throw std::runtime_error("unknown translit table " + v[1].str);
        return head(v) + items(*table) + "]";
    }
    if (fn == "is_latin") return head(v) + J(tl::is_latin(v[1].str)) + "]";
    if (fn == "guest_script") return head(v) + J(tl::guest_script(v[1].str)) + "]";
    if (fn == "segment_ipa") return head(v) + J(tl::segment_ipa(v[1].str)) + "]";
    if (fn == "nativization_table")
        return head(v) + guard([&] { return J(tl::detail::name_of(tl::nativization_table(v[1].str))); }) + "]";
    if (fn == "_targets") return head(v) + J(tl::detail::targets(v[1].str)) + "]";
    if (fn == "_established") return head(v) + J(tl::detail::established(v[1].str, v[2].str)) + "]";
    if (fn == "_project") return head(v) + J(tl::detail::project(v[1].str, v[2].str)) + "]";
    if (fn == "nativize") return head(v) + J(tl::nativize(v[1].str, v[2].str)) + "]";
    if (fn == "transliterate")
        return head(v) + guard([&] { return J(tl::transliterate(v[1].str, v[2].str, opt_str(v[3]), v[4].boolean)); }) +
               "]";
    throw std::runtime_error("unknown translit record " + fn);
}

}  // namespace parity
