"""Goldens of arbtok.fusion.

A logprobs record holds a float32 row and its result as little-endian hex.
The rows are the logits of the ensemble over Arabic test words, and synthetic
rows with ties, infinities, NaN and large values. A fusion record runs
FusionDiacritizer.diacritize on one shared diacritizer per lect and option set:
[lect, options, text, [output, the items each list gained, last_nbest]]. A
last_nbest entry is [text, repr(logprob), repr(lattice_cost), class_ids].
"""
import numpy as np

from arbtok import fusion as fu
from arbtok._ensemble import get_ensemble

from . import common
from .diacritize import CONFIG_LECTS, EDGE_TEXTS

LISTS = ("recovered", "looked_up", "dialect_looked_up", "rejected")
CONFIGS = [{"lexicon": None}, {"waqf": False}, {"beam": 2, "topk": 2}, {"lattice_weight": 0.0},
           {"dialect_lexicon": False}, {"ambiguous_share": None}, {"beam": 16, "topk": 6, "lexicon": None},
           {"beam": 1, "topk": 1, "lexicon": None}, {"topk": 75, "beam": 3, "lexicon": None}]
FALLBACK_TEXTS = ["كتاب 😀 جديد", "😀", "كتاب ™", "كتاب\tجديد قلم"]


def hexrow(a):
    return np.asarray(a, dtype="<f4").tobytes().hex()


def rows():
    ens = get_ensemble()
    out = []
    for w in common.unique(common.words_of(common.arabic_test_strings()))[:400]:
        _, logits, _ = ens.logits(w)
        if logits is not None:
            out += [hexrow(r) for r in logits]
    inf, nan = np.inf, np.nan
    synthetic = [[0.0] * 75, [1.0, 1.0, 1.0], [5.0], [-inf, 0.0, 1.0], [inf, 0.0], [nan, 1.0, 2.0], [1e30, -1e30, 0.0],
                 [88.0, 0.0, -88.0], [3.4e38, 3.4e38], [-104.0, 0.0], [-87.5, 0.0, -100.0] * 25,
                 list(np.linspace(-30, 30, 75)), [2.0] * 7 + [1.0] * 9, [-0.0, 0.0]]
    out += [hexrow(r) for r in synthetic]
    return common.unique(out)


def texts():
    out = common.corpus_words(2000) + [s for _, s in common.gold_sentences()]
    out += common.arabic_test_strings()[:600] + EDGE_TEXTS + FALLBACK_TEXTS
    return common.unique(out)


def corpora():
    readers = {}

    def reader(lect, config):
        key = (lect, repr(sorted(config.items())))
        if key not in readers:
            readers[key] = fu.FusionDiacritizer(lang=lect, **config)
        return readers[key]

    def fuse(p):
        lect, config, t = p
        d = reader(lect, config)
        before = {name: len(getattr(d, name)) for name in LISTS}
        out = d.diacritize(t)
        grown = {name: getattr(d, name)[before[name]:] for name in LISTS}
        nbest = [[h.text, repr(h.logprob), repr(h.lattice_cost), list(h.class_ids)] for h in d.last_nbest]
        return ["fusion", lect, config, t, [out, grown, nbest]]

    all_texts = texts()
    cases = [(lect, {}, t) for lect in CONFIG_LECTS for t in all_texts]
    cases += [(lect, config, t) for lect in ("ar", "arz-EG") for config in CONFIGS for t in all_texts[::3]]
    probes = common.unique(common.arabic_test_strings()[:2000] + common.UNICODE_PROBES + ["ﷺ", "ﻻ", "ء", "ـ", "٠", "ٰ", "ک", "ݐ", "ࢠ"])
    hyps = [("كِتَاب", -1.2345, 0.5), ("", 0.0, 0.0), ("x'y\"", -1e-4, 2.0005), ("a", -0.0005, 0.0015), ("b", 12.3456789, -3.0)]
    return [
        common.corpus("logprobs", rows(), lambda h: ["logprobs", h,
                                                     hexrow(fu.logprobs(np.frombuffer(bytes.fromhex(h), dtype="<f4")))]),
        common.corpus("has_arabic_letter", probes, lambda w: ["has_arabic_letter", w, fu._has_arabic_letter(w)]),
        common.corpus("hypothesis_repr", hyps, lambda h: ["hypothesis_repr", h[0], repr(h[1]), repr(h[2]),
                                                          repr(fu.Hypothesis((), h[1], h[0], h[2]))]),
        common.corpus("fusion", cases, fuse),
    ]
