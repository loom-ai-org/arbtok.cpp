#include "arbtok/diacritize.hpp"

#include <algorithm>
#include <map>

#include "arbtok/arbtok.hpp"
#include "arbtok/ensemble.hpp"
#include "common.hpp"
#include "detail.hpp"
#include "modules.hpp"

namespace parity {

namespace dz = arbtok::diacritize;

namespace {

// The options of a record: a dict of LatticeDiacritizer keywords, where
// "model_path": "bundled" names the bundled model and vocabulary.
dz::Options options(const Value& config) {
    dz::Options o;
    for (const auto& [k, v] : config.members) {
        if (k == "waqf") o.waqf = v.boolean;
        else if (k == "lexicon") o.lexicon = opt_str(v);
        else if (k == "dialect_lexicon") o.dialect_lexicon = v.boolean;
        else if (k == "model_path" && v.str == "bundled") {
            o.model_path = arbtok::ensemble::default_ensemble_onnx();
            o.vocab_path = arbtok::ensemble::default_vocab();
        } else throw std::runtime_error("unknown option " + k);
    }
    return o;
}

dz::LatticeDiacritizer& reader(const std::string& lect, const Value& config) {
    static std::map<std::string, dz::LatticeDiacritizer> readers;
    const std::string key = lect + "\t" + json_line::dump(config);
    auto it = readers.find(key);
    if (it == readers.end()) it = readers.emplace(key, dz::LatticeDiacritizer(lect, options(config))).first;
    return it->second;
}

std::vector<std::size_t> positions(const Value& v) {
    std::vector<std::size_t> out;
    for (const auto& p : v.items) out.push_back(static_cast<std::size_t>(p.integer));
    return out;
}

std::string words_json(const dz::DiacritizedText& t) {
    std::vector<std::string> words;
    for (const auto& w : t.words) words.push_back(list({J(w.surface), J(w.output), J(w.provenance)}));
    return list(words);
}

}  // namespace

std::string record_diacritize(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") {
        const std::string& name = v[1].str;
        if (name == "PROVENANCE") return head(v) + J(dz::PROVENANCE) + "]";
        // _MARKS: the code points strip_marks removes. _RESTORABLE: for each
        // letter, the letters skeleton_is_preserved accepts in its place.
        if (name == "_MARKS") {
            std::vector<std::string> marks;
            for (char32_t c = 1; c < 0x10000; ++c)
                if (!(c >= 0xD800 && c < 0xE000) && dz::strip_marks(arbtok::detail::s8(c)).empty())
                    marks.push_back(arbtok::detail::s8(c));
            return head(v) + J(marks) + "]";
        }
        std::vector<std::string> items;
        auto letter = [](char32_t c) { return !dz::strip_marks(arbtok::detail::s8(c)).empty(); };
        for (char32_t a = 0x0600; a < 0x0700; ++a) {
            if (!letter(a)) continue;
            std::vector<std::string> seats;
            for (char32_t b = 0x0600; b < 0x0700; ++b)
                if (a != b && letter(b) && dz::skeleton_is_preserved(arbtok::detail::s8(a), arbtok::detail::s8(b)))
                    seats.push_back(arbtok::detail::s8(b));
            if (!seats.empty()) items.push_back(list({J(arbtok::detail::s8(a)), J(seats)}));
        }
        return head(v) + list(items) + "]";
    }
    if (fn == "provenance_flags") {
        const dz::DiacritizedWord w{"x", "x", v[1].str};
        return head(v) + list({J(w.lect_constrained()), J(w.is_failure())}) + "]";
    }
    if (fn == "author_complete") return head(v) + J(dz::author_complete(v[1].str, positions(v[2]))) + "]";
    if (fn == "proclitic_complete") return head(v) + J(dz::proclitic_complete(v[1].str, positions(v[2]))) + "]";
    if (fn == "lattice_error")
        return head(v) + guard([&] {
                   dz::LatticeDiacritizer d(v[1].str);
                   return J("ok");
               }) + "]";
    if (fn == "word") {
        auto& d = reader(v[1].str, v[2]);
        const size_t before[] = {d.rejected.size(), d.repaired.size(), d.looked_up.size(), d.dialect_looked_up.size()};
        const dz::DiacritizedWord rec = d.diacritize_word_record(v[3].str);
        const size_t after[] = {d.rejected.size(), d.repaired.size(), d.looked_up.size(), d.dialect_looked_up.size()};
        static const char* names[] = {"rejected", "repaired", "looked_up", "dialect_looked_up"};
        std::vector<std::string> grown;
        for (int i = 0; i < 4; ++i)
            if (before[i] != after[i]) grown.push_back(names[i]);
        return head(v, 2) + list({J(rec.output), J(rec.provenance)}) + "," + J(grown) + "]";
    }
    if (fn == "text") {
        const dz::DiacritizedText t = reader(v[1].str, v[2]).diacritize_text(v[3].str);
        std::vector<std::string> by;
        for (const auto& [prov, surfaces] : t.by_provenance()) by.push_back(list({J(prov), J(surfaces)}));
        return head(v) + list({J(t.text), words_json(t), list(by), J(t.str())}) + "]";
    }
    if (fn == "vocalize") {
        const dz::DiacritizedText t = v[3].members.empty() ? arbtok::vocalize(v[1].str, v[2].str)
                                                           : arbtok::vocalize(v[1].str, v[2].str, options(v[3]));
        return head(v) + list({J(t.text), words_json(t)}) + "]";
    }
    throw std::runtime_error("unknown diacritize record " + fn);
}

}  // namespace parity
