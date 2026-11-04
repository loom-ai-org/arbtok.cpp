"""Write data/numbers/*.json from the reference ovos-number-parser,
ovos-date-parser and unicode-rbnf.

Each file holds module-level tables of the reference, by their Python names,
as the reference interpreter holds them after import: a set is a sorted list,
a dict keeps its insertion order, an int key is written as a string, and a
float is written as its repr string. Only the tables that the ported Arabic
code reads are written.

The holiday table is not a copy of reference data. ovos-date-parser asks
chronologia about a text that names a holiday, and chronologia is not ported.
holidays_ar.json holds, for every text of the domain arbtok calls
extract_datetime with that names a holiday, the answer of the reference.

Usage, with the reference interpreter:
    python -I data/numbers/gen_data.py data/numbers
"""
import datetime
import itertools
import json
import os
import pathlib
import sys

import ovos_date_parser
import ovos_date_parser.dates_ar as da
import ovos_number_parser.numbers_ar as na
import unicode_rbnf
from ovos_date_parser import extract_datetime, holidays

NUMBERS_AR = [
    "_ONES_AR", "_ONES_FEM_AR", "_TEENS_FEM_AR", "_TWELVE_FEM_OBLIQUE_AR", "_TENS_AR", "_HUNDREDS_AR",
    "_SCALES_AR", "_MINUS_AR", "_DECIMAL_AR", "_INFINITY_AR", "_FRACTIONS_AR", "_ORDINAL_STEMS_AR",
    "_ORDINAL_COMPOUND_UNIT_AR", "_ORDINAL_STEMS_FEM_AR", "_ORDINAL_COMPOUND_UNIT_FEM_AR",
    "AR_DIALECT_DEFAULT_CASE", "AR_LECT_FORMS",
    "_UNITS_LOOKUP", "_TENS_LOOKUP", "_HUNDREDS_LOOKUP", "_SCALES_LOOKUP", "_SCALE_DUALS_LOOKUP",
    "_FRACTIONS_LOOKUP", "_ACCUSATIVE_OR_CONSTRUCT_DUAL", "_TEEN_FIRST_LOOKUP", "_TEEN_SECOND_LOOKUP",
    "_AND_ONE", "_FUSED_TEENS_LOOKUP", "_MINUS_LOOKUP", "_DECIMAL_LOOKUP", "_HUNDRED_MULT_LOOKUP",
    "_CONSTRUCT_HUNDRED_LOOKUP", "_SPOKEN_HALF_LOOKUP", "_LECTS_WITH_SPOKEN_HALF", "_WATER_HUNDRED",
    "_DEAD_TWO_HUNDRED", "_CONSTRUCT_HUNDRED", "_DAY_NAMES", "_HUNDRED_UNIT_HOMOGRAPHS",
    "_CONSTRUCT_DUAL_SCALES", "_PRICE_OR_EIGHTH", "_HOMOGRAPHS_AR", "_DAY_WORDS", "_PER_CENT_WORDS",
    "_PER_CENT_AFTER_FI", "_UNIT_WORDS_AR", "_CONTEXT_CANONICAL_AR", "_ORDINAL_UNITS_LOOKUP",
    "_NUMBER_WORDS", "_PROCLITICS_AR", "_PROCLITIC_HOSTS", "_PROCLITIC_NOT_NUMBERS", "_PROCLITIC_NOT_HOSTS",
    "_CLOCK_WORD", "_FIRST_ORDINALS", "_SECOND_OR_A_SECOND", "_BROKEN_PLURAL_HEADS", "_PUNCT_AR",
    "_FUSED_LITERARY_TEENS", "_DUAL_NOUN_TOKENS",
]

DATES_AR = [
    "_FEM_TEENS_AR", "_HOUR_NAMES_AR", "_TO_THE_HOUR_AR", "_UNITS_LOOKUP_AR", "_DUALS_LOOKUP_AR",
    "_MONTHS_LOOKUP_AR", "_WEEKDAYS_LOOKUP_AR", "_HOUR_LOOKUP_AR", "_MINUTE_WORDS_AR", "_TODAY_WORDS",
    "_TOMORROW_WORDS", "_YESTERDAY_WORDS", "_AM_WORDS", "_PM_WORDS", "_NOON_WORDS", "_MIDNIGHT_WORD",
    "_NEXT_WORDS", "_PREV_WORDS", "_AFTER_WORDS", "_BEFORE_WORDS", "_CLOCK_WORDS", "_CALENDAR_UNITS",
    "_EXCEPT_WORD", "_QUARTER_WORDS", "_THIRD_WORDS", "_HALF_WORDS", "_DAY_WORD", "_OF_WORD", "_IN_WORD",
]


def plain(value):
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return value
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return {"float": repr(value)}
    if isinstance(value, datetime.timedelta):
        return {"days": value.days}
    if isinstance(value, (set, frozenset)):
        return sorted(plain(v) for v in value)
    if isinstance(value, (list, tuple)):
        return [plain(v) for v in value]
    if isinstance(value, dict):
        return {str(k): plain(v) for k, v in value.items()}
    raise TypeError(type(value))


def tables(module, names):
    return {name: plain(getattr(module, name)) for name in names}


def spellings(word):
    """Every word whose _loosen form is ``word``: a letter that NFD writes as a
    base letter and a combining mark may stand for that base letter."""
    sources = {"ا": "اآأإ", "و": "وؤ", "ي": "يئ"}
    return {"".join(p) for p in itertools.product(*(sources.get(c, c) for c in word))}


def holiday_table():
    phrases = sorted(holidays._holiday_phrases("ar"))
    anchor = datetime.datetime(2000, 1, 1)
    answers = {}
    for phrase in phrases:
        if " " in phrase:
            continue
        for word in sorted(spellings(phrase)):
            for text in [word] + [f"الساعة {hour} {word}" for hour in range(0, 25)]:
                assert holidays._names_a_holiday(text, "ar"), text
                read = extract_datetime(text, "ar", anchor)
                answers[text] = None if read is None else [read[0].isoformat(), read[1]]
    # The holiday door folds a text code point by code point; in the Arabic
    # block that is exact, because every mark NFC could compose is stripped.
    fold = {chr(c): holidays._fold(chr(c)) for c in range(0x0600, 0x0700) if holidays._fold(chr(c)) != chr(c)}
    return {"phrases": phrases, "fold": fold, "answers": answers}


def main():
    out = pathlib.Path(sys.argv[1])
    files = {
        "numbers_ar.json": tables(na, NUMBERS_AR),
        "dates_ar.json": tables(da, DATES_AR),
        "holidays_ar.json": holiday_table(),
        "rbnf_languages.json": sorted(p[:-4] for p in os.listdir(
            os.path.join(os.path.dirname(unicode_rbnf.__file__), "rbnf")) if p.endswith(".xml")),
    }
    for name, data in files.items():
        (out / name).write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    src = pathlib.Path(ovos_date_parser.__file__).parent / "res" / "ar" / "date_time.json"
    (out / "date_time_ar.json").write_bytes(src.read_bytes())
    print(f"wrote {len(files) + 1} files to {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
