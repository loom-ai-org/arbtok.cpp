"""Goldens of arbtok.sandhi.

apply_cross_word runs over the (ipa, surface, is_punct) words of every
tokenizer sentence, as Sentence.ipa builds them, in both waqf modes, with the
sentence's lect and with MSA. A grid adds the edge cases: empty IPA, runs of
punctuation, and the article and waṣl words after each kind of neighbour.
"""
import itertools

from arbtok import sandhi
from arbtok.tokenizer import Sentence

from . import common
from .tokenizer import sentence_inputs

GRID_WORDS = [("min", "مِن", False), ("man", "مَن", False), ("rabbihim", "رَبِّهِمْ", False),
              ("bajtika", "بَيْتِكَ", False), ("lisaːn", "لِسَان", False), ("jaquːlu", "يَقُولُ", False),
              ("fiː", "فِي", False), ("albajt", "الْبَيْت", False), ("ˈalqalam", "الْقَلَم", False),
              ("inta", "اِنْتَ", False), ("ana", "اَنَا", False), ("kitaːban", "كِتَابًا", False),
              ("madiːnatun", "مَدِينَةٌ", False), ("qaːdˤin", "قَاضٍ", False), ("huwa", "هُوَ", False),
              ("kataba", "كَتَبَ", False), ("madiːnata", "مَدِينَةَ", False), ("", "ـ", False),
              (".", ".", True), ("،", "،", True), ("ʔaː", "آ", False), ("ʃʃams", "الشَّمْس", False),
              ("ismu", "ٱسْمُ", False), ("ɑlwalad", "ٱلْوَلَد", False)]


def triples(s, lang):
    return [[w.ipa, w.surface, w.is_punct] for w in Sentence(s, lang=lang).tokens]


def grid():
    out = []
    for n in (1, 2, 3):
        for combo in itertools.product(GRID_WORDS, repeat=n):
            if n == 3 and len(out) > 6000:
                break
            out.append([list(w) for w in combo])
    return out


def corpora():
    inputs = []
    for s, lang in sentence_inputs():
        words = triples(s, lang)
        for code in common.unique([lang, "ar"]):
            inputs += [(words, p, code) for p in (True, False)]
    inputs += [(words, p, l) for words in grid() for p in (True, False) for l in ("ar", "arb", "arz-EG")][::2]

    def record(args):
        words, pausal, lang = args
        return ["apply_cross_word", words, pausal, lang,
                common.call(sandhi.apply_cross_word, [tuple(w) for w in words], pausal, lang)]
    surfaces = common.unique([w[1] for w in GRID_WORDS] + common.lexicon_words() + common.spec_exception_words())
    ipas = common.unique([w[0] for w in GRID_WORDS] + ["", "a", "aː", "atun", "atin", "atan", "un", "in", "an",
                                                       "kitaːbu", "qaːdˤi", "t"])
    return [
        common.corpus("constants", [0], lambda _: ["const", "NUN_ASSIMILATION", list(sandhi.NUN_ASSIMILATION.items())]),
        common.corpus("apply_cross_word", inputs, record),
        common.corpus("pausal", [(i, s) for i in ipas for s in surfaces[::4]],
                      lambda p: ["_pausal", p[0], p[1], sandhi._pausal(p[0], p[1])]),
        common.corpus("has_article", surfaces, lambda s: ["_has_article", s, sandhi._has_article(s)]),
        common.corpus("is_bare_wasl_alif", surfaces, lambda s: ["_is_bare_wasl_alif", s,
                                                                sandhi._is_bare_wasl_alif(s)]),
    ]
