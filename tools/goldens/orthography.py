"""Goldens of arbtok.orthography.

The class inventory is the rawi vocabulary the package bundles, in class-id
order, plus a small synthetic inventory with duplicate-mark classes. A mark
set is written sorted. source_marks runs with three normalizers: the identity,
NFC and NFD.
"""
import itertools
import json
import unicodedata

from arbtok import orthography

from . import common

NORMALIZERS = {"identity": lambda s: s, "NFC": lambda s: unicodedata.normalize("NFC", s),
               "NFD": lambda s: unicodedata.normalize("NFD", s)}
SYNTHETIC = ["", "ٔ", "ٕ", "ٓ", "َ", "ٔ" + "َ", "َ" + "ٔ", "ِِّ", "ِّ", "ّ" + "ِ", "ٓ" + "َ", "x", "ٔٔ"]


def rawi_classes():
    vocab = json.loads((common.package_dir() / "models" / "rawi_v2.vocab.json").read_text(encoding="utf-8"))
    by_id = {i: s for s, i in vocab["diac_to_idx"].items()}
    return [by_id[i] for i in range(len(by_id))]


def mark_sets(classes):
    sets = {frozenset(c) for c in classes}
    marks = sorted(orthography.ORTHOGRAPHIC_MARKS | orthography.VOCALIC_MARKS)
    sets |= {frozenset(p) for p in itertools.combinations(marks, 2)}
    sets |= {frozenset(), frozenset({"x"}), frozenset({"ٔ", "x"}), frozenset({"ٓ", "ٔ"}), frozenset({"ٕ", "ٔ"}),
             frozenset({"ٓ", "ّ", "َ"})}
    return sorted((sorted(s) for s in sets), key=lambda s: (len(s), s))


def corpora():
    inventories = {"rawi": rawi_classes(), "synthetic": SYNTHETIC}
    consts = [["const", "ORTHOGRAPHIC_MARKS", sorted(orthography.ORTHOGRAPHIC_MARKS)],
              ["const", "VOCALIC_MARKS", sorted(orthography.VOCALIC_MARKS)],
              ["const", "MADDA", orthography.MADDA],
              ["classes", "rawi", inventories["rawi"]], ["classes", "synthetic", SYNTHETIC]]
    allowed, pinned, projected = [], [], []
    for name, classes in inventories.items():
        for marks in mark_sets(classes):
            for allow in (True, False):
                allowed.append(["allowed_classes", name, marks, allow,
                                orthography.allowed_classes(classes, set(marks), allow)])
                for predicted in range(len(classes)):
                    projected.append(["project_class", name, predicted, marks, allow,
                                      orthography.project_class(classes, predicted, set(marks), allow)])
            pinned.append(["pinned_class", name, marks, orthography.pinned_class(classes, set(marks))])
    texts = common.unique(common.test_strings() + common.lexicon_words() + [s for _, s in common.gold_sentences()]
                          + common.UNICODE_PROBES)

    def source(pair):
        text, norm = pair
        bases, marks = orthography.source_marks(text, NORMALIZERS[norm])
        return ["source_marks", text, norm, [bases, [sorted(m) for m in marks]]]
    return [
        common.corpus("constants", consts, lambda r: r),
        common.corpus("allowed_classes", allowed, lambda r: r),
        common.corpus("pinned_class", pinned, lambda r: r),
        common.corpus("project_class", projected, lambda r: r),
        common.corpus("source_marks", [(t, n) for t in texts for n in NORMALIZERS], source),
    ]
