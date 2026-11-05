// arbtok.tashkeel: diacritization with the bundled rawi ensemble, and the
// error it raises.
#pragma once

#include <string>

#include "arbtok/errors.hpp"

namespace arbtok::ensemble {
class EnsembleDiacritizer;
}  // namespace arbtok::ensemble

namespace arbtok::tashkeel {

// TashkeelError(Exception): what() is str(error).
struct TashkeelError : Error {
    explicit TashkeelError(const std::string& message) : Error("TashkeelError", message) {}
};

// A wrapper over the shared bundled ensemble. The constructor loads it.
class TashkeelDiacritizer {
public:
    TashkeelDiacritizer();

    // text with its diacritics restored; with pausal, in its pausal form
    // (waqf::pausal). A failure of the model is a TashkeelError.
    std::string diacritize(const std::string& text, bool pausal = false) const;
    std::string operator()(const std::string& text, bool pausal = false) const { return diacritize(text, pausal); }

private:
    const ensemble::EnsembleDiacritizer* diacritizer_;
};

}  // namespace arbtok::tashkeel
