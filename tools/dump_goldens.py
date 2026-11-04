"""Dump the reference arbtok's output as the C++ port's parity goldens.

Each module listed in MODULES has a dumper at tools/goldens/<module>.py with a
corpora() function. This script writes tests/goldens/<module>.jsonl.gz for each
one, and prints the count of inputs and of records for each corpus.
tests/parity.cpp recomputes every record in C++ and compares the bytes.

Usage, with the reference interpreter, the tests/ directory of the arbtok
0.0.2a71 sdist and the data/ directory of the arbtok repository at the commit
of that release:
    python -I tools/dump_goldens.py --out-dir tests/goldens --sdist-tests <sdist>/tests --gold-dir <repo>/data [module ...]
"""
import argparse
import importlib
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from goldens import common  # noqa: E402

MODULES = [
    "constants",
    "orthography",
    "tashkeel",
    "nisba",
    "waqf",
    "arabizi",
    "unicode_symbol2label",
    "textnorm",
    "dialects",
    "stress",
    "sandhi",
    "lattice",
    "tokenizer",
    "version",
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--sdist-tests", required=True)
    parser.add_argument("--gold-dir", required=True)
    parser.add_argument("modules", nargs="*", default=MODULES)
    args = parser.parse_args()
    common.SOURCES["sdist_tests"] = args.sdist_tests
    common.SOURCES["gold_dir"] = args.gold_dir
    total = 0
    for name in args.modules:
        module = importlib.import_module(f"goldens.{name}")
        total += common.write(str(pathlib.Path(args.out_dir) / f"{name}.jsonl.gz"), name, module.corpora())
    print(f"total: {total} records", file=sys.stderr)


if __name__ == "__main__":
    main()
