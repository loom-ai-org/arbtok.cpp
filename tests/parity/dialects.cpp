#include "arbtok/dialects.hpp"

#include <orthography2ipa/orthography2ipa.hpp>

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace dl = arbtok::dialects;
namespace o2i = orthography2ipa;

namespace {

std::string pairs(const dl::OrderedMap& map) {
    std::vector<std::string> items;
    for (const auto& [k, v] : map) items.push_back(list({J(k), J(v)}));
    return list(items);
}

template <class Map>
std::string sorted_pairs(const Map& map) {
    std::vector<std::string> items;
    for (const auto& [k, v] : map) items.push_back(list({J(k), J(v)}));
    return list(items);
}

std::string constant(const std::string& name) {
    if (name == "DEFAULT_LANG") return J(dl::DEFAULT_LANG);
    if (name == "THEH") return J(dl::THEH);
    if (name == "THAL") return J(dl::THAL);
    if (name == "ZAH") return J(dl::ZAH);
    if (name == "ARABIC_TO_IPA_CONSONANTS") return pairs(dl::ARABIC_TO_IPA_CONSONANTS);
    if (name == "DIACRITIC_TO_IPA") return pairs(dl::DIACRITIC_TO_IPA);
    if (name == "TANWIN_TO_IPA") return pairs(dl::TANWIN_TO_IPA);
    if (name == "VOWEL_MAP") return pairs(dl::VOWEL_MAP);
    if (name == "WORD_EXCEPTIONS") return pairs(dl::WORD_EXCEPTIONS);
    if (name == "_REGION_DEFAULTS") return J(dl::detail::REGION_DEFAULTS);
    if (name == "_DIALECT_NAME_ALIASES") return pairs(dl::detail::DIALECT_NAME_ALIASES);
    if (name == "_SPEC_LANGUAGE_CODES") return pairs(dl::detail::SPEC_LANGUAGE_CODES);
    if (name == "_VOWEL_CHARS") return J(dl::detail::VOWEL_CHARS);
    if (name == "Lect._fields") return J(std::vector<std::string>{"code", "tier"});
    throw std::runtime_error("unknown dialects constant " + name);
}

// [value, [the messages of the UserWarnings raised]]
std::string with_warnings(const std::function<std::string()>& fn) {
    std::vector<std::string> messages;
    arbtok::set_warning_handler([&](const std::string& category, const std::string& message) {
        if (category == "UserWarning") messages.push_back(message);
    });
    std::string value;
    try {
        value = guard(fn);
    } catch (...) {
        arbtok::set_warning_handler(nullptr);
        throw;
    }
    arbtok::set_warning_handler(nullptr);
    return list({value, J(messages)});
}

}  // namespace

std::string record_dialects(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + constant(v[1].str) + "]";
    if (fn == "spec_for_lang")
        return head(v) + with_warnings([&] { return J(dl::spec_for_lang(opt_str(v[1]))); }) + "]";
    if (fn == "lect_code") return head(v) + guard([&] { return J(dl::lect_code(v[1].str)); }) + "]";
    if (fn == "resolve") return head(v) + guard([&] { return J(o2i::resolve(v[1].str)); }) + "]";
    if (fn == "resolves_exactly") return head(v) + guard([&] { return J(dl::detail::resolves_exactly(v[1].str)); }) + "]";
    if (fn == "closest_match")
        return head(v) + guard([&] {
                   const auto [best, distance] = dl::detail::closest_match(v[1].str, v[2].strings());
                   return list({J(best), J(distance)});
               }) +
               "]";
    if (fn == "supported_lects") {
        std::vector<std::string> items;
        for (const auto& lect : dl::supported_lects()) items.push_back(list({J(lect.code), J(lect.tier)}));
        return head(v) + list(items) + "]";
    }
    if (fn == "_arabic_codes") return head(v) + J(dl::detail::arabic_codes()) + "]";
    if (fn == "_dialect_index") return head(v) + sorted_pairs(dl::detail::dialect_index(dl::detail::arabic_codes())) + "]";
    if (fn == "_is_arabic_code") return head(v) + J(dl::detail::is_arabic_code(v[1].str)) + "]";
    if (fn == "_core_subtags") return head(v) + guard([&] { return J(dl::detail::core_subtags(v[1].str)); }) + "]";
    if (fn == "consonant_map") return head(v) + sorted_pairs(dl::consonant_map(v[1].str)) + "]";
    if (fn == "consonant_ipa") return head(v) + J(dl::consonant_ipa(v[1].str, v[2].str, v[3].str)) + "]";
    if (fn == "spec") {
        const o2i::LanguageSpec& spec = o2i::get(v[1].str);
        const auto none_if_empty = [](const std::string& s) { return s.empty() ? std::string("null") : J(s); };
        return head(v) + list({none_if_empty(spec.parent), (spec.iso639_3 == "null" ? std::string("null") : none_if_empty(spec.iso639_3)), J(spec.quality)}) + "]";
    }
    throw std::runtime_error("unknown dialects record " + fn);
}

}  // namespace parity
