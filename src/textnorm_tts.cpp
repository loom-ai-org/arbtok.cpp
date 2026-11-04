// normalize_for_tts and the TtsNorm config: the speech-synthesis half of
// arbtok.textnorm. Every pattern and table is the one textnorm.py holds, from
// textnorm_tables.inc; the rules run in the order the Python runs them.
#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "arbtok/dialects.hpp"
#include "arbtok/textnorm.hpp"
#include "arbtok/tokenizer.hpp"
#include "arbtok/util.hpp"
#include "json.hpp"
#include "textnorm_detail.hpp"

namespace arbtok::textnorm {

using namespace detail;
namespace onp = numbers::ovos_number_parser;
namespace odp = numbers::ovos_date_parser;

const std::vector<std::string> ARAB_PHONE_REGIONS = [] {
    std::vector<std::string> out;
    for (const auto& r : py::ARAB_PHONE_REGIONS) out.push_back(s8(r));
    return out;
}();

const std::string TTS_NORM_VERSION = s8(py::TTS_NORM_VERSION);

namespace {

constexpr const char* kNumberParser = "; ovos-number-parser 0.24.3a1";

U32 S(U32View v) { return U32(v); }

std::string str_tuple_repr(const std::vector<std::string>& items) {
    std::string out = "(";
    for (size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + repr(u(items[i]));
    if (items.size() == 1) out += ",";
    return out + ")";
}

std::string forms_repr(const NumberForms& forms) {
    std::string out = "(";
    for (size_t i = 0; i < forms.size(); ++i)
        out += (i ? ", (" : "(") + std::to_string(forms[i].first) + ", " + repr(u(forms[i].second)) + ")";
    if (forms.size() == 1) out += ",";
    return out + ")";
}

std::string describe_lexicon(const Lexicon& entries) {
    if (entries.empty()) return "";
    std::string body;
    for (size_t i = 0; i < entries.size(); ++i) body += (i ? "\n" : "") + entries[i].first + "\t" + entries[i].second;
    return "; lexicon " + std::to_string(entries.size()) + " entries sha256:" + sha256_hex(body, 16);
}

// ── Phone plans ─────────────────────────────────────────────────────────────

struct TypePlan {
    std::string kind;
    const Pattern* pattern;
    std::set<long long> lengths;
    std::vector<std::pair<U32, std::set<long long>>> leading, leading_three;
};

struct Plan {
    std::string region;
    U32 code;
    U32 trunk;
    std::vector<TypePlan> types;
    const TypePlan* type(const std::string& kind) const {
        for (const auto& t : types)
            if (t.kind == kind) return &t;
        return nullptr;
    }
};

const std::string& phone_plans_raw() {
    static const std::string raw = arbtok::detail::read_file(arbtok::detail::data_path("phone_plans.json"));
    return raw;
}

std::set<long long> int_set(const json::Value& array) {
    std::set<long long> out;
    for (const auto& v : array.items) out.insert(std::stoll(v.text));
    return out;
}

std::vector<std::pair<U32, std::set<long long>>> leads(const json::Value* object) {
    std::vector<std::pair<U32, std::set<long long>>> out;
    if (object == nullptr) return out;
    for (const auto& [lead, counts] : object->members) out.emplace_back(u(lead), int_set(counts));
    return out;
}

const std::vector<Plan>& phone_plans() {
    static const std::vector<Plan> plans = [] {
        const json::Value doc = json::loads(phone_plans_raw());
        std::vector<Plan> out;
        for (const auto& [region, plan] : doc.at("regions").members) {
            Plan p;
            p.region = region;
            p.code = u(plan.at("country_code").text);
            const json::Value& trunk = plan.at("national_prefix");
            if (trunk.kind == json::Value::String) p.trunk = u(trunk.text);
            for (const auto& [kind, t] : plan.at("types").members) {
                TypePlan tp;
                tp.kind = kind;
                tp.pattern = &pyre::compiled(t.at("national_number_pattern").text);
                tp.lengths = int_set(t.at("possible_lengths"));
                tp.leading = leads(&t.at("leading_digits"));
                tp.leading_three = leads(t.find("leading_three_digits"));
                p.types.push_back(std::move(tp));
            }
            out.push_back(std::move(p));
        }
        return out;
    }();
    return plans;
}

const Plan& plan_of(const std::string& region) {
    for (const auto& p : phone_plans())
        if (p.region == region) return p;
    throw KeyError(repr(u(region)));
}

std::string describe_phone_plans() {
    const json::Value doc = json::loads(phone_plans_raw());
    return "; phone plans libphonenumber " + doc.at("source").at("tag").text + " sha256:" +
           sha256_hex(phone_plans_raw(), 12);
}

std::vector<std::string> number_types(U32View nsn, const Plan& plan) {
    std::vector<std::string> out;
    for (const auto& t : plan.types)
        if (t.lengths.count(static_cast<long long>(nsn.size())) && t.pattern->fullmatch(nsn)) out.push_back(t.kind);
    return out;
}

bool has_kind(const std::vector<std::string>& kinds, const std::string& kind) {
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

std::set<U32> mobile_leads(const Plan& plan) {
    std::set<U32> out;
    if (const TypePlan* t = plan.type("mobile"))
        for (const auto& [lead, _] : t->leading) out.insert(lead);
    return out;
}

std::map<U32, std::set<long long>> marked_leads(const Plan& plan) {
    std::map<U32, std::set<long long>> out;
    for (const auto& kind : py::MARKED_TYPES) {
        const TypePlan* t = plan.type(s8(kind));
        if (t == nullptr) continue;
        if (t->leading_three.size() == t->leading.size())
            for (const auto& [head, counts] : t->leading_three) out[head].insert(counts.begin(), counts.end());
    }
    return out;
}

std::vector<U32> phone_groups(U32View run) {
    std::vector<U32> groups;
    for (const auto& g : re(py::PHONE_GROUP).findall(run)) groups.push_back(translate(g, eastern_digits()));
    return groups;
}

long long small_int(U32View digits) { return to_int(digits); }

bool is_date(const std::vector<U32>& groups) {
    if (groups.size() < 3) return false;
    const U32 &a = groups[0], &b = groups[1], &c = groups[2];
    auto day_month = [](const U32& x, const U32& y) {
        if (!(x.size() <= 2 && y.size() <= 2)) return false;
        const long long xi = small_int(x), yi = small_int(y);
        return (1 <= xi && xi <= 31 && 1 <= yi && yi <= 12) || (1 <= xi && xi <= 12 && 1 <= yi && yi <= 31);
    };
    if (day_month(a, b) && (c.size() == 4 || (c.size() == 2 && groups.size() == 3))) return true;
    return a.size() == 4 && day_month(c, b);
}

bool is_phone_number(U32View run, U32View before, const std::vector<std::string>& regions, const Pattern* context) {
    const auto groups = phone_groups(run);
    U32 digits = join(groups, U"");
    if (!(7 <= digits.size() && static_cast<long long>(digits.size()) <= py::LONGEST_NUMBER)) return false;
    if (starts(run, U"+") || starts(digits, U"00")) {
        if (!starts(run, U"+")) digits = digits.substr(2);
        for (const auto& plan : phone_plans())
            if (starts(digits, plan.code) && !number_types(U32View(digits).substr(plan.code.size()), plan).empty())
                return true;
        return false;
    }
    if (is_date(groups)) return false;
    const bool after_word = context != nullptr && context->search(before).has_value();
    for (const auto& region : regions) {
        const Plan& plan = plan_of(region);
        if (plan.code.size() == 3 && starts(digits, plan.code) &&
            has_kind(number_types(U32View(digits).substr(3), plan), "mobile"))
            return true;
        if (!plan.trunk.empty() && starts(digits, plan.trunk) &&
            !number_types(U32View(digits).substr(plan.trunk.size()), plan).empty())
            return true;
        const auto kinds = number_types(digits, plan);
        if (!kinds.empty()) {
            bool marked = false;
            for (const auto& k : kinds)
                if (in(u(k), py::MARKED_TYPES)) marked = true;
            if (after_word || (digits.size() >= 8 && marked)) return true;
        }
    }
    return false;
}

constexpr long long kInternational = 5, kCountryCodeAndMobile = 9, kMarkedLow = 8, kMarkedHigh = 11;

bool says_it_is_a_phone_number(U32View run, U32View before, const std::vector<std::string>& regions,
                               const Pattern* context) {
    const auto groups = phone_groups(run);
    const U32 digits = join(groups, U"");
    const long long n = static_cast<long long>(digits.size());
    if (n > py::LONGEST_NUMBER) return false;
    if (starts(run, U"+")) return n >= kInternational;
    if (starts(digits, U"00")) {
        if (n - 2 < kInternational) return false;
        for (const auto& plan : phone_plans())
            if (starts(U32View(digits).substr(2), plan.code)) return true;
        return false;
    }
    if (n < 7 || is_date(groups)) return false;
    if (context != nullptr && context->search(before)) return true;
    for (const auto& region : regions) {
        const Plan& plan = plan_of(region);
        if (plan.code.size() == 3 && n >= kCountryCodeAndMobile && starts(digits, plan.code) &&
            mobile_leads(plan).count(slice(digits, 3, 4)))
            return true;
        const auto marked = marked_leads(plan);
        const auto it = marked.find(slice(digits, 0, 3));
        if (kMarkedLow <= n && n <= kMarkedHigh && it != marked.end() && it->second.count(n)) return true;
    }
    return false;
}

using DigitForms = std::map<long long, U32>;

U32 digit_by_digit(U32View run, const DigitForms& forms) {
    std::vector<U32> words;
    for (const char32_t d : translate(run, eastern_digits())) {
        const U32 key(1, d);
        const auto* word = lookup(key, py::DIGIT_WORDS);
        if (word == nullptr) continue;
        const auto it = forms.find(d - U'0');
        words.push_back(it != forms.end() ? it->second : S(*word));
    }
    return join(words, U" ");
}

U32 read_phone_numbers(U32View text, const std::vector<std::string>& regions, const Pattern* context,
                       const DigitForms& forms) {
    U32 out;
    size_t last = 0;
    for (const auto& m : re(py::PHONE_CANDIDATE).finditer(text)) {
        const U32 whole = m.str(0);
        std::vector<size_t> ends;
        for (const auto& g : re(py::PHONE_GROUP).finditer(whole)) ends.push_back(g.end());
        const U32View before = text.substr(0, m.start());
        std::optional<size_t> end;
        for (int test = 0; test < 2 && !end; ++test) {
            for (auto it = ends.rbegin(); it != ends.rend(); ++it) {
                const U32View run = U32View(whole).substr(0, *it);
                const bool ok = test == 0 ? is_phone_number(run, before, regions, context)
                                          : says_it_is_a_phone_number(run, before, regions, context);
                if (ok) {
                    end = *it;
                    break;
                }
            }
        }
        if (end) {
            out += text.substr(last, m.start() - last);
            out += digit_by_digit(U32View(whole).substr(0, *end), forms);
            last = m.start() + *end;
        }
    }
    out += text.substr(last);
    return out;
}

// ── Numbers ─────────────────────────────────────────────────────────────────

std::string number_case(const TtsNorm& config) { return config.oblique_numbers ? "oblique" : "nominative"; }

std::string number_lang(const std::string& lang, const TtsNorm& config) {
    if (!config.dialect_numbers) return lang;
    const auto code = dialects::lect_code(lang);
    return code && !code->empty() ? *code : lang;
}

U32 say(const Number& value, const std::string& lang, const std::string& grammatical_case,
        onp::GrammaticalGender gender = onp::GrammaticalGender::MASCULINE) {
    return detail::pronounce_number(value, lang, gender, grammatical_case);
}

DigitForms digit_forms(const std::string& lang, const TtsNorm& config) {
    DigitForms forms;
    if (config.dialect_numbers) {
        const std::string spoken = number_lang(lang, config), grammatical_case = number_case(config);
        for (long long digit = 0; digit < 10; ++digit) {
            const U32 word = say(BigInt(digit), spoken, grammatical_case);
            if (word != say(BigInt(digit), "ar", grammatical_case)) forms[digit] = word;
        }
    }
    for (const auto& [value, word] : config.number_forms) forms[value] = u(word);
    return forms;
}

// An insertion-ordered dict of strings.
struct OrderedMap {
    std::vector<std::pair<U32, U32>> items;
    U32* find(U32View key) {
        for (auto& kv : items)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    void set(const U32& key, const U32& value) {
        if (U32* v = find(key)) {
            *v = value;
        } else {
            items.emplace_back(key, value);
        }
    }
    void setdefault(const U32& key, const U32& value) {
        if (!find(key)) items.emplace_back(key, value);
    }
};

// The keys longest first, ties in insertion order, as re.escape alternatives.
U32 longest_first(const OrderedMap& map) {
    std::vector<U32> keys;
    for (const auto& kv : map.items) keys.push_back(kv.first);
    std::stable_sort(keys.begin(), keys.end(), [](const U32& a, const U32& b) { return a.size() > b.size(); });
    U32 out;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) out += U'|';
        out += pyre::escape(keys[i]);
    }
    return out;
}

struct LectForms {
    std::map<long long, U32> forms;
    const Pattern* pattern = nullptr;
    OrderedMap said;
};

LectForms lect_forms(const std::string& lang, const TtsNorm& config) {
    LectForms out;
    for (const auto& [value, word] : config.number_forms) out.forms[value] = u(word);
    if (out.forms.empty()) return out;
    const std::string spoken = number_lang(lang, config), grammatical_case = number_case(config);
    for (const auto& [value, word] : out.forms) out.said.set(say(BigInt(value), spoken, grammatical_case), word);
    const U32 thousand = say(BigInt(1000), spoken, grammatical_case);
    for (const auto& [value, word] : out.forms) {
        if (11 <= value && value <= 999) {
            const U32 counted = say(BigInt(value * 1000), spoken, grammatical_case);
            if (ends(counted, U" " + thousand))
                out.said.setdefault(counted.substr(0, counted.size() - thousand.size() - 1), word);
        }
    }
    out.pattern = &pyre::compiled(U"(?<!\\S)(و?)(" + longest_first(out.said) + U")(?!\\S)");
    return out;
}

Number parse_number(U32View number) {
    const std::string text = s8(number);
    if (has(number, U'.')) {
        const auto v = numbers::py_float(text);
        if (!v) throw ValueError("could not convert string to float: " + repr(number));
        return *v;
    }
    const auto v = numbers::py_int(text);
    if (!v) throw ValueError("invalid literal for int() with base 10: " + repr(number));
    return *v;
}

onp::GrammaticalGender gender_of(U32View name) {
    if (name == U"masculine") return onp::GrammaticalGender::MASCULINE;
    if (name == U"feminine") return onp::GrammaticalGender::FEMININE;
    if (name == U"neutral") return onp::GrammaticalGender::NEUTRAL;
    throw ValueError(repr(name) + " is not a valid GrammaticalGender");
}

// The private-use characters that stand in for spans no later rule may read.
// A code point the text holds when one is asked for is passed over, as the
// generator in normalize_for_tts reads the text as it stands at that call.
struct Holder {
    const U32* text;
    char32_t next = static_cast<char32_t>(py::HELD_DIGITS);
    std::vector<std::pair<char32_t, U32>> held;
    U32 hold(const U32& span) {
        while (next < 0xF900 && has(*text, next)) ++next;
        if (next >= 0xF900) throw Error("StopIteration", "");
        const char32_t placeholder = next++;
        held.emplace_back(placeholder, span);
        return U32(1, placeholder);
    }
};

struct Speaker {
    const std::string& lang;
    const TtsNorm& config;
    std::optional<LectForms> lect;

    U32 cardinal(U32View number, bool feminine_noun = false) {
        const Number value = parse_number(number);
        const bool integer = numbers::is_int(value);
        const bool short_form = feminine_noun && integer && cmp(value, 3) >= 0 && cmp(value, 10) <= 0;
        U32 words = say(value, number_lang(lang, config), number_case(config),
                        short_form ? onp::GrammaticalGender::FEMININE : onp::GrammaticalGender::MASCULINE);
        if (short_form && words == U"ثمان") words = U"ثماني";
        if (!lect) lect = lect_forms(lang, config);
        std::optional<long long> key;
        if (integer) key = std::get<BigInt>(value).to_ll();
        if (key && lect->forms.count(*key)) {
            words = lect->forms.at(*key);
        } else if (lect->pattern) {
            words = lect->pattern->sub(
                [&](const Match& m) {
                    const U32* said = lect->said.find(m.str(2));
                    if (!said) throw KeyError(repr(m.str(2)));
                    return m.str(1) + *said;
                },
                words);
        }
        words = re(py::CONSTRUCT_HUNDREDS).sub(
            [](const Match& m) { return m.str(1) + (m.str(2) == U"ين" ? U"ي" : U"ا"); }, words);
        if (config.space_fused_hundreds)
            words = re(py::FUSED_HUNDREDS_RE).sub([](const Match& m) { return S(*lookup(m.str(0), py::FUSED_HUNDREDS)); },
                                                   words);
        return words;
    }
};

// ── Clock times, ranks, dates and years ────────────────────────────────────

const std::pair<int, std::u32string_view> kDayParts[] = {
    {5, U"الصباح"}, {12, U"الظهر"}, {15, U"العصر"}, {18, U"المغرب"}, {20, U"بالليل"}};

U32 colloquial_clock(int hour, int minute, bool period, Speaker& sp) {
    const bool back = minute == 40 || minute == 50;
    int twelve = (hour + (back ? 1 : 0)) % 12;
    if (twelve == 0) twelve = 12;
    std::vector<U32> words = {U"الساعة", twelve == 1 ? U32(U"واحدة") : sp.cardinal(itos(twelve))};
    if (minute == 15) {
        words.emplace_back(U"وربع");
    } else if (minute == 20) {
        words.emplace_back(U"وثلث");
    } else if (minute == 30) {
        words.emplace_back(U"ونص");
    } else if (minute == 40) {
        words.emplace_back(U"إلا ثلث");
    } else if (minute == 50) {
        words.push_back(U"إلا " + sp.cardinal(U"10"));
    } else if (minute == 1) {
        words.emplace_back(U"ودقيقة");
    } else if (minute == 2) {
        words.emplace_back(sp.config.oblique_numbers ? U"ودقيقتين" : U"ودقيقتان");
    } else if (minute) {
        words.push_back(U"و" + sp.cardinal(itos(minute)) + (minute > 10 ? U" دقيقة" : U""));
    }
    if (period) {
        U32 part = S(std::end(kDayParts)[-1].second);
        for (auto it = std::rbegin(kDayParts); it != std::rend(kDayParts); ++it) {
            if (hour >= it->first) {
                part = S(it->second);
                break;
            }
        }
        words.push_back(part);
    }
    return join(words, U" ");
}

std::optional<U32> spoken_clock(const Match& m, Speaker& sp) {
    if (m.str("sep") == U"." && re(py::AMOUNT_AFTER).match(m.string(), m.end())) return std::nullopt;
    int hour = static_cast<int>(to_int(translate(m.str("hour"), eastern_digits())));
    const bool has_minute = m.matched("minute");
    const int minute = has_minute ? static_cast<int>(to_int(translate(m.str("minute"), eastern_digits()))) : 0;
    std::optional<U32> marker;
    if (m.matched("marker")) marker = lower(m.str("marker"));
    if (m.matched("arabic_marker")) {
        if (!has_minute && !m.matched("saa")) return std::nullopt;
        if (1 <= hour && hour <= 12) {
            marker = S(*lookup(m.str("arabic_marker"), py::ARABIC_MARKERS));
        } else {
            marker.reset();
        }
    }
    if (!marker && !has_minute && !m.matched("saa")) return std::nullopt;
    if (minute > 59 || hour > 23 || (marker && !(1 <= hour && hour <= 12))) return std::nullopt;
    if (marker == U32(U"pm") && hour < 12) {
        hour += 12;
    } else if (marker == U32(U"am") && hour == 12) {
        hour = 0;
    }
    const bool period = marker.has_value() || hour == 0 || hour > 12;
    if (sp.config.colloquial_clock) return colloquial_clock(hour, minute, period, sp);
    odp::DateTime dt;
    dt.year = 2000;
    dt.hour = hour;
    dt.minute = minute;
    return detail::nice_time(dt, "ar", true, false, period);
}

U32 rank_ordinal(const Match& m, const TtsNorm& config) {
    const U32 proclitic = m.str(1), noun = m.str(2), gap = m.str(3), number = m.str(4);
    const long long value = to_int(translate(number, eastern_digits()));
    if (!(ends(proclitic, U"ال") || ends(proclitic, U"لل")) || value == 0) return m.str(0);
    const auto* gender = lookup(noun, py::RANK_NOUNS);
    if (gender == nullptr) throw KeyError(repr(noun));
    return proclitic + noun + gap + detail::pronounce_ordinal(BigInt(value), "ar", gender_of(*gender), number_case(config));
}

bool valid_date(long long year, long long month, long long day) {
    if (month < 1 || month > 12 || day < 1) return false;
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return day <= days[month - 1] + (month == 2 && leap ? 1 : 0);
}

U32 colloquial_year(U32View year, Speaker& sp) {
    const long long value = to_int(translate(year, eastern_digits()));
    if (!(2020 <= value && value <= 2099)) return U32(year);
    return sp.cardinal(U"20") + U" " + sp.cardinal(itos(value - 2000));
}

U32 spoken_date(const Match& m, Speaker& sp, Holder& holder) {
    U32 year, month, day;
    if (m.matched("iso_year") && !m.str("iso_year").empty()) {
        year = m.str("iso_year"), month = m.str("iso_month"), day = m.str("iso_day");
    } else {
        year = m.str("year"), month = m.str("month"), day = m.str("day");
    }
    const size_t written = year.size();
    long long y = to_int(translate(year, eastern_digits()));
    const long long mo = to_int(translate(month, eastern_digits()));
    const long long d = to_int(translate(day, eastern_digits()));
    if (written == 2) y += 2000;
    const U32 era = strip(m.str("era"));
    const bool dotted = (m.matched("sep") && m.str("sep") == U".") || (m.matched("iso_sep") && m.str("iso_sep") == U".");
    U32 name, after;
    if (era == U"هـ" || era == U"ه" || (py::HIJRI_YEARS.has(y) && era.empty())) {
        if (!(py::HIJRI_YEARS.has(y) && 1 <= mo && mo <= 12 && 1 <= d && d <= 30) || (dotted && era.empty()))
            return m.str(0);
        name = S(py::HIJRI_MONTH_NAMES[mo - 1]);
        after = era.empty() ? U"" : U" هجري";
    } else {
        if (!py::GREGORIAN_YEARS.has(y)) return m.str(0);
        if (!valid_date(y, mo, d)) return m.str(0);
        name = S(py::MONTH_NAMES[mo - 1]);
        after = era.empty() ? U"" : U" ميلادي";
        if (sp.config.colloquial_years && 2020 <= y && y <= 2099) {
            const U32 a = holder.hold(sp.cardinal(itos(d)));
            const U32 b = holder.hold(colloquial_year(itos(y), sp));
            return a + U" " + name + U" " + b + after;
        }
    }
    const U32 a = holder.hold(sp.cardinal(itos(d)));
    const U32 b = holder.hold(sp.cardinal(itos(y)));
    return a + U" " + name + U" " + b + after;
}

U32 colloquial_years(const Match& m, Speaker& sp) {
    const U32 cue = m.str(1), first = m.str(2);
    if (!m.matched(4)) return cue + colloquial_year(first, sp);
    const U32 joint = m.str(3), second = m.str(4);
    if (strip(joint) == U"و") return cue + colloquial_year(first, sp) + joint + colloquial_year(second, sp);
    for (const U32* y : {&first, &second}) {
        const long long v = to_int(translate(*y, eastern_digits()));
        if (!(2020 <= v && v <= 2099)) return m.str(0);
    }
    const U32 a = colloquial_year(first, sp);
    return cue + a + joint + colloquial_year(second, sp);
}

U32 named_month(const Match& m, const std::vector<std::string>& names) {
    const std::string group = m.lastgroup();
    const int month = std::stoi(group.substr(1));
    if (m.str(group) == U"مارس") {
        const size_t at = static_cast<size_t>(m.start(m.index(group)));
        if (!(re(py::MARCH_BEFORE).search(m.string(), 0, at) || re(py::MARCH_AFTER).match(m.string(), m.end())))
            return m.str(0);
    }
    return m.str(1) + u(names.at(static_cast<size_t>(month)));
}

// ── Codes, units and lexicons ───────────────────────────────────────────────

const std::set<U32>& currency_codes() {
    static const std::set<U32> codes = [] {
        std::set<U32> out;
        const std::string table_name = "currencies-cldr";
        for (const auto& row : term_rows(table_name)) {
            const std::string* key = cell(row, "cldr_key");
            if (key == nullptr) throw KeyError("'cldr_key'");
            if (key->rfind("numbers/currencies/", 0) != 0) continue;
            const auto parts = split(u(*key), U"/");
            out.insert(parts.at(2));
        }
        for (const auto& a : py::CURRENCY_ABBREVIATIONS) out.insert(S(a));
        return out;
    }();
    return codes;
}

std::set<U32> upper_units(const std::string& lang) {
    std::set<U32> out;
    for (const auto& [symbol, _] : cldr_units(lang)) {
        const U32 s = u(symbol);
        if (isupper(s)) out.insert(s);
    }
    return out;
}

std::set<U32> not_code_letters(const std::string& lang) {
    std::set<U32> out = upper_units(lang);
    out.insert(currency_codes().begin(), currency_codes().end());
    for (const auto& unit : py::CAPITALISED_UNITS) out.insert(upper(unit));
    return out;
}

struct TermPattern {
    const Pattern* pattern;
    OrderedMap said;
};

TermPattern term_pattern(const Lexicon& lexicon) {
    TermPattern out;
    for (const auto& [term, spoken] : lexicon) out.said.set(lower(u(term)), u(spoken));
    const U32 source = U"(?:" + S(py::TATWEEL_BEFORE_LATIN) + U")?" + S(py::LATIN_RUN_EDGE[0]) + U"(" +
                       longest_first(out.said) + U")" + S(py::LATIN_RUN_EDGE[1]);
    out.pattern = &pyre::compiled(source, pyre::IGNORECASE);
    return out;
}

U32 spelled(const std::map<U32, U32>& names, char32_t c) {
    const auto it = names.find(U32(1, c));
    if (it == names.end()) throw KeyError(repr(U32(1, c)));
    return it->second;
}

U32 letter_names(U32View letters) {
    std::vector<U32> out;
    for (const auto& c : split_ws(replace(letters, py::TATWEEL, U""))) {
        const auto* name = lookup(c, py::LETTER_NAMES);
        if (name == nullptr) throw KeyError(repr(c));
        out.push_back(S(*name));
    }
    return join(out, U" ");
}

// ── Plain orthography ───────────────────────────────────────────────────────

U32 plain_dagger(const Match& m) {
    U32 word = m.str(0);
    static const Table letters = table(py::PLAIN_LETTERS);
    const U32 skeleton =
        translate(pyre::compiled(U"[ً-ٰٕـۖ-ۭ]").sub(U32View(), word), letters);
    if (in(skeleton, py::DAGGER_SILENT) ||
        (first_in(skeleton, U"وفبل") && in(slice_from(skeleton, 1), py::DAGGER_SILENT)))
        return replace(word, U"ٰ", U"");
    word = pyre::compiled(U"و[ً-ْ]*ٰ").sub(U32View(U"ا"), word);
    word = pyre::compiled(U"ى[ً-ْ]*ٰ").sub(U32View(U"ى"), word);
    return replace(word, U"ٰ", U"ا");
}

U32 plain_orthography(U32 text) {
    text = re(py::DAGGER_WORD).sub(plain_dagger, text);
    text = re(py::PLAIN_MARKS).sub(U32View(), text);
    text = re(py::PLAIN_HAMZA_ON_ALEF).sub(U32View(), text);
    for (const auto& [decomposed, composed] : py::PLAIN_COMPOSED) text = replace(text, decomposed, composed);
    static const Table letters = table(py::PLAIN_LETTERS);
    return translate(re(py::PLAIN_STRAY_HAMZA).sub(U32View(), text), letters);
}

// ── The cardinals pass ──────────────────────────────────────────────────────

U32 speak_numbers(U32 text, Speaker& sp) {
    auto speak = [&](const U32& written, const U32& number, bool feminine_noun) -> U32 {
        try {
            return sp.cardinal(number, feminine_noun);
        } catch (const Error& e) {
            // Python has no NotImplementedError on this path: the port raises it only
            // for spoken forms it does not have, and those are not unspeakable numbers.
            if (e.python_type() == "NotImplementedError" || !sp.config.leave_unspeakable_numbers) throw;
            return written;
        }
    };
    auto eastern = [&](const Match& m) {
        U32 number = translate(m.str(0), eastern_digits());
        number = replace(replace(replace(number, U"٬", U""), U"،", U""), U"٫", U".");
        return speak(m.str(0), number, re(py::FEMININE_COUNTED).match(m.string(), m.end()).has_value());
    };
    auto western = [&](const Match& m) {
        const U32 raw = m.str(0);
        if (!has(raw, U',')) return speak(raw, raw, re(py::FEMININE_COUNTED).match(m.string(), m.end()).has_value());
        const size_t dot = raw.find(U'.');
        const U32 int_part = dot != U32::npos ? raw.substr(0, dot) : raw;
        const U32 frac_part = dot != U32::npos ? raw.substr(dot) : U32();
        const auto resolved = util::resolve_thousands_or_decimal(s8(int_part), ",");
        if (resolved) return speak(raw, u(*resolved) + frac_part, false);
        std::vector<U32> said;
        for (const auto& g : split(int_part, U",")) said.push_back(speak(g, g, false));
        return join(said, U",") + frac_part;
    };
    text = re(py::NUMBER_AGAINST_WORD).sub(U32View(U" "), text);
    if (sp.config.colloquial_years)
        text = re(py::YEAR).sub([&](const Match& m) { return colloquial_years(m, sp); }, text);
    text = re(py::RANK_NUMBER).sub([&](const Match& m) { return rank_ordinal(m, sp.config); }, text);
    text = re(py::EASTERN_NUMBER).sub(eastern, text);
    return re(py::WESTERN_NUMBER).sub(western, text);
}

const Pattern* compile(const U32& source, int flags = 0) {
    return &pyre::compiled(source, flags);
}

bool truthy_arabic_only(const TtsNorm& c, U32View name) {
    if (name == U"speak_percent") return c.speak_percent;
    if (name == U"phone_shapes") return !c.phone_shapes.empty();
    if (name == U"phone_regions") return !c.phone_regions.empty();
    if (name == U"long_digit_runs") return c.long_digit_runs;
    if (name == U"phone_prefixes") return !c.phone_prefixes.empty();
    if (name == U"identifier_words") return !c.identifier_words.empty();
    if (name == U"dialect_numbers") return c.dialect_numbers;
    if (name == U"number_forms") return !c.number_forms.empty();
    if (name == U"month_names") return !c.month_names.empty();
    if (name == U"colloquial_units") return c.colloquial_units;
    return false;
}

U32 alternatives(const std::vector<std::string>& items, bool group, bool escape) {
    U32 out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += U'|';
        const U32 item = escape ? pyre::escape(u(items[i])) : u(items[i]);
        out += group ? U"(?:" + item + U")" : item;
    }
    return out;
}

}  // namespace

// ── TtsNorm ─────────────────────────────────────────────────────────────────

TtsNorm TtsNorm::with_lexicon(const Pairs& entries) const {
    Lexicon sorted(entries.begin(), entries.end());
    std::sort(sorted.begin(), sorted.end());
    for (const auto& [spelling, term] : sorted)
        if (strip(u(spelling)).empty())
            throw ValueError("a lexicon entry is a non-empty string and the string it stands for, got (" +
                             repr(u(spelling)) + ", " + repr(u(term)) + ")");
    TtsNorm out = *this;
    out.lexicon = std::move(sorted);
    return out;
}

TtsNorm TtsNorm::with_number_forms(const NumberForms& forms) const {
    std::map<long long, std::string> out;
    for (const auto& [value, word] : forms) {
        if (value < 0) throw TypeError("a number form is keyed by a non-negative int, not " + std::to_string(value));
        const U32 stripped = strip(u(word));
        if (stripped.empty())
            throw TypeError("the form for " + std::to_string(value) + " is " + repr(u(word)) + "; it must be a word");
        out[value] = s8(stripped);
    }
    TtsNorm copy = *this;
    copy.number_forms.assign(out.begin(), out.end());
    return copy;
}

void TtsNorm::check() const {
    if (month_names.empty()) return;
    bool ok = month_names.size() == 12;
    for (const auto& w : month_names)
        if (strip(u(w)).empty()) ok = false;
    if (!ok) throw ValueError("month_names holds a word for each of the twelve months, not " + str_tuple_repr(month_names));
}

const std::vector<TtsNorm::Field>& TtsNorm::fields() {
    static const std::vector<Field> f = {
        {"strip_controls", &TtsNorm::strip_controls},
        {"spell_out_plates", &TtsNorm::spell_out_plates},
        {"spell_out_codes", &TtsNorm::spell_out_codes},
        {"speak_percent", &TtsNorm::speak_percent},
        {"keep_code_digits", &TtsNorm::keep_code_digits},
        {"long_digit_runs", &TtsNorm::long_digit_runs},
        {"cardinal_numbers", &TtsNorm::cardinal_numbers},
        {"colloquial_clock", &TtsNorm::colloquial_clock},
        {"colloquial_years", &TtsNorm::colloquial_years},
        {"colloquial_units", &TtsNorm::colloquial_units},
        {"leave_unspeakable_numbers", &TtsNorm::leave_unspeakable_numbers},
        {"oblique_numbers", &TtsNorm::oblique_numbers},
        {"space_fused_hundreds", &TtsNorm::space_fused_hundreds},
        {"dialect_numbers", &TtsNorm::dialect_numbers},
        {"spoken_forms", &TtsNorm::spoken_forms},
        {"canonical_unicode", &TtsNorm::canonical_unicode},
        {"drop_false_starts", &TtsNorm::drop_false_starts},
        {"plain_orthography", &TtsNorm::plain_orthography},
    };
    return f;
}

bool operator==(const TtsNorm& a, const TtsNorm& b) {
    for (const auto& f : TtsNorm::fields())
        if (a.*f.member != b.*f.member) return false;
    return a.phone_shapes == b.phone_shapes && a.phone_regions == b.phone_regions &&
           a.phone_prefixes == b.phone_prefixes && a.identifier_words == b.identifier_words &&
           a.number_forms == b.number_forms && a.month_names == b.month_names && a.lexicon == b.lexicon;
}

std::string TtsNorm::describe() const {
    std::vector<std::string> on;
    auto flag = [&](const char* name, bool value) {
        if (value) on.emplace_back(name);
    };
    auto values = [&](const char* name, bool present, const std::string& r) {
        if (present) on.push_back(std::string(name) + "=" + sha256_hex(r, 8));
    };
    flag("strip_controls", strip_controls);
    flag("spell_out_plates", spell_out_plates);
    flag("spell_out_codes", spell_out_codes);
    flag("speak_percent", speak_percent);
    flag("keep_code_digits", keep_code_digits);
    values("phone_shapes", !phone_shapes.empty(), str_tuple_repr(phone_shapes));
    values("phone_regions", !phone_regions.empty(), str_tuple_repr(phone_regions));
    flag("long_digit_runs", long_digit_runs);
    values("phone_prefixes", !phone_prefixes.empty(), str_tuple_repr(phone_prefixes));
    values("identifier_words", !identifier_words.empty(), str_tuple_repr(identifier_words));
    flag("cardinal_numbers", cardinal_numbers);
    flag("colloquial_clock", colloquial_clock);
    flag("colloquial_years", colloquial_years);
    flag("colloquial_units", colloquial_units);
    flag("leave_unspeakable_numbers", leave_unspeakable_numbers);
    flag("oblique_numbers", oblique_numbers);
    flag("space_fused_hundreds", space_fused_hundreds);
    flag("dialect_numbers", dialect_numbers);
    values("number_forms", !number_forms.empty(), forms_repr(number_forms));
    values("month_names", !month_names.empty(), str_tuple_repr(month_names));
    flag("spoken_forms", spoken_forms);
    flag("canonical_unicode", canonical_unicode);
    flag("drop_false_starts", drop_false_starts);
    flag("plain_orthography", plain_orthography);
    std::string names;
    for (size_t i = 0; i < on.size(); ++i) names += (i ? "," : "") + on[i];
    const bool speaks = cardinal_numbers || spoken_forms || dialect_numbers;
    return "arbtok-tts-norm " + TTS_NORM_VERSION + ": " + (names.empty() ? "none" : names) +
           describe_lexicon(lexicon) + (phone_regions.empty() ? "" : describe_phone_plans()) +
           (speaks ? kNumberParser : "");
}

// ── normalize_for_tts ───────────────────────────────────────────────────────

std::string normalize_for_tts(const std::string& input, const std::string& lang, const TtsNorm& config) {
    config.check();
    std::vector<std::string> speaks_arabic;
    for (const auto& name : py::ARABIC_ONLY)
        if (truthy_arabic_only(config, name)) speaks_arabic.push_back(s8(name));
    const bool arabic = is_arabic_lang(lang);
    if (!speaks_arabic.empty() && !arabic) {
        std::string names;
        for (size_t i = 0; i < speaks_arabic.size(); ++i) names += (i ? ", " : "") + speaks_arabic[i];
        throw ValueError(names + " speak Arabic and lang is " + repr(u(lang)));
    }
    const DigitForms forms = digit_forms(lang, config);
    const Pattern* phones =
        config.phone_shapes.empty()
            ? nullptr
            : compile(U"(?<![0-9])(?:" + alternatives(config.phone_shapes, false, false) + U")");
    const Pattern* prefixes =
        config.phone_prefixes.empty() ? nullptr : compile(alternatives(config.phone_prefixes, true, false));
    const Pattern* context =
        config.identifier_words.empty()
            ? nullptr
            : compile(U"(?:^|[\\s\\W])" + S(py::PROCLITIC) + U"(?:" +
                                  alternatives(config.identifier_words, false, true) + U")" + S(py::IDENTIFIER_GAP),
                              pyre::IGNORECASE);

    U32 text = u(input);
    if (config.strip_controls) text = re(py::CONTROLS).sub(U32View(), text);
    Holder holder{&text, static_cast<char32_t>(py::HELD_DIGITS), {}};
    auto hold = [&](const U32& span) { return holder.hold(span); };
    Speaker sp{lang, config, std::nullopt};

    if (config.spell_out_plates && arabic) {
        auto plate = [&](const Match& m) {
            const U32 ahead = slice(m.string(), 0, static_cast<long>(m.start()));
            const auto before = re(py::LETTER_BEFORE).search(ahead);
            if (before && before->str(1) != U"و") return m.str(0);
            return hold(letter_names(m.str(1)) + U" " + digit_by_digit(m.str(2), forms));
        };
        auto latin_plate = [&](const Match& m) {
            const auto names = spelled_codes_u(lang);
            const bool letters_first = !m.str(2).empty();
            const U32 letters = letters_first ? m.str(2) : m.str(5);
            const U32 digits = letters_first ? m.str(3) : m.str(4);
            std::vector<U32> parts;
            for (const char32_t c : upper(letters)) parts.push_back(spelled(names, c));
            const U32 spelled_letters = join(parts, U" ");
            const U32 said = digit_by_digit(digits, forms);
            return m.str(1) + hold(letters_first ? spelled_letters + U" " + said : said + U" " + spelled_letters);
        };
        text = re(py::PLATE).sub(plate, text);
        text = re(py::PLATE_DIGITS_FIRST).sub(
            [&](const Match& m) {
                const U32 digits = digit_by_digit(m.str(1), forms);
                return hold(digits + U" " + letter_names(m.str(2)));
            },
            text);
        text = re(py::LATIN_PLATE).sub(latin_plate, text);
    }
    if (config.spell_out_codes) {
        const Pattern& separators = pyre::compiled(U"[ -]");
        const Pattern& capital = pyre::compiled(U"[A-Z]");
        const Pattern& digit = pyre::compiled(U"[0-9]");
        auto grouped_vin = [&](const Match& m) {
            const U32 whole = m.str(0);
            const U32 run = separators.sub(U32View(), whole);
            if (!(capital.search(run) && digit.search(run))) return whole;
            if (re(py::VIN_WORD).search(slice(m.string(), 0, static_cast<long>(m.start()))) && 8 <= run.size() &&
                run.size() <= 17) {
                bool all_lettered = true;
                for (const auto& g : separators.split(whole))
                    if (!capital.search(g.value_or(U""))) all_lettered = false;
                if (run.size() == 17 || all_lettered) return run;
            }
            return run.size() == 17 && !has(whole, U' ') ? run : whole;
        };
        text = re(py::SMALL_VIN).sub([](const Match& m) { return upper(m.str(0)); }, text);
        text = re(py::GROUPED_VIN).sub(grouped_vin, text);
    }
    const bool cardinals = config.cardinal_numbers || config.dialect_numbers || !config.number_forms.empty();
    if (config.spell_out_codes || cardinals) text = re(py::GLUED_UNIT).sub(U32View(U"\\1 "), text);
    if (config.spell_out_codes) {
        text = re(py::SMALL_LETTER_CODE).sub([](const Match& m) { return upper(m.str(0)); }, text);
        const std::set<U32> amounts = not_code_letters(lang);
        text = re(py::SPACED_CODE).sub(
            [&](const Match& m) { return amounts.count(m.str(1)) ? m.str(0) : upper(m.str(1)) + m.str(2); }, text);
        std::set<U32> terms;
        for (const auto& [term, _] : config.lexicon) terms.insert(lower(u(term)));
        text = re(py::HYPHENATED_CODE).sub(
            [&](const Match& m) {
                return amounts.count(m.str(1)) || terms.count(lower(m.str(0))) ? m.str(0)
                                                                                : upper(m.str(1)) + m.str(2);
            },
            text);
    }
    if (!config.lexicon.empty()) {
        const TermPattern tp = term_pattern(config.lexicon);
        auto said = [&](const Match& m) {
            const U32 key = lower(m.str(1));
            for (const auto& kv : tp.said.items)
                if (kv.first == key) return kv.second;
            throw KeyError(repr(key));
        };
        if (arabic) {
            text = tp.pattern->sub(
                [&](const Match& m) {
                    return re(py::READING_DIGITS)
                        .sub(
                            [&](const Match& d) {
                                const auto names = spelled_codes_u("ar");
                                std::vector<U32> parts;
                                for (const char32_t c : d.str(0)) parts.push_back(spelled(names, c));
                                return U" " + join(parts, U" ");
                            },
                            said(m));
                },
                text);
        } else {
            text = tp.pattern->sub(said, text);
        }
    }
    if (config.colloquial_units) {
        text = re(py::KILOMETRE_SYMBOL)
                   .sub(
                       [&](const Match& m) {
                           if (!m.str(3).empty() &&
                               (re(py::AFTER_LATIN_WORD).search(slice(m.string(), 0, static_cast<long>(m.start()))) ||
                                re(py::YEAR_DIGITS).fullmatch(translate(m.str(1), eastern_digits())) ||
                                !re(py::AFTER_KILOMETRE).match(m.string(), m.end())))
                               return m.str(0);
                           return m.str(1) + (m.str(2).empty() ? U" كيلو" : U" كيلو بالساعة");
                       },
                       text);
    }
    if (cardinals && arabic) {
        const Pattern& digit_before = pyre::compiled(U"[0-9٠-٩]\\s*$");
        text = re(py::PO_BOX).sub(
            [&](const Match& m) {
                const U32 before = slice(m.string(), 0, static_cast<long>(m.start()));
                const auto letter = re(py::LETTER_BEFORE).search(before);
                if ((letter && letter->str(1) != U"و") || digit_before.search(before)) return m.str(0);
                return U32(U"صندوق بريد");
            },
            text);
    }
    if (config.drop_false_starts && arabic) text = re(py::FALSE_START).sub(U32View(), text);
    if ((config.spoken_forms || config.cardinal_numbers) && arabic) {
        text = re(py::CLOCK).sub(
            [&](const Match& m) {
                auto spoken = spoken_clock(m, sp);
                if (!spoken) return m.str(0);
                if (!m.str("saa").empty() && starts(*spoken, U"الساعة "))
                    spoken = m.str("saa") + spoken->substr(U32(U"الساعة").size());
                return hold(*spoken);
            },
            text);
    }
    if (config.spell_out_codes) {
        const auto names = spelled_codes_u(lang);
        const std::set<U32> units = upper_units(lang);
        auto spell = [&](const U32& run, bool vin) {
            std::vector<U32> parts;
            if (vin || re(py::VIN).fullmatch(run)) {
                for (const char32_t c : run)
                    parts.push_back(isdigit(c) ? digit_by_digit(U32(1, c), forms) : spelled(names, c));
            } else {
                for (const char32_t c : run) parts.push_back(spelled(names, c));
            }
            return join(parts, U" ");
        };
        auto is_unit = [&](const U32& run) { return units.count(run) || in(lower(run), py::CAPITALISED_UNITS); };
        text = re(py::SMALL_SUFFIX_CODE).sub([](const Match& m) { return m.str(1) + upper(m.str(2)); }, text);
        text = re(py::DECIMAL_CODE).sub(
            [&](const Match& m) {
                const U32 number = replace(translate(m.str(1), eastern_digits()), U"٫", U".");
                const U32 run = m.str(2);
                const U32 spoken_number = sp.cardinal(number);
                if (is_unit(run)) return hold(spoken_number + U" " + run);
                return hold(spoken_number + U" " + spell(run, false));
            },
            text);
        text = re(py::TATWEEL_BEFORE_CODE).sub(U32View(), text);
        text = re(py::CODE).sub(
            [&](const Match& m) {
                const U32 run = m.str("code");
                const U32 before = slice(m.string(), 0, static_cast<long>(m.start()));
                if (is_unit(run) && re(py::NUMBER_BEFORE).search(before)) return run;
                bool digits = false;
                for (const char32_t c : run)
                    if (isdigit(c)) digits = true;
                return spell(run, digits && re(py::VIN_WORD).search(before).has_value());
            },
            text);
    } else if (cardinals && arabic) {
        const std::set<U32> amounts = not_code_letters("ar");
        text = re(py::HYPHENATED_CODE).sub(
            [&](const Match& m) { return amounts.count(m.str(1)) ? m.str(0) : hold(m.str(0)); }, text);
    }
    text = re(py::MIXED_CODE).sub([&](const Match& m) { return hold(m.str(0)); }, text);
    if (cardinals && arabic)
        text = re(py::NUMERIC_DATE).sub([&](const Match& m) { return spoken_date(m, sp, holder); }, text);
    if (!config.month_names.empty())
        text = re(py::WRITTEN_MONTH).sub([&](const Match& m) { return named_month(m, config.month_names); }, text);
    if (config.speak_percent) {
        text = re(py::PERCENT).sub([](const Match& m) { return m.str(1) + U" في المئة"; }, text);
        text = re(py::PERCENT_SIGN_FIRST).sub([](const Match& m) { return m.str(1) + U" في المئة"; }, text);
    }
    if (config.keep_code_digits) {
        text = re(py::CODE_DIGITS).sub(
            [&](const Match& m) {
                auto beside = re(py::LATIN_WORD_BEFORE).search(m.string(), 0, m.start());
                if (!beside) beside = re(py::LATIN_WORD_AFTER).match(m.string(), m.end());
                return beside && currency_codes().count(beside->str(1)) ? m.str(0) : hold(m.str(0));
            },
            text);
    }
    if (phones) text = phones->sub([&](const Match& m) { return digit_by_digit(m.str(0), forms); }, text);
    if (!config.phone_regions.empty()) text = read_phone_numbers(text, config.phone_regions, context, forms);
    if (config.long_digit_runs) {
        text = re(py::LONG_RUN).sub(
            [&](const Match& m) {
                const U32 run = m.str(0);
                const bool round = !starts(run, U"+") && ends(translate(run, eastern_digits()), U"000");
                return round ? run : digit_by_digit(run, forms);
            },
            text);
    }
    if (prefixes || context) {
        const U32 whole = text;
        text = re(py::DIGIT_RUN).sub(
            [&](const Match& m) {
                const U32 run = m.str(0);
                if (prefixes && (starts(run, U"+") || prefixes->fullmatch(run))) return digit_by_digit(run, forms);
                if (context && context->search(slice(whole, 0, static_cast<long>(m.start()))))
                    return digit_by_digit(run, forms);
                return run;
            },
            text);
    }
    if (cardinals) {
        text = re(py::RIYAL_SIGN).sub(U32View(U" ريال"), text);
        text = speak_numbers(text, sp);
    }
    std::string out;
    if (config.spoken_forms) {
        text = u(util::normalize(s8(text), arabic ? "ar" : lang));
    }
    for (const auto& [placeholder, digits] : holder.held) text = replace(text, U32(1, placeholder), digits);
    out = s8(text);
    if (config.canonical_unicode) out = tokenizer::normalize_unicode(out);
    if (config.plain_orthography) out = s8(plain_orthography(u(out)));
    return out;
}

}  // namespace arbtok::textnorm
