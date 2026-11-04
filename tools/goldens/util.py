"""Goldens of arbtok.util.

The inputs are:
- the tables CONTRACTIONS, TITLES and UNITS, each written as [lang, pairs] rows;
- normalize over every text of textnorm.all_texts() with lang "ar", and over the
  generated texts of textnorm.generated_texts() with "ar-SA" and "ar-EG";
- normalize over the calls that the sdist test suite makes, recorded in
  tests/data/textnorm_calls.jsonl.gz;
- normalize with "en", "en-US", "pt", "fr", "de", "es" and "xx" over texts that
  hold no digit and no word that float() reads. The C++ port has the Arabic number
  words only, so no such call reaches a number word of another language;
- match_lang over a grid of tags, resolve_thousands_or_decimal and is_fraction over
  a grid of words, pronounce_date over a grid of dates and pronounce_time over a
  grid of "<h>h<mm>" strings, with Arabic codes.
"""
import datetime
import multiprocessing
import random

from arbtok import util

from . import common
from . import textnorm as tn_goldens

FOREIGN = ("en", "en-US", "pt", "fr", "de", "es", "xx")
ARABIC = ("ar-SA", "ar-EG")
SEED = 5
FOREIGN_SAMPLE = 500


def pairs(d):
    return [[k, v] for k, v in d.items()]


def numberless(text):
    """True when no rule of normalize can reach a number word: no digit of any kind,
    and no word that float() reads, such as "inf" or "nan"."""
    if any(c.isdigit() or c.isdecimal() or c.isnumeric() for c in text):
        return False
    lowered = text.lower()
    return "inf" not in lowered and "nan" not in lowered


def record(item):
    text, lang = item
    return ["normalize", text, lang, common.call(util.normalize, text, lang)]


def normalize_items():
    texts = tn_goldens.all_texts()
    generated = tn_goldens.generated_texts()
    foreign_pool = sorted(t for t in texts if numberless(t))
    rng = random.Random(SEED)
    foreign = sorted(rng.sample(foreign_pool, min(FOREIGN_SAMPLE, len(foreign_pool))))
    foreign = common.unique(foreign + [t for t in generated if numberless(t)])
    items = [(t, "ar") for t in texts]
    items += [(t, lang) for lang in ARABIC for t in generated]
    items += [(t, lang) for lang in FOREIGN for t in foreign]
    expected = len(texts) + len(ARABIC) * len(generated) + len(FOREIGN) * len(foreign)
    recorded = [(c[1], c[2]) for c in tn_goldens.recorded_calls() if c[0] == "normalize"]
    kept = [(t, lang) for t, lang in recorded if lang.split("-")[0] == "ar" or numberless(t)]
    return items + kept, expected + len(kept)


TAGS = ["ar", "ar-SA", "ar-EG", "arz", "en", "en-US", "en-GB", "pt", "pt-PT", "pt-BR", "es", "es-ES", "fr",
        "fr-CA", "de", "zh", "zh-Hant", "sr-Latn", "und", "", "xx", "not a tag", "a", "123", "en-", "-en",
        "EN-us", "ar_SA", "ara", "arb", "x-foo", "en-x-foo", "ar-SA-x-najd", "apc-TR-x-cukurova",
        "hr", "bs", "nb", "gsw"]
VALID = [["ar"], ["en"], ["en", "pt", "ar"], ["en-US", "en-GB"], "ar", "en-US", ["pt-BR", "pt-PT"],
         ["zh-Hans", "zh-Hant"], ["xx", "ar"], ["bad tag", "en"], [], ["ar-SA", "ar-EG", "arz"],
         ["sr-Cyrl", "sr-Latn"], ["en-US-x-foo", "fr"],
         # the closest candidate at distance 9, 10 and 12, either side of the cut-off at 10
         ["arz"], ["sr-Latn"], ["da"]]
WORDS = ["1,500", "1,234,567", "12,5", "1,0000", "1,2,3", "055,123,4567", "1,", ",5", ",", "", "1.500", "12.5",
         "1.234.567", "1,5.0", "١,٥٠٠", "١٬٥٠٠", "1٬500", "abc", "1,500,00", "1 500", "+1,5", "-1,500", "1_000",
         "1,5e3", "inf,0", "0,0", "00,000", "999,999", "1,23", "1234,567", "3/4", "1/2", "a/b", "1/2/3", "/", "1/",
         "/2", "١/٢", "²/³", "10/20", " 1/2"]
SEPARATORS = [",", ".", "٬"]


def match_lang_record(item):
    target, valid = item
    out = common.call(util.match_lang, target, valid)
    return ["match_lang", target, valid, list(out) if isinstance(out, tuple) else out]


def date_grid():
    out = []
    for year in (1900, 1999, 2000, 2004, 2023, 2024, 2025, 2026, 2030, 2099, 2100, 1, 9999):
        for month in range(1, 13):
            for day in (1, 2, 10, 11, 15, 20, 21, 28, 29, 30, 31):
                try:
                    datetime.date(year, month, day)
                except ValueError:
                    continue
                out.append((year, month, day))
    return out


TIMES = [f"{h}h{m:02d}" for h in (0, 1, 7, 9, 11, 12, 13, 15, 19, 23, 24, 25) for m in (0, 1, 5, 15, 30, 45, 59, 60)]
TIMES += ["9h5", "h", "1h2h3", "", "abh00", "١٥h٠١", "15H01", " 9h30", "9h30 ", "-1h00", "+1h00", "09h07"]


def run(fn, items):
    with multiprocessing.get_context("fork").Pool(tn_goldens.WORKERS) as pool:
        return pool.map(fn, items, chunksize=64)


def corpora():
    tables = [["const", "CONTRACTIONS", [[lang, pairs(d)] for lang, d in util.CONTRACTIONS.items()]],
              ["const", "TITLES", [[lang, pairs(d)] for lang, d in util.TITLES.items()]],
              ["const", "UNITS", [[lang, pairs(d)] for lang, d in util.UNITS.items()]]]
    items, n_items = normalize_items()
    langs = ["ar", "ar-SA"]
    dates = date_grid()
    return [
        ("constants", 3, tables),
        ("normalize", n_items, run(record, items)),
        ("match_lang", len(TAGS) * len(VALID),
         [match_lang_record((t, v)) for t in TAGS for v in VALID]),
        ("resolve_thousands_or_decimal", len(WORDS) * len(SEPARATORS),
         [["resolve_thousands_or_decimal", w, s, common.call(util.resolve_thousands_or_decimal, w, s)]
          for w in WORDS for s in SEPARATORS]),
        ("is_fraction", len(WORDS), [["is_fraction", w, util.is_fraction(w)] for w in WORDS]),
        ("pronounce_date", len(dates) * len(langs),
         [["pronounce_date", list(d), lang, common.call(util.pronounce_date, datetime.date(*d), lang)]
          for d in dates for lang in langs]),
        ("pronounce_time", len(TIMES) * len(langs),
         [["pronounce_time", t, lang, common.call(util.pronounce_time, t, lang)] for t in TIMES for lang in langs]),
    ]
