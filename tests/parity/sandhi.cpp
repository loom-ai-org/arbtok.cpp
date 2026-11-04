#include "arbtok/sandhi.hpp"

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace sd = arbtok::sandhi;

std::string record_sandhi(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") {
        std::vector<std::string> items;
        for (const auto& [from, to] : sd::NUN_ASSIMILATION) items.push_back(list({J(from), J(to)}));
        return head(v) + list(items) + "]";
    }
    if (fn == "apply_cross_word") {
        std::vector<sd::CrossWord> words;
        for (const auto& w : v[1].items) words.push_back({w[0].str, w[1].str, w[2].boolean});
        return head(v) + guard([&] { return J(sd::apply_cross_word(words, v[2].boolean, v[3].str)); }) + "]";
    }
    if (fn == "_pausal") return head(v) + J(sd::detail::pausal_form(v[1].str, v[2].str)) + "]";
    if (fn == "_has_article") return head(v) + J(sd::detail::has_article(v[1].str)) + "]";
    if (fn == "_is_bare_wasl_alif") return head(v) + J(sd::detail::is_bare_wasl_alif(v[1].str)) + "]";
    throw std::runtime_error("unknown sandhi record " + fn);
}

}  // namespace parity
