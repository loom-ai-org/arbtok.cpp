// The exception class that a caller catches is the one that stands for the
// Python exception, also when the number and date layer raises it.
#include <cstdio>
#include <functional>
#include <string>

#include "arbtok/errors.hpp"
#include "arbtok/textnorm.hpp"
#include "arbtok/util.hpp"

namespace {

int failures = 0;

void expect_not_implemented(const char* what, const std::function<void()>& call) {
    try {
        call();
        std::printf("FAIL %s: no exception\n", what);
    } catch (const arbtok::NotImplementedError&) {
        std::printf("ok   %s\n", what);
        return;
    } catch (const arbtok::Error& e) {
        std::printf("FAIL %s: arbtok::Error with python_type %s, not arbtok::NotImplementedError\n", what,
                    e.python_type().c_str());
    } catch (const std::exception& e) {
        std::printf("FAIL %s: %s\n", what, e.what());
    }
    ++failures;
}

}  // namespace

int main() {
    expect_not_implemented("util::normalize of a number in English", [] { arbtok::util::normalize("5", "en"); });
    expect_not_implemented("util::pronounce_time in English", [] { arbtok::util::pronounce_time("15h01", "en"); });
    expect_not_implemented("textnorm::normalize_for_tts of a number in English", [] {
        arbtok::textnorm::TtsNorm config;
        config.cardinal_numbers = false;
        arbtok::textnorm::normalize_for_tts("5", "en", config);
    });
    return failures == 0 ? 0 : 1;
}
