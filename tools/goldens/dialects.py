"""Goldens of arbtok.dialects.

spec_for_lang records carry the UserWarnings the call raises, as
[value, [message, ...]]. The calls that dialects makes into orthography2ipa
(resolve, resolves_exactly) and into langcodes (closest_match) are recorded as
well, with the arguments arbtok passes, so the C++ resolution layer is checked
on exactly the inputs it receives. A dict whose order comes from a spec file is
written as sorted items; a dict defined in arbtok is written in its own order.
"""
import functools
import warnings

import langcodes
import orthography2ipa
from orthography2ipa import available_codes, get, registry

from arbtok import dialects
from arbtok.constants import ALIF

from . import common

LANGUAGES = ["ar", "arb", "arz", "ars", "afb", "apc", "ary", "arq", "aeb", "acw", "aec", "ayl", "acm", "shu", "mey",
             "ara", "arc", "arn", "en", "fr", "pt", "mt", "xx", "und", "AR", "Ar"]
REGIONS = ["", "SA", "EG", "AE", "MA", "DZ", "TN", "IQ", "SY", "LB", "JO", "KW", "QA", "BH", "OM", "YE", "SD", "LY",
           "TR", "TD", "NG", "MR", "EH", "PS", "IL", "ZZ", "US", "FR", "001", "419", "sa", "eg"]
SUFFIXES = ["", "-x-najd", "-x-najdi", "-najdi", "-x-cairo", "-x-riyadh", "-Arab", "-Latn", "-Arab-x-gulf", "-x-gulf",
            "-x-hejaz", "-x-hejazi", "-x-saidi", "-x-verylongtoken", "-u-nu-arab", "-x-levantine", "-shami", "-1901",
            "-x-a", "-x", "-x-qassim", "-x-baharna", "-x-dhofari", "-maghrebi", "-x-buckwalt"]


def tag_grid():
    grid = []
    for lang in LANGUAGES:
        for region in REGIONS:
            base = lang + ("-" + region if region else "")
            grid += [base + suffix for suffix in SUFFIXES]
    return common.unique(grid)


def tag_inputs():
    codes = available_codes(include_clades=True)
    arabic = [c for c in codes if dialects._is_arabic_code(c)]
    aliases = list(registry._ALIASES)
    variants = []
    for tag in arabic + [a for a in aliases if registry._ALIASES[a] in arabic]:
        variants += [tag, tag.lower(), tag.upper(), tag.replace("-", "_"), tag + "-x-zz", tag + "-ZZ"]
    short = [s for s in common.test_strings() if 0 < len(s) <= 32 and " " not in s and "\n" not in s]
    return common.unique([None, ""] + variants + tag_grid() + short + common.UNICODE_PROBES + codes)


def with_warnings(fn, *args):
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        value = common.call(fn, *args)
    return [value, [str(w.message) for w in caught if w.category is UserWarning]]


class Recorder:
    """Records each call that dialects makes into orthography2ipa and langcodes."""

    def __init__(self):
        self.calls = {"resolve": {}, "resolves_exactly": {}, "closest_match": {}}
        self.saved = []

    def __enter__(self):
        for module, name in ((orthography2ipa, "resolve"), (orthography2ipa, "resolves_exactly"),
                             (langcodes, "closest_match")):
            original = getattr(module, name)
            self.saved.append((module, name, original))
            setattr(module, name, self.wrap(name, original))
        return self

    def wrap(self, name, original):
        @functools.wraps(original)
        def inner(*args):
            key = common.dumps(list(args))
            record = [name, *[list(a) if isinstance(a, (list, tuple)) else a for a in args]]
            try:
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore")
                    result = original(*args)
            except Exception as exc:
                self.calls[name].setdefault(key, record + [{"err": type(exc).__name__}])
                raise
            self.calls[name].setdefault(key, record + [list(result) if isinstance(result, tuple) else result])
            return result
        return inner

    def __exit__(self, *exc):
        for module, name, original in self.saved:
            setattr(module, name, original)


def consonant_graphemes():
    return list(dialects.ARABIC_TO_IPA_CONSONANTS) + [ALIF, "ى", "ة", "ـ", "a", "", "ٱ"]


def corpora():
    tags = tag_inputs()
    lects = common.arabic_lects()
    codes = available_codes(include_clades=True)
    dialects._arabic_codes.cache_clear()
    with Recorder() as rec:
        spec = [["spec_for_lang", t, with_warnings(dialects.spec_for_lang, t)] for t in tags]
    lect = [["lect_code", t, common.call(dialects.lect_code, t)] for t in tags if t is not None]
    calls = [r for name in ("resolve", "resolves_exactly", "closest_match") for r in rec.calls[name].values()]

    def consonant_map(code):
        return ["consonant_map", code, sorted(dialects.consonant_map(code).items())]

    pairs = [(g, code) for code in lects for g in consonant_graphemes()]
    consts = [
        ["const", "DEFAULT_LANG", dialects.DEFAULT_LANG],
        ["const", "THEH", dialects.THEH], ["const", "THAL", dialects.THAL], ["const", "ZAH", dialects.ZAH],
        ["const", "ARABIC_TO_IPA_CONSONANTS", list(dialects.ARABIC_TO_IPA_CONSONANTS.items())],
        ["const", "DIACRITIC_TO_IPA", list(dialects.DIACRITIC_TO_IPA.items())],
        ["const", "TANWIN_TO_IPA", list(dialects.TANWIN_TO_IPA.items())],
        ["const", "VOWEL_MAP", list(dialects.VOWEL_MAP.items())],
        ["const", "WORD_EXCEPTIONS", list(dialects.WORD_EXCEPTIONS.items())],
        ["const", "_REGION_DEFAULTS", list(dialects._REGION_DEFAULTS)],
        ["const", "_DIALECT_NAME_ALIASES", list(dialects._DIALECT_NAME_ALIASES.items())],
        ["const", "_SPEC_LANGUAGE_CODES", list(dialects._SPEC_LANGUAGE_CODES.items())],
        ["const", "_VOWEL_CHARS", sorted(dialects._VOWEL_CHARS)],
        ["const", "Lect._fields", list(dialects.Lect._fields)],
    ]
    return [
        common.corpus("constants", consts, lambda r: r),
        common.corpus("spec_for_lang", spec, lambda r: r),
        common.corpus("lect_code", lect, lambda r: r),
        common.corpus("o2i_calls", calls, lambda r: r),
        common.corpus("supported_lects", [0], lambda _: ["supported_lects",
                                                         [list(x) for x in dialects.supported_lects()]]),
        common.corpus("arabic_codes", [0], lambda _: ["_arabic_codes", list(dialects._arabic_codes())]),
        common.corpus("dialect_index", [0], lambda _: ["_dialect_index",
                                                       sorted(dialects._dialect_index(dialects._arabic_codes())
                                                              .items())]),
        common.corpus("is_arabic_code", codes, lambda c: ["_is_arabic_code", c, dialects._is_arabic_code(c)]),
        common.corpus("core_subtags", [t for t in tags if t], lambda t: ["_core_subtags", t,
                                                                         common.call(dialects._core_subtags, t)]),
        common.corpus("consonant_map", lects, consonant_map),
        common.corpus("consonant_ipa", pairs, lambda p: ["consonant_ipa", p[0], p[1], "?",
                                                         dialects.consonant_ipa(p[0], p[1], "?")]),
        common.corpus("spec", lects, lambda c: ["spec", c, [get(c).parent, get(c).iso639_3, get(c).quality.value]]),
    ]
