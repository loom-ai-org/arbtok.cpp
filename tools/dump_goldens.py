"""Dump the reference arbtok's output as the C++ port's parity goldens.

Each module listed in MODULES has a dumper at tools/goldens/<module>.py with a
corpora() function. This script writes tests/goldens/<module>.jsonl.gz for each
one, and prints the count of inputs and of records for each corpus.
tests/parity.cpp recomputes every record in C++ and compares the bytes.

Usage, with the reference interpreter, the tests/ directory of the arbtok
0.0.2a71 sdist and the data/ directory of the arbtok repository at the commit
of that release:
    python -I tools/dump_goldens.py --out-dir tests/goldens --sdist-tests <sdist>/tests --gold-dir <repo>/data \
        --corpus <repo>/tests/ar_test.txt [module ...]

The lexicon, diacritize and fusion goldens read the default stem lexicon from
the Hugging Face cache. Run with HF_HUB_OFFLINE=1 so that the cached revision,
which the lexicon goldens record, is the one in use.

The stem lexicon is GPL-2.0-derived data, and the goldens must not hold it. A
second interpreter dumps the same corpora with a stem lexicon that has a
header and no entries, in a Hugging Face cache of its own. A record that the
two runs write differently depends on the stem lexicon, and the golden holds
the SHA-256 of its line in place of its result (see common.write).

A golden whose dump runs the onnxruntime session of the ensemble names the
CPU class of this machine in its header, and holds output of that class only.
tools/dump_model_goldens.sh dumps those goldens again on another machine.
"""
import argparse
import importlib
import os
import pathlib
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import onnxruntime  # noqa: E402

from goldens import common  # noqa: E402
from goldens.cpu_class import cpu_class  # noqa: E402

SESSION_RUNS = [0]


def _counted(run):
    def counted(*args, **kwargs):
        SESSION_RUNS[0] += 1
        return run(*args, **kwargs)
    return counted


onnxruntime.InferenceSession.run = _counted(onnxruntime.InferenceSession.run)

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
    "util",
    "pyre",
    "lexicon",
    "dialect_lexicon",
    "donor_lexicon",
    "diacritize",
    "fusion",
    "translit",
    "plugin",
    "o2i_plugins",
    "phoonnx",
]


def empty_stem_cache(root):
    """A Hugging Face cache under root whose default stem lexicon has the header row of the real one and no entries."""
    from arbtok import lexicon as lx
    real = lx.resolve_source(lx.DEFAULT_LEXICON)
    if len(lx.StemLexicon()) == 0:
        raise SystemExit(f"the stem lexicon at {real} has no entries")
    org, name, filename = lx.DEFAULT_LEXICON[len("hf://"):].split("/", 2)
    repo = root / "hub" / f"datasets--{org}--{name}"
    snapshot = repo / "snapshots" / real.parent.name / filename
    snapshot.parent.mkdir(parents=True)
    (repo / "refs").mkdir()
    (repo / "refs" / "main").write_text(real.parent.name)
    with real.open(encoding="utf-8", newline="") as fh:
        snapshot.write_text(fh.readline(), encoding="utf-8", newline="")
    return root / "hub"


def stem_free_pass(args, root):
    """Start the dump with the empty stem lexicon in a child interpreter; it writes <root>/out/<module>.lines."""
    out = root / "out"
    out.mkdir()
    env = dict(os.environ, HF_HUB_CACHE=str(empty_stem_cache(root)), HF_HUB_OFFLINE="1")
    cmd = [sys.executable, "-I", __file__, "--out-dir", str(out), "--sdist-tests", args.sdist_tests,
           "--gold-dir", args.gold_dir, "--stem-free"]
    if args.corpus:
        cmd += ["--corpus", args.corpus]
    log = (root / "stem-free.log").open("w")
    return subprocess.Popen(cmd + args.modules, env=env, stdout=log, stderr=subprocess.STDOUT), out


def stem_free_lines(child, out, module):
    done = out / f"{module}.done"
    while not done.exists():
        if child.poll() is not None and not done.exists():
            raise SystemExit(f"the dump with the empty stem lexicon stopped with exit code {child.returncode}; "
                             f"see {out.parent / 'stem-free.log'}")
        time.sleep(1)
    return (out / f"{module}.lines").read_text(encoding="utf-8").split("\n")[:-1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--sdist-tests", required=True)
    parser.add_argument("--gold-dir", required=True)
    parser.add_argument("--corpus", help="tests/ar_test.txt of the arbtok repository at the gold commit")
    parser.add_argument("--stem-free", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("modules", nargs="*", default=MODULES)
    args = parser.parse_args()
    common.SOURCES["sdist_tests"] = args.sdist_tests
    common.SOURCES["gold_dir"] = args.gold_dir
    common.SOURCES["corpus"] = args.corpus
    if args.stem_free:
        from arbtok import lexicon as lx
        if len(lx.StemLexicon()) != 0:
            raise SystemExit("the stem-free dump reads a stem lexicon that has entries")
        for name in args.modules:
            module = importlib.import_module(f"goldens.{name}")
            common.write_lines(pathlib.Path(args.out_dir) / f"{name}.lines", module.corpora())
            (pathlib.Path(args.out_dir) / f"{name}.done").touch()
        return
    total = 0
    with tempfile.TemporaryDirectory() as tmp:
        child, out = stem_free_pass(args, pathlib.Path(tmp))
        try:
            for name in args.modules:
                module = importlib.import_module(f"goldens.{name}")
                runs = SESSION_RUNS[0]
                corpora = module.corpora()
                cpu = cpu_class() if SESSION_RUNS[0] > runs else None
                total += common.write(str(pathlib.Path(args.out_dir) / f"{name}.jsonl.gz"), name, corpora,
                                      stem_free_lines(child, out, name), cpu)
            if child.wait() != 0:
                raise SystemExit(f"the dump with the empty stem lexicon stopped with exit code {child.returncode}")
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
    print(f"total: {total} records", file=sys.stderr)


if __name__ == "__main__":
    main()
