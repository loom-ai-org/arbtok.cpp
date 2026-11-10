#include "arbtok/o2i_plugins.hpp"

#include <map>
#include <memory>
#include <optional>

#include "arbtok/lattice.hpp"
#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace op = arbtok::o2i_plugins;

namespace {

// One ArbtokDiacritizer per path, the default one for null, made on first use.
const op::ArbtokDiacritizer& diacritizer(const Value& path) {
    static std::map<std::string, std::unique_ptr<op::ArbtokDiacritizer>> made;
    const std::string key = json_line::dump(path);
    auto it = made.find(key);
    if (it == made.end()) {
        auto d = path.is_null() ? std::make_unique<op::ArbtokDiacritizer>()
                                : std::make_unique<op::ArbtokDiacritizer>(path.str);
        it = made.emplace(key, std::move(d)).first;
    }
    return *it->second;
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
    if (cls == "ArbtokDiacritizer") return diacritizer(Value()).language_codes();
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

// One G2P per (lang, path), built with pipeline(lang, path).
const orthography2ipa::G2P& pipeline_g2p(const std::string& lang, const std::string& path) {
    static std::map<std::pair<std::string, std::string>, std::unique_ptr<orthography2ipa::G2P>> engines;
    auto it = engines.find({lang, path});
    if (it == engines.end()) {
        auto g2p = std::make_unique<orthography2ipa::G2P>(lang, std::map<std::string, std::vector<std::string>>{}, "",
                                                          false, true, true, true, nullptr, nullptr,
                                                          op::pipeline(lang, path));
        it = engines.emplace(std::make_pair(lang, path), std::move(g2p)).first;
    }
    return *it->second;
}

// The message of the ValueError that the path refuses, or null.
std::string refused(const std::string& path) {
    try {
        op::ArbtokDiacritizer d(path);
    } catch (const arbtok::ValueError& e) {
        return J(e.what());
    }
    return "null";
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
                     J(std::vector<std::string>{"ArbtokDiacritizer", "ArbtokRescorers", "ArbtokSandhi", "pipeline"})});
    if (fn == "language_codes") return head(v) + J(codes_of(v[1].str)) + "]";
    if (fn == "rescorers") return head(v) + J(rescorer_names(v[1].str)) + "]";
    if (fn == "normalize")
        return head(v) + guard([&] { return J(diacritizer(v[1]).normalize(v[2].str, v[3].str)); }) + "]";
    if (fn == "refused") return head(v) + refused(v[1].str) + "]";
    if (fn == "sandhi")
        return head(v) +
               guard([&] { return J(sandhi().apply(v[1].strings(), v[2].strings(), bools(v[3]), v[4].str)); }) + "]";
    if (fn == "g2p") return head(v) + guard([&] { return J(g2p(v[1].str, v[2]).transcribe(v[3].str)); }) + "]";
    if (fn == "pipeline")
        return head(v) + guard([&] { return J(pipeline_g2p(v[1].str, v[2].str).transcribe(v[3].str)); }) + "]";
    if (fn == "mark")
        return head(v) + guard([&] { return J(pipeline_g2p(v[1].str, "lattice").pipeline().mark(v[2].str)); }) + "]";
    if (fn == "keep_letters") return head(v) + J(op::detail::keep_letters(v[1].str, v[2].str)) + "]";
    throw std::runtime_error("unknown o2i_plugins record " + fn);
}

}  // namespace parity
