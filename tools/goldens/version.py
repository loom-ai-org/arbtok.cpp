"""Goldens of arbtok.version: the version block and VERSION_STR."""
from arbtok import version

from . import common

NAMES = ["VERSION_MAJOR", "VERSION_MINOR", "VERSION_BUILD", "VERSION_ALPHA", "VERSION_STR"]


def corpora():
    return [common.corpus("constants", NAMES, lambda n: ["const", n, getattr(version, n)])]
