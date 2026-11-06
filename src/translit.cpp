#include "arbtok/translit.hpp"

#include <algorithm>
#include <map>
#include <set>

#include <orthography2ipa/orthography2ipa.hpp>

#include "arbtok/dialects.hpp"
#include "arbtok/donor_lexicon.hpp"
#include "detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::translit {

namespace o2i = orthography2ipa;
using arbtok::detail::s8;
using arbtok::detail::u;
using arbtok::detail::U32;

const std::string* table_get(const Table& table, const std::string& key) {
    for (const auto& [k, v] : table)
        if (k == key) return &v;
    return nullptr;
}

const std::string DONOR_LANG = "en-GB";

const std::vector<std::pair<std::string, std::string>> DONOR_BY_SCRIPT = {
    {"Latin", "en-GB"}, {"Cyrillic", "ru"}, {"Greek", "el"}};

const Table SEGMENT_MAP = {
    {"p", "b"}, {"v", "f"}, {"tʃ", "ʃ"}, {"dʒ", "dʒ"}, {"ʒ", "dʒ"}, {"ɡ", "ɡ"}, {"ɹ", "r"}, {"ɫ", "l"},
    {"ɪ", "i"}, {"iː", "iː"}, {"ʊ", "u"}, {"uː", "uː"}, {"ɛ", "i"}, {"æ", "a"}, {"ə", "a"}, {"ʌ", "a"},
    {"ɑː", "aː"}, {"ɔː", "uː"}, {"ɒ", "u"}, {"ɜː", "a"}, {"eɪ", "eː"}, {"əʊ", "oː"}, {"aɪ", "aj"},
    {"aʊ", "aw"}, {"ɔɪ", "uj"}, {"ɪə", "iː"}, {"eə", "eː"}, {"ʊə", "uː"}};

namespace {

// The rows the cited tables share, in the order of their dict literals:
// _PAN_ARABIC_CONSONANTS, then _THREE_VOWELS.
#define ARBTOK_PAN_ARABIC {"p", "b"}, {"v", "f"}, {"tʃ", "ʃ"}, {"ɹ", "r"}, {"ɫ", "l"}
#define ARBTOK_THREE_VOWELS                                                                                       \
    {"ɪ", "i"}, {"iː", "iː"}, {"ʊ", "u"}, {"uː", "uː"}, {"ɛ", "i"}, {"æ", "a"}, {"ə", "a"}, {"ʌ", "a"},          \
        {"ɑː", "aː"}, {"ɔː", "uː"}, {"ɒ", "u"}, {"ɜː", "a"}, {"aɪ", "aj"}, {"aʊ", "aw"}, {"ɔɪ", "uj"},          \
        {"ɪə", "iː"}, {"ʊə", "uː"}

const Table EGYPTIAN_MAP = {ARBTOK_PAN_ARABIC, ARBTOK_THREE_VOWELS,
                            {"dʒ", "ɡ"}, {"ʒ", "ʒ"}, {"ɡ", "ɡ"}, {"θ", "t"}, {"ð", "d"}};
const Table LEVANTINE_MAP = {ARBTOK_PAN_ARABIC, ARBTOK_THREE_VOWELS,
                             {"dʒ", "dʒ"}, {"ʒ", "ʒ"}, {"ɡ", "ɡ"}, {"eɪ", "eː"}, {"əʊ", "oː"}};
const Table DEFAULT_MAP = {ARBTOK_PAN_ARABIC, ARBTOK_THREE_VOWELS,
                           {"dʒ", "dʒ"}, {"ʒ", "dʒ"}, {"ɡ", "ɡ"}, {"eɪ", "eː"}, {"əʊ", "oː"}};
#undef ARBTOK_PAN_ARABIC
#undef ARBTOK_THREE_VOWELS

// _DEFAULT_MAP without p and v, then the Maghrebi rows: ʒ keeps its place.
Table maghrebi_map() {
    Table out;
    for (const auto& row : DEFAULT_MAP)
        if (row.first != "p" && row.first != "v") out.push_back(row);
    for (auto& row : out)
        if (row.first == "ʒ") row.second = "ʒ";
    for (const auto& row : Table{{"ʁ", "r"}, {"y", "i"}, {"ø", "u"}, {"œ", "u"}}) out.push_back(row);
    return out;
}
const Table MAGHREBI_MAP = maghrebi_map();

const Table RAISED = {{"o", "u"}, {"ɔ", "u"}, {"e", "i"}, {"ɛ", "i"}, {"ø", "i"}, {"œ", "i"}};

// _TABLES, keyed by the spec code each table is cited for.
const std::vector<std::pair<std::string, const Table*>> TABLES = {
    {"ar-SA-x-najd", &SEGMENT_MAP}, {"ar-EG", &EGYPTIAN_MAP}, {"ar-x-levantine", &LEVANTINE_MAP},
    {"ar-LY", &DEFAULT_MAP},        {"ar-MR", &DEFAULT_MAP},  {"ar-x-maghrebi", &MAGHREBI_MAP},
    {"ar", &DEFAULT_MAP}};

// _BORROWABLE_TABLES.
const std::vector<const Table*> BORROWABLE = {&SEGMENT_MAP, &EGYPTIAN_MAP, &LEVANTINE_MAP, &DEFAULT_MAP};

const std::string NG = "ŋ";
const std::string LONG = "ː";
const U32 COMBINING = U"ːʰʲˤ̃";

// _LIGATURES: the keys of SEGMENT_MAP and the affricates and long vowels,
// longest first. Two ligatures of one length never both match at one
// position, so their order among themselves does not matter.
std::vector<U32> ligatures() {
    std::set<U32> set;
    for (const auto& row : SEGMENT_MAP) set.insert(u(row.first));
    for (const char* s : {"tʃ", "dʒ", "ts", "dz", "aː", "iː", "uː", "eː", "oː"}) set.insert(u(s));
    std::vector<U32> out(set.begin(), set.end());
    std::stable_sort(out.begin(), out.end(), [](const U32& a, const U32& b) { return a.size() > b.size(); });
    return out;
}
const std::vector<U32> LIGATURES = ligatures();

// G2P(lang).spec, kept per lang: the spec lives in orthography2ipa's cache.
const o2i::LanguageSpec& spec_of(const std::string& lang) {
    thread_local std::map<std::string, const o2i::LanguageSpec*> specs;
    auto it = specs.find(lang);
    if (it == specs.end()) it = specs.emplace(lang, &o2i::G2P(lang).spec()).first;
    return *it->second;
}

bool ends_with_long(const std::string& s) {
    return s.size() >= LONG.size() && s.compare(s.size() - LONG.size(), LONG.size(), LONG) == 0;
}

// spec.primary_parent: the parent field, else the first PARENT ancestor.
std::optional<std::string> primary_parent(const o2i::LanguageSpec& spec) {
    if (!spec.parent.empty() && spec.parent != "null") return spec.parent;
    static const std::string role = "parent";
    const auto parents = spec.get_ancestors(&role);
    if (parents.empty()) return std::nullopt;
    return parents.front().code;
}

}  // namespace

const std::vector<std::pair<std::string, Table>> ESTABLISHED_LOANS = {
    {"en-GB", {{"model", "muːdiːl"}, {"video", "fiːdjo"}, {"automatic", "ʔotomatik"}}}};

const Table& nativization_table(const std::string& lang) {
    // A table is keyed by the code it was cited for, resolved before the walk.
    static const std::map<std::string, const Table*> tables = [] {
        std::map<std::string, const Table*> out;
        for (const auto& [key, table] : TABLES) out[o2i::resolve(key)] = table;
        return out;
    }();
    std::optional<std::string> code = dialects::spec_for_lang(lang);
    std::set<std::string> seen;
    while (code && !code->empty() && !seen.count(*code)) {
        const auto it = tables.find(*code);
        if (it != tables.end()) return *it->second;
        seen.insert(*code);
        try {
            code = primary_parent(o2i::get(*code));
        } catch (const std::exception&) {
            break;
        }
    }
    return DEFAULT_MAP;
}

bool is_latin(const std::string& word) {
    return std::any_of(word.begin(), word.end(),
                       [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); });
}

std::vector<std::string> segment_ipa(const std::string& ipa) {
    const U32 s = u(ipa);
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        bool matched = false;
        for (const auto& lig : LIGATURES) {
            if (s.compare(i, lig.size(), lig) == 0) {
                out.push_back(s8(lig));
                i += lig.size();
                matched = true;
                break;
            }
        }
        if (matched) continue;
        std::size_t j = i + 1;
        while (j < s.size() && COMBINING.find(s[j]) != U32::npos) ++j;
        out.push_back(s8(s.substr(i, j - i)));
        i = j;
    }
    return out;
}

std::optional<std::string> guest_script(const std::string& word) {
    for (char32_t c : u(word)) {
        if (!arbtok::detail::is_alpha(c)) continue;
        switch (arbtok::detail::named_script(c)) {
            case arbtok::detail::NamedScript::Latin: return std::string("Latin");
            case arbtok::detail::NamedScript::Cyrillic: return std::string("Cyrillic");
            case arbtok::detail::NamedScript::Greek: return std::string("Greek");
            case arbtok::detail::NamedScript::Arabic: return std::nullopt;
            case arbtok::detail::NamedScript::Other: break;
        }
    }
    return std::nullopt;
}

namespace detail {

const std::vector<std::pair<std::string, std::string>>& table_names() {
    static const std::vector<std::pair<std::string, std::string>> names = [] {
        std::vector<std::pair<std::string, std::string>> out;
        for (const auto& [code, table] : TABLES) out.emplace_back(code, name_of(*table));
        return out;
    }();
    return names;
}

std::vector<std::string> borrowable_names() {
    std::vector<std::string> out;
    for (const Table* table : BORROWABLE) out.push_back(name_of(*table));
    return out;
}

const Table* table_by_name(const std::string& name) {
    if (name == "SEGMENT_MAP") return &SEGMENT_MAP;
    if (name == "_EGYPTIAN_MAP") return &EGYPTIAN_MAP;
    if (name == "_LEVANTINE_MAP") return &LEVANTINE_MAP;
    if (name == "_DEFAULT_MAP") return &DEFAULT_MAP;
    if (name == "_MAGHREBI_MAP") return &MAGHREBI_MAP;
    if (name == "_RAISED") return &RAISED;
    return nullptr;
}

std::string name_of(const Table& table) {
    for (const char* name : {"SEGMENT_MAP", "_EGYPTIAN_MAP", "_LEVANTINE_MAP", "_DEFAULT_MAP", "_MAGHREBI_MAP",
                             "_RAISED"})
        if (table_by_name(name) == &table) return name;
    return "";
}

std::optional<std::string> established(const std::string& word, const std::string& donor) {
    const Table* table = nullptr;
    for (const auto& [code, loans] : ESTABLISHED_LOANS)
        if (code == donor) table = &loans;
    if (table == nullptr && donor.find('-') != std::string::npos) {
        const std::string base = donor.substr(0, donor.find('-'));
        for (const auto& [code, loans] : ESTABLISHED_LOANS)
            if (code == base) table = &loans;
    }
    if (table == nullptr || table->empty()) return std::nullopt;
    // str.strip(): whitespace off both ends.
    U32 stripped = u(word);
    while (!stripped.empty() && arbtok::detail::is_space(stripped.back())) stripped.pop_back();
    std::size_t start = 0;
    while (start < stripped.size() && arbtok::detail::is_space(stripped[start])) ++start;
    const U32 decomposed = arbtok::detail::nfd(arbtok::detail::lower(stripped.substr(start)));
    U32 key;
    for (char32_t c : decomposed)
        if (arbtok::detail::combining(c) == 0) key += c;
    const std::string* hit = table_get(*table, s8(key));
    if (hit == nullptr) return std::nullopt;
    return *hit;
}

const std::vector<std::string>& targets(const std::string& lang) {
    thread_local std::map<std::string, std::vector<std::string>> cache;
    auto it = cache.find(lang);
    if (it != cache.end()) return it->second;
    std::vector<std::string> out;
    for (const auto& p : o2i::phoneme_inventory(spec_of(lang)))
        if (!p.empty() && segment_ipa(p).size() == 1) out.push_back(p);
    std::sort(out.begin(), out.end());
    return cache.emplace(lang, std::move(out)).first->second;
}

std::optional<std::string> project(const std::string& segment, const std::string& lang) {
    thread_local std::map<std::pair<std::string, std::string>, std::optional<std::string>> cache;
    const auto key = std::make_pair(segment, lang);
    const auto cached = cache.find(key);
    if (cached != cache.end()) return cached->second;
    auto answer = [&]() -> std::optional<std::string> {
        const auto& candidates = targets(lang);
        if (candidates.empty()) return std::nullopt;
        auto in_targets = [&](const std::string& s) {
            return std::binary_search(candidates.begin(), candidates.end(), s);
        };
        if (in_targets(segment)) return segment;
        // The cited tables come before the feature metric.
        for (const Table* table : BORROWABLE) {
            const std::string* cited = table_get(*table, segment);
            if (cited != nullptr && in_targets(*cited)) return *cited;
        }
        std::vector<double> distances;
        distances.reserve(candidates.size());
        for (const auto& t : candidates) distances.push_back(o2i::segment_distance(segment, t));
        const double best = *std::min_element(distances.begin(), distances.end());
        std::vector<std::string> tied;
        for (std::size_t i = 0; i < candidates.size(); ++i)
            if (distances[i] == best) tied.push_back(candidates[i]);
        if (tied.size() == 1) return tied.front();
        // A tie: a mid vowel takes the high vowel of its backness and rounding.
        const bool is_long = ends_with_long(segment);
        const std::string base = is_long ? segment.substr(0, segment.size() - LONG.size()) : segment;
        const std::string length = is_long ? LONG : "";
        if (const std::string* raised = table_get(RAISED, base)) {
            for (const std::string& candidate : {*raised + length, *raised})
                if (in_targets(candidate)) return candidate;
        }
        // A short vowel follows what the tables do with its long counterpart.
        if (length.empty()) {
            for (const Table* table : BORROWABLE) {
                const std::string* cited = table_get(*table, segment + LONG);
                if (cited == nullptr) continue;
                const std::string short_form = ends_with_long(*cited) ? cited->substr(0, cited->size() - LONG.size())
                                                                      : *cited;
                if (in_targets(short_form)) return short_form;
            }
        }
        return tied.front();
    }();
    cache.emplace(key, answer);
    return answer;
}

std::vector<std::string> map_segments(const std::vector<std::string>& segments, const std::string& lang) {
    const Table& seg_map = nativization_table(lang);
    std::vector<std::string> out;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const std::string& seg = segments[i];
        if (seg == NG) {
            const std::string next = i + 1 < segments.size() ? segments[i + 1] : "";
            out.push_back(next == "k" ? "n" : "nɡ");
            continue;
        }
        if (const std::string* cited = table_get(seg_map, seg)) {
            // A cited value still lands inside the matrix inventory.
            const auto& candidates = targets(lang);
            std::string joined;
            for (const auto& t : segment_ipa(*cited)) {
                if (std::binary_search(candidates.begin(), candidates.end(), t)) {
                    joined += t;
                } else {
                    const auto projected = project(t, lang);
                    joined += projected ? *projected : t;
                }
            }
            out.push_back(joined);
            continue;
        }
        const auto projected = project(seg, lang);
        out.push_back(projected ? *projected : seg);
    }
    return out;
}

}  // namespace detail

std::string nativize(const std::string& donor_ipa, const std::string& lang, const std::string& /*donor*/) {
    std::string stripped;
    for (char32_t c : u(donor_ipa))
        if (c != U'ˈ' && c != U'ˌ') stripped += s8(c);
    std::string out;
    for (const auto& segment : detail::map_segments(segment_ipa(stripped), lang)) out += segment;
    return out;
}

std::optional<std::string> transliterate(const std::string& word, const std::string& lang,
                                         const std::optional<std::string>& donor_arg, bool strict) {
    std::string donor;
    if (donor_arg) {
        donor = *donor_arg;
    } else {
        donor = DONOR_LANG;
        const auto script = guest_script(word);
        for (const auto& [name, code] : DONOR_BY_SCRIPT)
            if (script && name == *script) donor = code;
    }
    // An established loan is the Arabic word already, returned as it stands.
    if (auto settled = detail::established(word, donor)) return settled;

    donor_lexicon::ensure_registered(donor);
    std::string donor_ipa;
    try {
        donor_ipa = o2i::G2P(donor).transcribe_word(word);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (donor_ipa.empty()) return std::nullopt;
    std::string adapted = nativize(donor_ipa, lang, donor);

    const auto& spec = spec_of(lang);
    const auto declared = o2i::phoneme_inventory(spec);
    std::vector<std::string> outside;
    for (const auto& t : o2i::tokenize(adapted, spec))
        if (!declared.count(t)) outside.push_back(t);
    if (outside.empty()) return adapted;
    if (strict) return std::nullopt;
    // Each residual symbol is projected onto the nearest declared phoneme.
    // Python walks set(outside), whose order follows the string hash; the
    // port walks first occurrence order.
    std::set<std::string> done;
    for (const auto& t : outside) {
        if (!done.insert(t).second) continue;
        if (const auto repl = detail::project(t, lang))
            adapted = s8(arbtok::detail::replace(u(adapted), u(t), u(*repl)));
    }
    return adapted;
}

}  // namespace arbtok::translit
