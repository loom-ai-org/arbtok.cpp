// arbtok._ensemble: the bundled rawi ensemble, run through onnxruntime.
//
// Text is UTF-8. Ill-formed input decodes as bytes.decode("utf-8", "replace")
// does. A class is the UTF-8 string of a whole combining-mark sequence, and a
// class id indexes classes().
//
// The session runs on the CPU provider with one intra-op thread, one inter-op
// thread, sequential execution and ORT_ENABLE_ALL, the settings of the Python
// session, so that the logits agree with the Python ones.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arbtok::ensemble {

// DEFAULT_ENSEMBLE_ONNX and DEFAULT_VOCAB. The files are read from
// $ARBTOK_DATA_DIR/models when that directory holds them, else from the data
// directory of the source tree.
std::string default_ensemble_onnx();
std::string default_vocab();

// The distribution that EnsembleDiacritizer::logits returns: one row of
// classes().size() float32 scores for each code point of bare.
struct Logits {
    std::string bare;
    // Row-major rows x cols. Empty, with rows == 0, where Python returns None.
    std::vector<float> values;
    std::size_t rows = 0;
    std::size_t cols = 0;

    bool empty() const { return rows == 0; }
    const float* row(std::size_t i) const { return values.data() + i * cols; }
};

// numpy logits.argmax(-1): the first maximum of each row, or the first NaN.
std::vector<std::int64_t> argmax(const Logits& logits);

class EnsembleDiacritizer {
public:
    explicit EnsembleDiacritizer(const std::string& model_path = default_ensemble_onnx(),
                                 const std::string& vocab_path = default_vocab());
    ~EnsembleDiacritizer();
    EnsembleDiacritizer(const EnsembleDiacritizer&) = delete;
    EnsembleDiacritizer& operator=(const EnsembleDiacritizer&) = delete;

    // The diacritic-class strings, indexed by class id.
    const std::vector<std::string>& classes() const;

    // c2i.get("<UNK>", 1).
    std::int64_t unk() const;

    // _normalize: NFD, with every character of category So removed.
    static std::string normalize(std::string_view text);

    // _bare: normalize(text) with every character of category Mn removed.
    static std::string bare(std::string_view text);

    // The input ids that logits() builds from bare: c2i.get(c, unk) for each
    // code point. The tensor is int64 with shape [1, size].
    std::vector<std::int64_t> encode(std::string_view bare) const;

    // logits(text): the raw gated_logits for bare(text).
    Logits logits(std::string_view text) const;

    // decode(bare, class_ids): each letter (category L*) followed by its
    // class, other characters unchanged, then NFC. Pairs stop at the shorter
    // input. An id outside the classes on a letter throws std::out_of_range,
    // where Python raises KeyError.
    std::string decode(std::string_view bare, const std::vector<std::int64_t>& class_ids) const;

    // _constrained_argmax(text, bare, logits).
    std::vector<std::int64_t> constrained_argmax(std::string_view text, std::string_view bare,
                                                 const Logits& logits) const;

    // diacritize(text): text unchanged when bare(text) is empty, else the
    // decoded constrained argmax.
    std::string diacritize(std::string_view text) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// get_ensemble(model_path, vocab_path): one shared diacritizer per pair of
// arguments. std::nullopt selects the bundled file and is a different key
// from its explicit path, as None is in Python. Safe to call from many
// threads.
const EnsembleDiacritizer& get_ensemble(std::optional<std::string> model_path = std::nullopt,
                                        std::optional<std::string> vocab_path = std::nullopt);

// The onnxruntime version that the library runs.
std::string onnxruntime_version();

}  // namespace arbtok::ensemble
