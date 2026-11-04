"""Goldens of arbtok.nisba: restore_nisba over every test string, lexicon word,
gold word and spec exception, and over a grid of words that end in ⟨ي⟩."""
import itertools

from arbtok import nisba

from . import common

MARKS = ["", "َ", "ِ", "ُ", "ْ", "ّ", "ً", "ٰ", "ِّ", "ّْ"]
STEMS = ["عرب", "مصر", "كتاب", "باق", "ثان", "جار", "ب", "ا", "اا", "عن", "تاكس", "الذ", "مَصْر", "عَرَب", "ق ب"]


def grid():
    out = []
    for stem, before, after in itertools.product(STEMS, MARKS, MARKS):
        out.append(stem + before + "ي" + after)
    out += [w + "ي" for w in nisba.NOT_NISBA] + sorted(nisba.NOT_NISBA)
    out += ["ي", "يي", "ْي", "aي", "abي", "ابي", "اباي", "ياي", "زاكي", "ىي", "ي\n", "عربي\n"]
    return out


def corpora():
    words = common.unique(common.test_strings() + common.lexicon_words() + common.spec_exception_words()
                          + common.words_of(s for _, s in common.gold_sentences())
                          + common.words_of(s for _, s in common.gold_sentences("raw")) + grid()
                          + common.UNICODE_PROBES)
    return [
        common.corpus("constants", [0], lambda _: ["const", "NOT_NISBA", sorted(nisba.NOT_NISBA)]),
        common.corpus("restore_nisba", words, lambda w: ["restore_nisba", w, nisba.restore_nisba(w)]),
    ]
