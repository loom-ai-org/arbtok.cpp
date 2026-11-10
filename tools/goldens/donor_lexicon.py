"""Goldens of arbtok.donor_lexicon.

A sequence record starts from an empty orthography2ipa lexicon registry and
runs its steps in order: donor_engine, same (whether two calls give the same
engine), lexicon_path, bundled_path, and register (a caller's own registration
of the bundled French file through the deprecated process-wide API). It leaves the registry empty. The
result of donor_engine is the language of the engine, the entry count of its
pipeline lexicon (None when it has none), and its reading of each probe word.
A path is written relative to the package directory.
"""
import warnings
from pathlib import Path

import orthography2ipa.lexicon as o2i_lexicon
from arbtok import donor_lexicon as dn

from . import common

CODES = ["en-GB", "fr-FR", "en", "en-US", "fr", "EN-gb", "en_GB", "de-DE", "xx", "", "fr-FR.sources", "ar"]
MISSING = "/nonexistent/caller/own.tsv"


#: Words of the en-GB and fr-FR lexicons, a word that neither holds, and a word only fr-FR holds.
PROBES = ["engine", "machine", "ok", "bmw", "le", "bonjour", "zzyzx", "après"]


def caller_file():
    """The caller's own lexicon in the register step: the bundled French file, whatever the code."""
    return common.package_dir() / "data" / "donor_lexicons" / "fr-FR.tsv"


def rel(path):
    if path is None:
        return None
    return str(Path(path).relative_to(common.package_dir()))


def engine(code):
    g2p = dn.donor_engine(code)
    lexicon = g2p.pipeline.lexicon
    return [g2p.lang, None if lexicon is None else len(lexicon),
            [common.call(g2p.transcribe_word, w) for w in PROBES]]


def step(op, code):
    if op == "donor_engine":
        return common.call(engine, code)
    if op == "same":
        return common.call(lambda: dn.donor_engine(code) is dn.donor_engine(code))
    if op == "lexicon_path":
        return common.call(lambda: rel(o2i_lexicon.lexicon_path(code)))
    if op == "bundled_path":
        return rel(dn.bundled_path(code))
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        if op == "register":
            o2i_lexicon.register_lexicon(code, str(caller_file()))
            return None
        if op == "register_missing":
            o2i_lexicon.register_lexicon(code, MISSING)
            return None
    raise ValueError(op)


def sequences():
    out = [[["bundled_path", c]] for c in CODES]
    out += [[["donor_engine", c], ["lexicon_path", c], ["same", c]] for c in CODES]
    out += [[["register", "en-GB"], ["donor_engine", "en-GB"], ["lexicon_path", "en-GB"], ["same", "en-GB"]],
            [["register", "en"], ["donor_engine", "en-GB"], ["lexicon_path", "en-GB"], ["lexicon_path", "en"]],
            [["donor_engine", "en-GB"], ["register", "en-GB"], ["lexicon_path", "en-GB"], ["donor_engine", "en-GB"]],
            [["register", "fr-FR"], ["donor_engine", "fr-FR"], ["donor_engine", "en-GB"], ["lexicon_path", "en-GB"]],
            [["donor_engine", "en"], ["lexicon_path", "en-GB"]],
            [["register", "xx"], ["donor_engine", "xx"]],
            [["register_missing", "en-GB"], ["donor_engine", "en-GB"], ["lexicon_path", "en-GB"]],
            [["register_missing", "fr-FR"], ["donor_engine", "en-GB"], ["donor_engine", "fr-FR"]]]
    return out


def clear_registry():
    o2i_lexicon._REGISTERED.clear()
    o2i_lexicon.get_lexicon.cache_clear()


def run(steps):
    clear_registry()
    try:
        return ["sequence", steps, [step(op, code) for op, code in steps]]
    finally:
        # The dump runs the next modules in this process, and they must not
        # see a registration of a sequence, such as fr-FR at a missing file.
        clear_registry()


def corpora():
    return [
        common.corpus("constants", ["BUNDLED_DONOR"], lambda n: ["const", n, dn.BUNDLED_DONOR]),
        common.corpus("bundled_path_default", [None], lambda _: ["bundled_path_default", rel(dn.bundled_path())]),
        common.corpus("sequence", sequences(), run),
    ]
