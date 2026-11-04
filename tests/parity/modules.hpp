// One parity module per Python module: tests/parity/<module>.cpp defines
// parity::record_<module>, which recomputes one golden record in C++, and
// tests/goldens/<module>.jsonl.gz holds the Python records.
#pragma once

#include <string>

#include "../json_line.hpp"

// One line per module.
#define ARBTOK_PARITY_MODULES(X) \
    X(constants)                 \
    X(orthography)               \
    X(tashkeel)                  \
    X(nisba)                     \
    X(waqf)                      \
    X(arabizi)                   \
    X(unicode_symbol2label)      \
    X(textnorm)                  \
    X(dialects)                  \
    X(stress)                    \
    X(sandhi)                    \
    X(lattice)                   \
    X(tokenizer)                 \
    X(version)                   \
    X(lexicon)                   \
    X(dialect_lexicon)           \
    X(donor_lexicon)             \
    X(diacritize)                \
    X(fusion)

namespace parity {
#define ARBTOK_PARITY_DECLARE(name) std::string record_##name(const json_line::Value& line);
ARBTOK_PARITY_MODULES(ARBTOK_PARITY_DECLARE)
#undef ARBTOK_PARITY_DECLARE
}  // namespace parity
