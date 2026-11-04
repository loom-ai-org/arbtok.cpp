"""Shared pieces of the golden dumpers.

A golden file is tests/goldens/<module>.jsonl.gz. Its first line is a header
with the reference versions, the input sources, the record count and the count
per corpus. Each other line is one record: [function, arguments..., result],
written as compact JSON. A record whose line changes when the stem lexicon has
no entries holds {"sha256": <hex>} in place of each result item, where <hex>
is the SHA-256 of the UTF-8 line. The header then gives the count of those
records as "sha256_records". An exception is recorded as {"err": "<class name>"},
because the exception class is part of the contract. A Python set is written
sorted, because its iteration order follows the hash seed.

The inputs come from four places that the command line names: the tests/
directory of the arbtok sdist, the gold sets of the arbtok repository at the
commit of the reference release, tests/ar_test.txt of that repository, and the
reference installation itself (its bundled lexicons and the orthography2ipa
specs).
"""
import ast
import csv
import gzip
import hashlib
import importlib.metadata
import io
import json
import pathlib
import random
import sys
import unicodedata

REFERENCE = ("arbtok", "orthography2ipa", "langcodes", "linguatag", "ovos-spec-tools")
#: The versions the port is held to. linguatag, langcodes and ovos-spec-tools are the versions that
#: resolve with arbtok and orthography2ipa under `uv --prerelease=allow`. They decide how a language tag
#: resolves, so a re-dump with other versions can change the dialects records.
PINNED = {"arbtok": "0.0.2a71", "orthography2ipa": "7.114.16a1", "linguatag": "0.1.5a1", "langcodes": "3.5.1",
          "ovos-spec-tools": "1.17.1a1"}
#: The arbtok commit whose version.py is 0.0.2a71; the gold sets are read there.
GOLD_COMMIT = "a95668cf097c5fcfb72b7ab75d7346579626e7a5"

SOURCES = {"sdist_tests": None, "gold_dir": None, "corpus": None}

# Strings that exercise the code-point layer: case mappings that change the
# length, non-ASCII digits and spaces, combining marks, astral characters, and
# the Arabic marks the rules look for.
UNICODE_PROBES = [
    "", " ", "\t\n", " ", " ", "　", "​", "ß", "İ", "ı", "Σ", "ﬁ", "K", "Å", "é", "é",
    "٣", "३", "１２", "²", "½", "Ⅻ", "𝟗", "😀", "€", "…", "–", "�", "\x00", "\x7f", "\u0085", "‌", "‍",
    "ـ", "ﻻ", "ﺍ", "ٱ", "ٰ", "ٓ", "ٔ", "ٕ", "ًّ", "َّ",
    "آ", "أ", "إ", "ؤ", "ئ", "ی", "ک", "ہ",
]


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def call(fn, *args, **kwargs):
    try:
        return fn(*args, **kwargs)
    except Exception as exc:  # the exception class is part of the contract
        return {"err": type(exc).__name__}


def versions():
    out = {name: importlib.metadata.version(name) for name in REFERENCE}
    wrong = {name: out[name] for name, version in PINNED.items() if out[name] != version}
    if wrong:
        raise SystemExit(f"the reference environment has {wrong}, the port is held to {PINNED}")
    out["python"] = sys.version.split()[0]
    out["unicode"] = unicodedata.unidata_version
    return out


def unique(items):
    return list(dict.fromkeys(items))


def fuzz_strings(seed, count, alphabet, max_len=12):
    rng = random.Random(seed)
    return ["".join(rng.choice(alphabet) for _ in range(rng.randint(0, max_len))) for _ in range(count)]


def test_strings():
    """Every string literal of the sdist tests, in first-seen order over sorted file names."""
    seen = {}
    for path in sorted(pathlib.Path(SOURCES["sdist_tests"]).glob("*.py")):
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
            if isinstance(node, ast.Constant) and isinstance(node.value, str):
                seen.setdefault(node.value, None)
    return list(seen)


def gold_rows():
    """(lect, column, text) of every gold sentence: the code-switched sets and the span gold."""
    root = pathlib.Path(SOURCES["gold_dir"])
    out = []
    for path in sorted((root / "gold_code_switched").glob("*.tsv")):
        with path.open(encoding="utf-8", newline="") as fh:
            for row in csv.DictReader(fh, delimiter="\t"):
                for column in ("sentence", "raw", "ipa", "cs_words"):
                    out.append((path.stem, column, row[column]))
    for path in sorted((root / "span_gold").glob("*.jsonl")):
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                row = json.loads(line)
                out.append((row["lect"], "sentence", row["sentence"]))
                for accepted in row.get("accepted", []):
                    out.append((row["lect"], "printed", accepted.get("printed", "")))
                    out.append((row["lect"], "phones", accepted.get("phones", "")))
    return out


def gold_sentences(column="sentence"):
    """(lect, sentence) pairs, in file order, without repeats."""
    return unique((lect, text) for lect, col, text in gold_rows() if col == column and text)


def package_dir():
    import arbtok
    return pathlib.Path(arbtok.__file__).parent


def lexicon_words():
    """The keys and vocalizations of the bundled dialect lexicons, in file order."""
    out = []
    for path in sorted((package_dir() / "data" / "lexicons").glob("*.tsv")):
        with path.open(encoding="utf-8", newline="") as fh:
            for row in csv.DictReader(fh, delimiter="\t"):
                out += [row["key"], row["vocalization"]]
    return unique(w for w in out if w)


def donor_words(limit_per_file=1500):
    """Latin words of the bundled donor lexicons: every n-th row, so that the sample spans the file."""
    out = []
    for path in sorted((package_dir() / "data" / "donor_lexicons").glob("*.tsv")):
        rows = [line.split("\t")[0] for line in path.read_text(encoding="utf-8").splitlines()
                if line and not line.startswith("#")]
        step = max(1, len(rows) // limit_per_file)
        out += rows[::step]
    return unique(out)


def arabic_lects():
    from arbtok.dialects import supported_lects
    return [lect.code for lect in supported_lects()]


def spec_exception_words():
    """The word_exceptions keys of every Arabic spec, in spec order."""
    from orthography2ipa import get
    out = []
    for code in arabic_lects():
        out += list((get(code).word_exceptions or {}).keys())
    return unique(out)


def corpus_words(step):
    """The Arabic word of every step-th line of tests/ar_test.txt (word<TAB>IPA), from the first line."""
    lines = pathlib.Path(SOURCES["corpus"]).read_text(encoding="utf-8").splitlines()
    return [line.split("\t")[0] for line in lines[::step]]


def arabic_test_strings():
    """The sdist test strings that hold a character of the Arabic block."""
    return [s for s in test_strings() if any("\u0600" <= c <= "\u06ff" for c in s)]


def words_of(texts):
    out = []
    for text in texts:
        out += text.split()
    return unique(out)


#: The number of result items at the end of a record, where it is not one.
RESULT_ITEMS = {("diacritize", "word"): 2}


def hashed(module, record, line):
    """The record with each result item replaced by the SHA-256 of its line."""
    n = RESULT_ITEMS.get((module, record[0]), 1)
    digest = {"sha256": hashlib.sha256(line.encode("utf-8")).hexdigest()}
    return dumps(record[:-n] + [digest] * n)


def write_lines(path, corpora):
    """The records of corpora as lines, with no header and no hashing: the output of the stem-free dump."""
    with open(path, "w", encoding="utf-8") as fh:
        for _, _, records in corpora:
            for r in records:
                fh.write(dumps(r) + "\n")


def write(path, module, corpora, stem_free):
    """corpora: [(corpus name, input count, records)]. stem_free: the lines of the
    same records, dumped with a stem lexicon that has no entries.

    The header holds the input count of each corpus. A record whose line differs
    from its stem-free line is written hashed. After the file is written, its
    records are counted again from the file, and the file must hold one record
    for each input. The C++ test also compares the header count with the
    records it reads.
    """
    lines = []
    counts = {}
    n_hashed = 0
    for name, n_in, records in corpora:
        counts[name] = n_in
        corpus_hashed = 0
        for r in records:
            line = dumps(r)
            if len(lines) >= len(stem_free):
                raise SystemExit(f"{module}: the stem-free dump has only {len(stem_free)} records")
            if line != stem_free[len(lines)]:
                line = hashed(module, r, line)
                corpus_hashed += 1
            lines.append(line)
        n_hashed += corpus_hashed
        print(f"  {module}/{name}: {n_in} in, {len(records)} out, {corpus_hashed} hashed", file=sys.stderr)
        if n_in != len(records):
            raise SystemExit(f"{module}/{name}: {n_in} inputs gave {len(records)} records")
    if len(lines) != len(stem_free):
        raise SystemExit(f"{module}: {len(lines)} records, the stem-free dump has {len(stem_free)}")
    header = {"module": module, "versions": versions(), "gold_commit": GOLD_COMMIT, "corpora": counts,
              "count": sum(counts.values())}
    if n_hashed:
        header["sha256_records"] = n_hashed
    raw = io.BytesIO()
    with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as gz:
        gz.write((dumps(header) + "\n").encode("utf-8"))
        for line in lines:
            gz.write((line + "\n").encode("utf-8"))
    with open(path, "wb") as fh:
        fh.write(raw.getvalue())
    with gzip.open(path, "rb") as fh:
        written = fh.read().count(b"\n") - 1
    if written != header["count"]:
        raise SystemExit(f"{path}: {header['count']} inputs, {written} records in the file")
    print(f"{path}: {written} records", file=sys.stderr)
    return written


def corpus(name, inputs, fn):
    """One corpus: fn(input) -> record for each input. The input count is taken before fn runs."""
    inputs = list(inputs)
    return name, len(inputs), [fn(x) for x in inputs]
