"""Goldens of arbtok.dialect_lexicon.

The lects are every supported lect, the names of the bundled files, the gold
lects and a set of tags that do not resolve. A path is written relative to the
package directory, whose data/ directory is arbtok::data_dir() in C++. parse
runs over generated TSV texts, and get over the keys and vocalizations of the
bundled files and the Arabic test words.
"""
import random
from pathlib import Path

from arbtok import dialect_lexicon as dl

from . import common, envcase
from .lexicon import LINE_BREAKS


def rel(path):
    return None if path is None else str(Path(path).relative_to(common.package_dir()))


def lects():
    out = common.arabic_lects()
    out += [p.stem for p in sorted(dl.lexicon_dir().glob("*.tsv"))]
    out += [lect for lect, _ in common.gold_sentences()]
    out += ["", "xx", "en", "ar-EG", "arz", "ar-MA", "AR-ma", "ar_EG", "ar-x-maghrebi", "ar-SA", "x-clade-afro1255", "sem"]
    return common.unique(out)


def tsv_texts():
    rng = random.Random(7)
    pieces = ["key", "vocalization", "كي", "كِي", "ﻻ", "مش", "مِش", "#c", "# x", "", " ", "a", "كَّ", "src", "gloss"]
    out = ["", "#", "# only a comment\n", "key\tvocalization\tsource\tgloss\n", "a\tb", "a", "a\t", "\tb", "a\tb\tc\td\te",
           "a\tb\na\tc", " #\tx", "#a\tb", "a\tb\n\n\nc\td"]
    for _ in range(600):
        lines = []
        for _ in range(rng.randint(1, 6)):
            lines.append("\t".join(rng.choice(pieces) for _ in range(rng.randint(1, 4))))
        out.append("".join(line + rng.choice(LINE_BREAKS) for line in lines))
    return common.unique(out)


def lookup_words():
    words = common.lexicon_words() + common.words_of(common.arabic_test_strings())[:1500]
    return common.unique(words + ["", " ", "كي ", "ﻛﻲ"])


def corpora():
    all_lects = lects()
    lexicons = {lect: dl.DialectLexicon(lect) for lect in all_lects}
    words = lookup_words()
    with_files = [lect for lect in all_lects if dl.lexicon_file(lect) is not None or len(lexicons[lect]) > 0]
    gets = [(lect, w) for lect in with_files for w in words]
    cases = [{"call": ["dialect_entries", lect, "<ROOT>/lex"],
              "files": {"lex/ar-EG.tsv": envcase.hexof("key\tvocalization\nمش\tمِش\nده\tدَه\n"),
                        "lex/ar-x-mashriqi.tsv": envcase.hexof("مش\tمُش\nفين\tفِين\n"),
                        "lex/notes.txt": envcase.hexof("x\ty\n")}}
             for lect in ["ar-EG", "arz-EG", "aec-EG", "ar", "ar-x-mashriqi", "xx"]]
    cases.append({"call": ["dialect_entries", "ar-EG", "<ROOT>/none"]})
    return [
        common.corpus("lexicon_dir", [None], lambda _: ["lexicon_dir", rel(dl.lexicon_dir())]),
        common.corpus("available_lects", [None], lambda _: ["available_lects", dl.available_lects()]),
        common.corpus("lexicon_file", all_lects, lambda lect: ["lexicon_file", lect, common.call(lambda: rel(dl.lexicon_file(lect)))]),
        common.corpus("parent_chain", all_lects, lambda lect: ["parent_chain", lect, dl._parent_chain(lect)]),
        common.corpus("entries", all_lects, lambda lect: ["entries", lect, common.call(lambda: list(lexicons[lect].entries.items()))]),
        common.corpus("len", all_lects, lambda lect: ["len", lect, common.call(lambda: len(lexicons[lect]))]),
        common.corpus("get", gets, lambda p: ["get", p[0], p[1], lexicons[p[0]].get(p[1])]),
        common.corpus("contains", gets[::7], lambda p: ["contains", p[0], p[1], p[1] in lexicons[p[0]]]),
        common.corpus("parse", tsv_texts(), lambda t: ["parse", t, list(dl.parse(t).items())]),
        common.corpus("env", cases, envcase.run),
    ]
