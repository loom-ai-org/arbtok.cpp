"""Goldens of the part of arbtok.lattice that the tokenizer calls.

tokenizer.WordToken reads each word from the lattice, so word_ipa,
defers_to_cascade, spec_word_exception and the five structural rescorers come
with the tokenizer in phase 1. A lattice slot is written as [grapheme, [start,
end], [[ipa, repr(cost)], ...]].
"""
from arbtok import lattice
from arbtok.dialects import spec_for_lang

from . import common
from .tokenizer import sentence_inputs, word_inputs


def slot(s):
    return [s.grapheme, list(s.span), [[c.ipa, repr(c.cost)] for c in s.candidates]]


def corpora():
    pairs = word_inputs()
    words = common.unique(w for w, _ in pairs)
    lects = common.unique(spec_for_lang(lect) for _, lect in sentence_inputs())
    exception_pairs = [(w, lect) for lect in lects for w in common.spec_exception_words()]
    return [
        common.corpus("constants", ["DEFAULT_RESCORERS", "DEFAULT_LANG"],
                      lambda n: ["const", n, [type(r).__name__ for r in lattice.DEFAULT_RESCORERS]
                                 if n == "DEFAULT_RESCORERS" else lattice.DEFAULT_LANG]),
        common.corpus("defers_to_cascade", words, lambda w: ["defers_to_cascade", w, lattice.defers_to_cascade(w)]),
        common.corpus("word_ipa", [(w, l, s) for w, l in pairs for s in (False, True)],
                      lambda p: ["word_ipa", p[0], p[1], p[2], common.call(lattice.word_ipa, *p)]),
        common.corpus("word_lattice", pairs[::3],
                      lambda p: ["word_lattice", p[0], p[1], [slot(s) for s in lattice.word_lattice(*p)]]),
        common.corpus("spec_word_exception", common.unique(pairs[::5] + exception_pairs),
                      lambda p: ["spec_word_exception", p[0], p[1], lattice.spec_word_exception(*p)]),
    ]
