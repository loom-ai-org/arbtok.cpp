# arbtok.cpp

A C++17 port of [arbtok](https://github.com/TigreGotico/arbtok), rule-based Arabic text-to-IPA with tashkeel diacritization. For each function it ports, the port gives the same bytes as the Python package. `PARITY.md` names the reference versions, lists the whole public surface with the status of each name, and records the gaps.

The port holds the modules that need no model and no number words: constants, orthography, tashkeel (its error class), nisba, waqf, arabizi, unicode_symbol2label, stress, sandhi, lattice, tokenizer, dialects, version, and the language test of textnorm. The diacritizer, the lexicons, text normalization and the plugins are not ported.

## Build

The build needs CMake 3.16 or later, a C++17 compiler, zlib, ICU and the Boost headers. orthography2ipa.cpp is a submodule, so clone with `--recurse-submodules` or run `git submodule update --init --recursive`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -j 4 --output-on-failure
```

When the Boost headers are not in a system path, add `-DBoost_INCLUDE_DIR=<dir>` to the first command.

`cmake --install build` installs the library, the headers, the data files under `share/arbtok`, and the CMake package. A project then uses it with `find_package(arbtok)` and links `arbtok::arbtok`. `tests/package_consumer` is such a project.

## Use

```cpp
#include <arbtok/arbtok.hpp>

arbtok::tokenizer::Sentence sentence("كَتَبَ الْوَلَدُ", "ar");
sentence.ipa();                     // "ˈkataba lˈwaladu"
arbtok::spec_for_lang("ar-EG");     // "arz-EG"
```

A function raises the C++ class that stands for the Python exception: `arbtok::ValueError`, `arbtok::KeyError` and so on. Each is an `arbtok::Error`, and `python_type()` gives the Python class name. A Python warning goes to the handler that `arbtok::set_warning_handler` installs.

## Tests

`tests/goldens/<module>.jsonl.gz` holds the output of the reference package for each ported module. `arbtok_parity` recomputes every record in C++ and compares the bytes. ctest runs one test per module and splits dialects into eight shards. `tests/parity/expected_divergences.tsv` lists the records that differ because orthography2ipa.cpp ports the lineage data of linguatag 0.1.3a1 and the goldens use linguatag 0.1.5a1. Each entry gives the C++ line that the record must give. A listed record that gives another C++ line, or that starts to match Python, fails its test.

To dump the goldens again, install the reference versions that `PARITY.md` names into a venv, then run the dumper with that interpreter:

```sh
python -I tools/dump_goldens.py --out-dir tests/goldens \
    --sdist-tests <arbtok-0.0.2a71 sdist>/tests --gold-dir <arbtok repository at the gold commit>/data
```

The dumper stops when an installed reference package has another version than `PARITY.md` names. It prints the input count and the record count of each corpus. It counts the records again in each file it writes, and stops when the counts differ. `tools/inventory.py` writes the surface table of `PARITY.md` and fails when a name marked as ported is missing from its header.
