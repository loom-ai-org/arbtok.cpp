// The front end of a phoonnx arbtok voice. Each record holds the
// ArbtokG2PPlugin calls that phoonnx and scriptconv made for one input text,
// as tools/goldens/record_phoonnx.py recorded them. This replays them on the
// C++ ArbtokG2PPlugin and applies phoonnx's own steps to the results: each
// "(...)" run removed, the rest split into code points, the empty lists left
// out, and each list turned into the voice's token ids. The phoneme_id_map
// comes from the header of tests/data/phoonnx_calls.jsonl.gz.
#include <map>

#include "arbtok/plugin.hpp"
#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace pl = arbtok::plugin;

namespace {

pl::Options options(const Value& keywords) {
    pl::Options o;
    for (const auto& [k, v] : keywords.members) {
        if (k == "lang") o.lang = opt_str(v);
        else if (k == "diacritize") o.diacritize = v.boolean;
        else if (k == "register") o.register_name = v.str;
        else throw std::runtime_error("unknown phoonnx keyword " + k);
    }
    return o;
}

pl::ArbtokG2PPlugin& engine(const std::string& lang, const std::string& model, const Value& keywords) {
    static std::map<std::string, pl::ArbtokG2PPlugin> engines;
    const std::string key = lang + "\t" + model + "\t" + json_line::dump(keywords);
    auto it = engines.find(key);
    if (it == engines.end()) it = engines.emplace(key, pl::ArbtokG2PPlugin(options(keywords))).first;
    return it->second;
}

// re.sub(r"\([^)]+\)", "", s): "(", one or more characters that are not ")",
// then ")". The bytes of "(" and ")" never occur inside a UTF-8 sequence.
std::string drop_flags(const std::string& s) {
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '(') {
            const size_t close = s.find(')', i + 1);
            if (close != std::string::npos && close > i + 1) {
                i = close + 1;
                continue;
            }
        }
        out += s[i++];
    }
    return out;
}

std::vector<std::string> code_points(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        const auto lead = static_cast<unsigned char>(s[i]);
        const size_t n = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        out.push_back(s.substr(i, n));
        i += n;
    }
    return out;
}

const std::map<std::string, long long>& phoneme_ids() {
    static const std::map<std::string, long long> ids = [] {
        const std::string path = std::string(ARBTOK_TEST_DATA_DIR) + "/phoonnx_calls.jsonl.gz";
        const auto lines = json_line::read_gz_lines(path);
        if (lines.empty()) throw std::runtime_error("empty " + path);
        const Value header = json_line::parse(lines[0]);
        const Value* map = nullptr;
        for (const auto& [k, v] : header.members)
            if (k == "phoneme_id_map") map = &v;
        if (map == nullptr) throw std::runtime_error(path + " has no phoneme_id_map");
        std::map<std::string, long long> out;
        for (const auto& [phoneme, id] : map->members) out[phoneme] = id.integer;
        for (const char* special : {"_", "^", "$"})
            if (!out.count(special)) throw std::runtime_error(path + " maps no " + special);
        return out;
    }();
    return ids;
}

std::vector<long long> token_ids(const std::vector<std::string>& phonemes) {
    const auto& ids = phoneme_ids();
    const long long blank = ids.at("_");
    std::vector<long long> out = {ids.at("^"), blank};
    for (const auto& p : phonemes) {
        const auto it = ids.find(p);
        if (it == ids.end()) continue;
        out.push_back(it->second);
        out.push_back(blank);
    }
    out.push_back(ids.at("$"));
    return out;
}

}  // namespace

std::string record_phoonnx(const Value& v) {
    if (v[0].str != "voice") throw std::runtime_error("unknown phoonnx record " + v[0].str);
    std::vector<std::string> outputs;
    std::vector<std::vector<std::string>> phonemes;
    std::vector<std::vector<long long>> ids;
    for (const auto& call : v[4].items) {
        outputs.push_back(engine(v[1].str, v[2].str, call[0]).transcribe(call[1].str));
        std::vector<std::string> p = code_points(drop_flags(outputs.back()));
        if (p.empty()) continue;
        ids.push_back(token_ids(p));
        phonemes.push_back(std::move(p));
    }
    return head(v, 3) + J(outputs) + "," + J(phonemes) + "," + J(ids) + "]";
}

}  // namespace parity
