// The exception class that a caller catches is the one that stands for the
// Python exception, also when the number and date layer raises it. Where
// Python's `except Exception` catches what the reference libraries raise for a
// language they reject, the call gives Python's value. Where the reference
// speaks with the module of another language, the call raises NotPortedError.
// The expected values are the outputs of arbtok 0.0.2a74 under the pinned
// reference libraries.
#include <cstdio>
#include <functional>
#include <string>

#include "arbtok/errors.hpp"
#include "arbtok/textnorm.hpp"
#include "arbtok/util.hpp"

namespace {

int failures = 0;

template <class Expected>
void expect_raises(const char* what, const char* expected, const std::function<void()>& call) {
    try {
        call();
        std::printf("FAIL %s: no exception\n", what);
    } catch (const Expected&) {
        std::printf("ok   %s\n", what);
        return;
    } catch (const arbtok::Error& e) {
        std::printf("FAIL %s: arbtok::Error with python_type %s, not %s\n", what, e.python_type().c_str(), expected);
    } catch (const std::exception& e) {
        std::printf("FAIL %s: %s, not %s\n", what, e.what(), expected);
    }
    ++failures;
}

void expect_value(const char* what, const std::string& expected, const std::function<std::string()>& call) {
    try {
        const std::string got = call();
        if (got == expected) {
            std::printf("ok   %s\n", what);
            return;
        }
        std::printf("FAIL %s: got \"%s\", want \"%s\"\n", what, got.c_str(), expected.c_str());
    } catch (const std::exception& e) {
        std::printf("FAIL %s: raised %s, want \"%s\"\n", what, e.what(), expected.c_str());
    }
    ++failures;
}

arbtok::textnorm::TtsNorm leave_unspeakable() {
    arbtok::textnorm::TtsNorm config;
    config.cardinal_numbers = true;
    config.leave_unspeakable_numbers = true;
    return config;
}

const arbtok::numbers::ovos_date_parser::DateTime kDate{2025, 4, 30};

}  // namespace

int main() {
    using arbtok::NotImplementedError;
    using arbtok::NotPortedError;
    namespace util = arbtok::util;
    namespace textnorm = arbtok::textnorm;

    expect_raises<NotPortedError>("util::normalize of a number in English", "NotPortedError",
                                  [] { util::normalize("5", "en"); });
    expect_raises<NotPortedError>("util::pronounce_time in English", "NotPortedError",
                                  [] { util::pronounce_time("15h01", "en"); });
    expect_raises<NotPortedError>("textnorm::normalize_for_tts of a number in English", "NotPortedError", [] {
        textnorm::TtsNorm config;
        config.cardinal_numbers = false;
        textnorm::normalize_for_tts("5", "en", config);
    });

    expect_raises<NotImplementedError>("util::pronounce_date in xx", "arbtok::NotImplementedError",
                                       [] { util::pronounce_date(kDate, "xx"); });
    expect_raises<NotImplementedError>("util::pronounce_date in arz", "arbtok::NotImplementedError",
                                       [] { util::pronounce_date(kDate, "arz"); });

    expect_value("util::normalize of 749 in ur", "749", [] { return util::normalize("749", "ur"); });
    expect_value("util::normalize of 749 in mey-MR", "749", [] { return util::normalize("749", "mey-MR"); });
    expect_value("util::normalize of 749 in ar_SA", "749", [] { return util::normalize("749", "ar_SA"); });
    expect_value("util::normalize of 749 in xx", "749", [] { return util::normalize("749", "xx"); });
    expect_value("util::normalize of a date in arz", "30/4/2025", [] { return util::normalize("30/4/2025", "arz"); });
    expect_value("util::pronounce_time in AR", "5 42", [] { return util::pronounce_time("5h42", "AR"); });
    expect_value("util::pronounce_time in apc", "15 01", [] { return util::pronounce_time("15h01", "apc"); });
    expect_value("util::pronounce_time in ar_SA", "خمسة عشر ودقيقة",
                 [] { return util::pronounce_time("15h01", "ar_SA"); });
    expect_value("textnorm::normalize_for_tts leaves 83731 in ur", "83731",
                 [] { return textnorm::normalize_for_tts("83731", "ur", leave_unspeakable()); });
    expect_value("textnorm::normalize_for_tts speaks 83731 in mey-MR", "ثلاثة وثمانون ألف وسبعمئة وواحد وثلاثون",
                 [] { return textnorm::normalize_for_tts("83731", "mey-MR", leave_unspeakable()); });
    return failures == 0 ? 0 : 1;
}
