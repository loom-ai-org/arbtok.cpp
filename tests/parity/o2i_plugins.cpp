#include "arbtok/o2i_plugins.hpp"

#include <cstdlib>
#include <map>
#include <memory>
#include <optional>

#include "arbtok/lattice.hpp"
#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace op = arbtok::o2i_plugins;

namespace {

const op::ArbtokDiacritizer& diacritizer() {
    static const op::ArbtokDiacritizer d;
    return d;
}

const op::ArbtokRescorers& rescorers() {
    static const op::ArbtokRescorers r;
    return r;
}

const op::ArbtokSandhi& sandhi() {
    static const op::ArbtokSandhi s;
    return s;
}

std::vector<std::string> codes_of(const std::string& cls) {
    if (cls == "ArbtokDiacritizer") return diacritizer().language_codes();
    if (cls == "ArbtokRescorers") return rescorers().language_codes();
    if (cls == "ArbtokSandhi") return sandhi().language_codes();
    throw std::runtime_error("unknown plugin class " + cls);
}

// The Python class name of each rescorer, found by its place in
// DEFAULT_RESCORERS.
std::vector<std::string> rescorer_names(const std::string& lang) {
    const auto& all = arbtok::lattice::default_rescorers();
    const auto& names = arbtok::lattice::default_rescorer_names();
    std::vector<std::string> out;
    for (const auto* r : rescorers().rescorers(lang)) {
        size_t i = 0;
        while (i < all.size() && all[i] != r) ++i;
        out.push_back(i < all.size() ? names[i] : "?");
    }
    return out;
}

// The call with ARBTOK_DIACRITIZER set to value, or unset for null, and the
// variable restored after it.
std::string with_env(const Value& value, const std::function<std::string()>& call) {
    const char* old = std::getenv("ARBTOK_DIACRITIZER");
    const std::optional<std::string> saved = old ? std::optional<std::string>(old) : std::nullopt;
    if (value.is_null()) ::unsetenv("ARBTOK_DIACRITIZER");
    else ::setenv("ARBTOK_DIACRITIZER", value.str.c_str(), 1);
    std::string out;
    try {
        out = guard(call);
    } catch (...) {
        if (saved) ::setenv("ARBTOK_DIACRITIZER", saved->c_str(), 1);
        else ::unsetenv("ARBTOK_DIACRITIZER");
        throw;
    }
    if (saved) ::setenv("ARBTOK_DIACRITIZER", saved->c_str(), 1);
    else ::unsetenv("ARBTOK_DIACRITIZER");
    return out;
}

// One G2P per (lang, plugins), with the plugins registered once.
const orthography2ipa::G2P& g2p(const std::string& lang, const Value& plugins) {
    static const bool registered = (op::register_plugins(), true);
    (void)registered;
    static std::map<std::string, std::unique_ptr<orthography2ipa::G2P>> engines;
    const std::string key = lang + "\t" + json_line::dump(plugins);
    auto it = engines.find(key);
    if (it == engines.end()) {
        std::map<std::string, std::vector<std::string>> overrides;
        for (const auto& [stage, names] : plugins.members) overrides[stage] = names.strings();
        it = engines.emplace(key, std::make_unique<orthography2ipa::G2P>(lang, overrides)).first;
    }
    return *it->second;
}

std::vector<bool> bools(const Value& v) {
    std::vector<bool> out;
    for (const auto& b : v.items) out.push_back(b.boolean);
    return out;
}

}  // namespace

std::string record_o2i_plugins(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const")
        return list({J("const"), J(op::ArbtokDiacritizer::PATHS),
                     J(std::vector<std::string>{"ArbtokDiacritizer", "ArbtokRescorers", "ArbtokSandhi"})});
    if (fn == "language_codes") return head(v) + J(codes_of(v[1].str)) + "]";
    if (fn == "rescorers") return head(v) + J(rescorer_names(v[1].str)) + "]";
    if (fn == "normalize")
        return head(v) + with_env(v[1], [&] { return J(diacritizer().normalize(v[2].str, v[3].str)); }) + "]";
    if (fn == "sandhi")
        return head(v) +
               guard([&] { return J(sandhi().apply(v[1].strings(), v[2].strings(), bools(v[3]), v[4].str)); }) + "]";
    if (fn == "g2p") return head(v) + guard([&] { return J(g2p(v[1].str, v[2]).transcribe(v[3].str)); }) + "]";
    throw std::runtime_error("unknown o2i_plugins record " + fn);
}

}  // namespace parity
