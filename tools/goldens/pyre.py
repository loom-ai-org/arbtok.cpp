"""Goldens of the Python re behaviour that textnorm and util depend on.

The C++ port reads the patterns of arbtok.textnorm and arbtok.util with its own
regex engine, and these records hold what Python's re gives for each of them. A
"pattern" record names a pattern, its source text and its flags. The other records
name the pattern and hold one call and its result, in code-point offsets:

    ["finditer", name, text, [[start, end, [g1start, g1end, ...]], ...]]
    ["fullmatch", name, text, [start, end, groups] or null]
    ["match", name, text, pos, [start, end, groups] or null]
    ["search", name, text, 0, endpos, [start, end, groups] or null]

A group that does not take part in a match is -1, -1.

The patterns are every compiled pattern among the module globals of
arbtok.textnorm and arbtok.util, sorted by name; the patterns the two modules
compile at call time (the unit names, a term lexicon, the identifier-word context,
the phone shapes and prefixes, a lect's number forms, the mark rules, the national
number pattern of each bundled phone plan, and util's unit, date and time
patterns); and the pattern strings the two modules pass to re functions directly.

The texts of finditer are the generated texts of textnorm.generated_texts(), the
Unicode probes, and SAMPLE other texts of textnorm.all_texts() drawn with
random.Random(SEED). fullmatch reads the phone-plan patterns over generated digit
strings and the patterns the modules fullmatch over the finditer texts. match and
search read the patterns the modules call with a pos or an endpos, at every
position of the short texts of a sample.
"""
import random
import re

from arbtok import textnorm as tn
from arbtok import util

from . import common
from . import textnorm as tn_goldens

SEED = 11
SAMPLE = 400
SHORT_SAMPLE = 60
SHORT = 40
DIGIT_STRINGS = 300

#: Pattern strings the modules pass to re functions directly.
INLINE = {
    "textnorm:nondigit": r"\D", "textnorm:digits": r"\d+", "textnorm:latin_word": "[A-Za-z]+",
    "textnorm:group_separators": "[,٬]", "textnorm:capitals": "[A-Z]{1,4}", "textnorm:space_digit": r" ?[0-9]",
    "textnorm:digit_space_end": r"[0-9] ?$", "textnorm:small_capital": "[a-z]?[A-Z]",
    "textnorm:small_capitals": "[a-z][A-Z]{1,3}", "textnorm:code_pieces": "[0-9]+|[A-Za-z]",
    "textnorm:arabic_letters": "[ء-ي]+", "textnorm:plain_skeleton": "[ً-ٰٕـۖ-ۭ]",
    "textnorm:waw_dagger": "و[ً-ْ]*ٰ", "textnorm:maqsura_dagger": "ى[ً-ْ]*ٰ",
    "textnorm:space_or_hyphen": "[ -]", "textnorm:capital": "[A-Z]", "textnorm:digit": "[0-9]",
    "textnorm:digit_space_end_any": r"[0-9٠-٩]\s*$", "textnorm:_TERM_PROCLITIC": tn._TERM_PROCLITIC,
    "util:ampm": r"(?i)(\d+)(am|pm)", "util:time": (r"(\d{1,2})h(\d{2})", re.IGNORECASE),
    "util:date": r"(\d{1,4})[/-](\d{1,2})[/-](\d{1,4})", "util:word_hyphen_digit": r"(\w+)-(\d+)",
}
#: The patterns the modules fullmatch.
FULLMATCH = ["textnorm._CLOCK_WORD", "textnorm._ARABIC_WORD", "textnorm._DEFINITE", "textnorm._YEAR_WORD",
             "textnorm._NAME_AND_DIGITS", "textnorm._VIN", "textnorm._YEAR_DIGITS", "textnorm._SPEAKABLE",
             "textnorm._EDGE_PUNCTUATION", "textnorm:_TERM_PROCLITIC", "textnorm:latin_word", "textnorm:capitals",
             "textnorm:small_capital", "textnorm:small_capitals", "textnorm:arabic_letters", "tts:prefixes"]
#: The patterns the modules match at a pos, and those they search up to an endpos.
MATCH_AT = ["textnorm._MONTH_BESIDE_YEAR", "textnorm._AFTER_KILOMETRE", "textnorm._PER_HOUR",
            "textnorm._AMOUNT_AFTER", "textnorm._FEMININE_COUNTED", "textnorm._MARCH_AFTER",
            "textnorm._LATIN_WORD_AFTER", "textnorm._EDGE_PUNCTUATION"]
SEARCH_TO = ["textnorm._IDENTIFIER_BEFORE", "textnorm._MARCH_BEFORE", "textnorm._LATIN_WORD_BEFORE"]


def unit_patterns(full_lang):
    """The patterns util._normalize_units compiles for full_lang, built as it builds them."""
    lang_code = full_lang.split("-")[0]
    decimal_separator, thousands_separator = util._get_number_separators(full_lang)
    thousands, decimal = re.escape(thousands_separator), re.escape(decimal_separator)
    grouped = re.compile(rf"\d{{1,3}}(?:{thousands}\d{{3}})+(?:{decimal}\d+)?")
    number_pattern_str = (rf"(?<![\d.,])({grouped.pattern}"
                          rf"|\d+(?:[{decimal}.]\d+)?)(?![\d.,]*\d)")
    units = util.UNITS[lang_code]
    symbolic = sorted((k for k in units if not k.isalnum()), key=len, reverse=True)
    alphanumeric = sorted((k for k in units if k.isalnum()), key=len, reverse=True)
    out = {f"util:grouped:{full_lang}": grouped}
    if symbolic:
        out[f"util:symbolic:{full_lang}"] = re.compile(
            number_pattern_str + r"\s*(" + "|".join(re.escape(u) for u in symbolic) + r")", re.IGNORECASE)
    if alphanumeric:
        out[f"util:alphanumeric:{full_lang}"] = re.compile(
            number_pattern_str + r"\s*(" + "|".join(re.escape(u) for u in alphanumeric) + r")\b", re.IGNORECASE)
    return out


def patterns():
    out = {}
    for module, prefix in ((tn, "textnorm"), (util, "util")):
        for name in sorted(vars(module)):
            if isinstance(getattr(module, name), re.Pattern):
                out[f"{prefix}.{name}"] = getattr(module, name)
    out["textnorm._unit_names"] = tn._unit_names()[0]
    out["textnorm._term_pattern:small"] = tn._term_pattern(tn._as_lexicon(
        {"BMW": "بي", "X5": "اكس", "Land Cruiser": "لاند", "MG": "ام", "C-HR": "سي", "e-tron": "اي"}))[0]
    out["textnorm._term_pattern:cars-sa"] = tn._term_pattern(tn._as_lexicon(tn.bundled_tts_lexicon("cars-sa")))[0]
    phones, prefixes, context = tn._tts_patterns(tn.TtsNorm(
        identifier_words=tn.IDENTIFIER_WORDS, phone_shapes=tn_goldens.TTS_TUPLE_VALUES["phone_shapes"],
        phone_prefixes=tn_goldens.TTS_TUPLE_VALUES["phone_prefixes"]))
    out["tts:phones"], out["tts:prefixes"], out["tts:context"] = phones, prefixes, context
    out["textnorm._lect_forms:ar"] = tn._lect_forms("ar", tn.TtsNorm(number_forms=tn_goldens.NUMBER_FORMS))[1]
    out["textnorm._lect_forms:arz"] = tn._lect_forms("arz", tn.TtsNorm(dialect_numbers=True,
                                                                       number_forms=tn_goldens.NUMBER_FORMS))[1]
    for name, flags in (("all", dict(strip_harakat=True, strip_extended_marks=True, strip_quranic_marks=True,
                                     strip_tatweel=True)),
                        ("harakat_tatweel", dict(strip_harakat=True, strip_tatweel=True)),
                        ("quranic", dict(strip_quranic_marks=True))):
        out[f"textnorm._compiled:{name}"] = tn._compiled(tn.AsrNorm(**flags))[0]
    for region, (_, _, types) in tn._phone_plans().items():
        for kind, (pattern, _) in types.items():
            out[f"phone:{region}:{kind}"] = pattern
    for full_lang in ("ar", "en", "pt", "de"):
        out.update(unit_patterns(full_lang))
    for name, source in INLINE.items():
        out[name] = re.compile(*source) if isinstance(source, tuple) else re.compile(source)
    return out


def match_value(m):
    if m is None:
        return None
    groups = []
    for g in range(1, (m.re.groups or 0) + 1):
        groups += list(m.span(g))
    return [m.start(), m.end(), groups]


def finditer_texts():
    texts = tn_goldens.all_texts()
    generated = tn_goldens.generated_texts()
    rest = [t for t in texts if t not in set(generated)]
    rng = random.Random(SEED)
    return common.unique(generated + common.UNICODE_PROBES + sorted(rng.sample(rest, min(SAMPLE, len(rest)))))


def short_texts():
    pool = sorted(t for t in tn_goldens.all_texts() if 0 < len(t) <= SHORT)
    rng = random.Random(SEED + 1)
    generated = [t for t in tn_goldens.generated_texts() if len(t) <= SHORT]
    return common.unique(generated + sorted(rng.sample(pool, min(SHORT_SAMPLE, len(pool)))))


def digit_strings():
    rng = random.Random(SEED + 2)
    out = []
    for region, kinds in sorted(tn._plan_starts().items()):
        for kind, (one, three) in sorted(kinds.items()):
            for lead, lengths in sorted(list(one.items()) + list(three.items())):
                for length in sorted(lengths):
                    out.append(lead + "".join(rng.choice("0123456789") for _ in range(length - len(lead))))
    while len(out) < DIGIT_STRINGS:
        out.append("".join(rng.choice("0123456789") for _ in range(rng.randint(0, 13))))
    return common.unique(out + ["", "0", "5", "٥٥١٢٣٤٥٦٧", "55123456a", " 551234567"])


def corpora():
    table = patterns()
    names = list(table)
    texts = finditer_texts()
    digits = digit_strings()
    short = short_texts()
    phone_names = [n for n in names if n.startswith("phone:")]
    fullmatch_names = [n for n in FULLMATCH if n in table]
    positions = sum(len(t) + 1 for t in short)
    return [
        ("patterns", len(names), [["pattern", n, table[n].pattern, table[n].flags] for n in names]),
        ("finditer", len(names) * len(texts),
         [["finditer", n, t, [match_value(m) for m in table[n].finditer(t)]] for n in names for t in texts]),
        ("fullmatch", len(phone_names) * len(digits) + len(fullmatch_names) * len(texts),
         [["fullmatch", n, t, match_value(table[n].fullmatch(t))] for n in phone_names for t in digits]
         + [["fullmatch", n, t, match_value(table[n].fullmatch(t))] for n in fullmatch_names for t in texts]),
        ("match", len(MATCH_AT) * positions,
         [["match", n, t, p, match_value(table[n].match(t, p))] for n in MATCH_AT for t in short
          for p in range(len(t) + 1)]),
        ("search", len(SEARCH_TO) * positions,
         [["search", n, t, 0, p, match_value(table[n].search(t, 0, p))] for n in SEARCH_TO for t in short
          for p in range(len(t) + 1)]),
    ]
