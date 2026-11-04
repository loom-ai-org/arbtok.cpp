#include "arbtok/ensemble/ensemble.hpp"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>

#include "json.hpp"
#include "unicode.hpp"

namespace arbtok::ensemble {

namespace {

using detail::Category;
using MarkSet = std::set<char32_t>;

constexpr char32_t kMadda = 0x0653;

bool is_orthographic(char32_t c) { return c == 0x0653 || c == 0x0654 || c == 0x0655; }

MarkSet orthographic(const MarkSet& marks) {
    MarkSet out;
    for (char32_t c : marks)
        if (is_orthographic(c)) out.insert(c);
    return out;
}

bool file_exists(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string data_file(const char* name) {
    if (const char* env = std::getenv("ARBTOK_DATA_DIR"); env && *env) {
        std::string p = std::string(env) + "/models/" + name;
        if (file_exists(p)) return p;
    }
    return std::string(ARBTOK_ENSEMBLE_SOURCE_DATA_DIR) + "/models/" + name;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::u32string normalize32(std::string_view text) {
    std::u32string out;
    for (char32_t c : detail::nfd(detail::decode_utf8(text)))
        if (detail::category(c) != Category::So) out.push_back(c);
    return out;
}

std::u32string bare32(std::string_view text) {
    std::u32string out;
    for (char32_t c : normalize32(text))
        if (detail::category(c) != Category::Mn) out.push_back(c);
    return out;
}

// Never destroyed: a session in static storage, such as one held by
// get_ensemble, can outlive any static Env, and onnxruntime requires the Env
// to outlive every session.
Ort::Env& ort_env() {
    static Ort::Env* env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "arbtok");
    return *env;
}

}  // namespace

struct EnsembleDiacritizer::Impl {
    std::unordered_map<char32_t, std::int64_t> c2i;
    std::vector<std::string> classes;
    // set(c) and len(c) of each class, in code points.
    std::vector<MarkSet> class_marks;
    std::vector<std::size_t> class_len;
    std::int64_t unk = 1;
    Ort::Session session{nullptr};

    // orthography.pinned_class.
    std::int64_t pinned_class(const MarkSet& marks) const {
        std::int64_t best = -1;
        for (std::size_t i = 0; i < classes.size(); ++i) {
            if (class_marks[i] != marks) continue;
            if (best < 0 || class_len[i] < class_len[static_cast<std::size_t>(best)])
                best = static_cast<std::int64_t>(i);
        }
        return best;
    }

    // orthography.allowed_classes with allow_restoration=True.
    std::vector<std::int64_t> allowed_classes(const MarkSet& marks) const {
        const MarkSet here = orthographic(marks);
        std::vector<std::int64_t> out;
        for (std::size_t i = 0; i < classes.size(); ++i) {
            bool ok;
            if (here.count(kMadda)) ok = class_marks[i] == MarkSet{kMadda};
            else if (!here.empty()) ok = orthographic(class_marks[i]) == here;
            else ok = true;
            if (ok) out.push_back(static_cast<std::int64_t>(i));
        }
        return out;
    }
};

std::string default_ensemble_onnx() { return data_file("rawi_ensemble.logits.int8.onnx"); }

std::string default_vocab() { return data_file("rawi_v2.vocab.json"); }

std::string onnxruntime_version() { return Ort::GetVersionString(); }

std::vector<std::int64_t> argmax(const Logits& logits) {
    if (logits.rows && !logits.cols) throw std::invalid_argument("attempt to get argmax of an empty sequence");
    std::vector<std::int64_t> out(logits.rows, 0);
    for (std::size_t r = 0; r < logits.rows; ++r) {
        const float* row = logits.row(r);
        std::size_t best = 0;
        for (std::size_t j = 1; j < logits.cols && !std::isnan(row[best]); ++j)
            if (std::isnan(row[j]) || row[j] > row[best]) best = j;
        out[r] = static_cast<std::int64_t>(best);
    }
    return out;
}

EnsembleDiacritizer::EnsembleDiacritizer(const std::string& model_path, const std::string& vocab_path)
    : impl_(std::make_unique<Impl>()) {
    const auto vocab = detail::json::parse(read_file(vocab_path));
    for (const auto& [key, id] : vocab.at("char_to_idx").members) {
        const std::u32string k = detail::decode_utf8(key);
        if (k.size() == 1) impl_->c2i[k[0]] = id.i;
        if (key == "<UNK>") impl_->unk = id.i;
    }
    std::map<std::int64_t, std::string> i2d;
    for (const auto& [diac, id] : vocab.at("diac_to_idx").members) i2d[id.i] = diac;
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(i2d.size()); ++i) {
        auto it = i2d.find(i);
        if (it == i2d.end()) throw std::out_of_range("diac_to_idx has no class " + std::to_string(i));
        impl_->classes.push_back(it->second);
        const std::u32string c = detail::decode_utf8(it->second);
        impl_->class_marks.emplace_back(c.begin(), c.end());
        impl_->class_len.push_back(c.size());
    }

    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    opts.SetInterOpNumThreads(1);
    opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    impl_->session = Ort::Session(ort_env(), model_path.c_str(), opts);
}

EnsembleDiacritizer::~EnsembleDiacritizer() = default;

const std::vector<std::string>& EnsembleDiacritizer::classes() const { return impl_->classes; }

std::int64_t EnsembleDiacritizer::unk() const { return impl_->unk; }

std::string EnsembleDiacritizer::normalize(std::string_view text) { return detail::encode_utf8(normalize32(text)); }

std::string EnsembleDiacritizer::bare(std::string_view text) { return detail::encode_utf8(bare32(text)); }

std::vector<std::int64_t> EnsembleDiacritizer::encode(std::string_view bare) const {
    std::vector<std::int64_t> ids;
    for (char32_t c : detail::decode_utf8(bare)) {
        auto it = impl_->c2i.find(c);
        ids.push_back(it == impl_->c2i.end() ? impl_->unk : it->second);
    }
    return ids;
}

Logits EnsembleDiacritizer::logits(std::string_view text) const {
    Logits out;
    out.bare = bare(text);
    if (out.bare.empty()) return out;
    std::vector<std::int64_t> ids = encode(out.bare);
    const std::array<std::int64_t, 2> shape{1, static_cast<std::int64_t>(ids.size())};
    const auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input = Ort::Value::CreateTensor<std::int64_t>(mem, ids.data(), ids.size(), shape.data(), shape.size());
    const char* in_names[] = {"input"};
    const char* out_names[] = {"gated_logits"};
    auto result = impl_->session.Run(Ort::RunOptions{nullptr}, in_names, &input, 1, out_names, 1);
    const auto info = result[0].GetTensorTypeAndShapeInfo();
    const std::vector<std::int64_t> dims = info.GetShape();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || dims.size() != 3 || dims[0] != 1 ||
        dims[1] != shape[1] || dims[2] != static_cast<std::int64_t>(impl_->classes.size()))
        throw std::runtime_error("gated_logits has an unexpected type or shape");
    out.rows = static_cast<std::size_t>(dims[1]);
    out.cols = static_cast<std::size_t>(dims[2]);
    const float* data = result[0].GetTensorData<float>();
    out.values.assign(data, data + out.rows * out.cols);
    return out;
}

std::string EnsembleDiacritizer::decode(std::string_view bare, const std::vector<std::int64_t>& class_ids) const {
    const std::u32string b = detail::decode_utf8(bare);
    std::u32string out;
    const std::size_t n = std::min(b.size(), class_ids.size());
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(b[i]);
        if (!detail::is_letter(b[i])) continue;
        const std::int64_t p = class_ids[i];
        if (p < 0 || p >= static_cast<std::int64_t>(impl_->classes.size()))
            throw std::out_of_range("no diacritic class " + std::to_string(p));
        out += detail::decode_utf8(impl_->classes[static_cast<std::size_t>(p)]);
    }
    return detail::encode_utf8(detail::nfc(out));
}

std::vector<std::int64_t> EnsembleDiacritizer::constrained_argmax(std::string_view text, std::string_view bare,
                                                                  const Logits& logits) const {
    // orthography.source_marks(text, self._normalize).
    std::u32string src_bare;
    std::vector<MarkSet> src_marks;
    for (char32_t c : normalize32(text)) {
        if (detail::category(c) == Category::Mn) {
            if (!src_marks.empty()) src_marks.back().insert(c);
        } else {
            src_bare.push_back(c);
            src_marks.emplace_back();
        }
    }
    const std::u32string b = detail::decode_utf8(bare);
    if (src_bare != b || src_marks.size() != b.size()) return argmax(logits);

    std::vector<std::int64_t> out(b.size(), 0);
    for (std::size_t i = 0; i < b.size(); ++i) {
        if (!detail::is_letter(b[i])) continue;
        if (i >= logits.rows || logits.cols < impl_->classes.size())
            throw std::out_of_range("logits do not cover the bare text and the classes");
        const MarkSet& marks = src_marks[i];
        bool vocalic = false;
        for (char32_t m : marks)
            if (!is_orthographic(m)) vocalic = true;
        if (vocalic) {
            const std::int64_t pin = impl_->pinned_class(marks);
            if (pin >= 0) {
                out[i] = pin;
                continue;
            }
        }
        const std::vector<std::int64_t> allowed = impl_->allowed_classes(marks);
        if (allowed.empty()) continue;
        // Python max() keeps the first item whose key no later key exceeds.
        const float* row = logits.row(i);
        std::int64_t best = allowed[0];
        for (std::size_t k = 1; k < allowed.size(); ++k)
            if (row[allowed[k]] > row[best]) best = allowed[k];
        out[i] = best;
    }
    return out;
}

std::string EnsembleDiacritizer::diacritize(std::string_view text) const {
    const Logits l = logits(text);
    if (l.bare.empty()) return detail::encode_utf8(detail::decode_utf8(text));
    return decode(l.bare, constrained_argmax(text, l.bare, l));
}

const EnsembleDiacritizer& get_ensemble(std::optional<std::string> model_path,
                                        std::optional<std::string> vocab_path) {
    using Key = std::pair<std::optional<std::string>, std::optional<std::string>>;
    static std::mutex mu;
    static std::map<Key, std::unique_ptr<EnsembleDiacritizer>> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto& slot = cache[Key{model_path, vocab_path}];
    if (!slot)
        slot = std::make_unique<EnsembleDiacritizer>(model_path ? *model_path : default_ensemble_onnx(),
                                                     vocab_path ? *vocab_path : default_vocab());
    return *slot;
}

}  // namespace arbtok::ensemble
