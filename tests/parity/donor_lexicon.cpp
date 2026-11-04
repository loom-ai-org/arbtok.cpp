#include "arbtok/donor_lexicon.hpp"

#include <orthography2ipa/orthography2ipa.hpp>

#include "common.hpp"
#include "data_path.hpp"
#include "modules.hpp"

namespace parity {

namespace dn = arbtok::donor_lexicon;

namespace {

std::string step(const std::string& op, const std::string& code) {
    if (op == "ensure_registered") return guard([&] { return J(dn::ensure_registered(code)); });
    if (op == "lexicon_path")
        return guard([&] {
            try {
                return J(rel_data(orthography2ipa::lexicon_path(code)));
            } catch (const orthography2ipa::FileNotFoundError& e) {
                throw arbtok::Error("FileNotFoundError", e.what());
            }
        });
    if (op == "bundled_path") return J(rel_data(dn::bundled_path(code)));
    if (op == "register") {
        orthography2ipa::register_lexicon(code, arbtok::data_dir() + "/lexicons/ar-EG.tsv");
        return "null";
    }
    if (op == "register_missing") {
        orthography2ipa::register_lexicon(code, "/nonexistent/caller/own.tsv");
        return "null";
    }
    throw std::runtime_error("unknown donor step " + op);
}

}  // namespace

std::string record_donor_lexicon(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + J(dn::BUNDLED_DONOR) + "]";
    if (fn == "bundled_path_default") return head(v) + J(rel_data(dn::bundled_path())) + "]";
    if (fn == "sequence") {
        orthography2ipa::clear_lexicons();
        std::vector<std::string> out;
        for (const auto& s : v[1].items) out.push_back(step(s[0].str, s[1].str));
        orthography2ipa::clear_lexicons();
        return head(v) + list(out) + "]";
    }
    throw std::runtime_error("unknown donor_lexicon record " + fn);
}

}  // namespace parity
