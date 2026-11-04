"""Goldens of arbtok.arabizi over every test string, the Latin words of the gold
sets and the donor lexicons, and a seeded fuzz set of ASCII, digits, primes,
punctuation and Unicode case probes."""
from arbtok import arabizi

from . import common

ALPHABET = "abcdefghijklmnopqrstuvwxyzSDTZAEKX0123456789'-.,!?()\"،؛؟ _" + "İıßé٣ك"


def corpora():
    words = common.unique(common.test_strings() + common.words_of(s for _, s in common.gold_sentences("cs_words"))
                          + [w for _, s in common.gold_sentences("cs_words") for w in s.split(";")]
                          + common.words_of(s for _, s in common.gold_sentences())
                          + common.donor_words() + common.fuzz_strings(11, 3000, ALPHABET)
                          + common.UNICODE_PROBES)
    hints = [None, True, False]
    return [
        common.corpus("constants", [0], lambda _: ["const", "ARABIZI_DIGITS", list(arabizi.ARABIZI_DIGITS.items())]),
        common.corpus("is_arabizi", [(w, h) for w in words for h in hints],
                      lambda p: ["is_arabizi", p[0], p[1], arabizi.is_arabizi(p[0], p[1])]),
        common.corpus("arabizi_covers", words, lambda w: ["arabizi_covers", w, arabizi.arabizi_covers(w)]),
        common.corpus("to_arabic_skeleton", words, lambda w: ["to_arabic_skeleton", w,
                                                              arabizi.to_arabic_skeleton(w)]),
    ]
