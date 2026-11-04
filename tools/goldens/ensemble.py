"""Dump the ensemble goldens from the pinned arbtok release.

Every value in the output comes from the installed Python package: the input
encoding, the logits of onnxruntime, the decisions of _ensemble.py and the
skeleton guards of diacritize.py. The C++ port never writes a golden.

Usage:
  python tools/goldens/ensemble.py --tests-dir <arbtok tests at the pinned tag> \
      --out tests/goldens/ensemble.jsonl.gz [--full-logits-out <path>]

The corpus is a deterministic sample of tests/ar_test.txt (every 42nd line),
every Arabic string literal in tests/*.py, edge cases, a seeded fuzz set and
ill-formed UTF-8. Every record holds the decisions and the SHA-256 of its
logits. --out also keeps the logits of every LOGITS_STRIDE[kind]-th record of a
kind whose bare text has at most LOGITS_MAX_ROWS characters, which keeps the
file small. --full-logits-out writes the same records with the logits of all
of them.
"""

import argparse
import ast
import base64
import gzip
import hashlib
import json
import random
import sys
import unicodedata
from importlib.metadata import version
from pathlib import Path

import numpy as np

from arbtok import _ensemble
from arbtok import diacritize as dz
from arbtok._ensemble import EnsembleDiacritizer, get_ensemble

CORPUS_STEP = 42
LOGITS_STRIDE = {"corpus": 40, "corpus-line": 40, "tests-py": 20, "fuzz": 2, "edge": 1, "bytes": 1}
LOGITS_MAX_ROWS = 80
FUZZ_SEED = 20260
FUZZ_COUNT = 3000


def has_arabic(s):
    return dz._has_arabic(s)


def corpus_records(tests_dir):
    lines = (tests_dir / "ar_test.txt").read_text(encoding="utf-8").split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    sampled = lines[::CORPUS_STEP]
    out = []
    for n, line in enumerate(sampled):
        word = line.split("\t", 1)[0]
        out.append(("corpus", word))
        if n % 4 == 0:
            out.append(("corpus-line", line))
    return len(lines), len(sampled), out


def tests_py_records(tests_dir):
    seen = {}
    files = sorted(tests_dir.glob("*.py"))
    for f in files:
        tree = ast.parse(f.read_text(encoding="utf-8"), filename=str(f))
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, str) and has_arabic(node.value):
                seen.setdefault(node.value, None)
    return len(files), [("tests-py", s) for s in seen]


def edge_records():
    word = "كتاب"
    cases = [
        "", " ", "  \t ", "\n", ".", "...!", "؟،؛", "«»", "123", "١٢٣", "٫٬",
        "hello", "hello world", "Café", "café", "naïve", "ÅΩ", "ﬁ",
        "hello كتاب 123", "كتاب hello", "the كتاب is new", "E=mc² كتاب",
        word, "كِتَاب", "كِتَابٌ", "الكِتَابُ", "كتابٌ", "مُدَرِّسَةٌ", "مدرّسة",
        "آبد", "أبد", "إسلام", "ٱلكتاب", "قرآن", "آَ", "أَحمد", "ءامن", "سؤال",
        "رئيس", "مسؤول", "شيء", "مائة", "إِ", "أُ", "آ", "آ", "أ", "إ",
        "َ", "َُ", "ِّ", "َ" + word, "كِِّ",
        "كِّ", "كِّ", "كًَ", "ك️", "كٰ", "كۡ",
        "ـ", "كـتـاب", "ﻻ", "ﷲ", "ﺑ", "‍", "ك‍تاب", "ك‌تاب",
        "☺ كتاب", "🙂كتاب🙂", "©", "♥♥", "كتاب ☪", " كتاب ",
        "한국어", "한", "각", "각", "豈", "Ω", "Å",
        "é̂", "ế", "ą́", "́a", "ḍ̇", "q̣̇",
        "Ǆ", "ǅ", "ǆ", "ʔaːbid", "/aːbid/", "ﻛﺘﺎﺏ", "ك̀", "ا̀",
        "عربي", "عَرَبِيّ", "مصري", "وكَان", "والكتاب", "بالكتاب", "فالكتاب",
        "الله", "إن شاء الله", "السلام عليكم ورحمة الله وبركاته",
        "ٱللَّٰه", "مُحَمَّد", "سَمَاوَات", "ئ", "ؤ", "ى", "ة", "ه", "ي",
        "x" * 300, "كتاب " * 600, ("كتب الولد الدرس في المدرسة " * 1200).strip(),
        "ك" * 5000, "َ" * 50, "aَ", "1َ", " َ", "؟َ",
        "هٔ", "ئ", "ؤ", "ىٔ", "ٰ", "كٓ", "كَٔ",
        "آَ", "أٓ", "إٔ", "كَّٔ",
        "\U0001F600َ", "\U0001F600ك", "\U00010000", "\U0010FFFF", "�",
        "\t\tكتاب\t", "كتاب\nكتاب", "كتاب\r\n",
    ]
    return [("edge", c) for c in cases]


def fuzz_records():
    rng = random.Random(FUZZ_SEED)
    letters = [chr(c) for c in range(0x0621, 0x064B) if unicodedata.category(chr(c)).startswith("L")]
    letters += ["ٱ", "پ", "چ", "ڤ", "گ", "ی", "ک", "ۀ", "ە"]
    marks = [chr(c) for c in range(0x064B, 0x0656)] + ["ٰ", "ۡ", "️", "̀", "́"]
    latin = list("abcdeéz") + ["é", "A", "ʔ", "ː"]
    other = list(" .,!?؟،0123456789٠١٢٣") + ["‍", "ـ", "☺", "🙂", "©", "ﻻ", " "]
    out = []
    for _ in range(FUZZ_COUNT):
        n = rng.randint(1, 14)
        parts = []
        for _ in range(n):
            r = rng.random()
            if r < 0.55:
                parts.append(rng.choice(letters))
            elif r < 0.85:
                parts.append(rng.choice(marks))
            elif r < 0.93:
                parts.append(rng.choice(latin))
            else:
                parts.append(rng.choice(other))
        out.append(("fuzz", "".join(parts)))
    return out


def byte_records():
    cases = [
        b"\xff", b"\xc3", b"\xc3\x28", b"\xe2\x82", b"\xe2\x28\xa1", b"\xf0\x9f\x98",
        b"\xed\xa0\x80", b"\xc0\xaf", b"\xf4\x90\x80\x80", b"\x80\x80",
        "كتاب".encode() + b"\xff" + "كتاب".encode(), b"\xd9" + "كتاب".encode(),
        "كتاب".encode()[:-1], b"\xf8\x88\x80\x80\x80", b"\xe0\x80\xaf",
    ]
    return [("bytes", b) for b in cases]


def guard_proposals(rng, text, proposed):
    """Proposals derived from the model's output that walk every guard path."""
    out = [proposed]
    letters = [i for i, c in enumerate(proposed) if c not in dz._MARKS]
    if letters:
        i = rng.choice(letters)
        repl = dz._RESTORABLE.get(proposed[i])
        if repl:
            out.append(proposed[:i] + sorted(repl)[0] + proposed[i + 1:])
        out.append(proposed[:i] + "ب" + proposed[i + 1:])
        out.append(proposed[:i] + proposed[i + 1:])
        out.append(proposed[:i] + "ي" + proposed[i:])
    for a, b in (("ا", "أ"), ("ا", "ب"), ("ه", "ة"), ("ي", "ى"), ("ة", "ه")):
        if a in proposed:
            out.append(proposed.replace(a, b, 1))
    out.append(proposed + "َ")
    seen = []
    for p in out:
        if p not in seen:
            seen.append(p)
    return [[p, dz._skeleton_is_preserved(text, p), dz.repair_skeleton(text, p)] for p in seen]


def build(ens, kind, raw, index, full, rng):
    text = raw.decode("utf-8", "replace") if isinstance(raw, bytes) else raw
    rec = {"kind": kind}
    if isinstance(raw, bytes):
        rec["bytes_hex"] = raw.hex()
    rec["text"] = text
    rec["normalized"] = EnsembleDiacritizer._normalize(text)
    bare, logits, classes = ens.logits(text)
    rec["bare"] = bare
    if logits is None:
        rec["ids"] = []
        rec["logits"] = None
        rec["argmax"] = None
        rec["constrained"] = None
        rec["decoded"] = None
    else:
        ids = np.array([[ens.c2i.get(c, ens.unk) for c in bare]], np.int64)
        assert ids.shape == (1, len(bare))
        rec["ids"] = ids[0].tolist()
        assert logits.dtype == np.float32 and logits.shape == (len(bare), len(classes))
        le = np.ascontiguousarray(logits, dtype="<f4").tobytes()
        rec["logits_sha256"] = hashlib.sha256(le).hexdigest()
        keep = full or (index % LOGITS_STRIDE[kind] == 0 and len(bare) <= LOGITS_MAX_ROWS)
        rec["logits"] = base64.b64encode(le).decode("ascii") if keep else None
        am = logits.argmax(-1)
        rec["argmax"] = am.tolist()
        con = ens._constrained_argmax(text, bare, logits)
        rec["constrained"] = con.tolist()
        rec["decoded"] = ens.decode(bare, am)
    rec["diacritized"] = ens.diacritize(text)
    rec["has_arabic"] = dz._has_arabic(text)
    rec["strip_marks"] = dz.strip_marks(text)
    rec["guards"] = guard_proposals(rng, text, rec["diacritized"])
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests-dir", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--full-logits-out", type=Path)
    args = ap.parse_args()

    ens = get_ensemble()
    assert get_ensemble() is ens and get_ensemble(None, None) is ens

    n_lines, n_sampled, corpus = corpus_records(args.tests_dir)
    n_files, tests_py = tests_py_records(args.tests_dir)
    inputs = corpus + tests_py + edge_records() + fuzz_records() + byte_records()
    print(f"ar_test.txt lines {n_lines}, sampled {n_sampled}; tests/*.py files {n_files}, "
          f"Arabic literals {len(tests_py)}", file=sys.stderr)

    header = {
        "kind": "header",
        "arbtok": version("arbtok"),
        "onnxruntime": version("onnxruntime"),
        "numpy": version("numpy"),
        "python": sys.version.split()[0],
        "unidata": unicodedata.unidata_version,
        "model_sha256": hashlib.sha256(Path(_ensemble.DEFAULT_ENSEMBLE_ONNX).read_bytes()).hexdigest(),
        "vocab_sha256": hashlib.sha256(Path(_ensemble.DEFAULT_VOCAB).read_bytes()).hexdigest(),
        "classes": ens.classes,
        "unk": ens.unk,
        "records": len(inputs),
    }

    outs = [(args.out, False)]
    if args.full_logits_out:
        outs.append((args.full_logits_out, True))
    for path, full in outs:
        rng = random.Random(FUZZ_SEED)
        counts = {}
        with gzip.open(path, "wt", encoding="utf-8", compresslevel=9) as f:
            f.write(json.dumps(header, ensure_ascii=False) + "\n")
            for kind, raw in inputs:
                rec = build(ens, kind, raw, counts.get(kind, 0), full, rng)
                f.write(json.dumps(rec, ensure_ascii=False) + "\n")
                counts[kind] = counts.get(kind, 0) + 1
        n_out = sum(counts.values())
        print(f"{path}: N in {len(inputs)}, N out {n_out} {counts}", file=sys.stderr)
        assert n_out == len(inputs)


if __name__ == "__main__":
    main()
