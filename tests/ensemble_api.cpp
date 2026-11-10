// The parts of the ensemble API that the goldens do not reach: the cache of
// get_ensemble, errors, and argmax on NaN.

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "arbtok/ensemble.hpp"

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << what << "\n";
    }
}

template <typename F>
bool throws(F f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    using namespace arbtok::ensemble;

    const EnsembleDiacritizer* shared = &get_ensemble();
    const EnsembleDiacritizer& a = *shared;
    expect(shared == &get_ensemble(std::nullopt, std::nullopt), "get_ensemble() and get_ensemble(None, None) share one reader");
    const std::string model = default_ensemble_onnx();
    const EnsembleDiacritizer* explicit_path = &get_ensemble(model);
    expect(shared != explicit_path, "an explicit model path is a different cache key from None");
    expect(explicit_path == &get_ensemble(model), "the same explicit path returns the same reader");
    expect(a.classes().size() == 75, "75 classes");
    expect(a.classes().at(0).empty(), "class 0 is the empty mark sequence");

    expect(throws([&] { a.decode("ك", {75}); }), "decode throws on a class id outside the classes on a letter");
    expect(!throws([&] { a.decode("1", {75}); }), "decode ignores the class id of a non-letter");
    expect(a.decode("كك", {24}) == "كَ", "decode stops at the shorter input");
    expect(throws([] { EnsembleDiacritizer("/nonexistent/model.onnx"); }), "a missing model throws");
    expect(throws([] { EnsembleDiacritizer(default_ensemble_onnx(), "/nonexistent/vocab.json"); }), "a missing vocabulary throws");

    Logits l;
    l.rows = 1;
    l.cols = 4;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    l.values = {1.0f, nan, 3.0f, nan};
    expect(argmax(l) == std::vector<std::int64_t>{1}, "argmax picks the first NaN, as numpy does");
    l.values = {2.0f, 5.0f, 5.0f, -1.0f};
    expect(argmax(l) == std::vector<std::int64_t>{1}, "argmax picks the first maximum");

    // Synthetic rows with exact ties and NaN, which the model does not emit on
    // the corpus. The expected ids come from EnsembleDiacritizer._constrained_argmax
    // of arbtok 0.0.2a74 on the same rows.
    const auto constrained = [&](const std::string& text, auto fill) {
        Logits s;
        s.bare = EnsembleDiacritizer::bare(text);
        s.rows = 1;
        s.cols = a.classes().size();
        s.values.assign(s.cols, 0.0f);
        fill(s.values);
        return a.constrained_argmax(text, s.bare, s);
    };
    using Row = std::vector<float>;
    const auto tie = [](Row& r) {
        r.assign(r.size(), -1.0f);
        r[30] = r[5] = r[60] = 2.0f;
    };
    expect(constrained("ك", tie) == std::vector<std::int64_t>{5}, "a tie keeps the first allowed class");
    expect(constrained("ك", [&](Row& r) { r[0] = nan; r[7] = 3.0f; }) == std::vector<std::int64_t>{0},
           "a NaN first class is kept, as Python max() keeps it");
    expect(constrained("ك", [&](Row& r) { r[3] = 1.0f; r[9] = nan; r[40] = 1.0f; }) == std::vector<std::int64_t>{3},
           "a later NaN never wins");
    expect(constrained("أ", [](Row& r) { r[36] = r[13] = 4.0f; }) == std::vector<std::int64_t>{13},
           "a tie among the hamza classes keeps the first");
    expect(constrained("آ", tie) == std::vector<std::int64_t>{67}, "a madda alif takes the madda class only");

    expect(a.logits("").empty() && a.logits("...").rows == 3, "logits of empty and punctuation-only text");
    expect(a.diacritize("") == "" && a.diacritize("َ") == "َ", "text with no base is returned as given");
    expect(a.diacritize("\xff") == "\xef\xbf\xbd", "ill-formed UTF-8 decodes to U+FFFD");

    std::cout << (failures ? "FAILED " : "OK ") << failures << " failures\n";
    return failures ? 1 : 0;
}
