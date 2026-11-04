"""Goldens of arbtok.stress.

The IPA words are the gold transcriptions with their stress marks removed,
the unstressed transcriptions of the tokenizer sentences, and a grid of
syllable shapes. Each runs with MSA; every seventh runs with every Arabic lect.
proclitic_onset takes 0, 1 and 2 on every pair, and on the MSA pairs also a
negative value and one past the end, which Python slices by its own rules.
"""
import itertools

from arbtok import stress
from arbtok.tokenizer import Sentence

from . import common
from .tokenizer import sentence_inputs

SHAPES = ["ka", "kaː", "kat", "kaːt", "katt", "ʔa", "sˤa", "ðˤaː", "dʒi", "tʃu", "iː", "a", "u"]


def ipa_inputs():
    gold = common.words_of(s.replace("ˈ", "").replace("ˌ", "") for _, s in common.gold_sentences("ipa"))
    gold += common.words_of(s for _, s in common.gold_sentences("phones"))
    made = []
    for s, lang in sentence_inputs():
        ipa = Sentence(s, lang=lang, stress=False).ipa
        made += ipa.split(" ")
    grid = ["".join(p) for n in (1, 2, 3) for p in itertools.product(SHAPES[:9], repeat=n)]
    return common.unique(gold + made + grid + ["", " ", ".", "؟", "ˈka", "a b", " a  b ", "ŋ", "ɑ̃"])


def corpora():
    words = ipa_inputs()
    lects = common.arabic_lects()
    pairs = [(w, "ar") for w in words] + [(w, l) for w in words[::7] for l in lects]
    phrases = common.unique([" ".join(words[i:i + 3]) for i in range(0, len(words), 3)])
    return [
        common.corpus("first_segment_len", words, lambda w: ["_first_segment_len", w, stress._first_segment_len(w)]),
        common.corpus("stress_ipa", [(w, l, o) for w, l in pairs
                                     for o in ((0, 1, 2, -1, -3, 99) if l == "ar" else (0, 1, 2))],
                      lambda p: ["stress_ipa", p[0], p[1], p[2], common.call(stress.stress_ipa, p[0], p[1], p[2])]),
        common.corpus("stress_words", [(p, l) for p in phrases for l in ("ar", "ars-SA", "arz-EG")],
                      lambda p: ["stress_words", p[0], p[1], common.call(stress.stress_words, p[0], p[1])]),
    ]
