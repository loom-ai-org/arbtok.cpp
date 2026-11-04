#include "arbtok/textnorm.hpp"

#include <algorithm>
#include <unordered_map>

#include "detail.hpp"
#include "textnorm_detail.hpp"

namespace arbtok::textnorm {

using detail::s8;
using detail::U32;
using detail::u;

const std::set<std::string> ARABIC_LANGUAGE_CODES = {
    "arb", "aao", "abh", "abv", "acm", "acq", "acw", "acx", "acy", "adf", "aeb", "aec", "afb", "ajp", "apc",
    "apd", "arq", "ars", "ary", "arz", "auz", "avl", "ayh", "ayl", "ayn", "ayp", "bbz", "pga", "shu", "ssh",
};

namespace {

const std::set<std::string> kArabicLanguagesOutsideAra = {"mey"};

// A dict: a later value for a key replaces the earlier one in its place.
class Dict {
public:
    void set(const std::string& key, const std::string& value) {
        const auto it = index_.find(key);
        if (it == index_.end()) {
            index_.emplace(key, items_.size());
            items_.emplace_back(key, value);
        } else {
            items_[it->second].second = value;
        }
    }
    void setdefault(const std::string& key, const std::string& value) {
        if (!index_.count(key)) set(key, value);
    }
    bool has(const std::string& key) const { return index_.count(key) != 0; }
    const std::string& at(const std::string& key) const {
        const auto it = index_.find(key);
        if (it == index_.end()) throw KeyError(detail::repr(u(key)));
        return items_[it->second].second;
    }
    const Pairs& items() const { return items_; }

private:
    Pairs items_;
    std::unordered_map<std::string, size_t> index_;
};

std::string tuple_repr(const std::pair<std::string, std::string>& entry) {
    return "(" + detail::repr(u(entry.first)) + ", " + detail::repr(u(entry.second)) + ")";
}

Lexicon as_lexicon(const Pairs& entries) {
    Lexicon out(entries.begin(), entries.end());
    std::sort(out.begin(), out.end());
    for (const auto& entry : out)
        if (detail::strip(u(entry.first)).empty())
            throw ValueError("a lexicon entry is a non-empty string and the string it stands for, got " +
                             tuple_repr(entry));
    return out;
}

constexpr const char* kNumberParser = "; ovos-number-parser 0.24.3a1";
constexpr const char* kDateParser = "; ovos-date-parser 0.35.7a1";

std::string first_subtag(const std::string& lang) { return lang.substr(0, lang.find('-')); }

const std::string& cell_or_throw(const detail::Row& row, const char* column) {
    const std::string* value = detail::cell(row, column);
    if (value == nullptr) throw KeyError(detail::repr(u(std::string(column))));
    return *value;
}

std::vector<std::string> key_parts(const std::string& key) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t at = key.find('/', start);
        if (at == std::string::npos) break;
        out.push_back(key.substr(start, at - start));
        start = at + 1;
    }
    out.push_back(key.substr(start));
    return out;
}

const std::string& part(const std::vector<std::string>& parts, size_t i) {
    if (i >= parts.size()) throw IndexError("list index out of range");
    return parts[i];
}

bool speakable(const std::string& text) { return detail::re(py::SPEAKABLE).fullmatches(u(text)); }

// _cldr_rows(name)
std::vector<const detail::Row*> cldr_rows(const std::string& name) {
    std::vector<const detail::Row*> rows;
    for (const auto& r : detail::term_rows(name)) {
        const std::string& key = cell_or_throw(r, "cldr_key");
        if (!speakable(cell_or_throw(r, "arabic_form"))) continue;
        const std::string last = key_parts(key).back();
        if (detail::in(u(last), py::NOT_A_PLACE)) continue;
        if (key.rfind("units/", 0) == 0 && key.rfind("units/long/", 0) != 0) continue;
        rows.push_back(&r);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const detail::Row* a, const detail::Row* b) {
        const U32 ka = u(*detail::cell(*a, "cldr_key")), kb = u(*detail::cell(*b, "cldr_key"));
        return ka.size() != kb.size() ? ka.size() < kb.size() : ka < kb;
    });
    return rows;
}

}  // namespace

bool is_arabic_lang(const std::string& lang) {
    const U32 folded = detail::replace(detail::lower(u(lang)), U"_", U"-");
    const std::string language = s8(detail::split(folded, U"-").front());
    return language == "ar" || ARABIC_LANGUAGE_CODES.count(language) || kArabicLanguagesOutsideAra.count(language);
}

// ── AsrNorm ─────────────────────────────────────────────────────────────────

const std::vector<AsrNorm::Field>& AsrNorm::fields() {
    static const std::vector<Field> f = {
        {"nfc", &AsrNorm::nfc},
        {"strip_event_markup", &AsrNorm::strip_event_markup},
        {"strip_controls", &AsrNorm::strip_controls},
        {"punctuation_before_marks", &AsrNorm::punctuation_before_marks},
        {"strip_harakat", &AsrNorm::strip_harakat},
        {"strip_extended_marks", &AsrNorm::strip_extended_marks},
        {"strip_quranic_marks", &AsrNorm::strip_quranic_marks},
        {"strip_tatweel", &AsrNorm::strip_tatweel},
        {"split_term_proclitics", &AsrNorm::split_term_proclitics},
        {"fix_asr_errors", &AsrNorm::fix_asr_errors},
        {"unify_spoken_variants", &AsrNorm::unify_spoken_variants},
        {"unify_dialect_spellings", &AsrNorm::unify_dialect_spellings},
        {"number_forms", &AsrNorm::number_forms},
        {"spoken_numbers_to_digits", &AsrNorm::spoken_numbers_to_digits},
        {"join_dictated_digits", &AsrNorm::join_dictated_digits},
        {"spell_codes", &AsrNorm::spell_codes},
        {"unify_alef", &AsrNorm::unify_alef},
        {"unify_ta_marbuta", &AsrNorm::unify_ta_marbuta},
        {"unify_alef_maqsura", &AsrNorm::unify_alef_maqsura},
        {"unify_hamza_carriers", &AsrNorm::unify_hamza_carriers},
        {"unify_loan_letters", &AsrNorm::unify_loan_letters},
        {"fold_digits", &AsrNorm::fold_digits},
        {"fold_case", &AsrNorm::fold_case},
        {"arabic_block_only", &AsrNorm::arabic_block_only},
        {"blank_punctuation", &AsrNorm::blank_punctuation},
        {"drop_word_final_hamza", &AsrNorm::drop_word_final_hamza},
        {"join_clitics", &AsrNorm::join_clitics},
        {"collapse_whitespace", &AsrNorm::collapse_whitespace},
    };
    return f;
}

bool operator==(const AsrNorm& a, const AsrNorm& b) {
    for (const auto& f : AsrNorm::fields())
        if (a.*f.member != b.*f.member) return false;
    return a.lexicon == b.lexicon;
}

AsrNorm AsrNorm::with_lexicon(const Pairs& entries) const {
    AsrNorm out = *this;
    out.lexicon = as_lexicon(entries);
    return out;
}

namespace {
std::string describe_lexicon(const Lexicon& entries) {
    if (entries.empty()) return "";
    std::string body;
    for (size_t i = 0; i < entries.size(); ++i) body += (i ? "\n" : "") + entries[i].first + "\t" + entries[i].second;
    return "; lexicon " + std::to_string(entries.size()) + " entries sha256:" + detail::sha256_hex(body, 16);
}
}  // namespace

std::string AsrNorm::describe() const {
    std::string on;
    for (const auto& f : fields())
        if (this->*f.member) on += (on.empty() ? "" : ",") + std::string(f.name);
    std::string out = "arbtok-asr-norm " + ASR_NORM_VERSION + ": " + (on.empty() ? "none" : on);
    out += describe_lexicon(lexicon);
    if (spoken_numbers_to_digits) out += kDateParser;
    if (spoken_numbers_to_digits || fix_asr_errors) out += kNumberParser;
    return out;
}

// ── Bundles ─────────────────────────────────────────────────────────────────

namespace {
AsrNorm all_marks(AsrNorm c) {
    c.strip_harakat = c.strip_extended_marks = c.strip_quranic_marks = c.strip_tatweel = true;
    return c;
}
AsrNorm all_letter_forms(AsrNorm c) {
    c.unify_alef = c.unify_ta_marbuta = c.unify_alef_maqsura = c.unify_hamza_carriers = true;
    return c;
}
}  // namespace

const AsrNorm TRUTH_CHECK = [] {
    AsrNorm c;
    c.nfc = c.collapse_whitespace = true;
    return all_letter_forms(all_marks(c));
}();

const AsrNorm CER_STRIP = [] {
    AsrNorm c;
    c.blank_punctuation = c.punctuation_before_marks = c.strip_harakat = c.strip_tatweel = c.collapse_whitespace =
        true;
    return c;
}();

const AsrNorm CER_NORM = [] {
    AsrNorm c = CER_STRIP;
    c.drop_word_final_hamza = c.unify_spoken_variants = true;
    return all_letter_forms(c);
}();

const AsrNorm CER_MARKS_FIRST = [] {
    AsrNorm c;
    c.strip_event_markup = c.blank_punctuation = c.collapse_whitespace = true;
    return all_marks(c);
}();

const AsrNorm CER_NORM_MARKS_FIRST = [] {
    AsrNorm c = CER_MARKS_FIRST;
    c.drop_word_final_hamza = c.unify_spoken_variants = true;
    return all_letter_forms(c);
}();

const AsrNorm INTELLIGIBILITY_GATE = [] {
    AsrNorm c;
    c.strip_harakat = c.strip_extended_marks = c.strip_tatweel = c.arabic_block_only = c.collapse_whitespace = true;
    return all_letter_forms(c);
}();

const AsrNorm& CER_CANONICAL() {
    static const AsrNorm c = [] {
        AsrNorm out = CER_NORM_MARKS_FIRST;
        out.split_term_proclitics = out.fix_asr_errors = out.unify_dialect_spellings = out.number_forms =
            out.spoken_numbers_to_digits = out.join_dictated_digits = out.spell_codes = out.unify_loan_letters =
                out.fold_case = out.join_clitics = true;
        return out.with_lexicon(canonical_term_lexicon());
    }();
    return c;
}

const std::vector<std::string> IDENTIFIER_WORDS = [] {
    std::vector<std::string> out;
    for (const auto& w : py::IDENTIFIER_WORDS) out.push_back(s8(w));
    return out;
}();

const std::string ASR_NORM_VERSION = s8(py::ASR_NORM_VERSION);

// ── Bundled tables ──────────────────────────────────────────────────────────

Pairs spelled_codes(const std::string& lang) {
    const std::string stem = first_subtag(lang);
    const std::string path = arbtok::detail::data_path("tts_lexicons/en-codes." + stem + ".tsv");
    if (stem.find('/') != std::string::npos || !detail::file_exists(path))
        throw ValueError("no bundled code spellings for " + detail::repr(u(lang)) +
                         "; put the terms in the config's lexicon");
    Dict out;
    for (const auto& line : detail::splitlines(arbtok::detail::read_file(path))) {
        if (line.empty() || line[0] == '#') continue;
        const auto cells = detail::split_tab(line);
        if (cells.size() != 3)
            throw ValueError(cells.size() < 3 ? "not enough values to unpack (expected 3, got " +
                                                    std::to_string(cells.size()) + ")"
                                              : "too many values to unpack (expected 3)");
        out.set(cells[0], cells[2]);
    }
    return out.items();
}

Pairs cldr_units(const std::string& lang) {
    if (first_subtag(lang) != "ar") throw ValueError("no bundled CLDR unit names for " + detail::repr(u(lang)));
    static const Pairs symbols = [] {
        static const std::string kUnits = "units-cldr";
        const auto& rows = detail::term_rows(kUnits);
        Dict names;
        for (const auto& r : rows) {
            const std::string& key = cell_or_throw(r, "cldr_key");
            if (key.rfind("units/long/", 0) == 0 && speakable(cell_or_throw(r, "arabic_form")))
                names.set(part(key_parts(key), 2), cell_or_throw(r, "arabic_form"));
        }
        Dict out;
        for (const auto& r : rows) {
            const auto parts = key_parts(cell_or_throw(r, "cldr_key"));
            const std::string& width = part(parts, 1);
            const std::string& unit = part(parts, 2);
            const std::string& symbol = cell_or_throw(r, "latin");
            const U32 s = u(symbol);
            if (s.size() == 1 && s[0] < 0x80 && detail::isalpha(s[0]) && symbol != "m") continue;
            if (s == U"%" || s == U"٪") continue;
            const bool digit = std::any_of(s.begin(), s.end(), [](char32_t c) { return detail::isdigit(c); });
            if (width != "long" && names.has(unit) && !digit) out.setdefault(symbol, names.at(unit));
        }
        return out.items();
    }();
    return symbols;
}

namespace {
bool ends_cldr(const std::string& name) {
    return name.size() >= 5 && name.compare(name.size() - 5, 5, "-cldr") == 0;
}
}  // namespace

Pairs bundled_asr_lexicon(const std::string& name) {
    if (ends_cldr(name))
        throw ValueError(detail::repr(u(name)) +
                         " holds Arabic names of things, not spellings of Latin-script terms: read backwards it "
                         "would translate Arabic words into English");
    Dict out;
    for (const auto& row : detail::term_rows(name)) out.set(cell_or_throw(row, "arabic_form"), cell_or_throw(row, "latin"));
    return out.items();
}

Pairs bundled_tts_lexicon(const std::string& name, bool pointed) {
    if (ends_cldr(name)) {
        Dict lexicon;
        for (const auto* row : cldr_rows(name)) {
            lexicon.setdefault(cell_or_throw(*row, "latin"), cell_or_throw(*row, "arabic_form"));
            const std::string& key = cell_or_throw(*row, "cldr_key");
            if (key.rfind("numbers/currencies/", 0) == 0)
                lexicon.setdefault(part(key_parts(key), 2), cell_or_throw(*row, "arabic_form"));
        }
        return lexicon.items();
    }
    std::vector<std::string> order;
    std::unordered_map<std::string, std::pair<size_t, std::string>> chosen;
    for (const auto& row : detail::term_rows(name)) {
        size_t rank = 0;
        if (const std::string* kind = detail::cell(row, "form_kind")) {
            const U32 k = u(*kind);
            const auto* begin = std::begin(py::PUBLISHED_BY);
            const auto* end = std::end(py::PUBLISHED_BY);
            const auto* hit = std::find(begin, end, std::u32string_view(k));
            if (hit == end) throw ValueError("tuple.index(x): x not in tuple");
            rank = static_cast<size_t>(hit - begin);
        }
        const std::string* p = detail::cell(row, "pointed");
        const std::string& spelling = pointed && p != nullptr ? *p : cell_or_throw(row, "arabic_form");
        const std::string& latin = cell_or_throw(row, "latin");
        const auto it = chosen.find(latin);
        if (it == chosen.end()) {
            order.push_back(latin);
            chosen.emplace(latin, std::make_pair(rank, spelling));
        } else if (rank < it->second.first) {
            it->second = {rank, spelling};
        }
    }
    Pairs out;
    for (const auto& latin : order) out.emplace_back(latin, chosen.at(latin).second);
    return out;
}

Pairs canonical_term_lexicon(const std::vector<std::string>& names) {
    Dict lexicon;
    for (const auto& name : names) {
        Dict said;
        for (const auto& [k, v] : bundled_tts_lexicon(name, false)) said.set(k, v);
        for (const auto& [spelling, term] : bundled_asr_lexicon(name)) {
            lexicon.setdefault(spelling, said.at(term));
            lexicon.setdefault(s8(detail::replace(u(spelling), U" ", U"")), said.at(term));
        }
        for (const auto& [term, spelling] : said.items()) lexicon.setdefault(term, spelling);
    }
    return lexicon.items();
}

}  // namespace arbtok::textnorm
