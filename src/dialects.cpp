#include "arbtok/dialects.hpp"

#include <orthography2ipa/langcodes.hpp>
#include <orthography2ipa/orthography2ipa.hpp>

#include <algorithm>
#include <set>
#include <tuple>

#include "arbtok/textnorm.hpp"
#include "detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::dialects {

namespace o2i = orthography2ipa;
namespace u = arbtok::detail;

const std::string THEH = "ث", THAL = "ذ", ZAH = "ظ";
const std::string DEFAULT_LANG = "ar";

const OrderedMap ARABIC_TO_IPA_CONSONANTS = {
    {"\u0621", "\u0294"}, {"\u0623", "\u0294"}, {"\u0625", "\u0294"}, {"\u0624", "\u0294"}, {"\u0626", "\u0294"}, {"\u0628", "b"}, {"\u062A", "t"}, {"\u062B", "\u03B8"}, {"\u062C", "d\u0292"}, {"\u062D", "\u0127"}, {"\u062E", "x"}, {"\u062F", "d"}, {"\u0630", "\u00F0"}, {"\u0631", "r"}, {"\u0632", "z"}, {"\u0633", "s"}, {"\u0634", "\u0283"}, {"\u0635", "s\u02E4"}, {"\u0636", "d\u02E4"}, {"\u0637", "t\u02E4"}, {"\u0638", "\u00F0\u02E4"}, {"\u0639", "\u0295"}, {"\u063A", "\u0263"}, {"\u0641", "f"}, {"\u0642", "q"}, {"\u0643", "k"}, {"\u0645", "m"}, {"\u0646", "n"}, {"\u0647", "h"}, {"\u0644", "l"}, {"\u0648", "w"}, {"\u064A", "j"}, {"\u0686", "t\u0283"}, {"\u06AF", "\u0261"}, {"\u067E", "p"}, {"\u06A4", "v"}, {"\u0698", "\u0292"}, {"\uFEFB", "la\u02D0"},
};
const OrderedMap DIACRITIC_TO_IPA = {
    {"\u064E", "a"}, {"\u064F", "u"}, {"\u0650", "i"}, {"\u0670", "a\u02D0"}, {"\u0653", "a\u02D0"},
};
const OrderedMap TANWIN_TO_IPA = {
    {"\u064B", "an"}, {"\u064C", "un"}, {"\u064D", "in"},
};
const OrderedMap VOWEL_MAP = {
    {"\u064E", "a"}, {"\u064F", "u"}, {"\u0650", "i"}, {"\u0670", "a\u02D0"}, {"\u0653", ":"}, {"\u064B", "an"}, {"\u064C", "un"}, {"\u064D", "in"}, {"\u0649", "a\u02D0"},
};
const OrderedMap WORD_EXCEPTIONS = {
    {"\u0623\u064F\u0648\u0644\u064E\u0670\u0626\u0650\u0643\u064E", "\u0294ula\u02D0\u0294ika"}, {"\u0627\u0644\u0644\u0651\u0670\u0647", "alla\u02D0h"}, {"\u0639\u064E\u0645\u0652\u0631\u064C\u0648", "\u0295amrun"}, {"\u0623\u064F\u0648\u0644\u0650\u064A", "\u0294uli\u02D0"}, {"\u0623\u064F\u0648\u0644\u064F\u0648", "\u0294ulu\u02D0"},
};

namespace detail {

const std::vector<std::string> REGION_DEFAULTS = {"ar-SA-x-najd"};
const OrderedMap DIALECT_NAME_ALIASES = {
    {"najd", "ar-SA-x-najd"}, {"najdi", "ar-SA-x-najd"}, {"hejaz", "ar-SA-x-hejaz"}, {"hejazi", "ar-SA-x-hejaz"}, {"hijazi", "ar-SA-x-hejaz"}, {"hijaz", "ar-SA-x-hejaz"}, {"qassimi", "ar-SA-x-qassim"}, {"sharqiyya", "ar-SA-x-sharqiyya"}, {"gulf", "ar-x-gulf"}, {"gulfi", "ar-x-gulf"}, {"khaleeji", "ar-x-gulf"}, {"levantine", "ar-x-levantine"}, {"levant", "ar-x-levantine"}, {"shami", "ar-x-levantine"}, {"maghrebi", "ar-x-maghrebi"}, {"maghribi", "ar-x-maghrebi"}, {"saidi", "ar-EG-x-saidi"}, {"bedawi", "ar-EG-x-bedawi"}, {"dhofari", "ar-OM-x-dhofari"}, {"baharna", "ar-BH-x-baharna"},
};
const OrderedMap SPEC_LANGUAGE_CODES = {
    {"ar-EG", "arz"}, {"ar-SA-x-hejaz", "acw"}, {"ar-SA-x-najd", "ars"}, {"ar-x-gulf", "afb"},
};
const std::vector<std::string> VOWEL_CHARS = {"a", "e", "i", "o", "u", "æ", "ɑ", "ə", "ː"};

}  // namespace detail

const std::string* find(const OrderedMap& map, const std::string& key) {
    for (const auto& [k, v] : map)
        if (k == key) return &v;
    return nullptr;
}

namespace {

std::string py_lower(const std::string& s) { return u::s8(u::lower(u::u(s))); }

// The spec's iso639_3, or nullopt for None. orthography2ipa's C++ loader keeps
// a JSON null as the text "null", and an absent key as "".
std::optional<std::string> declared_iso639_3(const o2i::LanguageSpec& spec) {
    if (spec.iso639_3.empty() || spec.iso639_3 == "null") return std::nullopt;
    return spec.iso639_3;
}

bool contains(const std::vector<std::string>& codes, const std::string& code) {
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

std::vector<std::string> split_dash(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t at = s.find('-', start);
        out.push_back(s.substr(start, at - start));
        if (at == std::string::npos) return out;
        start = at + 1;
    }
}

std::string join_dash(const std::vector<std::string>& parts) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i ? "-" : "") + parts[i];
    return out;
}

size_t length(const std::string& s) { return u::u(s).size(); }

const std::map<std::string, std::string>& cached_dialect_index() {
    static const std::map<std::string, std::string> index = detail::dialect_index(detail::arabic_codes());
    return index;
}

// _exact_spec
std::optional<std::string> exact_spec(const std::string& tag, const std::vector<std::string>& codes) {
    const std::string folded = py_lower(tag);
    for (const auto& code : codes)
        if (py_lower(code) == folded) return code;
    if (!detail::resolves_exactly(tag)) return std::nullopt;
    const std::string spec = o2i::resolve(tag);
    if (contains(codes, spec)) return spec;
    if (textnorm::is_arabic_lang(tag))
        u::warn("UserWarning", u::py_str_repr(u::u(tag)) + " names the orthography2ipa spec " +
                                   u::py_str_repr(u::u(spec)) + ", which arbtok has no lect for; reading it as " +
                                   u::py_str_repr(u::u(DEFAULT_LANG)) + " (Modern Standard Arabic)");
    return DEFAULT_LANG;
}

}  // namespace

namespace detail {

bool resolves_exactly(const std::string& code) { return o2i::resolves_exactly(code); }

std::pair<std::string, int> closest_match(const std::string& desired_in, const std::vector<std::string>& supported) {
    if (contains(supported, desired_in)) return {desired_in, 0};
    const auto standardized = o2i::langcodes::try_standardize_tag(desired_in);
    if (!standardized) throw Error("LanguageTagError", desired_in);
    const std::string& desired = *standardized;
    if (contains(supported, desired)) return {desired, 0};
    std::pair<std::string, int> best{"und", 1000};
    for (const auto& tag : supported) {
        const auto distance = o2i::langcodes::tag_distance(desired, tag);
        if (!distance) throw Error("LanguageTagError", tag);
        // A stable sort by distance keeps the first of a tie.
        if (*distance <= 25 && *distance < best.second) best = {tag, *distance};
    }
    return best;
}

bool is_arabic_code(const std::string& code) {
    static std::map<std::string, bool> cache;
    if (auto it = cache.find(code); it != cache.end()) return it->second;
    bool answer;
    if (o2i::resolve(code) != code) {
        answer = false;
    } else {
        std::optional<std::string> language;
        std::set<std::string> seen;
        std::string node = code;
        while (!node.empty() && !seen.count(node) && !language) {
            seen.insert(node);
            const o2i::LanguageSpec& spec = o2i::get(node);
            language = declared_iso639_3(spec);
            node = spec.parent;
        }
        answer = language ? (*language == "ara" || textnorm::is_arabic_lang(*language)) : textnorm::is_arabic_lang(code);
    }
    cache.emplace(code, answer);
    return answer;
}

const std::vector<std::string>& arabic_codes() {
    static const std::vector<std::string> codes = [] {
        std::vector<std::string> out;
        for (const auto& c : o2i::available_codes()) {
            if (is_arabic_code(c)) out.push_back(c);
        }
        std::set<std::string> defaults;
        for (const auto& c : REGION_DEFAULTS) defaults.insert(o2i::resolve(c));
        std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
            return std::make_tuple(a != DEFAULT_LANG, !defaults.count(a), a) <
                   std::make_tuple(b != DEFAULT_LANG, !defaults.count(b), b);
        });
        return out;
    }();
    return codes;
}

std::map<std::string, std::string> dialect_index(const std::vector<std::string>& codes) {
    std::map<std::string, std::string> index;
    for (const auto& code : codes) {
        // code.split("-x-")[-1]: the pieces are cut left to right.
        size_t last = std::string::npos;
        for (size_t at = code.find("-x-"); at != std::string::npos; at = code.find("-x-", at + 3)) last = at + 3;
        if (last != std::string::npos) index.emplace(py_lower(code.substr(last)), code);
    }
    for (const auto& [alias, tag] : DIALECT_NAME_ALIASES) {
        const std::string target = o2i::resolve(tag);
        if (contains(codes, target)) index[alias] = target;
    }
    return index;
}

std::vector<std::string> core_subtags(const std::string& tag) {
    const std::vector<std::string> subtags = split_dash(tag);
    std::vector<std::string> core(subtags.begin(), subtags.begin() + 1);
    size_t r = 1;
    const auto alpha = [](const std::string& s) { return u::all_alpha(u::u(s)); };
    const auto digit = [](const std::string& s) { return u::all_digit(u::u(s)); };
    while (r < subtags.size() && core.size() < 4 && length(subtags[r]) == 3 && alpha(subtags[r]))
        core.push_back(subtags[r++]);
    if (r < subtags.size() && length(subtags[r]) == 4 && alpha(subtags[r])) core.push_back(subtags[r++]);
    if (r < subtags.size() && ((length(subtags[r]) == 2 && alpha(subtags[r])) ||
                               (length(subtags[r]) == 3 && digit(subtags[r]))))
        core.push_back(subtags[r++]);
    return core;
}

}  // namespace detail

const std::map<std::string, std::string>& consonant_map(const std::string& lang) {
    static std::map<std::string, std::map<std::string, std::string>> cache;
    if (auto it = cache.find(lang); it != cache.end()) return it->second;
    std::map<std::string, std::string> out;
    for (const auto& [grapheme, readings] : o2i::get(lang).graphemes) {
        if (!find(ARABIC_TO_IPA_CONSONANTS, grapheme) || readings.empty()) continue;
        const std::string& first = readings.front();
        bool has_vowel = false;
        for (const auto& v : detail::VOWEL_CHARS)
            if (first.find(v) != std::string::npos) has_vowel = true;
        if (!has_vowel) out.emplace(grapheme, first);
    }
    return cache.emplace(lang, std::move(out)).first->second;
}

std::string consonant_ipa(const std::string& grapheme, const std::string& lang, const std::string& default_ipa) {
    const auto& map = consonant_map(lang);
    auto it = map.find(grapheme);
    return it == map.end() ? default_ipa : it->second;
}

std::string spec_for_lang(const std::optional<std::string>& lang) {
    if (!lang || lang->empty()) return DEFAULT_LANG;
    const auto& codes = detail::arabic_codes();
    std::string normalized = u::s8(u::replace(u::u(*lang), U"_", U"-"));

    if (auto exact = exact_spec(normalized, codes)) return *exact;

    const auto& dialects = cached_dialect_index();
    for (const auto& token : split_dash(py_lower(normalized))) {
        if (token == "ar" || token == "x") continue;
        if (auto it = dialects.find(token); it != dialects.end()) return it->second;
    }

    const std::vector<std::string> core = detail::core_subtags(normalized);
    std::vector<std::string> subtags = split_dash(normalized);
    while (subtags.size() > core.size()) {
        subtags.pop_back();
        if (subtags.size() > core.size() && length(subtags.back()) == 1) subtags.pop_back();
        if (auto exact = exact_spec(join_dash(subtags), codes)) return *exact;
    }
    normalized = join_dash(core);

    const std::string primary = split_dash(py_lower(normalized)).front();
    for (const auto& [spec, code] : detail::SPEC_LANGUAGE_CODES)
        if (code == primary) return o2i::resolve(spec);

    const std::string nearest = o2i::resolve(normalized);
    if (contains(codes, nearest) && (nearest != DEFAULT_LANG || py_lower(split_dash(normalized).front()) == "ar"))
        return nearest;

    std::vector<std::string> matchable;
    for (const auto& c : codes) {
        const auto parts = split_dash(c);
        if (std::all_of(parts.begin(), parts.end(), [](const std::string& p) { return length(p) <= 8; }))
            matchable.push_back(c);
    }
    std::string best;
    try {
        best = detail::closest_match(normalized, matchable).first;
    } catch (const Error&) {
        best = "und";
    }
    if (contains(codes, best)) return best;
    return DEFAULT_LANG;
}

std::optional<std::string> lect_code(const std::string& lang) {
    static std::map<std::string, std::optional<std::string>> cache;
    if (auto it = cache.find(lang); it != cache.end()) return it->second;
    std::map<std::string, std::string> languages;
    for (const auto& [spec, code] : detail::SPEC_LANGUAGE_CODES) languages[o2i::resolve(spec)] = code;
    std::optional<std::string> answer;
    std::string code = spec_for_lang(lang);
    std::set<std::string> seen;
    while (!code.empty() && !seen.count(code)) {
        if (auto it = languages.find(code); it != languages.end()) {
            answer = it->second;
            break;
        }
        seen.insert(code);
        code = o2i::get(code).parent;
    }
    cache.emplace(lang, answer);
    return answer;
}

std::vector<Lect> supported_lects() {
    std::vector<Lect> out;
    for (const auto& code : o2i::available_codes())
        if (detail::is_arabic_code(code)) out.push_back({code, o2i::get(code).quality});
    std::stable_sort(out.begin(), out.end(), [](const Lect& a, const Lect& b) { return a.code < b.code; });
    return out;
}

}  // namespace arbtok::dialects
