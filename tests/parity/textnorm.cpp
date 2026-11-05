#include "arbtok/textnorm.hpp"

#include <algorithm>

#include "common.hpp"
#include "modules.hpp"

namespace parity {

namespace t = arbtok::textnorm;

namespace {

// ── Lexicon references ──────────────────────────────────────────────────────

const std::vector<std::string>& term_names() {
    static const std::vector<std::string> names = {"cars-sa",   "common-en",        "currencies-cldr", "languages-cldr",
                                                   "spoken-sa", "territories-cldr", "units-cldr"};
    return names;
}

t::Lexicon sorted_lexicon(const t::Pairs& pairs) { return t::AsrNorm().with_lexicon(pairs).lexicon; }

const std::vector<std::pair<std::string, t::Lexicon>>& lexicon_references() {
    static const std::vector<std::pair<std::string, t::Lexicon>> refs = [] {
        std::vector<std::pair<std::string, t::Lexicon>> out;
        out.emplace_back("@canonical", sorted_lexicon(t::canonical_term_lexicon()));
        for (const auto& name : term_names()) {
            if (name.size() < 5 || name.compare(name.size() - 5, 5, "-cldr") != 0)
                out.emplace_back("@asr:" + name, sorted_lexicon(t::bundled_asr_lexicon(name)));
            out.emplace_back("@tts:" + name + ":1", sorted_lexicon(t::bundled_tts_lexicon(name, true)));
            out.emplace_back("@tts:" + name + ":0", sorted_lexicon(t::bundled_tts_lexicon(name, false)));
        }
        return out;
    }();
    return refs;
}

std::string encode_lexicon(const t::Lexicon& lexicon) {
    for (const auto& [ref, entries] : lexicon_references())
        if (entries == lexicon) return J(ref);
    std::vector<std::string> parts;
    for (const auto& [a, b] : lexicon) parts.push_back("[" + J(a) + "," + J(b) + "]");
    return list(parts);
}

t::Lexicon decode_lexicon(const Value& v) {
    if (v.kind == Value::String) {
        for (const auto& [ref, entries] : lexicon_references())
            if (ref == v.str) return entries;
        throw std::runtime_error("unknown lexicon reference " + v.str);
    }
    t::Lexicon out;
    for (const auto& e : v.items) out.emplace_back(e[0].str, e[1].str);
    return out;
}

// ── Configs, as the dumper writes them ──────────────────────────────────────

struct Member {
    std::string text;
    std::string name;
};

std::string object(const std::vector<Member>& members) {
    std::string out = "{";
    for (size_t i = 0; i < members.size(); ++i) out += (i ? "," : "") + J(members[i].name) + ":" + members[i].text;
    return out + "}";
}

std::string encode(const t::AsrNorm& c) {
    std::vector<Member> m;
    for (const auto& f : t::AsrNorm::fields())
        if (c.*f.member) m.push_back({"true", f.name});
    if (!c.lexicon.empty()) m.push_back({encode_lexicon(c.lexicon), "lexicon"});
    return object(m);
}

t::AsrNorm decode_asr(const Value& v) {
    t::AsrNorm c;
    for (const auto& member : v.members) {
        const std::string& name = member.first;
        const Value& value = member.second;
        if (name == "lexicon") {
            c.lexicon = decode_lexicon(value);
            continue;
        }
        const auto& fields = t::AsrNorm::fields();
        const auto it = std::find_if(fields.begin(), fields.end(), [&](const auto& f) { return name == f.name; });
        if (it == fields.end()) throw std::runtime_error("unknown AsrNorm field " + name);
        c.*it->member = value.boolean;
    }
    return c;
}

std::string encode(const t::TtsNorm& c) {
    const t::TtsNorm d;
    std::vector<Member> m;
    auto flag = [&](const char* name, bool t::TtsNorm::*member) {
        if (c.*member != d.*member) m.push_back({J(c.*member), name});
    };
    auto strings = [&](const char* name, const std::vector<std::string>& v) {
        if (!v.empty()) m.push_back({J(v), name});
    };
    flag("strip_controls", &t::TtsNorm::strip_controls);
    flag("spell_out_plates", &t::TtsNorm::spell_out_plates);
    flag("spell_out_codes", &t::TtsNorm::spell_out_codes);
    flag("speak_percent", &t::TtsNorm::speak_percent);
    flag("keep_code_digits", &t::TtsNorm::keep_code_digits);
    strings("phone_shapes", c.phone_shapes);
    strings("phone_regions", c.phone_regions);
    flag("long_digit_runs", &t::TtsNorm::long_digit_runs);
    strings("phone_prefixes", c.phone_prefixes);
    strings("identifier_words", c.identifier_words);
    flag("cardinal_numbers", &t::TtsNorm::cardinal_numbers);
    flag("colloquial_clock", &t::TtsNorm::colloquial_clock);
    flag("colloquial_years", &t::TtsNorm::colloquial_years);
    flag("colloquial_units", &t::TtsNorm::colloquial_units);
    flag("leave_unspeakable_numbers", &t::TtsNorm::leave_unspeakable_numbers);
    flag("oblique_numbers", &t::TtsNorm::oblique_numbers);
    flag("space_fused_hundreds", &t::TtsNorm::space_fused_hundreds);
    flag("dialect_numbers", &t::TtsNorm::dialect_numbers);
    if (!c.number_forms.empty()) {
        std::vector<std::string> parts;
        for (const auto& [value, word] : c.number_forms) parts.push_back("[" + J(value) + "," + J(word) + "]");
        m.push_back({list(parts), "number_forms"});
    }
    strings("month_names", c.month_names);
    flag("spoken_forms", &t::TtsNorm::spoken_forms);
    flag("canonical_unicode", &t::TtsNorm::canonical_unicode);
    flag("drop_false_starts", &t::TtsNorm::drop_false_starts);
    flag("plain_orthography", &t::TtsNorm::plain_orthography);
    if (!c.lexicon.empty()) m.push_back({encode_lexicon(c.lexicon), "lexicon"});
    return object(m);
}

t::TtsNorm decode_tts(const Value& v) {
    t::TtsNorm c;
    for (const auto& member : v.members) {
        const std::string& name = member.first;
        const Value& value = member.second;
        if (name == "lexicon") {
            c.lexicon = decode_lexicon(value);
        } else if (name == "number_forms") {
            for (const auto& e : value.items) c.number_forms.emplace_back(e[0].integer, e[1].str);
        } else if (name == "phone_shapes") {
            c.phone_shapes = value.strings();
        } else if (name == "phone_regions") {
            c.phone_regions = value.strings();
        } else if (name == "phone_prefixes") {
            c.phone_prefixes = value.strings();
        } else if (name == "identifier_words") {
            c.identifier_words = value.strings();
        } else if (name == "month_names") {
            c.month_names = value.strings();
        } else {
            const auto& fields = t::TtsNorm::fields();
            const auto it = std::find_if(fields.begin(), fields.end(), [&](const auto& f) { return name == f.name; });
            if (it == fields.end()) throw std::runtime_error("unknown TtsNorm field " + name);
            c.*it->member = value.boolean;
        }
    }
    return c;
}

std::string pairs(const t::Pairs& p) {
    std::vector<std::string> parts;
    for (const auto& [a, b] : p) parts.push_back("[" + J(a) + "," + J(b) + "]");
    return list(parts);
}

// Lexicon entries as the dumper passes them. An entry that is not two strings
// cannot be built in C++; Python raises ValueError for it.
std::optional<t::Pairs> lexicon_entries(const Value& v) {
    t::Pairs out;
    for (const auto& e : v.items) {
        if (e.size() != 2 || e[0].kind != Value::String || e[1].kind != Value::String) return std::nullopt;
        out.emplace_back(e[0].str, e[1].str);
    }
    return out;
}

// Number forms as the dumper passes them. A key that is not an int, a bool
// key or a word that is not a string cannot be built in C++; Python raises
// TypeError for each.
std::optional<t::NumberForms> number_form_entries(const Value& v) {
    t::NumberForms out;
    for (const auto& e : v.items) {
        if (e[0].kind != Value::Int || e[1].kind != Value::String) return std::nullopt;
        out.emplace_back(e[0].integer, e[1].str);
    }
    return out;
}

const std::string kValueError = "{\"err\":\"ValueError\"}";
const std::string kTypeError = "{\"err\":\"TypeError\"}";

std::string constant(const std::string& name) {
    if (name == "ARABIC_LANGUAGE_CODES") return J(t::ARABIC_LANGUAGE_CODES);
    if (name == "ASR_NORM_VERSION") return J(t::ASR_NORM_VERSION);
    if (name == "TTS_NORM_VERSION") return J(t::TTS_NORM_VERSION);
    if (name == "IDENTIFIER_WORDS") return J(t::IDENTIFIER_WORDS);
    if (name == "ARAB_PHONE_REGIONS") return J(t::ARAB_PHONE_REGIONS);
    if (name == "TRUTH_CHECK") return encode(t::TRUTH_CHECK);
    if (name == "CER_STRIP") return encode(t::CER_STRIP);
    if (name == "CER_NORM") return encode(t::CER_NORM);
    if (name == "CER_MARKS_FIRST") return encode(t::CER_MARKS_FIRST);
    if (name == "CER_NORM_MARKS_FIRST") return encode(t::CER_NORM_MARKS_FIRST);
    if (name == "INTELLIGIBILITY_GATE") return encode(t::INTELLIGIBILITY_GATE);
    if (name == "CER_CANONICAL") return encode(t::CER_CANONICAL());
    throw std::runtime_error("unknown textnorm constant " + name);
}

}  // namespace

std::string record_textnorm(const Value& v) {
    const std::string& fn = v[0].str;
    if (fn == "const") return head(v) + constant(v[1].str) + "]";
    if (fn == "is_arabic_lang") return head(v) + guard([&] { return J(t::is_arabic_lang(v[1].str)); }) + "]";
    if (fn == "spelled_codes") return head(v) + guard([&] { return pairs(t::spelled_codes(v[1].str)); }) + "]";
    if (fn == "cldr_units") return head(v) + guard([&] { return pairs(t::cldr_units(v[1].str)); }) + "]";
    if (fn == "bundled_asr_lexicon")
        return head(v) + guard([&] { return pairs(t::bundled_asr_lexicon(v[1].str)); }) + "]";
    if (fn == "bundled_tts_lexicon")
        return head(v) + guard([&] { return pairs(t::bundled_tts_lexicon(v[1].str, v[2].boolean)); }) + "]";
    if (fn == "canonical_term_lexicon")
        return head(v) + guard([&] { return pairs(t::canonical_term_lexicon(v[1].strings())); }) + "]";
    if (fn == "AsrNorm.describe") return head(v) + J(decode_asr(v[1]).describe()) + "]";
    if (fn == "TtsNorm.describe") return head(v) + J(decode_tts(v[1]).describe()) + "]";
    if (fn == "AsrNorm.with_lexicon" || fn == "TtsNorm.with_lexicon") {
        const auto entries = lexicon_entries(v[1]);
        if (!entries) return head(v) + kValueError + "]";
        if (fn == "AsrNorm.with_lexicon") return head(v) + guard([&] { return encode(t::AsrNorm().with_lexicon(*entries)); }) + "]";
        return head(v) + guard([&] { return encode(t::TtsNorm().with_lexicon(*entries)); }) + "]";
    }
    if (fn == "TtsNorm.with_number_forms") {
        const auto forms = number_form_entries(v[1]);
        if (!forms) return head(v) + kTypeError + "]";
        return head(v) + guard([&] { return encode(t::TtsNorm().with_number_forms(*forms)); }) + "]";
    }
    if (fn == "TtsNorm") {
        return head(v) + guard([&] {
                   const t::TtsNorm c = decode_tts(v[1]);
                   c.check();
                   return encode(c);
               }) +
               "]";
    }
    if (fn == "normalize_asr")
        return head(v) + guard([&] { return J(t::normalize_asr(v[1].str, decode_asr(v[2]), v[3].str)); }) + "]";
    if (fn == "normalize_for_tts")
        return head(v) + guard([&] { return J(t::normalize_for_tts(v[1].str, v[2].str, decode_tts(v[3]))); }) + "]";
    throw std::runtime_error("unknown textnorm record " + fn);
}

}  // namespace parity
