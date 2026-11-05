#include "arbtok/plugin.hpp"

#include <map>

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace pl = arbtok::plugin;

namespace {

pl::Options options(const Value& config) {
    pl::Options o;
    for (const auto& [k, v] : config.members) {
        if (k == "lang") o.lang = opt_str(v);
        else if (k == "register") o.register_name = v.str;
        else if (k == "diacritize") o.diacritize = v.boolean;
        else if (k == "stress") o.stress = v.boolean;
        else if (k == "lexicon") o.lexicon = opt_str(v);
        else if (k == "dialect_lexicon") o.dialect_lexicon = v.boolean;
        else if (k == "nativize") o.nativize = v.boolean;
        else if (k == "arabizi") o.arabizi = v.boolean;
        else if (k == "pausal") o.pausal = v.is_null() ? std::nullopt : std::optional<bool>(v.boolean);
        else if (k == "fusion") o.fusion = v.boolean;
        else throw std::runtime_error("unknown option " + k);
    }
    return o;
}

// One instance per keyword set, shared by the records in file order, as the
// dump shares one.
pl::ArbtokG2PPlugin& instance(const Value& config) {
    static std::map<std::string, pl::ArbtokG2PPlugin> instances;
    const std::string key = json_line::dump(config);
    auto it = instances.find(key);
    if (it == instances.end()) it = instances.emplace(key, pl::ArbtokG2PPlugin(options(config))).first;
    return it->second;
}

pl::WordContext context(const Value& spec) {
    pl::WordContext c;
    for (const auto& [k, v] : spec.members) {
        if (k == "prev_word") c.prev_word = opt_str(v);
        else if (k == "next_word") c.next_word = opt_str(v);
        else if (k == "lang") c.lang = opt_str(v);
        else throw std::runtime_error("unknown context field " + k);
    }
    return c;
}

std::optional<bool> opt_bool(const Value& v) {
    if (v.is_null()) return std::nullopt;
    return v.boolean;
}

}  // namespace

std::string record_plugin(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const")
        return list({J("const"), J("PUNCT_STRIP"), J(pl::PUNCT_STRIP), J(pl::ArbtokG2PPlugin().language_codes())});
    if (fn == "init")
        return head(v) + guard([&] {
                   const pl::ArbtokG2PPlugin p(options(v[1]));
                   return list({J(p.lang), J(p.fusion), J(p.diacritize), J(p.stress), J(p.lexicon),
                                J(p.dialect_lexicon), J(p.nativize), J(p.arabizi), J(p.pausal)});
               }) +
               "]";
    if (fn == "normalize") return head(v) + guard([&] { return J(instance(v[1]).normalize(v[2].str)); }) + "]";
    if (fn == "transcribe")
        return head(v) + guard([&] { return J(instance(v[1]).transcribe(v[2].str, opt_bool(v[3]))); }) + "]";
    if (fn == "transcribe_word")
        return head(v) + guard([&] {
                   if (v[3].is_null()) return J(instance(v[1]).transcribe_word(v[2].str));
                   const pl::WordContext c = context(v[3]);
                   return J(instance(v[1]).transcribe_word(v[2].str, &c));
               }) +
               "]";
    throw std::runtime_error("unknown plugin record " + fn);
}

}  // namespace parity
