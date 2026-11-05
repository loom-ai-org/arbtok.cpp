"""Goldens of arbtok.donor_lexicon.

A sequence record starts from an empty orthography2ipa lexicon registry and
runs its steps in order: ensure_registered, lexicon_path, bundled_path, and
register (a caller's own registration). A path is written relative to the
package directory, or as "<other>" for a caller's file outside it.
"""
from pathlib import Path

import orthography2ipa.lexicon as o2i_lexicon
from arbtok import donor_lexicon as dn

from . import common

CODES = ["en-GB", "fr-FR", "en", "en-US", "fr", "EN-gb", "en_GB", "de-DE", "xx", "", "fr-FR.sources", "ar"]
MISSING = "/nonexistent/caller/own.tsv"


def rel(path):
    if path is None:
        return None
    path = Path(path)
    return str(path.relative_to(common.package_dir()))


def step(op, code):
    if op == "ensure_registered":
        return common.call(dn.ensure_registered, code)
    if op == "lexicon_path":
        return common.call(lambda: rel(o2i_lexicon.lexicon_path(code)))
    if op == "bundled_path":
        return rel(dn.bundled_path(code))
    if op == "register":
        o2i_lexicon.register_lexicon(code, str(common.package_dir() / "data" / "lexicons" / "ar-EG.tsv"))
        return None
    if op == "register_missing":
        o2i_lexicon.register_lexicon(code, MISSING)
        return None
    raise ValueError(op)


def sequences():
    out = [[["bundled_path", c]] for c in CODES]
    out += [[["ensure_registered", c], ["lexicon_path", c], ["ensure_registered", c], ["lexicon_path", c]] for c in CODES]
    out += [[["register", "en-GB"], ["ensure_registered", "en-GB"], ["lexicon_path", "en-GB"]],
            [["register", "en"], ["ensure_registered", "en-GB"], ["lexicon_path", "en-GB"], ["lexicon_path", "en"]],
            [["ensure_registered", "en-GB"], ["register", "en-GB"], ["lexicon_path", "en-GB"], ["ensure_registered", "en-GB"]],
            [["ensure_registered", "fr-FR"], ["ensure_registered", "en-GB"], ["lexicon_path", "fr-FR"], ["lexicon_path", "en-GB"]],
            [["ensure_registered", "en"], ["lexicon_path", "en-GB"]],
            [["register_missing", "en-GB"], ["ensure_registered", "en-GB"], ["lexicon_path", "en-GB"]],
            [["register_missing", "fr-FR"], ["ensure_registered", "en-GB"], ["ensure_registered", "fr-FR"]]]
    return out


def run(steps):
    o2i_lexicon._REGISTERED.clear()
    o2i_lexicon.get_lexicon.cache_clear()
    return ["sequence", steps, [step(op, code) for op, code in steps]]


def corpora():
    return [
        common.corpus("constants", ["BUNDLED_DONOR"], lambda n: ["const", n, dn.BUNDLED_DONOR]),
        common.corpus("bundled_path_default", [None], lambda _: ["bundled_path_default", rel(dn.bundled_path())]),
        common.corpus("sequence", sequences(), run),
    ]
