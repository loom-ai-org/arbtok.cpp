"""Goldens of arbtok.waqf: pausal in both modes over every test string, gold
sentence, lexicon word and spec exception, and over a grid of marked words in
phrases."""
import itertools

from arbtok import waqf

from . import common

ENDINGS = ["", "َ", "ُ", "ِ", "ً", "ٌ", "ٍ", "ْ", "ّ", "ّ" + "ُ", "ُ" + "ّ", "ٰ", "ًا", "اً", "ةً", "ًة", "ةٌ", "َةٌ", "ّ" + "ً"]
STEMS = ["كِتَاب", "حَقّ", "مَدِينَ", "هُو", "قَاضٍ", "ب", "ٱلْ", "ـ", "ۓ", "ٟ"]
JOINS = [" ", ".", "، ", "؟", "!", "\n", "…", "?", " abc ", "؛", "  ", "١٢"]


def grid():
    words = [s + e for s, e in itertools.product(STEMS, ENDINGS)]
    out = list(words) + sorted(waqf.LEXICAL_FINAL_VOWEL)
    for a, j, b in itertools.product(words[::7], JOINS, words[::11]):
        out.append(a + j + b)
    return out


def corpora():
    texts = common.unique(common.test_strings() + [s for _, s in common.gold_sentences()] + common.lexicon_words()
                          + common.spec_exception_words() + grid() + common.UNICODE_PROBES)
    marks = [["const", n, getattr(waqf, n)] for n in ("FATHA", "DAMMA", "KASRA", "SUKUN", "SHADDA", "TANWIN_FATH",
                                                       "TANWIN_DAMM", "TANWIN_KASR", "TA_MARBUTA")]
    consts = marks + [["const", "LEXICAL_FINAL_VOWEL", sorted(waqf.LEXICAL_FINAL_VOWEL)],
              ["const", "SHORT_VOWELS", list(waqf.SHORT_VOWELS)], ["const", "DROPPED_TANWIN", list(waqf.DROPPED_TANWIN)],
              ["const", "KEPT", list(waqf.KEPT)]]
    return [
        common.corpus("constants", consts, lambda r: r),
        common.corpus("pausal", [(t, f) for t in texts for f in (False, True)],
                      lambda p: ["pausal", p[0], p[1], waqf.pausal(p[0], p[1])]),
    ]
