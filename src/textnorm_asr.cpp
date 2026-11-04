// normalize_asr and the rules it runs, in the order of arbtok 0.0.2a71.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>

#include "arbtok/textnorm.hpp"
#include "textnorm_detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::textnorm {

namespace {

using namespace detail;
using std::nullopt;
using std::optional;
using Words = std::vector<U32>;

// ── Small pieces of Python ──────────────────────────────────────────────────

// " ".join(words[a:b]) with Python's slice bounds.
U32 joined(const Words& words, long a, long b) {
    const long n = static_cast<long>(words.size());
    a = std::clamp(a, 0L, n);
    b = std::clamp(b, a, n);
    U32 out;
    for (long i = a; i < b; ++i) {
        if (i > a) out += U' ';
        out += words[static_cast<size_t>(i)];
    }
    return out;
}

Words without(const Words& words, size_t k) {
    Words out;
    for (size_t i = 0; i < words.size(); ++i)
        if (i != k) out.push_back(words[i]);
    return out;
}

Words sub_words(const Words& words, long a, long b) {
    const long n = static_cast<long>(words.size());
    a = std::clamp(a, 0L, n);
    b = std::clamp(b, a, n);
    return Words(words.begin() + a, words.begin() + b);
}

// a == b for two values that may be None.
bool same(const optional<Number>& a, const optional<Number>& b) {
    if (!a || !b) return !a && !b;
    return numbers::eq(*a, *b);
}

bool between(const Number& v, long long lo, long long hi) { return cmp(v, lo) >= 0 && cmp(v, hi) <= 0; }

bool same_as(const optional<Number>& a, long long b) { return a && cmp(*a, b) == 0; }

U32 str_of(const BigInt& n) { return u(n.str()); }

bool digit_at_end(U32View word) { return !word.empty() && isdigit(word.back()); }

const Table& clock_fold_table() {
    static const Table t = table(py::CLOCK_FOLD);
    return t;
}

const Table& clock_marks_table() {
    static const Table t = table(py::CLOCK_MARKS);
    return t;
}

U32 folded(U32View word) { return translate(word, clock_fold_table()); }

template <size_t N>
const long long* minute_word(U32View word, const std::pair<std::u32string_view, long long> (&dict)[N]) {
    return lookup(word, dict);
}

// ── The number parser, cached ───────────────────────────────────────────────

optional<Number> number_value(U32View words) {
    thread_local std::unordered_map<U32, optional<Number>> cache;
    const U32 key(words);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    auto value = extract_number(words, "ar");
    if (cache.size() > (1u << 16)) cache.clear();
    return cache.emplace(key, std::move(value)).first->second;
}

optional<Number> hour_value(U32View words) {
    thread_local std::unordered_map<U32, optional<Number>> cache;
    const U32 key(words);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    auto value = extract_number(words, "ar", true);
    if (cache.size() > (1u << 12)) cache.clear();
    return cache.emplace(key, std::move(value)).first->second;
}

// ── Lexicon ─────────────────────────────────────────────────────────────────

AsrNorm character_rules(const AsrNorm& config) {
    AsrNorm rules = config;
    rules.spoken_numbers_to_digits = rules.strip_event_markup = false;
    rules.collapse_whitespace = true;
    rules.lexicon.clear();
    rules.split_term_proclitics = rules.number_forms = rules.join_dictated_digits = rules.spell_codes =
        rules.join_clitics = false;
    return rules;
}

std::string flag_key(const AsrNorm& config) {
    std::string key;
    for (const auto& f : AsrNorm::fields()) key += config.*f.member ? '1' : '0';
    return key;
}

U32 word_form(const AsrNorm& rules, const std::string& rules_key, U32View word) {
    thread_local std::unordered_map<std::string, U32> cache;
    std::string key = rules_key + '\x1f' + s8(word);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    U32 form = u(normalize_asr(s8(word), rules));
    if (cache.size() > (1u << 16)) cache.clear();
    return cache.emplace(std::move(key), std::move(form)).first->second;
}

struct LexiconIndex {
    std::map<Words, U32> index;
    size_t longest = 0;
};

const LexiconIndex& lexicon_index(const AsrNorm& config) {
    struct Entry {
        std::string flags;
        Lexicon lexicon;
        LexiconIndex value;
    };
    thread_local std::vector<Entry> cache;
    const std::string flags = flag_key(config);
    for (const auto& e : cache)
        if (e.flags == flags && e.lexicon == config.lexicon) return e.value;
    const AsrNorm rules = character_rules(config);
    LexiconIndex out;
    for (const auto& [spelling, term] : config.lexicon) {
        Words words = split_ws(u(normalize_asr(spelling, rules)));
        if (!words.empty()) out.index[words] = u(term);
    }
    for (const auto& kv : out.index) out.longest = std::max(out.longest, kv.first.size());
    if (cache.size() >= 8) cache.erase(cache.begin());
    cache.push_back({flags, config.lexicon, std::move(out)});
    return cache.back().value;
}

U32 apply_lexicon(U32View text, const AsrNorm& config) {
    const LexiconIndex& lex = lexicon_index(config);
    const Words parts = words_and_gaps(text);
    const AsrNorm rules = character_rules(config);
    const std::string rules_key = flag_key(rules);
    std::vector<Edges> edge;
    Words forms;
    for (size_t i = 0; i < parts.size(); i += 2) {
        edge.push_back(edges(parts[i]));
        forms.push_back(word_form(rules, rules_key, edge.back().core));
    }
    const size_t n_edges = edge.size();

    auto match = [&](size_t i, const U32& first) -> optional<std::pair<size_t, U32>> {
        for (size_t n = std::min(lex.longest, n_edges - i); n > 0; --n) {
            bool inside_clean = true;
            for (size_t j = i; j + 1 < i + n; ++j)
                if (!edge[j].trail.empty()) inside_clean = false;
            for (size_t j = i + 1; j < i + n; ++j)
                if (!edge[j].lead.empty()) inside_clean = false;
            Words words = {first};
            for (size_t j = i + 1; j < i + n; ++j) words.push_back(forms[j]);
            const bool all = std::all_of(words.begin(), words.end(), [](const U32& w) { return !w.empty(); });
            if (!inside_clean || !all) continue;
            const auto it = lex.index.find(words);
            if (it != lex.index.end()) return std::make_pair(n, it->second);
        }
        return nullopt;
    };

    const Pattern& proclitic_re = pyre::compiled(py::TERM_PROCLITIC);
    U32 out;
    size_t i = 0;
    while (i < n_edges) {
        auto found = match(i, forms[i]);
        U32 proclitic;
        if (!found && config.split_term_proclitics && !forms[i].empty()) {
            for (long k = static_cast<long>(forms[i].size()) - 2; k > 0; --k) {
                if (proclitic_re.fullmatches(U32View(forms[i]).substr(0, static_cast<size_t>(k)))) {
                    found = match(i, forms[i].substr(static_cast<size_t>(k)));
                    if (found) {
                        proclitic = forms[i].substr(0, static_cast<size_t>(k)) + U" ";
                        break;
                    }
                }
            }
        }
        if (found) {
            const auto& [n, term] = *found;
            out += edge[i].lead + proclitic + term + edge[i + n - 1].trail;
            if (2 * (i + n) - 1 < parts.size()) out += parts[2 * (i + n) - 1];
            i += n;
        } else {
            out += parts[2 * i];
            if (2 * i + 1 < parts.size()) out += parts[2 * i + 1];
            i += 1;
        }
    }
    return out;
}

// ── Numbers, as the parser reads them ───────────────────────────────────────

U32 numbers_to_digits_kept(U32View text, const std::string& lang) {
    const Words words = split_ws(text);
    if (words.empty()) return U32(text);
    const U32 read = numbers_to_digits(join(words, U" "), lang);
    if (split_ws(read) == words) return U32(text);
    const size_t lead = text.size() - lstrip(text).size();
    const size_t kept = rstrip(text).size();
    return U32(text.substr(0, lead)) + read + U32(text.substr(kept));
}

bool unit_word(U32View w) { return in(w, py::HUNDRED_UNITS); }

U32 maya_as_hundred(U32View text) {
    Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(parts[i]);
    const long n = static_cast<long>(words.size());

    auto value = [&](long k) -> optional<Number> {
        if (0 <= k && k < n && !words[static_cast<size_t>(k)].empty()) return number_value(words[static_cast<size_t>(k)]);
        return nullopt;
    };
    auto thousands = [&](long k) {
        const auto v = value(k);
        return v && cmp(*v, 1000) >= 0;
    };
    auto word_is = [&](long k, std::u32string_view w) { return 0 <= k && k < n && words[static_cast<size_t>(k)] == w; };

    for (long i = 0; i < n; ++i) {
        const Edges e = edges(words[static_cast<size_t>(i)]);
        optional<std::u32string_view> spelling;
        for (const auto& s : py::MAYA_SPELLINGS)
            if (ends(e.core, s)) {
                spelling = s;
                break;
            }
        if (!spelling) continue;
        const U32 head = e.core.substr(0, e.core.size() - spelling->size());
        const bool after_number = thousands(i - 1) || (i > 0 && word_is(i - 1, U"و") && thousands(i - 2));
        bool inside;
        if (head.empty()) {
            inside = (i > 0 && unit_word(words[static_cast<size_t>(i - 1)])) || same_as(value(i + 1), 1000) ||
                     (i > 0 && word_is(i - 1, U"و") && thousands(i - 2));
        } else if (head == U"و") {
            inside = after_number;
        } else if (head[0] == U'و' && unit_word(U32View(head).substr(1))) {
            inside = after_number;
        } else {
            inside = unit_word(head) || (first_in(head, U"بلف") && unit_word(U32View(head).substr(1)));
        }
        if (inside) parts[2 * static_cast<size_t>(i)] = e.lead + head + U32(py::HUNDRED) + e.trail;
    }
    return join(parts, U"");
}

bool hundreds_value(const optional<Number>& v) {
    if (!v || !between(*v, 100, 900)) return false;
    if (const auto* d = std::get_if<double>(&*v)) return std::fmod(*d, 100.0) == 0;
    return ll_of(*v) % 100 == 0;
}

bool counts_value(const optional<Number>& v) { return v && between(*v, 1, 99) && integral(*v); }

U32 or_as_and(U32View text) {
    Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(parts[i]);
    const long n = static_cast<long>(words.size());

    auto word = [&](long k) -> U32 { return 0 <= k && k < n ? words[static_cast<size_t>(k)] : U32(); };
    auto value = [&](long k) -> optional<Number> {
        const U32 w = word(k);
        if (w.empty()) return nullopt;
        return number_value(w);
    };
    auto waw = [&](long k) {
        const U32 w = word(k);
        return !w.empty() && w[0] == U'و' && !in(w, py::WAW_INITIAL_NUMBERS) &&
               number_value(U32View(w).substr(1)).has_value();
    };
    auto joins = [&](long i) {
        const auto unit = value(i - 2);
        long start;
        if (same_as(value(i - 1), 100) && counts_value(unit) && cmp(*unit, 9) <= 0 &&
            same(number_value(word(i - 2) + U" " + word(i - 1)), numbers::mul(*unit, Number(BigInt(100))))) {
            start = i - 2;
        } else if (hundreds_value(value(i - 1))) {
            start = i - 1;
        } else {
            return false;
        }
        long k = start - 1;
        if (!waw(start) && word(k) == U"و") k -= 1;
        const auto earlier = value(k);
        if (earlier && cmp(*earlier, 1000000) < 0) return false;
        if (!counts_value(value(i + 1)) || waw(i + 1)) return false;
        for (long m = i + 2; m < n; ++m) {
            if (same_as(value(m), 1000)) return true;
            if (word(m) != U"و" && !counts_value(value(m))) return false;
        }
        return false;
    };

    static const Table alef = table(py::ALEF_TABLE);
    for (long i = 1; i < n - 1; ++i) {
        if (translate(words[static_cast<size_t>(i)], alef) == py::OR && joins(i)) {
            const size_t p = 2 * static_cast<size_t>(i);
            parts[p] = parts[p + 1] = U"";
            parts[p + 2] = U"و" + parts[p + 2];
        }
    }
    return join(parts, U"");
}

U32 spoken_variants(U32View text) {
    return re(py::FAREWELL_SPELLINGS).sub(U"\\1" + U32(py::FAREWELL), text);
}

// ── Clock times ─────────────────────────────────────────────────────────────

bool arabic_word(U32View w) { return re(py::ARABIC_WORD).fullmatches(w); }

optional<long long> night_hour(long long hour) {
    if (8 <= hour && hour <= 11) return hour + 12;
    if (hour == 12) return 0;
    if (1 <= hour && hour <= 4) return hour;
    return nullopt;
}

optional<long long> period_hour(long long hour, U32View word, bool evening = false) {
    thread_local std::map<std::tuple<long long, U32, bool>, optional<long long>> cache;
    const auto key = std::make_tuple(hour, U32(word), evening);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    optional<long long> out;
    const U32 f = folded(word);
    if (in(f, py::NIGHT_WORDS)) {
        out = night_hour(hour);
    } else if (in(f, py::MORNING_WORDS)) {
        if (1 <= hour && hour <= 11) out = hour;
    } else if (in(f, py::EVENING_WORDS)) {
        if (evening && 5 <= hour && hour <= 7) out = hour + 12;
    } else {
        const auto read = extract_datetime(U"الساعة " + itos(hour) + U" " + U32(word), "ar");
        if (read && strip(u(read->remainder)).empty()) out = read->moment.hour;
    }
    if (cache.size() > (1u << 12)) cache.clear();
    cache.emplace(key, out);
    return out;
}

bool is_day(U32View word) {
    thread_local std::unordered_map<U32, bool> cache;
    const U32 key(word);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    const auto read = extract_datetime(word, "ar");
    const bool out = read && strip(u(read->remainder)).empty();
    if (cache.size() > (1u << 12)) cache.clear();
    cache.emplace(key, out);
    return out;
}

bool ends_time(const Words& keys, size_t end) {
    return end == keys.size() || in(folded(keys[end]), py::TIME_ENDS) || (arabic_word(keys[end]) && is_day(keys[end]));
}

bool is_day_period(U32View word, bool evening = false) {
    if (!arabic_word(word)) return false;
    const U32 f = folded(word);
    if (in(f, py::NIGHT_WORDS) || in(f, py::MORNING_WORDS)) return true;
    if (evening && in(f, py::EVENING_WORDS)) return true;
    const auto one = period_hour(1, word);
    if (one && *one != 1) return true;
    const auto twelve = period_hour(12, word);
    return twelve && *twelve != 12;
}

using Said = std::pair<long long, size_t>;

optional<Said> minute_number(const Words& keys, size_t j) {
    Words words;
    for (size_t k = j; k < std::min(j + 4, keys.size()); ++k) {
        if (!arabic_word(keys[k])) break;
        words.push_back(keys[k]);
    }
    if (!words.empty() && !words[0].empty() && words[0][0] == U'و' && !in(words[0], py::WAW_INITIAL_NUMBERS))
        words[0] = words[0].substr(1);
    for (size_t n = words.size(); n > 0; --n) {
        const Words run(words.begin(), words.begin() + static_cast<long>(n));
        const auto read = number_value(joined(run, 0, static_cast<long>(n)));
        if (!read || !integral(*read) || !between(*read, 1, 59)) continue;
        bool every = true;
        for (size_t k = 0; k < n && every; ++k) {
            const Words rest = without(run, k);
            if (same(number_value(joined(rest, 0, static_cast<long>(rest.size()))), read)) every = false;
        }
        if (every) return Said{ll_of(*read), j + n};
    }
    return nullopt;
}

bool one_number(U32View words) {
    thread_local std::unordered_map<U32, bool> cache;
    const U32 key(words);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    const U32 read = numbers_to_digits(words, "ar");
    const bool out = isascii(read) && isdigit(read);
    if (cache.size() > (1u << 12)) cache.clear();
    cache.emplace(key, out);
    return out;
}

U32 indefinite_minute(const U32& word) {
    const U32 bare = slice_from(word, 2);
    return starts(word, U"ال") && minute_word(bare, py::MINUTE_WORDS) ? bare : word;
}

struct Minutes {
    long long minutes;
    bool back;
    size_t end;
};

optional<Minutes> minutes_before(const Words& keys, size_t j) {
    if (j >= keys.size()) return nullopt;
    const U32 word = folded(keys[j]);
    long long minutes;
    size_t end;
    const long long* mw = minute_word(word, py::MINUTE_WORDS);
    if (const long long* fraction = lookup(word, py::HOUR_FRACTIONS)) {
        minutes = *fraction;
        end = j + 1;
    } else if (mw && *mw > 0) {
        minutes = *mw;
        end = j + 1;
    } else {
        const auto said = minute_number(keys, j);
        if (!said) return nullopt;
        std::tie(minutes, end) = *said;
        if (end < keys.size() && minute_word(folded(keys[end]), py::MINUTE_WORDS)) end += 1;
    }
    if (minutes < 30) return Minutes{minutes, true, end};
    return nullopt;
}

optional<Minutes> read_minutes(const Words& keys, size_t j, optional<size_t> cardinal_hour) {
    if (j >= keys.size()) return nullopt;
    if (folded(keys[j]) == py::EXCEPT) return minutes_before(keys, j + 1);
    U32 word = folded(keys[j]);
    size_t k = j;
    if (word == U"و" && j + 1 < keys.size()) {
        word = folded(keys[j + 1]);
        k = j + 1;
    } else if (!word.empty() && word[0] == U'و') {
        word = word.substr(1);
    } else if (!minute_word(indefinite_minute(word), py::MINUTE_WORDS)) {
        return nullopt;
    }
    const bool definite = indefinite_minute(word) != word;
    word = indefinite_minute(word);
    const U32 bare = starts(word, U"ال") ? slice_from(word, 2) : word;
    if (const long long* fraction = lookup(bare, py::HOUR_FRACTIONS)) return Minutes{*fraction, false, k + 1};
    if (const long long* mw = minute_word(word, py::MINUTE_WORDS)) {
        if (const auto said = minute_number(keys, k + 1)) return Minutes{said->first, false, said->second};
        if (definite && *mw == 1) return nullopt;
        if (*mw > 0) return Minutes{*mw, false, k + 1};
        return nullopt;
    }
    const auto said = minute_number(keys, k);
    if (!said) return nullopt;
    auto [minutes, end] = *said;
    if (cardinal_hour && one_number(joined(keys, static_cast<long>(*cardinal_hour), static_cast<long>(end))))
        return nullopt;
    if (end < keys.size() && minute_word(folded(keys[end]), py::MINUTE_WORDS)) end += 1;
    return Minutes{minutes, false, end};
}

bool every_word_counts(const Words& span, const optional<Number>& value) {
    if (span.size() == 1) return true;
    for (size_t k = 0; k < span.size(); ++k) {
        const Words rest = without(span, k);
        if (same(hour_value(joined(rest, 0, static_cast<long>(rest.size()))), value)) return false;
    }
    return true;
}

U32 period_spelling(const U32& word) {
    if (const auto* s = lookup(folded(word), py::PERIOD_SPELLINGS)) return U32(*s);
    return word;
}

struct Clock {
    U32 time;
    size_t end;
    U32 residue;
};

optional<Clock> read_clock(Words keys, size_t i, bool tolerant) {
    const size_t j = i + 1;
    if (j >= keys.size() || !arabic_word(keys[j])) return nullopt;
    size_t words = 1;
    if (j + 1 < keys.size() && in(folded(keys[j + 1]), py::HOUR_TENS)) {
        const Words span = sub_words(keys, static_cast<long>(j), static_cast<long>(j + 2));
        if (every_word_counts(span, hour_value(joined(keys, static_cast<long>(j), static_cast<long>(j + 2)))))
            words = 2;
    }
    const auto read = hour_value(joined(keys, static_cast<long>(j), static_cast<long>(j + words)));
    if (!read || !integral(*read) || !between(*read, 0, 23)) return nullopt;
    long long hour = ll_of(*read);
    if (tolerant)
        for (auto& k : keys) k = period_spelling(k);
    const bool cardinal = !starts(keys[j], U"ال") &&
                          same_as(number_value(joined(keys, static_cast<long>(j), static_cast<long>(j + words))), hour);
    size_t end = j + words;
    optional<U32> period;
    optional<long long> minutes;
    U32 residue;
    if (end < keys.size() && is_day_period(keys[end], tolerant)) {
        period = keys[end];
        end += 1;
    }
    const optional<size_t> cardinal_at = cardinal ? optional<size_t>(j) : nullopt;
    auto said = read_minutes(keys, end, cardinal_at);
    const bool filler = tolerant && end + 1 < keys.size() && arabic_word(keys[end]) &&
                        !number_value(keys[end]).has_value() &&
                        minute_word(indefinite_minute(lstrip_chars(folded(keys[end + 1]), U"و")), py::MINUTE_WORDS);
    if (!said && filler) {
        said = read_minutes(keys, end + 1, cardinal_at);
        if (said) residue = keys[end];
    }
    bool back = false;
    if (said) {
        minutes = said->minutes;
        back = said->back;
        end = said->end;
        if (!period && end < keys.size() && is_day_period(keys[end], tolerant)) {
            period = keys[end];
            end += 1;
        }
    }
    if (period && end < keys.size() && is_day_period(keys[end], tolerant)) return nullopt;
    if (!period && !minutes) {
        if (!cardinal && ends_time(keys, end)) return Clock{itos(hour), end, U""};
        return nullopt;
    }
    if (period && !(1 <= hour && hour <= 12)) return nullopt;
    long long m = minutes.value_or(0);
    if (said && back) {
        hour -= 1;
        m = 60 - m;
        if (hour == 0) {
            hour = 12;
        } else if (hour < 0) {
            hour = 23;
        }
    }
    if (period && hour == 12 && folded(*period) == py::NIGHT) {
        hour = 0;
    } else if (period) {
        const auto placed = period_hour(hour, *period, tolerant);
        if (!placed) return nullopt;
        hour = *placed;
    }
    return Clock{itos(hour) + U":" + itos2(m), end, residue};
}

U32 clock_times(U32View text, bool tolerant) {
    Words parts = words_and_gaps(text);
    std::vector<Edges> edge;
    Words keys;
    for (size_t i = 0; i < parts.size(); i += 2) {
        edge.push_back(edges(parts[i]));
        keys.push_back(translate(edge.back().core, clock_marks_table()));
    }
    const Pattern& clock_word = re(py::CLOCK_WORD);
    size_t i = 0;
    while (i < keys.size()) {
        if (!clock_word.fullmatches(folded(keys[i]))) {
            i += 1;
            continue;
        }
        size_t k = i + 1;
        while (k < keys.size() && edge[k - 1].trail.empty() && edge[k].lead.empty()) k += 1;
        const size_t stop = std::min(k, i + static_cast<size_t>(py::CLOCK_WINDOW));
        const auto read = read_clock(Words(keys.begin() + static_cast<long>(i), keys.begin() + static_cast<long>(stop)), 0,
                                     tolerant);
        if (!read) {
            i += 1;
            continue;
        }
        const size_t end = read->end + i;
        parts[2 * i] = edge[i].lead + edge[i].core + U" " + read->time +
                       (read->residue.empty() ? U32() : U" " + read->residue) + edge[end - 1].trail;
        for (size_t p = 2 * i + 1; p + 1 < 2 * end; ++p) parts[p] = U"";
        i = end;
    }
    return join(parts, U"");
}

// ── Words mapped one by one ─────────────────────────────────────────────────

template <class F>
U32 map_words(U32View text, F read) {
    Words parts = words_and_gaps(text);
    for (size_t i = 0; i < parts.size(); i += 2) {
        if (parts[i].empty()) continue;
        const Edges e = edges(parts[i]);
        const optional<U32> fresh = e.core.empty() ? nullopt : read(e.core);
        if (fresh) parts[i] = e.lead + *fresh + e.trail;
    }
    return join(parts, U"");
}

U32 split_latin_proclitics(U32View text) {
    return re(py::LATIN_HYPHEN).sub(U" ", re(py::LATIN_PROCLITIC).sub(U"\\1 ", text));
}

optional<U32> dialect_spelling(const U32& word) {
    const U32 key = fold_key(word);
    if (const auto* s = lookup(key, py::DIALECT_SPELLINGS)) return U32(*s);
    if (!key.empty() && first_in(key, U"وف"))
        if (const auto* s = lookup(U32View(key).substr(1), py::DIALECT_SPELLINGS)) return key.substr(0, 1) + U32(*s);
    return nullopt;
}

optional<U32> number_spelling(const U32& word) {
    const U32 key = fold_key(word);
    if (const auto* s = lookup(key, py::NUMBER_SPELLINGS)) return U32(*s);
    if (!key.empty() && first_in(key, U"ولب") && in(U32View(key).substr(1), py::NUMBER_SPELLINGS_PROCLITIC))
        return key.substr(0, 1) + U32(*lookup(U32View(key).substr(1), py::NUMBER_SPELLINGS));
    return nullopt;
}

// ── Digits read one by one ──────────────────────────────────────────────────

U32 phone_groups(const Match& m) {
    const U32 digits = pyre::compiled(U"\\D").sub(U"", m.str(0));
    const U32 first = m.str(1);
    const bool one = first_in(first, U"+0") || pyre::compiled(U"\\d+").findall(m.str(2)).size() >= 2;
    return one && py::PHONE_LENGTHS.has(static_cast<long long>(digits.size())) ? digits : m.str(0);
}

U32 joined_digits(U32View text) {
    const Pattern& ws = re(py::WHITESPACE);
    U32 out = re(py::DICTATED_DIGITS).sub([&](const Match& m) { return ws.sub(U"", m.str(0)); }, text);
    out = re(py::DIGIT_GROUPS).sub(phone_groups, out);
    const Pattern& before = re(py::IDENTIFIER_BEFORE);
    out = re(py::SHORT_DICTATED).sub(
        [&](const Match& m) { return before.search(m.string(), 0, m.start()) ? ws.sub(U"", m.str(0)) : m.str(0); }, out);
    return re(py::PLATE_DIGITS).sub([](const Match& m) { return m.str(1) + replace(m.str(2), U" ", U""); }, out);
}

const std::map<Words, U32>& plate_letter_names() {
    static const std::map<Words, U32> names = [] {
        std::map<Words, U32> out;
        for (const auto& [letter, name] : py::LETTER_NAMES) {
            Words words = split_ws(fold_key(name));
            out.emplace(words, U32(letter));
            words.back() = rstrip_chars(words.back(), U"ء");
            out.emplace(words, U32(letter));
        }
        return out;
    }();
    return names;
}

U32 plate_letters(U32View text) {
    const auto& names = plate_letter_names();
    Words parts = words_and_gaps(text);
    Words keys;
    for (size_t i = 0; i < parts.size(); i += 2) keys.push_back(fold_key(parts[i]));

    auto name_at = [&](size_t i) -> optional<std::pair<size_t, U32>> {
        for (const size_t n : {2u, 1u}) {
            if (i + n > keys.size()) continue;
            const auto it = names.find(Words(keys.begin() + static_cast<long>(i), keys.begin() + static_cast<long>(i + n)));
            if (it != names.end()) return std::make_pair(n, it->second);
        }
        return nullopt;
    };

    size_t i = 0;
    while (i < keys.size()) {
        std::vector<std::tuple<size_t, size_t, U32>> read;
        size_t j = i;
        while (read.size() < 3 && j < keys.size()) {
            const auto found = name_at(j);
            if (!found) break;
            read.emplace_back(j, found->first, found->second);
            j += found->first;
        }
        const U32 after = j < keys.size() ? keys[j] : U32();
        if (read.size() == 3 && !after.empty() && (isdigit(after) || number_value(after).has_value())) {
            for (const auto& [start, n, letter] : read) {
                parts[2 * start] = letter;
                for (size_t p = 2 * start + 1; p + 1 < 2 * (start + n); ++p) parts[p] = U"";
            }
            i = j;
        } else {
            i += 1;
        }
    }
    return join(parts, U"");
}

const U32* formal_teen(U32View a, U32View b) {
    static const std::map<std::pair<U32, U32>, U32> teens = [] {
        std::map<std::pair<U32, U32>, U32> out;
        for (const auto& t : py::FORMAL_TEENS) out[{U32(t.a), U32(t.b)}] = U32(t.to);
        return out;
    }();
    const auto it = teens.find({U32(a), U32(b)});
    return it == teens.end() ? nullptr : &it->second;
}

U32 number_words(U32View text) {
    const U32 mapped = map_words(text, number_spelling);
    Words parts = words_and_gaps(mapped);
    Words keys;
    for (size_t i = 0; i < parts.size(); i += 2) keys.push_back(fold_key(parts[i]));
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == py::ONE_SAID && i > 0 && in(keys[i - 1], py::ONE_COUNTS)) parts[2 * i] = U"واحده";
        if (i + 1 < keys.size()) {
            const bool waw = !keys[i].empty() && keys[i][0] == U'و' &&
                             formal_teen(U32View(keys[i]).substr(1), keys[i + 1]) != nullptr;
            const U32 lead = waw ? U32(U"و") : U32();
            if (const U32* teen = formal_teen(U32View(keys[i]).substr(lead.size()), keys[i + 1])) {
                parts[2 * i] = lead + *teen;
                parts[2 * i + 1] = parts[2 * i + 2] = U"";
                keys[i + 1] = U"";
            }
        }
    }
    return join(parts, U"");
}

// ── Numbers written in digits ───────────────────────────────────────────────

long long month_of(U32View folded_name) {
    const long long* m = lookup(folded_name, py::MONTHS);
    if (m == nullptr) throw KeyError(repr(folded_name));
    return *m;
}

U32 named_date(const Match& m) {
    const long long day = to_int(m.str(1));
    const long long month = month_of(fold_key(m.str(2)));
    if (!(1 <= day && day <= 31)) return m.str(0);
    U32 out = itos(day) + U" " + U32(py::MONTH_NAMES[month - 1]);
    if (m.matched(3) && !m.str(3).empty()) out += U" " + m.str(3);
    return out;
}

U32 date_words(const Match& m, const U32& day_text, const U32& separator, const U32& month_text,
               const U32& year_text, const optional<U32>& era) {
    const long long day = to_int(day_text), month = to_int(month_text);
    const long long year = year_text.size() == 4 ? to_int(year_text) : 2000 + to_int(year_text);
    const bool hijri = (era && (*era == U"ه" || *era == U"هـ")) || (!era && py::HIJRI_YEARS.has(year));
    if (!(1 <= day && day <= (hijri ? 30 : 31) && 1 <= month && month <= 12) || (hijri && !py::HIJRI_YEARS.has(year)) ||
        (hijri && !era && separator == U"."))
        return m.str(0);
    const std::u32string_view name = hijri ? py::HIJRI_MONTH_NAMES[month - 1] : py::MONTH_NAMES[month - 1];
    U32 said;
    if (era && (*era == U"ه" || *era == U"هـ")) said = U" هجري";
    if (era && *era == U"م") said = U" ميلادي";
    return itos(day) + U" " + U32(name) + U" " + itos(year) + said;
}

U32 digit_date(const Match& m) { return date_words(m, m.str(1), m.str(2), m.str(3), m.str(4), m.group(5)); }
U32 iso_date(const Match& m) { return date_words(m, m.str(4), m.str(2), m.str(3), m.str(1), m.group(5)); }

struct UnitNames {
    Pattern pattern;
    std::map<U32, U32> names;
};

const UnitNames& unit_names() {
    static const UnitNames units = [] {
        std::vector<std::pair<U32, U32>> names;
        auto set = [&](const U32& k, const U32& v) {
            for (auto& kv : names)
                if (kv.first == k) {
                    kv.second = v;
                    return;
                }
            names.emplace_back(k, v);
        };
        const Pattern& latin = pyre::compiled(U"[A-Za-z]+");
        for (const auto& [symbol, name] : cldr_units("ar"))
            if (latin.fullmatches(u(symbol))) set(u(symbol), translate(u(name), clock_marks_table()));
        for (const auto& unit : py::CAPITALISED_UNITS) {
            U32 found;
            bool has = false;
            for (const auto& kv : names)
                if (kv.first == unit) {
                    found = kv.second;
                    has = true;
                }
            if (has) set(upper(unit), found);
        }
        std::vector<U32> escaped;
        for (const auto& kv : names) escaped.push_back(pyre::escape(kv.first));
        std::stable_sort(escaped.begin(), escaped.end(), [](const U32& a, const U32& b) { return a.size() > b.size(); });
        const U32 alternatives = join(escaped, U"|");
        const U32 format(py::UNIT_AFTER_NUMBER);
        const size_t at = format.find(U"{}");
        const U32 pattern = format.substr(0, at) + alternatives + format.substr(at + 2);
        return UnitNames{Pattern(U32View(pattern)), std::map<U32, U32>(names.begin(), names.end())};
    }();
    return units;
}

bool distance_before(U32View text) {
    const Words found = re(py::ARABIC_WORD).findall(text);
    for (size_t i = found.size() > 4 ? found.size() - 4 : 0; i < found.size(); ++i) {
        const U32 key = fold_key(found[i]);
        if (in(key, py::DISTANCE_WORDS) || (first_in(key, U"وفبل") && in(slice_from(key, 1), py::DISTANCE_WORDS)))
            return true;
    }
    return false;
}

U32 kilometres(U32View text) {
    const Pattern& per_hour = re(py::PER_HOUR);
    return re(py::KILOMETRE_SAID).sub(
        [&](const Match& m) {
            if (m.str(2) == U"كيلو" &&
                !(distance_before(text.substr(0, m.start())) || per_hour.match(text, m.end()).has_value()))
                return m.str(0);
            return m.str(1) + U" كيلومتر";
        },
        text);
}

optional<long long> placed_hour(long long hour, const U32& word) {
    if (!(1 <= hour && hour <= 12)) return nullopt;
    if (word == U"pm" || word == U"PM" || word == U"م") return hour % 12 + 12;
    if (word == U"am" || word == U"AM" || word == U"ص") return hour % 12;
    if (is_day_period(word, true)) {
        if (hour == 12 && folded(word) == py::NIGHT) return 0;
        return period_hour(hour, word, true);
    }
    return nullopt;
}

U32 day_month(const Match& m) {
    const long long day = to_int(m.str(1)), month = to_int(m.str(2));
    const Words before = split_ws(m.string().substr(0, m.start()));
    const bool dated = !before.empty() && in(fold_key(before.back()), py::DATE_WORDS);
    if (!(1 <= day && day <= 31 && 1 <= month && month <= 12 && (day >= 13 || dated))) return m.str(0);
    return itos(day) + U" " + U32(py::MONTH_NAMES[month - 1]);
}

U32 written_numbers(U32View input) {
    const Pattern& comma = pyre::compiled(U"[,٬]");
    U32 text = re(py::GROUPED_DIGITS).sub([&](const Match& m) { return comma.sub(U"", m.str(1)); }, input);
    text = re(py::ARABIC_DECIMAL).sub(U".", text);
    text = re(py::ISO_DATE).sub(iso_date, re(py::DIGIT_DATE).sub(digit_date, translate(text, eastern_digits())));

    U32 fold = translate(text, clock_fold_table());
    U32 out;
    size_t last = 0;
    for (const Match& m : re(py::NAMED_DATE).finditer(fold)) {
        out += text.substr(last, m.start() - last) + named_date(m);
        last = m.end();
    }
    text = out + text.substr(last);

    fold = translate(text, clock_fold_table());
    out.clear();
    last = 0;
    const Pattern& beside_year = re(py::MONTH_BESIDE_YEAR);
    for (const Match& m : re(py::LONE_MONTH).finditer(fold)) {
        if (beside_year.match(fold, m.end()) || ends(rstrip(U32View(fold).substr(0, m.start())), U"شهر")) {
            out += text.substr(last, m.start(2) - last) + U32(py::MONTH_NAMES[month_of(m.str(2)) - 1]);
            last = m.end();
        }
    }
    text = out + text.substr(last);

    text = re(py::DECIMAL_WORD).sub([](const Match& m) { return m.str(1) + U"." + replace(m.str(2), U" ", U""); }, text);
    text = re(py::DECIMAL_ZEROS).sub(
        [](const Match& m) { return m.str(2).empty() ? m.str(1) : m.str(1) + U"." + m.str(2); }, text);
    text = re(py::PERCENT_WORDS).sub(U"\\1%", re(py::PERCENT_BEFORE).sub(U"\\1%", text));
    text = re(py::CLOCK_MINUTE_WORD).sub(
        [](const Match& m) {
            const U32 minute = m.matched(3) && !m.str(3).empty() ? m.str(3) : m.str(4);
            if (to_int(m.str(2)) <= 24 && to_int(minute) <= 59)
                return m.str(1) + U" " + itos(to_int(m.str(2))) + U":" + itos2(to_int(minute));
            return m.str(0);
        },
        text);
    text = re(py::CLOCK_MINUTE_LOOSE).sub(
        [](const Match& m) {
            if (to_int(m.str(2)) <= 24 && to_int(m.str(4)) <= 59)
                return m.str(1) + U" " + itos(to_int(m.str(2))) + U":" + itos2(to_int(m.str(4))) + U" " + m.str(3);
            return m.str(0);
        },
        text);
    text = re(py::HOUR_PERIOD).sub(
        [](const Match& m) {
            const auto hour = placed_hour(to_int(m.str(2)), m.str(3));
            return hour ? m.str(1) + U" " + itos(*hour) : m.str(0);
        },
        re(py::BARE_CLOCK).sub(
            [](const Match& m) {
                const long long hour = to_int(m.str(1));
                const bool fits = hour <= 23 && (!m.matched(2) || to_int(m.str(2)) <= 59);
                return fits ? U32(U"الساعة ") : U32();
            },
            text));
    text = re(py::DAY_MONTH).sub(day_month, re(py::PADDED_HOUR).sub(U"\\1:", text));
    text = re(py::ON_THE_HOUR).sub(U"\\1", re(py::DIGIT_TIME_PERIOD).sub(
                                              [](const Match& m) {
                                                  const auto hour = placed_hour(to_int(m.str(1)), m.str(3));
                                                  return hour ? itos(*hour) + U":" + m.str(2) : m.str(0);
                                              },
                                              text));
    const UnitNames& units = unit_names();
    return kilometres(units.pattern.sub([&](const Match& m) { return U" " + units.names.at(m.str(2)); }, text));
}

// ── Number words before the parser ──────────────────────────────────────────

U32 held(const U32& word) {
    const auto* begin = std::begin(py::FRACTIONS);
    const auto* hit = std::find(begin, std::end(py::FRACTIONS), std::u32string_view(word));
    return U32(1, static_cast<char32_t>(py::HELD_FRACTION + (hit - begin)));
}

U32 hold_fractions(U32View text) {
    Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(parts[i]);
    for (size_t i = 0; i < words.size(); ++i) {
        const U32 key = fold_key(words[i]);
        if (!in(key, py::FRACTIONS)) continue;
        const U32 before = i > 0 ? words[i - 1] : U32();
        if (!before.empty() && (digit_at_end(before) || number_value(before).has_value())) continue;
        parts[2 * i] = held(key);
    }
    return join(parts, U"");
}

U32 percent_words(U32View input) {
    const U32 original(input);
    const U32 text = re(py::HUNDRED_PERCENT).sub(
        [&](const Match& m) {
            const Words before = split_ws(U32View(original).substr(0, m.start()));
            const U32 last = before.empty() ? U32() : before.back();
            const bool part =
                !last.empty() && (digit_at_end(last) || unit_word(last) || number_value(last).has_value());
            return part ? m.str(0) : U"100" + m.str(1);
        },
        original);
    return re(py::PERCENT_SAID).sub(
        [&](const Match& m) {
            const Words before = split_ws(U32View(text).substr(0, m.start()));
            const U32 last = before.empty() ? U32() : before.back();
            return !last.empty() && (digit_at_end(last) || number_value(last).has_value()) ? U32(U"٪") : m.str(0);
        },
        text);
}

bool definite_count(const U32& word) {
    const U32 key = fold_key(word);
    const auto m = re(py::DEFINITE).fullmatch(key);
    if (!m) return false;
    const auto value = number_value(m->str(1));
    return value && integral(*value) && between(*value, 3, 10);
}

U32 numbers_read_apart(U32View text, const std::string& lang) {
    const Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(parts[i]);
    std::vector<U32> pieces;
    size_t start = 0, offset = 0;
    for (size_t i = 0; i + 1 < words.size(); ++i) {
        offset += parts[2 * i].size();
        if (definite_count(words[i]) && fold_key(words[i + 1]) == py::THOUSAND) {
            pieces.emplace_back(text.substr(start, offset - start));
            start = offset;
        }
        offset += parts[2 * i + 1].size();
    }
    pieces.emplace_back(text.substr(start));
    U32 out;
    for (const auto& piece : pieces) out += numbers_to_digits_kept(piece, lang);
    return out;
}

U32 release_fractions(U32View text) {
    const long long n = static_cast<long long>(std::size(py::FRACTIONS));
    U32 out;
    for (const char32_t c : text) {
        if (py::HELD_FRACTION <= c && c < py::HELD_FRACTION + n) {
            out += py::FRACTIONS[c - py::HELD_FRACTION];
        } else {
            out += c;
        }
    }
    return out;
}

optional<std::pair<long long, size_t>> year_value(const Words& words) {
    for (const size_t n : {3u, 2u, 1u}) {
        if (words.size() < n) continue;
        const Words run(words.begin(), words.begin() + static_cast<long>(n));
        const auto value = number_value(joined(run, 0, static_cast<long>(n)));
        if (!value || !integral(*value) || !between(*value, 20, 99)) continue;
        bool every = true;
        for (size_t k = 0; k < n && every; ++k) {
            const Words rest = without(run, k);
            if (same(number_value(joined(rest, 0, static_cast<long>(rest.size()))), value)) every = false;
        }
        if (every) return std::make_pair(ll_of(*value), n);
    }
    return nullopt;
}

U32 spoken_years(U32View text) {
    Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(fold_key(parts[i]));
    const Pattern& year_word = re(py::YEAR_WORD);
    size_t i = 1;
    while (i + 1 < words.size()) {
        if (in(words[i], py::TWENTY) &&
            (year_word.fullmatches(words[i - 1]) || (i > 1 && year_word.fullmatches(words[i - 2] + U" " + words[i - 1])))) {
            if (const auto read = year_value(sub_words(words, static_cast<long>(i + 1), static_cast<long>(i + 4)))) {
                const auto [value, n] = *read;
                parts[2 * i] = U"20" + itos(value);
                for (size_t p = 2 * i + 1; p < 2 * (i + n) + 1; ++p) parts[p] = U"";
                i += n;
            }
        }
        i += 1;
    }
    return join(parts, U"");
}

bool ordinal_words(const Words& span) {
    for (const auto& w : span)
        if (!arabic_word(w) || !hour_value(w).has_value()) return false;
    return true;
}

optional<std::pair<long long, size_t>> ordinal_day(const Words& words, size_t i) {
    for (const size_t n : {3u, 2u, 1u}) {
        const size_t end = i + n;
        const size_t month_at = end < words.size() && words[end] == U"من" ? end + 1 : end;
        if (month_at >= words.size()) continue;
        if (!lookup(words[month_at], py::MONTHS) &&
            !lookup(joined(words, static_cast<long>(month_at), static_cast<long>(month_at + 2)), py::MONTHS))
            continue;
        const Words span = sub_words(words, static_cast<long>(i), static_cast<long>(end));
        if (!ordinal_words(span) || number_value(span[0]).has_value()) continue;
        const auto value = hour_value(joined(span, 0, static_cast<long>(span.size())));
        if (value && integral(*value) && between(*value, 1, 31) && every_word_counts(span, value))
            return std::make_pair(ll_of(*value), end);
    }
    return nullopt;
}

U32 ordinal_days(U32View text) {
    Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(fold_key(parts[i]));
    size_t i = 0;
    while (i < words.size()) {
        const auto read = starts(words[i], U"ال") ? ordinal_day(words, i) : nullopt;
        if (!read) {
            i += 1;
            continue;
        }
        const auto [value, end] = *read;
        parts[2 * i] = itos(value);
        for (size_t p = 2 * i + 1; p + 1 < 2 * end; ++p) parts[p] = U"";
        i = end;
    }
    return join(parts, U"");
}

U32 rank_ordinals(U32View text) {
    static const std::set<U32> nouns = [] {
        std::set<U32> out;
        for (const auto& kv : py::RANK_NOUNS) out.insert(fold_key(kv.first));
        return out;
    }();
    Words parts = words_and_gaps(text);
    Words words;
    for (size_t i = 0; i < parts.size(); i += 2) words.push_back(fold_key(parts[i]));
    for (size_t i = 0; i + 1 < words.size(); ++i) {
        const U32& noun = words[i];
        const U32 three = noun.substr(0, 3);
        const U32 bare = starts(noun, U"ال")                                       ? slice_from(noun, 2)
                         : (three == U"وال" || three == U"بال" || three == U"فال") ? slice_from(noun, 3)
                                                                                   : noun;
        if (!nouns.count(bare)) continue;
        for (const size_t n : {3u, 2u, 1u}) {
            const Words span = sub_words(words, static_cast<long>(i + 1), static_cast<long>(i + 1 + n));
            if (span.size() < n || !ordinal_words(span) || number_value(span[0]).has_value()) continue;
            const auto value = hour_value(joined(span, 0, static_cast<long>(span.size())));
            if (value && integral(*value) && between(*value, 1, 99) && every_word_counts(span, value)) {
                parts[2 * (i + 1)] = str_of(int_of(*value));
                for (size_t p = 2 * (i + 1) + 1; p < 2 * (i + n) + 1; ++p) parts[p] = U"";
                break;
            }
        }
    }
    return join(parts, U"");
}

// ── Codes ───────────────────────────────────────────────────────────────────

struct CodeNames {
    std::map<U32, U32> letters;  // capital -> name
    std::map<U32, U32> digits;   // name -> digits
    std::set<U32> letter_names;
};

const CodeNames& code_names() {
    static const CodeNames names = [] {
        CodeNames out;
        std::vector<std::pair<U32, U32>> folded_names;
        for (const auto& [c, n] : spelled_codes("ar")) folded_names.emplace_back(u(c), fold_key(u(n)));
        for (const auto& [c, n] : folded_names)
            if (!c.empty() && std::all_of(c.begin(), c.end(), [](char32_t x) { return isalpha(x); }))
                out.letters[c] = n;
        out.letters[U"J"] = U"جاي";
        for (const auto& [c, n] : folded_names)
            if (isdigit(c)) out.digits[n] = c;
        const std::pair<std::u32string_view, std::u32string_view> extra[] = {
            {U"وان", U"1"}, {U"تري", U"3"}, {U"فايف", U"5"}, {U"سيكس", U"6"}, {U"سيفن", U"7"}, {U"ناين", U"9"}, {U"ايت", U"8"}};
        for (const auto& [n, c] : extra) out.digits[U32(n)] = U32(c);
        for (const auto& [n, c] : py::ENGLISH_TENS) out.digits[U32(n)] = U32(c);
        for (const auto& kv : out.letters) out.letter_names.insert(kv.second);
        return out;
    }();
    return names;
}

bool all_consonant_caps(const U32& token) {
    return std::all_of(token.begin(), token.end(),
                       [](char32_t c) { return py::CONSONANT_CAPS.find(c) != std::u32string_view::npos; });
}

U32 spelled_code(const Match& m) {
    const U32 token = m.str(0);
    const U32View s = m.string();
    const bool beside_number = pyre::compiled(U"[A-Z]{1,4}").fullmatches(token) &&
                               (pyre::compiled(U" ?[0-9]").match(s.substr(m.end())).has_value() ||
                                pyre::compiled(U"[0-9] ?$").search(s.substr(0, m.start())).has_value());
    const bool any_digit = std::any_of(token.begin(), token.end(), [](char32_t c) { return isdigit(c); });
    const bool is_code = any_digit || (2 <= token.size() && token.size() <= 4 && all_consonant_caps(token)) ||
                         pyre::compiled(U"[a-z]?[A-Z]").fullmatches(token) ||
                         pyre::compiled(U"[a-z][A-Z]{1,3}").fullmatches(token) || beside_number;
    if (!is_code) return token;
    const auto& letters = code_names().letters;
    std::vector<U32> out;
    for (const U32& p : pyre::compiled(U"[0-9]+|[A-Za-z]").findall(token)) {
        if (isdigit(p)) {
            out.push_back(p);
        } else {
            const auto it = letters.find(upper(p));
            if (it == letters.end()) throw KeyError(repr(upper(p)));
            out.push_back(it->second);
        }
    }
    return join(out, U" ");
}

optional<Words> split_names(const U32& rest) {
    thread_local std::unordered_map<U32, optional<Words>> cache;
    if (rest.empty()) return Words{};
    const auto hit = cache.find(rest);
    if (hit != cache.end()) return hit->second;
    const CodeNames& names = code_names();
    optional<Words> out;
    for (size_t k = rest.size(); k > 1; --k) {
        const U32 head = rest.substr(0, k);
        if (!names.letter_names.count(head) && !names.digits.count(head)) continue;
        if (auto tail = split_names(rest.substr(k))) {
            Words found = {head};
            found.insert(found.end(), tail->begin(), tail->end());
            out = std::move(found);
            break;
        }
    }
    if (cache.size() > (1u << 16)) cache.clear();
    cache.emplace(rest, out);
    return out;
}

optional<Words> fused_names(const U32& word) {
    const CodeNames& names = code_names();
    const auto pieces = split_names(word);
    if (!pieces || pieces->size() < 2 || (*pieces)[0] == U"ال") return nullopt;
    const bool all_digits = std::all_of(pieces->begin(), pieces->end(), [&](const U32& p) { return names.digits.count(p) > 0; });
    const bool name_only = std::any_of(pieces->begin(), pieces->end(), [](const U32& p) { return in(p, py::NAME_ONLY_LETTERS); });
    if (all_digits || name_only) return pieces;
    return nullopt;
}

U32 code_runs(U32View text) {
    const CodeNames& names = code_names();
    Words words;
    const Pattern& glued_re = re(py::NAME_AND_DIGITS);
    for (const U32& word : split(text, U" ")) {
        const U32 key = fold_key(word);
        optional<Words> fused;
        if (!key.empty() && !isdigit(word) && !names.letter_names.count(key) && !names.digits.count(key))
            fused = fused_names(key);
        if (const auto glued = glued_re.fullmatch(key); glued && names.letter_names.count(glued->str(1)))
            fused = Words{glued->str(1), glued->str(2)};
        if (fused && !fused->empty()) {
            words.insert(words.end(), fused->begin(), fused->end());
        } else {
            words.push_back(word);
        }
    }

    auto kind = [&](const U32& w) -> char {
        const U32 k = fold_key(w);
        if (names.letter_names.count(k)) return 'L';
        if (names.digits.count(k)) return 'E';
        if (isdigit(w)) return 'D';
        return 0;
    };

    Words out;
    size_t i = 0;
    while (i < words.size()) {
        if (kind(words[i]) == 0) {
            out.push_back(words[i]);
            i += 1;
            continue;
        }
        size_t j = i;
        while (j < words.size() && kind(words[j]) != 0) j += 1;
        Words run(words.begin() + static_cast<long>(i), words.begin() + static_cast<long>(j));
        std::vector<char> kinds;
        for (const auto& w : run) kinds.push_back(kind(w));
        bool only_prepositions = true;
        size_t letter_count = 0;
        for (size_t r = 0; r < run.size(); ++r)
            if (kinds[r] == 'L') {
                letter_count += 1;
                if (!in(fold_key(run[r]), py::PREPOSITION_LETTERS)) only_prepositions = false;
            }
        bool digit_said = false, name_only = false;
        for (size_t r = 0; r < run.size(); ++r) {
            if (kinds[r] == 'E' && (!in(fold_key(run[r]), py::WORD_DIGIT_NAMES) || !only_prepositions)) digit_said = true;
            if (in(fold_key(run[r]), py::NAME_ONLY_LETTERS)) name_only = true;
        }
        if (run.size() >= 2 && letter_count > 0 && (digit_said || letter_count >= 2 || name_only)) {
            Words merged;
            bool tens = false;
            for (size_t r = 0; r < run.size(); ++r) {
                const U32& w = run[r];
                optional<U32> d;
                if (kinds[r] == 'E') d = names.digits.at(fold_key(w));
                if (kinds[r] == 'D') d = w;
                if (d && !merged.empty() && isdigit(merged.back())) {
                    if (tens && d->size() == 1) {
                        merged.back() = str_of(to_bigint(merged.back()) + to_bigint(*d));
                    } else {
                        merged.back() += *d;
                    }
                } else {
                    merged.push_back(d ? *d : w);
                }
                tens = kinds[r] == 'E' && lookup(fold_key(w), py::ENGLISH_TENS) != nullptr;
            }
            run = std::move(merged);
        }
        out.insert(out.end(), run.begin(), run.end());
        i = j;
    }
    return join(out, U" ");
}

// ── Clitics ─────────────────────────────────────────────────────────────────

U32 join_clitics(U32View text) {
    static const std::pair<std::u32string_view, std::vector<std::u32string_view>> compounds[] = {
        {U"ان", {U"شا", U"شاء"}}, {U"الحمد", {U"لله"}}, {U"يا", {U"رب"}}, {U"ما", {U"في"}}};
    const Pattern& arabic = pyre::compiled(U"[ء-ي]+");
    Words out;
    for (const U32& w : split(text, U" ")) {
        if (w.empty()) {
            out.push_back(w);
            continue;
        }
        U32 prev = out.empty() ? U32() : out.back();
        bool compound = false;
        for (const auto& [first, second] : compounds)
            if ((prev == first || prev == U"و" + U32(first)) &&
                std::find(second.begin(), second.end(), std::u32string_view(w)) != second.end())
                compound = true;
        if (prev == U"و") {
            out.back() = U"و" + w;
        } else if (in(w, py::LAM_PRONOUNS) && arabic.fullmatches(prev)) {
            if (ends(prev, U"وا")) prev.pop_back();
            out.back() = prev + w;
        } else if ((prev == U"عبد" || prev == U"وعبد") && starts(w, U"ال")) {
            out.back() = prev + w;
        } else if (compound) {
            out.back() = prev + w;
        } else {
            out.push_back(w);
        }
    }
    return join(out, U" ");
}

// ── The character rules ─────────────────────────────────────────────────────

struct Compiled {
    optional<Pattern> marks;
    optional<Table> table;
};

template <size_t N>
void add_pairs(Table& t, const std::pair<std::u32string_view, std::u32string_view> (&pairs)[N]) {
    for (const auto& [from, to] : pairs) t[from[0]] = U32(to);
}

const Compiled& compiled_rules(const AsrNorm& config) {
    thread_local std::map<std::string, Compiled> cache;
    const std::string key = flag_key(config);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;
    U32 ranges;
    if (config.strip_harakat) ranges += py::HARAKAT;
    if (config.strip_extended_marks) ranges += py::EXTENDED_MARKS;
    if (config.strip_quranic_marks) ranges += py::QURANIC_MARKS;
    if (config.strip_tatweel) ranges += py::TATWEEL;
    Table t;
    if (config.unify_alef) add_pairs(t, py::ALEF);
    if (config.unify_ta_marbuta) add_pairs(t, py::TA_MARBUTA);
    if (config.unify_alef_maqsura) add_pairs(t, py::ALEF_MAQSURA);
    if (config.unify_hamza_carriers) add_pairs(t, py::HAMZA_CARRIERS);
    if (config.unify_loan_letters) add_pairs(t, py::LOAN_LETTERS);
    if (config.fold_digits) add_pairs(t, py::DIGITS);
    Compiled out;
    if (!ranges.empty()) out.marks.emplace(U32View(U"[" + ranges + U"]"));
    if (!t.empty()) out.table = std::move(t);
    return cache.emplace(key, std::move(out)).first->second;
}

}  // namespace

std::string normalize_asr(const std::string& input, const AsrNorm& config, const std::string& lang) {
    const Compiled& rules = compiled_rules(config);
    U32 text = u(input);
    if (config.nfc) text = arbtok::detail::nfc(text);
    if (config.strip_event_markup) text = re(py::LANGUAGE_WRAPPER).sub(U" \\1 ", re(py::EVENT).sub(U" ", text));
    if (config.strip_controls) text = re(py::CONTROLS).sub(U"", text);
    if (config.blank_punctuation && config.punctuation_before_marks) text = re(py::PUNCTUATION).sub(U" ", text);
    if (rules.marks) text = rules.marks->sub(U"", text);
    const bool arabic = is_arabic_lang(lang);
    if (config.split_term_proclitics && arabic) text = split_latin_proclitics(text);
    if (!config.lexicon.empty()) text = apply_lexicon(text, config);
    if (config.fix_asr_errors && arabic) text = or_as_and(maya_as_hundred(text));
    if (config.unify_spoken_variants && arabic) text = spoken_variants(text);
    if (config.unify_dialect_spellings && arabic) text = map_words(text, dialect_spelling);
    if (config.join_dictated_digits && arabic) text = plate_letters(text);
    if (config.number_forms) {
        text = written_numbers(text);
        if (arabic) text = number_words(text);
    }
    if (config.spoken_numbers_to_digits) {
        if (arabic) {
            text = clock_times(text, config.number_forms);
            if (config.number_forms)
                text = percent_words(hold_fractions(rank_ordinals(ordinal_days(spoken_years(text)))));
        }
        text = config.number_forms && arabic ? numbers_read_apart(text, lang) : numbers_to_digits_kept(text, lang);
        if (config.number_forms) text = release_fractions(written_numbers(text));
    }
    if (config.join_dictated_digits) text = joined_digits(text);
    if (config.spell_codes && arabic) text = re(py::LATIN_CODE).sub(spelled_code, text);
    if (rules.table) text = translate(text, *rules.table);
    if (config.fold_case) text = lower(text);
    if (config.arabic_block_only) text = re(py::NOT_ARABIC_BLOCK).sub(U" ", text);
    if (config.blank_punctuation && !config.punctuation_before_marks)
        text = re(config.number_forms ? py::PUNCTUATION_OUTSIDE_NUMBERS : py::PUNCTUATION).sub(U" ", text);
    if (config.drop_word_final_hamza) text = re(py::WORD_FINAL_HAMZA).sub(U"", text);
    if (config.spell_codes && arabic) text = code_runs(text);
    if (config.join_clitics && arabic) text = join_clitics(text);
    if (config.collapse_whitespace) text = strip(re(py::WHITESPACE).sub(U" ", text));
    return s8(text);
}

}  // namespace arbtok::textnorm
