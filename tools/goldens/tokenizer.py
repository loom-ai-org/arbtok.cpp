"""Goldens of arbtok.tokenizer.

The sentences are the diacritized gold sentences, each with the lect of its
gold file, the undiacritized ones with MSA, and every Arabic test string with
MSA. A lect is the spec code spec_for_lang gives, as ArbtokG2PPlugin passes it.
A Sentence record is [surface, lang, stress, pausal, [word surfaces, ipa]]. A
word record holds every WordToken property, and a character record every
CharToken property, each with bool() applied where Python returns a truthy
value.
"""
from arbtok import tokenizer
from arbtok.dialects import spec_for_lang

from . import common

ARABIC = range(0x0600, 0x0700)


def has_arabic(text):
    return any(ord(c) in ARABIC for c in text)


def sentence_inputs():
    out = [(s, lect) for lect, s in common.gold_sentences()]
    out += [(s, "ar") for _, s in common.gold_sentences("raw")]
    out += [(s, "ar") for s in common.test_strings() if has_arabic(s)]
    out += [(s, "ar") for s in grid()]
    return common.unique((s, spec_for_lang(lect)) for s, lect in out)


def grid():
    letters = "بتثجحخدذرزسشصضطظعغفقكلمنهوي"
    out = []
    for c in letters:
        out += ["ال" + c + "ّ" + "َ" + "ب", "ال" + c + "َ" + "ب", "لِل" + c + "ّ" + "َ", "وَال" + c + "ِ", c + "ّ"]
    out += ["مِائَة", "مائتان", "ثلاثمائة", "بالمائة", "مَائِدَة", "مائت", "مائتًا", "مائتي", "مِائَتَيْنِ", "مِِائة",
            "ٱلْ", "ٱسْم", "اِسْم وَاِسْم", "ّ", "َّ", "ا", "آ", "إ", "ى", "ة", "يّ", "وّ", "ِيّ", "ُوّ", "ﻻ", "چا", "المـرء",
            "مِن رَبِّهِمْ", "مَن مَعِي", "فِي الْبَيْت.", "كِتَابًا.", "abc ك 12", "ك\tم\nن", "؟،؛", "…ـ", "a.b"]
    return out


def word_inputs():
    pairs = []
    for s, lang in sentence_inputs():
        for w in tokenizer.Sentence(s, lang=lang).tokens:
            pairs.append((w.surface, lang))
    return common.unique(pairs)


def char_record(t):
    return [t.surface, t.char_idx, t.is_first_char, t.is_first_word, t.is_last_char, t.is_last_word, t.is_punct,
            t.lang, t.normalized, t.is_vowel, bool(t.has_diacritic), bool(t.has_shada), bool(t.has_sukun),
            bool(t.has_vowel_mark), t.is_sun, t.is_moon, bool(t.is_silent), t.is_tanwin, t.ipa]


def word_record(w):
    return [w.surface, w.word_idx, w.lang, w.is_punct, w.is_first_word, w.is_last_word, w.end_with_vowel,
            w.normalized, w.has_definite_article, w.is_proclitic, w.is_sun, w.is_reference_register, w.ipa,
            [char_record(t) for t in w.tokens]]


def sentence(args):
    s, lang, stress, pausal = args
    sent = tokenizer.Sentence(s, lang=lang, stress=stress, pausal=pausal)
    return ["Sentence", s, lang, stress, pausal, [sent.normalized, [w.surface for w in sent.tokens],
                                                  common.call(lambda: sent.ipa)]]


def words(args):
    s, lang = args
    return ["Sentence.tokens", s, lang, common.call(lambda: [word_record(w) for w in
                                                             tokenizer.Sentence(s, lang=lang).tokens])]


def corpora():
    sentences = sentence_inputs()
    texts = common.unique([s for s, _ in sentences] + common.test_strings() + common.lexicon_words()
                          + common.UNICODE_PROBES)
    flags = [(st, pa) for st in (True, False) for pa in (True, False)]
    return [
        common.corpus("normalize_unicode", texts, lambda t: ["normalize_unicode", t, tokenizer.normalize_unicode(t)]),
        common.corpus("elide_silent_alif", texts, lambda t: ["elide_silent_alif", t, tokenizer.elide_silent_alif(t)]),
        common.corpus("reorder_diacritics", texts, lambda t: ["_reorder_diacritics", t,
                                                              tokenizer._reorder_diacritics(t)]),
        common.corpus("sentence", [(s, l, st, pa) for s, l in sentences for st, pa in flags], sentence),
        common.corpus("words", sentences, words),
    ]
