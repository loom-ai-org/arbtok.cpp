"""Write the Unicode goldens of the ensemble: tests/goldens/ensemble_unicode.jsonl.gz.

The ensemble reads the general category of a code point (L*, Mn and So), its
canonical combining class, and NFD and NFC. The C++ takes them from the tables
of src/unicode_tables.inc and src/unicode_norm_tables.inc, which
tools/gen_unicode_tables.py and tools/gen_unicode_norm_tables.py write from the
reference interpreter. These goldens check those tables against the same
interpreter: the category and combining class of every code point, and NFD and
NFC of every code point that they change, of every Hangul syllable, and of a
seeded set of random mark sequences.

Usage: python -I tools/goldens/ensemble_unicode.py tests/goldens/ensemble_unicode.jsonl.gz
"""

import gzip
import json
import random
import sys
import unicodedata

MAX_CP = 0x10FFFF


def golden(path):
    cps = [cp for cp in range(MAX_CP + 1) if not 0xD800 <= cp <= 0xDFFF]
    categories = "".join(unicodedata.category(chr(cp)) if not 0xD800 <= cp <= 0xDFFF else "Cs"
                         for cp in range(MAX_CP + 1))
    combining = "".join(f"{unicodedata.combining(chr(cp)):02x}" for cp in range(MAX_CP + 1))
    texts = []
    for cp in cps:
        c = chr(cp)
        if (unicodedata.normalize("NFD", c) != c or unicodedata.normalize("NFC", c) != c
                or unicodedata.combining(c)):
            texts.append(c)
    marks = [chr(cp) for cp in cps if unicodedata.combining(chr(cp))]
    composable = sorted({c for t in texts for c in unicodedata.normalize("NFD", t)})
    jamo = [chr(c) for c in list(range(0x1100, 0x1113)) + list(range(0x1161, 0x1176)) + list(range(0x11A8, 0x11C3))]
    rng = random.Random(15)
    for _ in range(30000):
        n = rng.randint(1, 8)
        parts = []
        for _ in range(n):
            r = rng.random()
            pool = marks if r < 0.35 else composable if r < 0.75 else jamo if r < 0.9 else texts
            parts.append(rng.choice(pool))
        texts.append("".join(parts))
    with gzip.open(path, "wt", encoding="utf-8", compresslevel=9) as f:
        f.write(json.dumps({"kind": "unicode-header", "unidata": unicodedata.unidata_version,
                            "python": sys.version.split()[0], "records": len(texts) + 1}) + "\n")
        f.write(json.dumps({"categories": categories, "combining": combining}) + "\n")
        for t in texts:
            f.write(json.dumps({"text": t, "nfd": unicodedata.normalize("NFD", t),
                                "nfc": unicodedata.normalize("NFC", t)}, ensure_ascii=False) + "\n")
    print(f"{path}: N in {len(texts) + 1}, N out {len(texts) + 1}", file=sys.stderr)


if __name__ == "__main__":
    golden(sys.argv[1])
