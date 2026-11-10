"""Record the calls that the arbtok test suite makes into the text normalizers.

The script wraps arbtok.textnorm.normalize_asr, arbtok.textnorm.normalize_for_tts
and arbtok.util.normalize, runs pytest over a copy of the tests/ directory of the
arbtok sdist, and writes each unique call to a gzip file of JSON lines:

    ["normalize_asr", text, CONFIG, lang]
    ["normalize_for_tts", text, lang, CONFIG]
    ["normalize", text, lang]

CONFIG is the config after the flags of the call are applied, encoded as
tools/goldens/textnorm.py encodes it. A call whose text or lang is not a str,
or whose flags do not make a config, is not recorded. normalize_asr and
normalize_for_tts are recorded only where a test calls them, not where they
call themselves. util.normalize is recorded at every depth, because
normalize_for_tts calls it. The lines are written sorted.

Run it with an interpreter that has the reference packages, pytest, pandas and
pyarrow, and with network access to the Hugging Face Hub. Without them the live
test of tests/test_iqraeval_benchmark.py skips, and its calls are not recorded:
    python -I tools/goldens/record_calls.py --sdist-tests <sdist>/tests --overlay <repo at the gold commit> --scratch <dir> --out tests/data/textnorm_calls.jsonl.gz

The sdist leaves out tests/voice_agent.py and other helpers that some tests
import; --overlay adds them, and the gold data, from the repository at the gold commit.
"""
import argparse
import dataclasses
import gzip
import inspect
import io
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

from goldens import common  # noqa: E402
from goldens.textnorm import encode_config  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdist-tests", required=True)
    parser.add_argument("--scratch", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--overlay", help="the arbtok repository at the gold commit: its tests/ files that the "
                        "sdist leaves out, its scripts/, data/ and requirements.txt are added to the copy")
    args = parser.parse_args()

    import pytest
    from arbtok import textnorm, util

    calls = set()
    depth = [0]

    def wrap_top(fn, kind):
        signature = inspect.signature(fn)

        def wrapper(*a, **kw):
            if depth[0] == 0:
                try:
                    bound = signature.bind(*a, **kw)
                    bound.apply_defaults()
                    text = bound.arguments["text"]
                    lang = bound.arguments["lang"]
                    flags = dict(bound.arguments.get("flags", {}))
                    default = textnorm._NOTHING if kind == "normalize_asr" else textnorm._PLUGIN_DEFAULT
                    config = bound.arguments["config"] or default
                    if "lexicon" not in flags and isinstance(text, str) and isinstance(lang, str):
                        if flags:
                            config = dataclasses.replace(config, **flags)
                        encoded = encode_config(config)
                        record = [kind, text, encoded, lang] if kind == "normalize_asr" else \
                            [kind, text, lang, encoded]
                        calls.add(common.dumps(record))
                except Exception:
                    pass
            depth[0] += 1
            try:
                return fn(*a, **kw)
            finally:
                depth[0] -= 1
        return wrapper

    original_util = util.normalize

    def util_wrapper(text, lang):
        if isinstance(text, str) and isinstance(lang, str):
            calls.add(common.dumps(["normalize", text, lang]))
        return original_util(text, lang)

    import arbtok
    textnorm.normalize_asr = arbtok.normalize_asr = wrap_top(textnorm.normalize_asr, "normalize_asr")
    textnorm.normalize_for_tts = arbtok.normalize_for_tts = wrap_top(textnorm.normalize_for_tts, "normalize_for_tts")
    util.normalize = util_wrapper

    # The tests import their helpers as the package "tests", so its parent is on the path.
    scratch = pathlib.Path(args.scratch) / "tests"
    shutil.copytree(args.sdist_tests, scratch)
    if args.overlay:
        overlay = pathlib.Path(args.overlay)
        for path in sorted((overlay / "tests").iterdir()):
            if not (scratch / path.name).exists():
                (shutil.copytree if path.is_dir() else shutil.copy2)(path, scratch / path.name)
                print(f"added {path.name} from the overlay", file=sys.stderr)
        shutil.copytree(overlay / "scripts", scratch.parent / "scripts")
        shutil.copy2(overlay / "requirements.txt", scratch.parent / "requirements.txt")
        if (overlay / "data").is_dir():
            shutil.copytree(overlay / "data", scratch.parent / "data")
    sys.path.insert(0, str(scratch.parent))
    status = pytest.main([str(scratch), "-q", "-p", "no:cacheprovider", "--rootdir", str(scratch.parent),
                          "--continue-on-collection-errors"])
    print(f"pytest exit status {int(status)}", file=sys.stderr)

    lines = sorted(calls)
    raw = io.BytesIO()
    with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as gz:
        for line in lines:
            gz.write((line + "\n").encode("utf-8"))
    pathlib.Path(args.out).write_bytes(raw.getvalue())
    counts = {}
    for line in lines:
        kind = line[2:line.index('"', 2)]
        counts[kind] = counts.get(kind, 0) + 1
    print(f"{args.out}: {len(lines)} calls {counts}", file=sys.stderr)


if __name__ == "__main__":
    main()
