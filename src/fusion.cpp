#include "arbtok/fusion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <set>

#include <orthography2ipa/orthography2ipa.hpp>
#include <orthography2ipa/phonetok.hpp>
#include <orthography2ipa/underspecification.hpp>
#include "py_float.hpp"

#include "arbtok/diacritize.hpp"
#include "arbtok/ensemble.hpp"
#include "arbtok/lattice.hpp"
#include "arbtok/nisba.hpp"
#include "arbtok/orthography.hpp"
#include "arbtok/tokenizer.hpp"
#include "arbtok/waqf.hpp"
#include "detail.hpp"
#include "unicode_norm.hpp"

namespace arbtok::fusion {

namespace o2i = orthography2ipa;
using detail::U32;
using diacritize::repair_skeleton;
using diacritize::skeleton_is_preserved;
using diacritize::strip_marks;
using tokenizer::normalize_unicode;

namespace {

// numpy's float32 exp (the AVX2 and AVX512F loops): a Cody-Waite reduction
// by ln 2, a rational approximation, and a scale by 2^n. It is not correctly
// rounded, so std::exp gives other bits. Every NaN gives the positive quiet
// NaN.
float np_expf(float x) {
    if (std::isnan(x)) return std::numeric_limits<float>::quiet_NaN();
    if (x >= 88.72283935546875f) return INFINITY;
    if (x <= -103.97208404541015625f) return 0.0f;
    float q = x * 1.442695040888963407359924681001892137f;
    q = q + 0x1.800000p+23f;
    q = q - 0x1.800000p+23f;
    float r = std::fma(q, -6.93145752e-1f, x);
    r = std::fma(q, -1.42860677e-6f, r);
    r = std::fma(q, 0.0f, r);
    float num = std::fma(5.082762527590693718096e-04f, r, 6.757896990527504603057e-03f);
    num = std::fma(num, r, 5.114512081637298353406e-02f);
    num = std::fma(num, r, 2.473615434895520810817e-01f);
    num = std::fma(num, r, 7.257664613233124478488e-01f);
    num = std::fma(num, r, 9.999999999980870924916e-01f);
    float den = std::fma(2.159509375685829852307e-02f, r, -2.742335390411667452936e-01f);
    den = std::fma(den, r, 1.0f);
    return std::ldexp(num / den, static_cast<int>(q));
}

// numpy's pairwise sum of float32 values, in float32.
float pairwise_sum(const float* a, std::size_t n) {
    if (n < 8) {
        float res = 0.0f;
        for (std::size_t i = 0; i < n; ++i) res += a[i];
        return res;
    }
    if (n <= 128) {
        float r[8];
        std::copy(a, a + 8, r);
        std::size_t i = 8;
        for (; i < n - (n % 8); i += 8)
            for (std::size_t j = 0; j < 8; ++j) r[j] += a[i + j];
        float res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; ++i) res += a[i];
        return res;
    }
    std::size_t n2 = n / 2;
    n2 -= n2 % 8;
    return pairwise_sum(a, n2) + pairwise_sum(a + n2, n - n2);
}

// np.argsort(values)[::-1]: a stable ascending sort, NaN last, reversed.
std::vector<std::size_t> argsort_descending(const std::vector<float>& values) {
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t i, std::size_t j) {
        return !std::isnan(values[i]) && (std::isnan(values[j]) || values[i] < values[j]);
    });
    std::reverse(order.begin(), order.end());
    return order;
}

// The length of items[:n] for a Python slice bound n.
std::size_t slice_head(long n, std::size_t size) {
    if (n >= 0) return std::min(static_cast<std::size_t>(n), size);
    const long kept = static_cast<long>(size) + n;
    return kept > 0 ? static_cast<std::size_t>(kept) : 0;
}

bool all_space(const std::string& s) {
    for (char32_t c : detail::from_utf8(s))
        if (!detail::is_space(c)) return false;
    return true;
}

std::vector<std::string> split_spaces(const std::string& text) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t space = text.find(' ', start);
        if (space == std::string::npos) {
            out.push_back(text.substr(start));
            return out;
        }
        out.push_back(text.substr(start, space - start));
        start = space + 1;
    }
}

// _bare_words(bare): the runs of bare between spaces, with their code point
// offsets.
std::vector<std::pair<U32, std::size_t>> bare_words(const U32& bare) {
    std::vector<std::pair<U32, std::size_t>> words;
    std::optional<std::size_t> start;
    for (std::size_t i = 0; i < bare.size(); ++i) {
        if (bare[i] == U' ') {
            if (start) words.emplace_back(bare.substr(*start, i - *start), *start);
            start.reset();
        } else if (!start) {
            start = i;
        }
    }
    if (start) words.emplace_back(bare.substr(*start), *start);
    return words;
}

std::string format3(double v) {
    if (std::isnan(v)) return "nan";
    char buffer[400];
    std::snprintf(buffer, sizeof buffer, "%.3f", v);
    return buffer;
}

}  // namespace

bool has_arabic_letter(std::string_view word) {
    for (char32_t c : detail::from_utf8(word))
        if (detail::is_arabic_named_letter(c)) return true;
    return false;
}

std::vector<float> logprobs(const float* row, std::size_t size) {
    if (size == 0) throw ValueError("zero-size array to reduction operation maximum which has no identity");
    float m = row[0];
    for (std::size_t i = 1; i < size && !std::isnan(m); ++i)
        if (std::isnan(row[i]) || row[i] > m) m = row[i];
    std::vector<float> shifted(size), exps(size);
    for (std::size_t i = 0; i < size; ++i) {
        shifted[i] = row[i] - m;
        exps[i] = np_expf(shifted[i]);
    }
    const float log_sum = static_cast<float>(std::log(static_cast<double>(pairwise_sum(exps.data(), size))));
    for (float& v : shifted) v = v - log_sum;
    return shifted;
}

std::string Hypothesis::repr() const {
    return "Hypothesis(" + detail::py_str_repr(detail::from_utf8(text)) + ", logprob=" + format3(logprob) +
           ", lattice_cost=" + format3(lattice_cost) + ")";
}

// One letter's constraint: a pinned class, a set of allowed classes, or none.
struct FusionDiacritizer::Pin {
    std::optional<int> pinned;
    std::optional<std::set<int>> allowed;
};

FusionDiacritizer::FusionDiacritizer(const std::string& lang, Options options)
    : lang_(lang), options_(std::move(options)) {
    try {
        spec_ = &o2i::get(lang_);
    } catch (const std::exception& e) {
        throw KeyError(e.what());
    }
    tokenizer_ = std::make_unique<o2i::PhonetokTokenizer>(*spec_);
    if (options_.lexicon && !options_.lexicon->empty()) lexicon_.emplace(*options_.lexicon, options_.ambiguous_share);
    if (options_.dialect_lexicon) dialect_lexicon_.emplace(lang_);
}

FusionDiacritizer::~FusionDiacritizer() = default;
FusionDiacritizer::FusionDiacritizer(FusionDiacritizer&&) noexcept = default;
FusionDiacritizer& FusionDiacritizer::operator=(FusionDiacritizer&&) noexcept = default;

const ensemble::EnsembleDiacritizer& FusionDiacritizer::diacritizer() const { return ensemble::get_ensemble(); }

bool FusionDiacritizer::is_licensed(const std::string& word) const {
    std::vector<o2i::Token> tokens;
    try {
        tokens = tokenizer_->tokenize(normalize_unicode(word));
    } catch (const std::exception&) {
        return false;
    }
    for (const auto& t : tokens)
        if (t.kind == o2i::TokenKind::UNKNOWN) return false;
    return true;
}

double FusionDiacritizer::lattice_cost(const std::string& word) const {
    try {
        o2i::detail::PythonFloatSum total;
        for (const auto& slot : lattice::word_lattice(word, lang_)) {
            if (slot.candidates.empty()) return 0.0;
            total.add(slot.top().score);
        }
        return total.value();
    } catch (const std::exception&) {
        return 0.0;
    }
}

std::optional<std::string> FusionDiacritizer::dialect_lookup(const std::string& word) const {
    if (!dialect_lexicon_) return std::nullopt;
    auto voc = dialect_lexicon_->get(word);
    if (!voc) return std::nullopt;
    if (strip_marks(*voc) != strip_marks(word) || !is_licensed(*voc)) return std::nullopt;
    return voc;
}

std::optional<std::string> FusionDiacritizer::lookup(const std::string& word) const {
    if (!lexicon_ || !options_.waqf) return std::nullopt;
    auto stem = lexicon_->get(word, context_words_);
    if (!stem) return std::nullopt;
    stem = nisba::restore_nisba(*stem);
    if (strip_marks(*stem) != strip_marks(word) || !is_licensed(*stem)) return std::nullopt;
    return stem;
}

std::optional<std::vector<FusionDiacritizer::Pin>> FusionDiacritizer::pins(const U32& word,
                                                                           const U32& bare_word) const {
    const ensemble::EnsembleDiacritizer* ens = &diacritizer();
    const auto [src_bare, src_marks] = orthography::source_marks(
        detail::to_utf8(word), [](const std::string& t) { return ensemble::EnsembleDiacritizer::normalize(t); });
    if (detail::from_utf8(src_bare) != bare_word || src_marks.size() != bare_word.size()) return std::nullopt;
    const auto& classes = ens->classes();
    std::vector<Pin> out;
    for (std::size_t i = 0; i < bare_word.size(); ++i) {
        const char32_t ch = bare_word[i];
        const orthography::MarkSet& marks = src_marks[i];
        Pin pin;
        if (!detail::is_alpha(ch)) {
            pin.pinned = 0;
        } else if (std::any_of(marks.begin(), marks.end(),
                               [](const std::string& m) { return !orthography::ORTHOGRAPHIC_MARKS.count(m); })) {
            const int c = orthography::pinned_class(classes, marks);
            if (c >= 0) pin.pinned = c;
        } else if (!marks.empty()) {
            const auto allowed = orthography::allowed_classes(classes, marks);
            pin.allowed.emplace(allowed.begin(), allowed.end());
        } else {
            const auto allowed = orthography::allowed_classes(classes, marks, ch == U'ا');
            pin.allowed.emplace(allowed.begin(), allowed.end());
        }
        out.push_back(std::move(pin));
    }
    return out;
}

std::vector<Hypothesis> FusionDiacritizer::beam_search(const U32& bare_word, const float* logits, std::size_t cols,
                                                       const std::optional<std::vector<Pin>>& pins) const {
    using Beam = std::pair<double, std::vector<std::int64_t>>;
    std::vector<Beam> beams{{0.0, {}}};
    for (std::size_t i = 0; i < bare_word.size(); ++i) {
        const Pin* pin = pins ? &(*pins)[i] : nullptr;
        std::vector<std::pair<std::int64_t, double>> choices;
        if (pin && pin->pinned) {
            choices = {{*pin->pinned, 0.0}};
        } else if (!detail::is_alpha(bare_word[i])) {
            choices = {{0, 0.0}};
        } else {
            const std::vector<float> lp = logprobs(logits + i * cols, cols);
            std::vector<std::size_t> order = argsort_descending(lp);
            if (pin && pin->allowed) {
                std::vector<std::size_t> kept;
                for (std::size_t c : order)
                    if (pin->allowed->count(static_cast<int>(c))) kept.push_back(c);
                order = std::move(kept);
            }
            order.resize(slice_head(options_.topk, order.size()));
            for (std::size_t c : order) choices.emplace_back(static_cast<std::int64_t>(c), lp[c]);
            if (choices.empty()) choices = {{0, 0.0}};
        }
        std::vector<Beam> expanded;
        for (const auto& [score, ids] : beams)
            for (const auto& [cid, clp] : choices) {
                auto next = ids;
                next.push_back(cid);
                expanded.emplace_back(score + clp, std::move(next));
            }
        std::stable_sort(expanded.begin(), expanded.end(),
                         [](const Beam& a, const Beam& b) { return a.first > b.first; });
        expanded.resize(slice_head(options_.beam, expanded.size()));
        beams = std::move(expanded);
    }
    std::vector<Hypothesis> out;
    for (auto& [score, ids] : beams) out.push_back({std::move(ids), score, "", 0.0});
    return out;
}

std::string FusionDiacritizer::generate(const std::string& orig_word, const std::string& normalized, bool recovery) {
    std::optional<std::string> proposed = diacritizer().diacritize(normalized);
    if (options_.waqf) proposed = waqf::pausal(*proposed);
    if (!skeleton_is_preserved(normalized, *proposed)) proposed = repair_skeleton(normalized, *proposed);
    if (proposed) {
        proposed = nisba::restore_nisba(*proposed);
        if (is_licensed(*proposed)) {
            if (recovery) recovered.push_back(orig_word);
            return *proposed;
        }
    }
    rejected.push_back(orig_word);
    return orig_word;
}

std::string FusionDiacritizer::fuse_word(const std::string& orig_word, const U32& bare_word, const float* logits,
                                         std::size_t cols) {
    const std::string normalized = normalize_unicode(orig_word);
    last_nbest.clear();
    const auto positions = o2i::underspecification::underdetermined_positions(normalized, *spec_);
    if (diacritize::author_complete(normalized, positions)) return orig_word;
    if (auto dialect = dialect_lookup(normalized)) {
        dialect_looked_up.push_back(orig_word);
        return *dialect;
    }
    if (auto entry = lookup(normalized)) {
        looked_up.push_back(orig_word);
        return *entry;
    }
    if (diacritize::proclitic_complete(normalized, positions)) return orig_word;
    if (strip_marks(normalized) != normalized) return generate(orig_word, normalized);
    const ensemble::EnsembleDiacritizer* ens = &diacritizer();
    const auto word_pins = pins(detail::from_utf8(normalized), bare_word);
    const std::string bare_utf8 = detail::to_utf8(bare_word);
    std::vector<Hypothesis> licensed;
    for (auto& hyp : beam_search(bare_word, logits, cols, word_pins)) {
        std::string rendered = nisba::restore_nisba(ens->decode(bare_utf8, hyp.class_ids));
        if (options_.waqf) rendered = waqf::pausal(rendered);
        if (!is_licensed(rendered)) continue;
        hyp.text = rendered;
        hyp.lattice_cost = lattice_cost(rendered);
        licensed.push_back(std::move(hyp));
    }
    if (licensed.empty()) return generate(orig_word, normalized, true);
    const double weight = options_.lattice_weight;
    std::stable_sort(licensed.begin(), licensed.end(), [weight](const Hypothesis& a, const Hypothesis& b) {
        return a.logprob - weight * a.lattice_cost > b.logprob - weight * b.lattice_cost;
    });
    last_nbest = licensed;
    std::string argmax = nisba::restore_nisba(ens->diacritize(normalized));
    if (options_.waqf) argmax = waqf::pausal(argmax);
    if (!is_licensed(argmax)) recovered.push_back(orig_word);
    return licensed.front().text;
}

std::string FusionDiacritizer::diacritize(const std::string& text) {
    const ensemble::EnsembleDiacritizer* ens = &diacritizer();
    const ensemble::Logits logits = ens->logits(text);
    const U32 bare = detail::from_utf8(logits.bare);
    const std::vector<std::string> orig_words = split_spaces(text);
    const auto words = bare.empty() ? std::vector<std::pair<U32, std::size_t>>{} : bare_words(bare);
    std::size_t nonempty = 0, arabic = 0;
    for (const auto& w : orig_words)
        if (!all_space(w)) {
            ++nonempty;
            if (has_arabic_letter(w)) ++arabic;
        }
    if (logits.empty() || words.size() != nonempty) {
        diacritize::Options guard_options;
        guard_options.waqf = options_.waqf;
        guard_options.lexicon = lexicon_ ? std::optional<std::string>(lexicon::DEFAULT_LEXICON) : std::nullopt;
        guard_options.dialect_lexicon = options_.dialect_lexicon;
        return diacritize::LatticeDiacritizer(lang_, guard_options).diacritize(text);
    }
    std::string out;
    std::size_t bi = 0;
    context_words_ = static_cast<long>(arabic);
    try {
        for (std::size_t k = 0; k < orig_words.size(); ++k) {
            if (k) out += ' ';
            const std::string& w = orig_words[k];
            if (all_space(w)) {
                out += w;
                continue;
            }
            const auto& [bare_word, start] = words[bi++];
            out += fuse_word(w, bare_word, logits.row(start), logits.cols);
        }
    } catch (...) {
        context_words_.reset();
        throw;
    }
    context_words_.reset();
    return out;
}

}  // namespace arbtok::fusion
