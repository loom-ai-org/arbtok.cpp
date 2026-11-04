"""Goldens of arbtok.constants: every public constant, a set written sorted."""
from arbtok import constants

from . import common


def names():
    return [n for n, v in vars(constants).items() if not n.startswith("_") and isinstance(v, (str, set))]


def corpora():
    def record(name):
        value = getattr(constants, name)
        return ["const", name, sorted(value) if isinstance(value, set) else value]
    return [common.corpus("constants", names(), record)]
