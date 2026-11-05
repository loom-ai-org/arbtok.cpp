"""Goldens of arbtok.diacritize and of the package function arbtok.vocalize.

A word record runs LatticeDiacritizer.diacritize_word_record on one shared
diacritizer per lect and option set: [lect, options, word, [output,
provenance], the names of the lists the call appended the word to]. The
options are those of LatticeDiacritizer as a dict, and "model_path": "bundled"
passes the bundled model and vocabulary paths. A vocalize record is
arbtok.vocalize(text, lect, **options): [text, lect, options, [text, [[surface,
output, provenance], ...]]]. The vocalize corpus is the Arabic word of every
400th line of tests/ar_test.txt under every supported lect, the gold sentences
under their own lect, and the Arabic test strings under MSA. The default stem
lexicon is read from the Hugging Face cache.
"""
import arbtok
from arbtok import diacritize as dz
from arbtok._ensemble import DEFAULT_ENSEMBLE_ONNX, DEFAULT_VOCAB
from arbtok.tokenizer import normalize_unicode
from orthography2ipa import get, underdetermined_positions

from . import common

LISTS = ("rejected", "repaired", "looked_up", "dialect_looked_up")
CONFIG_LECTS = ["ar", "arz-EG", "ary-MA", "ars-SA", "apc-SY", "acm-IQ", "aeb-TN", "arq-DZ", "ar-x-maghrebi",
                "ar-Latn-x-buckwalt"]
CONFIGS = [{"waqf": False}, {"lexicon": None}, {"dialect_lexicon": False}, {"model_path": "bundled"},
           {"waqf": False, "lexicon": None, "dialect_lexicon": False}]
EDGE_WORDS = ["", " ", "abc", "123", "٣", "...", "؟", "ـ", "كتاب", "الكتاب", "وكَان", "كِي", "عِيش", "آبد", "عربي",
              "ﻻ", "ٱلْكِتَاب", "كَتَبَ", "xك", "َ", "كََ", "الـكتاب", "bّ"]
EDGE_TEXTS = ["", " ", "  ", "كتاب  جديد", " كتاب", "كتاب ", "كتاب\tجديد", " كتاب", "كتاب　جديد",
              "abc كتاب 12", "\n", "كتاب\n"]


def options(config):
    kw = dict(config)
    if kw.get("model_path") == "bundled":
        kw["model_path"], kw["vocab_path"] = DEFAULT_ENSEMBLE_ONNX, DEFAULT_VOCAB
    return kw


def word_inputs():
    words = common.unique(common.words_of(common.arabic_test_strings()))[:2500]
    words += common.corpus_words(1000) + EDGE_WORDS
    return common.unique(words)


def positions_inputs():
    spec = get("ar")
    words = common.unique(normalize_unicode(w) for w in word_inputs())
    out = [(w, underdetermined_positions(w, spec)) for w in words]
    synthetic = [[], [0], [1], [0, 1], [1, 2], [2, 3], [0, 2], [3, 4], [1, 1], [5]]
    for w in ["الكتاب", "والكتاب", "وَالكتاب", "بالبيت", "ٱلبيت", "أل", "ال", "لل", "فَٱل", "وكَان", "وك", "سيكتب", "و كان",
              "بَك", "وٌك", "كتاب"]:
        out += [(w, p) for p in synthetic]
    return out


def text_record(out):
    return [out.text, [[w.surface, w.output, w.provenance] for w in out.words]]


def vocalize_inputs():
    lects = common.arabic_lects()
    out = [(w, lect, {}) for w in common.corpus_words(400) for lect in lects]
    out += [(s, lect, {}) for lect, s in common.gold_sentences()]
    out += [(s, "ar", {}) for s in common.arabic_test_strings()]
    out += [(s, "ar", {}) for s in EDGE_TEXTS]
    out += [(s, lect, {"lexicon": None}) for lect, s in common.gold_sentences()[::3]]
    out += [(s, lect, {"waqf": False, "lexicon": None}) for lect, s in common.gold_sentences()[1::3]]
    out += [(s, lect, {"dialect_lexicon": False}) for lect, s in common.gold_sentences()[:6]]
    return out


def corpora():
    readers = {}

    def reader(lect, config):
        key = (lect, repr(sorted(config.items())))
        if key not in readers:
            readers[key] = dz.LatticeDiacritizer(lang=lect, **options(config))
        return readers[key]

    def word(p):
        lect, config, w = p
        d = reader(lect, config)
        before = {name: len(getattr(d, name)) for name in LISTS}
        rec = d.diacritize_word_record(w)
        grown = [name for name in LISTS if len(getattr(d, name)) != before[name]]
        return ["word", lect, config, w, [rec.output, rec.provenance], grown]

    words = word_inputs()
    word_cases = [(lect, config, w) for lect in CONFIG_LECTS for config in CONFIGS for w in words]
    text_cases = [(lect, config, t) for lect in ("ar", "arz-EG") for config in ({}, {"lexicon": None})
                  for t in EDGE_TEXTS + common.arabic_test_strings()[:300]]

    def text(p):
        lect, config, t = p
        out = reader(lect, config).diacritize_text(t)
        return ["text", lect, config, t, text_record(out) + [list(out.by_provenance.items()), str(out)]]

    flags = list(dz.PROVENANCE) + ["", "Model", "unknown"]
    bad_lects = ["zz-invalid", "", "en", "xx", "de-DE"]
    return [
        common.corpus("constants", ["PROVENANCE", "_MARKS", "_RESTORABLE"],
                      lambda n: ["const", n, list(dz.PROVENANCE) if n == "PROVENANCE" else sorted(dz._MARKS)
                                 if n == "_MARKS" else [[k, sorted(v)] for k, v in dz._RESTORABLE.items()]]),
        common.corpus("provenance_flags", flags, lambda p: ["provenance_flags", p,
                                                            [dz.DiacritizedWord("x", "x", p).lect_constrained,
                                                             dz.DiacritizedWord("x", "x", p).is_failure]]),
        common.corpus("author_complete", positions_inputs(),
                      lambda p: ["author_complete", p[0], p[1], dz._author_complete(p[0], p[1])]),
        common.corpus("proclitic_complete", positions_inputs(),
                      lambda p: ["proclitic_complete", p[0], p[1], dz._proclitic_complete(p[0], p[1])]),
        common.corpus("lattice_errors", bad_lects,
                      lambda lect: ["lattice_error", lect, common.call(lambda: dz.LatticeDiacritizer(lang=lect) and "ok")]),
        common.corpus("word", word_cases, word),
        common.corpus("text", text_cases, text),
        common.corpus("vocalize", vocalize_inputs(),
                      lambda p: ["vocalize", p[0], p[1], p[2], text_record(arbtok.vocalize(p[0], p[1], **p[2]))]),
    ]
