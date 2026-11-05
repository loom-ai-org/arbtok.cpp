"""Golden records for the Arabic number functions arbtok calls.

The gold comes from the Python reference: ovos-number-parser and unicode-rbnf,
at the versions arbtok 0.0.2a71 resolves to. Each output line is a JSON record
[kind, arguments..., result]. A result is a string, a bool, {"int": digits},
{"float": repr}, or {"err": "<Python exception class>"}. A record whose kind
ends in "_block" holds the SHA-256 of the results of a whole range, one JSON
text per line, in place of the results.

The inputs are of two kinds. The calls arbtok's own test suite makes into
these libraries are recorded once with the record command, and kept in
tests/goldens/numbers_calls.jsonl.gz. The grids are generated here: every
integer from 0 to 10**6 in each case, gender and lect arbtok passes, powers of
ten, negatives, floats, fractions, every lect code arbtok knows, and random
sequences of the number vocabulary.

Usage, with the reference interpreter:
    python -I tools/goldens/numbers_ar.py record <arbtok checkout> tests/goldens/numbers_calls.jsonl.gz
    python -I tools/goldens/numbers_ar.py build tests/goldens/numbers_calls.jsonl.gz \\
        tests/goldens/numbers_ar.jsonl.gz [--jobs N]
"""
import datetime
import enum
import gzip
import hashlib
import json
import math
import multiprocessing
import os
import random
import pathlib
import sys
import unicodedata

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

from goldens import common  # noqa: E402

LANGS_WITH_TABLES = ["ar", "acw", "afb", "ars", "arz"]
GENDERS = ["MASCULINE", "FEMININE"]
CASES = [None, "nominative", "oblique"]


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def tag(value):
    if isinstance(value, bool) or value is None or isinstance(value, str) and not isinstance(value, enum.Enum):
        return value
    if isinstance(value, enum.Enum):
        return {"enum": type(value).__name__, "name": value.name}
    if isinstance(value, int):
        return {"int": str(value)}
    if isinstance(value, float):
        return {"float": repr(value)}
    if isinstance(value, datetime.datetime):
        return {"datetime": value.isoformat()}
    if isinstance(value, datetime.date):
        return {"date": value.isoformat()}
    if isinstance(value, datetime.time):
        return {"time": value.isoformat()}
    if isinstance(value, (list, tuple)):
        return [tag(v) for v in value]
    return {"repr": repr(value)}


def untag(value):
    if isinstance(value, dict):
        if "int" in value:
            return int(value["int"])
        if "float" in value:
            return float(value["float"])
        if "enum" in value:
            from ovos_number_parser.util import GrammaticalGender
            return GrammaticalGender[value["name"]]
    return value


def enc(fn):
    try:
        return tag(fn())
    except Exception as e:  # the class of the error is part of the gold
        return {"err": type(e).__name__}


def digest(results):
    return hashlib.sha256("\n".join(dumps(r) for r in results).encode("utf-8")).hexdigest()


# ---------------------------------------------------------------------------
# record: the calls arbtok's test suite makes
# ---------------------------------------------------------------------------

_OUT = None
_DEPTH = [0]


def _wrap(name, fn):
    def inner(*args, **kwargs):
        if _DEPTH[0] == 0:
            _OUT.append(dumps([name, [tag(a) for a in args], {k: tag(v) for k, v in kwargs.items()}]))
        _DEPTH[0] += 1
        try:
            return fn(*args, **kwargs)
        finally:
            _DEPTH[0] -= 1
    inner.__wrapped__ = fn
    return inner


class _Recorder:
    """pytest plugin: record the top-level calls arbtok makes into the libraries."""

    def pytest_configure(self, config):
        import ovos_date_parser as odp
        import ovos_number_parser as onp
        import arbtok.util as au
        for n in ("pronounce_number", "pronounce_ordinal", "pronounce_fraction", "extract_number",
                  "numbers_to_digits"):
            setattr(onp, n, _wrap(n, getattr(onp, n)))
        for n in ("nice_time", "extract_datetime", "nice_date"):
            setattr(odp, n, _wrap(n, getattr(odp, n)))
        for n in ("pronounce_number", "pronounce_fraction", "is_numeric", "nice_time", "nice_date"):
            setattr(au, n, _wrap(n, getattr(au, n)))
        real_for_language = au.RbnfEngine.for_language

        class EngineProxy:
            def __init__(self, engine):
                self._engine = engine

            def format_number(self, *a, **k):
                return _wrap("rbnf_format_number", self._engine.format_number)(*a, **k)

        class RbnfProxy:
            @staticmethod
            def for_language(lang):
                return EngineProxy(_wrap("rbnf_for_language", real_for_language)(lang))

        au.RbnfEngine = RbnfProxy


def record(arbtok_dir, out_path):
    global _OUT
    import pytest
    _OUT = []
    os.chdir(arbtok_dir)
    status = pytest.main(["tests", "-q", "-p", "no:cacheprovider"], plugins=[_Recorder()])
    unique = list(dict.fromkeys(_OUT))
    with gzip.open(out_path, "wt", encoding="utf-8") as f:
        for line in unique:
            f.write(line + "\n")
    print(f"recorded {len(_OUT)} calls, {len(unique)} unique, pytest status {status}", file=sys.stderr)


# ---------------------------------------------------------------------------
# which calls the port covers
# ---------------------------------------------------------------------------

def is_ar(lang):
    from ovos_number_parser import _is_ar
    return _is_ar(lang)


EXTRACT_PREFIXES = ("en", "az", "bg", "ca", "cs", "da", "de", "el", "es", "gl", "eu", "et", "fa", "fi", "fr",
                    "id", "it", "ms", "tr", "kab", "fy", "nn", "nb", "no", "nl", "pl", "pt", "mwl", "ro",
                    "ast", "oc", "an", "ru", "sk", "sv", "uk", "hr", "he", "hu", "sl")
TO_DIGITS_PREFIXES = ("en", "ast", "oc", "an", "fy", "gl", "de", "pt", "mwl", "ro", "bg", "hr", "ru", "sk", "id",
                      "ms", "tr", "uk")


KINDS_WITH_LANG = ("pronounce_number", "pronounce_ordinal", "pronounce_fraction", "extract_number",
                   "numbers_to_digits")


def in_scope(kind, lang):
    """Whether the reference routes lang to the Arabic code the port holds."""
    if kind == "pronounce_number" or kind == "extract_number":
        return is_ar(lang) and not lang.startswith(("en", "az"))
    if kind == "pronounce_ordinal":
        return is_ar(lang) and not lang.startswith(("pt", "mwl", "ro", "ast", "ca", "oc", "an"))
    if kind == "pronounce_fraction":
        return is_ar(lang) and not lang.startswith(("pt", "ast", "ca", "oc", "an", "mwl", "gl", "ro"))
    if kind == "numbers_to_digits":
        if lang.startswith(TO_DIGITS_PREFIXES):
            return False
        return is_ar(lang) or not lang.startswith(EXTRACT_PREFIXES)
    raise ValueError(kind)


# ---------------------------------------------------------------------------
# records
# ---------------------------------------------------------------------------

def r_pronounce_number(n, lang, places=3, gender="MASCULINE", case=None):
    from ovos_number_parser import pronounce_number
    from ovos_number_parser.util import GrammaticalGender
    return ["pronounce_number", tag(n), lang, places, gender, case,
            enc(lambda: pronounce_number(n, lang, places=places, gender=GrammaticalGender[gender], case=case))]


def r_pronounce_ordinal(n, lang, gender="MASCULINE", case=None):
    from ovos_number_parser import pronounce_ordinal
    from ovos_number_parser.util import GrammaticalGender
    return ["pronounce_ordinal", tag(n), lang, gender, case,
            enc(lambda: pronounce_ordinal(n, lang, gender=GrammaticalGender[gender], case=case))]


def r_pronounce_fraction(text, lang):
    from ovos_number_parser import pronounce_fraction
    return ["pronounce_fraction", text, lang, enc(lambda: pronounce_fraction(text, lang))]


def r_extract_number(text, lang, ordinals=False):
    from ovos_number_parser import extract_number
    return ["extract_number", text, lang, ordinals, enc(lambda: extract_number(text, lang, ordinals=ordinals))]


def r_numbers_to_digits(text, lang):
    from ovos_number_parser import numbers_to_digits
    return ["numbers_to_digits", text, lang, enc(lambda: numbers_to_digits(text, lang))]


def r_is_numeric(text):
    from ovos_number_parser.util import is_numeric
    return ["is_numeric", text, is_numeric(text)]


def r_rbnf(lang, text):
    from unicode_rbnf import FormatPurpose, RbnfEngine

    def run():
        engine = RbnfEngine.for_language(lang)
        if text is None:
            return True
        return engine.format_number(text, FormatPurpose.CARDINAL).text
    return ["rbnf", lang, text, enc(run)]


def block(kind, start, count, lang, gender, case):
    from ovos_number_parser import extract_number, pronounce_number, pronounce_ordinal
    from ovos_number_parser.util import GrammaticalGender
    g = GrammaticalGender[gender]
    results = []
    for n in range(start, start + count):
        if kind == "pronounce_number_block":
            results.append(enc(lambda: pronounce_number(n, lang, gender=g, case=case)))
        elif kind == "pronounce_ordinal_block":
            results.append(enc(lambda: pronounce_ordinal(n, lang, gender=g, case=case)))
        else:
            try:
                spoken = pronounce_number(n, lang, gender=g, case=case)
                results.append(enc(lambda: extract_number(spoken, lang)))
            except Exception as e:
                results.append({"err": type(e).__name__})
    return [kind, str(start), count, lang, gender, case, digest(results)]


def _block_star(args):
    return block(*args)


def _call_star(args):
    fn, a = args
    return globals()[fn](*a)


# ---------------------------------------------------------------------------
# inputs
# ---------------------------------------------------------------------------

def lect_codes():
    """Every lect code arbtok knows, and the code its text normalizer speaks numbers in for it."""
    from arbtok.dialects import lect_code, supported_lects
    codes = {lect.code for lect in supported_lects()} | {"ar"}
    return sorted(codes | {lect_code(code) or code for code in codes})


def from_recorded(calls_path):
    """The recorded calls, as records, and the count the port does not cover."""
    tasks, skipped = [], {}
    with gzip.open(calls_path, "rt", encoding="utf-8") as f:
        lines = [json.loads(line) for line in f]
    for name, args, kwargs in lines:
        args = [untag(a) for a in args]
        kwargs = {k: untag(v) for k, v in kwargs.items()}
        if name in ("pronounce_number", "pronounce_ordinal", "pronounce_fraction", "extract_number",
                    "numbers_to_digits"):
            lang = kwargs.pop("lang", None) or args[1]
            args = args[:1]
            if not in_scope(name, lang):
                skipped[(name, lang)] = skipped.get((name, lang), 0) + 1
                continue
            if name == "pronounce_number":
                places = kwargs.pop("places", 3)
                gender = kwargs.pop("gender", None)
                case = kwargs.pop("case", None)
                assert not kwargs, kwargs
                tasks.append(("r_pronounce_number", (args[0], lang, places, gender.name if gender else "MASCULINE",
                                                     case)))
            elif name == "pronounce_ordinal":
                gender = kwargs.pop("gender", None)
                case = kwargs.pop("case", None)
                assert not kwargs, kwargs
                tasks.append(("r_pronounce_ordinal", (args[0], lang, gender.name if gender else "MASCULINE", case)))
            elif name == "pronounce_fraction":
                assert not kwargs, kwargs
                tasks.append(("r_pronounce_fraction", (args[0], lang)))
            elif name == "extract_number":
                ordinals = kwargs.pop("ordinals", False)
                assert not kwargs, kwargs
                tasks.append(("r_extract_number", (args[0], lang, ordinals)))
            else:
                assert not kwargs, kwargs
                tasks.append(("r_numbers_to_digits", (args[0], lang)))
        elif name == "is_numeric":
            tasks.append(("r_is_numeric", (args[0],)))
        elif name == "rbnf_for_language":
            tasks.append(("r_rbnf", (args[0], None)))
        elif name == "rbnf_format_number":
            tasks.append(("r_rbnf", ("ar", args[0])))
    return len(lines), tasks, skipped


INT_SAMPLE = sorted({*range(0, 31), 40, 55, 99, 100, 101, 102, 110, 111, 112, 120, 199, 200, 201, 202, 210, 300, 999,
                     1000, 1001, 1002, 1100, 2000, 2002, 3000, 10000, 11000, 12000, 100000, 200000, 200200,
                     999999, 10 ** 6, 2 * 10 ** 6, 10 ** 9, 10 ** 12, 10 ** 15, 10 ** 18, 10 ** 24, -1, -2, -12,
                     -200, -10 ** 6, -10 ** 15})
FLOAT_SAMPLE = [0.0, -0.0, 0.5, 1.5, 2.25, 3.14159, -2.5, 0.001, 0.0005, 0.0004, 0.9995, 2.675, 1e-05, 12.345,
                100.1, 1e15 + 0.5, 1e16, 1.5e16, 1e20, 1.5e300, float("inf"), float("-inf"), float("nan"),
                0.1 + 0.2, 1 / 3, 2 / 3, 1234567.891, 5.0, -7.0]


def number_vocabulary():
    import ovos_number_parser.numbers_ar as na
    words = set()
    for name in ("_UNITS_LOOKUP", "_TENS_LOOKUP", "_HUNDREDS_LOOKUP", "_SCALES_LOOKUP", "_SCALE_DUALS_LOOKUP",
                 "_FRACTIONS_LOOKUP", "_TEEN_FIRST_LOOKUP", "_TEEN_SECOND_LOOKUP", "_FUSED_TEENS_LOOKUP",
                 "_ORDINAL_UNITS_LOOKUP", "_MINUS_LOOKUP", "_DECIMAL_LOOKUP", "_HUNDRED_MULT_LOOKUP",
                 "_CONSTRUCT_HUNDRED_LOOKUP", "_SPOKEN_HALF_LOOKUP", "_HOMOGRAPHS_AR", "_DAY_WORDS",
                 "_PER_CENT_WORDS", "_PER_CENT_AFTER_FI", "_UNIT_WORDS_AR", "_CONTEXT_CANONICAL_AR",
                 "_BROKEN_PLURAL_HEADS", "_DUAL_NOUNS_AR", "_ACCUSATIVE_OR_CONSTRUCT_DUAL"):
        words.update(getattr(na, name))
    words.update(w for w in na._ONES_AR + na._ONES_FEM_AR if w)
    for table in (na._TENS_AR, na._HUNDREDS_AR, na._ORDINAL_STEMS_AR, na._ORDINAL_STEMS_FEM_AR, na._TEENS_FEM_AR):
        words.update(table.values())
    for _, singular, dual, plural in na._SCALES_AR:
        words.update((singular, dual, plural))
    for forms in na.AR_LECT_FORMS.values():
        words.update(forms.values())
    words.update(["الساعة", "في", "يوم", "سيارة", "كتاب", "الحلقة", "رجال", "ثانية", "ألفاً", "مئةٍ", "الحادي",
                  "العشرون", "والعشرين", "بالمئة", "لست", "بخمسة", "فثلاثة", "للعشرين", "والخمسين", "عندي"])
    return sorted(words)


EXTRA_TOKENS = ["و", "5", "12", "3.5", "١٢", "٣٫٥", "۴", "-3", "1,000", "1e3", "inf", "%", "،", ".", "؟",
                "ـ", "ب", "ل", "ف", "ال", "و5", "ب10"]


def random_texts(rng, vocab, count, max_len):
    pool = vocab + EXTRA_TOKENS * 4
    out = []
    for _ in range(count):
        words = [rng.choice(pool) for _ in range(rng.randint(1, max_len))]
        if rng.random() < 0.2:
            k = rng.randrange(len(words))
            words[k] = words[k] + rng.choice([".", "،", "؟", "!", ":"])
        if rng.random() < 0.15:
            k = rng.randrange(len(words))
            words[k] = rng.choice(["و", "ب", "ف", "ل", "ال", "وال"]) + words[k]
        out.append(" ".join(words))
    return out


def grid_tasks():
    tasks = []
    codes = lect_codes()
    # every lect code, every flag arbtok passes, a sample of numbers
    for lang in codes:
        for gender in GENDERS + ["NEUTRAL"]:
            for case in CASES + ["genitive", "accusative", "dual"]:
                numbers = INT_SAMPLE + FLOAT_SAMPLE if lang in LANGS_WITH_TABLES or case is None \
                    else INT_SAMPLE[::3] + FLOAT_SAMPLE[::3]
                for n in numbers:
                    tasks.append(("r_pronounce_number", (n, lang, 3, gender, case)))
    for lang in ("ar", "acw", "AR-eg", "ar-SA"):
        for places in (0, 1, 2, 3, 4, 10, 30, -1):
            for n in FLOAT_SAMPLE + [1, 10 ** 15, 10 ** 20, 123456789012345678]:
                tasks.append(("r_pronounce_number", (n, lang, places, "MASCULINE", None)))
    rng = random.Random(3)
    for _ in range(3000):
        n = rng.choice([rng.randrange(10 ** k) for k in (7, 10, 13, 16, 19, 25, 40)])
        n = -n if rng.random() < 0.2 else n
        tasks.append(("r_pronounce_number", (n, rng.choice(LANGS_WITH_TABLES), 3, rng.choice(GENDERS),
                                             rng.choice(CASES))))
    for _ in range(3000):
        x = rng.choice([rng.uniform(0, 10), rng.uniform(0, 1e6), rng.uniform(-1e3, 1e3), rng.random() * 1e-3,
                        round(rng.uniform(0, 100), 3), round(rng.uniform(0, 100), 1)])
        tasks.append(("r_pronounce_number", (x, rng.choice(LANGS_WITH_TABLES), 3, rng.choice(GENDERS),
                                             rng.choice(CASES))))
    # ordinals
    for lang in ["ar", "arb", "acw", "arz", "ar-SA", "apc-LB", "aao", "en"][:7]:
        for gender in GENDERS + ["NEUTRAL"]:
            for case in CASES + ["genitive", "accusative", "dual"]:
                for n in [0, -1, *range(1, 26), 29, 30, 31, 99, 100, 101, 111, 200, 1000, 1001, 10 ** 6, 1.5, 2.0,
                          float("inf"), float("nan"), float("-inf")]:
                    tasks.append(("r_pronounce_ordinal", (n, lang, gender, case)))
    # fractions
    for lang in ("ar", "arz", "acw", "ars", "ar-SA", "apc-LB", "arb"):
        span = range(-3, 26) if lang == "ar" else range(0, 22)
        for a in span:
            for b in span:
                tasks.append(("r_pronounce_fraction", (f"{a}/{b}", lang)))
    for text in ["1/2/3", "a/b", " 1/2", "1 /2", "١/٢", "1_0/3", "", "/", "1/", "/2", "+1/+2",
                 "10000000000000000000000/7", "1/21", "7/1", "3/3", "22/7", "1000000/3", "2/4", "1.5/2",
                 "10**400/3", "9" * 400 + "/2", "1" * 5000 + "/3"]:
        for lang in ("ar", "acw"):
            tasks.append(("r_pronounce_fraction", (text, lang)))
    # extract_number: each vocabulary word with each clitic, and random sequences
    vocab = number_vocabulary()
    for word in vocab + EXTRA_TOKENS:
        for prefix in ("", "و", "ب", "ل", "ف", "ال", "وال", "بال", "لل"):
            for lang in ("ar", "ars"):
                for ordinals in (False, True):
                    tasks.append(("r_extract_number", (prefix + word, lang, ordinals)))
    for text in random_texts(rng, vocab, 30000, 6):
        tasks.append(("r_extract_number", (text, rng.choice(["ar", "ars", "acw", "arz", "ar-SA"]),
                                           rng.random() < 0.3)))
    for text in ["", " ", "خمسة-عشر", "خمسة_عشر", "a-b", "5-6", "خمسة—عشر", "ثلاثة-آلاف"]:
        for lang in ("ar", "acw"):
            tasks.append(("r_extract_number", (text, lang, False)))
    # numbers_to_digits: random sentences, and the codes with no Arabic module
    for text in random_texts(rng, vocab, 20000, 10):
        tasks.append(("r_numbers_to_digits", (text, rng.choice(["ar", "ars", "acw", "arz", "ar-SA", "afb"]))))
    for lang in ("aao", "shu-TD", "zz", "", "x-ar"):
        for text in ["خمسة  و  عشرون", " ثلاثة\tآلاف ", "", "5 6"]:
            tasks.append(("r_numbers_to_digits", (text, lang)))
    # is_numeric
    for text in ["", " ", "1", "-1", "+1", "1.5", "1e309", "-1e309", "inf", "nan", "1_000", "١٢", "²", "1²",
                 "12" * 200, "-" + "9" * 400, "+" + "9" * 400, " 12 ", " 12", "0x10", "1e", ".", "1.", ".5",
                 "--1", "+-1", "٫5", "1٫5", "Infinity", "-iNF", "1__0", "_1", "1_", "١٢٣"]:
        tasks.append(("r_is_numeric", (text,)))
    # unicode_rbnf: every digit that is not a decimal digit, the only text arbtok sends
    for lang in ("ar", "en", "AR", "xx", "", "de_CH", "aao", "ar-SA"):
        tasks.append(("r_rbnf", (lang, None)))
    odd_digits = [chr(c) for c in range(sys.maxunicode + 1)
                  if chr(c).isdigit() and not chr(c).isdecimal()]
    for d in odd_digits:
        for text in (d, "1" + d, d + "2" + d):
            tasks.append(("r_rbnf", ("ar", text)))
    return tasks


def block_tasks():
    tasks = []
    for lang in LANGS_WITH_TABLES:
        for gender in GENDERS:
            for case in CASES:
                for start in range(0, 10 ** 6, 1000):
                    tasks.append(("pronounce_number_block", start, 1000, lang, gender, case))
                tasks.append(("pronounce_number_block", 10 ** 6, 1, lang, gender, case))
                tasks.append(("pronounce_number_block", -10000, 10000, lang, gender, case))
                for k in range(7, 22):
                    tasks.append(("pronounce_number_block", 10 ** k - 500, 1000, lang, gender, case))
        for gender, case in (("MASCULINE", None), ("FEMININE", "oblique")):
            for start in range(0, 10 ** 6, 1000):
                tasks.append(("extract_number_block", start, 1000, lang, gender, case))
            for k in range(7, 16):
                tasks.append(("extract_number_block", 10 ** k - 500, 1000, lang, gender, case))
    for gender in GENDERS:
        for case in (None, "oblique"):
            for start in range(0, 10 ** 5, 1000):
                tasks.append(("pronounce_ordinal_block", start, 1000, "ar", gender, case))
    return tasks


def build(calls_path, out_path, jobs):
    n_in, recorded, skipped = from_recorded(calls_path)
    grids = grid_tasks()
    blocks = block_tasks()
    n_tasks = len(recorded) + len(grids) + len(blocks)
    with multiprocessing.Pool(jobs) as pool:
        computed = pool.map(_call_star, recorded + grids, chunksize=200)
        block_records = pool.map(_block_star, blocks, chunksize=4)
    # A code the reference does not route to its Arabic module is kept only
    # where the reference raises NotImplementedError, as the port does.
    records, dropped = [], 0
    for r in computed:
        if r[0] in KINDS_WITH_LANG and not in_scope(r[0], r[2]) and r[-1] != {"err": "NotImplementedError"}:
            dropped += 1
            continue
        records.append(r)
    records += block_records
    records, duplicates = common.deduplicated(records)
    n_skipped = sum(skipped.values())
    print(f"recorded calls in: {n_in}; out: {len(recorded)} records + {n_skipped} not covered by the port "
          f"({n_in - len(recorded) - n_skipped} other calls)", file=sys.stderr)
    for (name, lang), count in sorted(skipped.items()):
        print(f"  not covered: {name} lang={lang!r}: {count}", file=sys.stderr)
    print(f"tasks in: {n_tasks} ({len(recorded)} recorded, {len(grids)} grid, {len(blocks)} blocks); "
          f"dropped as not routed to Arabic: {dropped}; repeats: {duplicates}; records out: {len(records)}",
          file=sys.stderr)
    common.write(out_path, "numbers_ar", [("numbers", n_tasks - dropped - duplicates, records)])


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == "record":
        record(sys.argv[2], os.path.abspath(sys.argv[3]))
    elif len(sys.argv) >= 4 and sys.argv[1] == "build":
        jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else os.cpu_count()
        build(sys.argv[2], sys.argv[3], jobs)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
