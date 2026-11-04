#include "arbtok/fusion.hpp"

#include <cstring>
#include <map>

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace fu = arbtok::fusion;

namespace {

std::vector<float> from_hex(const std::string& hex) {
    std::vector<float> out(hex.size() / 8);
    for (size_t i = 0; i < out.size(); ++i) {
        std::uint32_t bits = 0;
        for (int b = 0; b < 4; ++b)
            bits |= static_cast<std::uint32_t>(std::stoul(hex.substr(i * 8 + b * 2, 2), nullptr, 16)) << (8 * b);
        std::memcpy(&out[i], &bits, 4);
    }
    return out;
}

std::string to_hex(const std::vector<float>& row) {
    std::string out;
    char buffer[3];
    for (float f : row) {
        std::uint32_t bits;
        std::memcpy(&bits, &f, 4);
        for (int b = 0; b < 4; ++b) {
            std::snprintf(buffer, sizeof buffer, "%02x", (bits >> (8 * b)) & 0xff);
            out += buffer;
        }
    }
    return out;
}

fu::Options options(const Value& config) {
    fu::Options o;
    for (const auto& [k, v] : config.members) {
        if (k == "waqf") o.waqf = v.boolean;
        else if (k == "lexicon") o.lexicon = opt_str(v);
        else if (k == "ambiguous_share") o.ambiguous_share = v.is_null() ? std::nullopt : std::optional<double>(v.real);
        else if (k == "dialect_lexicon") o.dialect_lexicon = v.boolean;
        else if (k == "beam") o.beam = static_cast<long>(v.integer);
        else if (k == "topk") o.topk = static_cast<long>(v.integer);
        else if (k == "lattice_weight") o.lattice_weight = v.real;
        else throw std::runtime_error("unknown option " + k);
    }
    return o;
}

fu::FusionDiacritizer& reader(const std::string& lect, const Value& config) {
    static std::map<std::string, fu::FusionDiacritizer> readers;
    const std::string key = lect + "\t" + json_line::dump(config);
    auto it = readers.find(key);
    if (it == readers.end()) it = readers.emplace(key, fu::FusionDiacritizer(lect, options(config))).first;
    return it->second;
}

}  // namespace

std::string record_fusion(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "logprobs") {
        const auto row = from_hex(v[1].str);
        return head(v) + J(to_hex(fu::logprobs(row.data(), row.size()))) + "]";
    }
    if (fn == "has_arabic_letter") return head(v) + J(fu::has_arabic_letter(v[1].str)) + "]";
    if (fn == "hypothesis_repr") {
        const fu::Hypothesis h{{}, std::stod(v[2].str), v[1].str, std::stod(v[3].str)};
        return head(v) + J(h.repr()) + "]";
    }
    if (fn == "fusion") {
        auto& d = reader(v[1].str, v[2]);
        std::vector<std::string>* lists[] = {&d.recovered, &d.looked_up, &d.dialect_looked_up, &d.rejected};
        static const char* names[] = {"recovered", "looked_up", "dialect_looked_up", "rejected"};
        size_t before[4];
        for (int i = 0; i < 4; ++i) before[i] = lists[i]->size();
        const std::string out = d.diacritize(v[3].str);
        std::string grown = "{";
        for (int i = 0; i < 4; ++i) {
            const std::vector<std::string> added(lists[i]->begin() + static_cast<long>(before[i]), lists[i]->end());
            grown += std::string(i ? "," : "") + J(names[i]) + ":" + J(added);
        }
        grown += "}";
        std::vector<std::string> nbest;
        for (const auto& h : d.last_nbest) {
            std::vector<long long> ids(h.class_ids.begin(), h.class_ids.end());
            nbest.push_back(list({J(h.text), F(h.logprob), F(h.lattice_cost), J(ids)}));
        }
        return head(v) + list({J(out), grown, list(nbest)}) + "]";
    }
    throw std::runtime_error("unknown fusion record " + fn);
}

}  // namespace parity
