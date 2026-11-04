"""Goldens of arbtok.unicode_symbol2label: every item of UNICODE_SYMBOL_TO_LABEL in dict order."""
from arbtok.unicode_symbol2label import UNICODE_SYMBOL_TO_LABEL

from . import common


def corpora():
    items = list(UNICODE_SYMBOL_TO_LABEL.items())
    lookups = common.unique(common.UNICODE_PROBES + ["{", "🇵🇹", "🇵", "a", "0⃣", "0"])
    return [
        common.corpus("items", range(len(items)), lambda i: ["item", i, list(items[i])]),
        common.corpus("size", [0], lambda _: ["len", len(UNICODE_SYMBOL_TO_LABEL)]),
        common.corpus("lookup", lookups, lambda k: ["get", k, UNICODE_SYMBOL_TO_LABEL.get(k)]),
    ]
