"""Golden records for how the reference number and date libraries route a language tag.

textnorm and util pass the caller's language to ovos-number-parser, ovos-date-parser
and unicode-rbnf, and they catch what those raise. For each entry point and each tag
of a grid, a record holds one call of the kind tools/goldens/numbers_ar.py and
tools/goldens/dates_ar.py write, its reference result, and how the reference
reached it:

    ["dispatch", route, record]

route is one of tools/goldens/routes.py: "arabic" when an Arabic implementation
ran, "rejected" when the reference rejected the language, and "module" when a
module of another language gave the result. The port gives the reference result
for an "arabic" or a "rejected" record, and raises NotPortedError for a "module"
record, because it holds only the Arabic implementations.

The grid holds every language prefix that the entry points test, every two-letter
code, the codes of the unicode-rbnf rules, the Arabic lect codes, each of them with
a region, in upper case and with "_", and tags that are not codes.

Usage, with the reference interpreter:
    python -I tools/goldens/numbers_dispatch.py tests/goldens/numbers_dispatch.jsonl.gz [--jobs N]
"""
import collections
import datetime
import itertools
import multiprocessing
import os
import pathlib
import re
import string
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

import ovos_date_parser  # noqa: E402
import ovos_number_parser  # noqa: E402

from goldens import common  # noqa: E402
from goldens import dates_ar  # noqa: E402
from goldens import numbers_ar  # noqa: E402
from goldens import routes  # noqa: E402


def prefixes():
    out = set()
    for module in (ovos_number_parser, ovos_date_parser):
        source = pathlib.Path(module.__file__).read_text(encoding="utf-8")
        out.update(re.findall(r'lang\.startswith\("([a-z]+)"\)', source))
    return out


def tags():
    rbnf = [p.stem for p in (pathlib.Path(__import__("unicode_rbnf").__file__).parent / "rbnf").glob("*.xml")]
    lects = ["ar", "arb", "ars", "acw", "afb", "arz", "apc", "ajp", "acm", "ary", "aeb", "ayl", "arq", "abv", "shu",
             "ssh", "acx", "acy", "adf", "aec", "auz", "apd", "avl", "pga", "abh", "aao", "mey", "ara", "arc"]
    base = sorted(prefixes() | set(rbnf) | set(lects)
                  | {a + b for a, b in itertools.product(string.ascii_lowercase, repeat=2)})
    out = set(base)
    for code in sorted(prefixes() | set(rbnf) | set(lects)):
        out.update({code + "-SA", code + "-US", code + "-MR", code + "_SA", code.upper(), code.capitalize(),
                    code.upper() + "-SA", code + "-Latn", code + "-x-foo"})
    out.update(["", "x", "-", "-ar", "ar-", "und", "zxx", "mul", "xx", "xx-XX", "not a tag", "123", "ar ", " ar",
                "ar-SA-x-najd", "ar-Arab", "ar_EG", "AR-sa", "Ar-Sa", "mey-MR", "ur-PK", "en_US", "pt-BR", "pt-br",
                "ca-valencia", "no-NO", "nb-NO", "sr-Cyrl"])
    return sorted(out)


DT = datetime.datetime(2000, 1, 1, 15, 1)
DATE = datetime.datetime(2025, 4, 30)
ANCHOR = datetime.datetime(2000, 1, 1)


def calls(lang):
    return [
        ("pronounce_number", lambda: numbers_ar.r_pronounce_number(5, lang)),
        ("pronounce_number", lambda: numbers_ar.r_pronounce_number(749, lang)),
        ("pronounce_ordinal", lambda: numbers_ar.r_pronounce_ordinal(5, lang)),
        ("pronounce_fraction", lambda: numbers_ar.r_pronounce_fraction("1/2", lang)),
        ("extract_number", lambda: numbers_ar.r_extract_number("5", lang)),
        ("extract_number", lambda: numbers_ar.r_extract_number("خمسة", lang)),
        ("numbers_to_digits", lambda: numbers_ar.r_numbers_to_digits("خمسة 5", lang)),
        ("nice_time", lambda: dates_ar.r_nice_time(DT, lang, True, True, False)),
        ("nice_time", lambda: dates_ar.r_nice_time(DT, lang, True, False, True)),
        ("nice_date", lambda: dates_ar.r_nice_date(DATE, lang)),
        ("extract_datetime", lambda: dates_ar.r_extract_datetime("الساعة 5 مساء", lang, ANCHOR)),
    ]


def records_for(lang):
    out = []
    for _, make in calls(lang):
        route, record = routes.route_of(make)
        out.append(["dispatch", route, record])
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out_path = sys.argv[1]
    jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else os.cpu_count()
    routes.install(numbers_ar, dates_ar)
    grid = tags()
    per_lang = len(calls(""))
    with multiprocessing.get_context("fork").Pool(jobs) as pool:
        records = [r for rs in pool.map(records_for, grid, chunksize=8) for r in rs]
    counts = collections.Counter(r[1] for r in records)
    print(f"numbers_dispatch: {len(grid)} tags x {per_lang} calls; routes {dict(sorted(counts.items()))}",
          file=sys.stderr)
    # No record reads the stem lexicon, so the stem-free dump is the records themselves.
    common.write(out_path, "numbers_dispatch", [("dispatch", len(grid) * per_lang, records)],
                 [common.dumps(r) for r in records])


if __name__ == "__main__":
    main()
