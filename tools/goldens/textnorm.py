"""Goldens of the part of arbtok.textnorm that phase 1 ports: is_arabic_lang.

dialects calls it, so it comes with dialects. The rest of textnorm is phase 3.
"""
from arbtok.textnorm import ARABIC_LANGUAGE_CODES, is_arabic_lang

from . import common
from .dialects import tag_grid


def corpora():
    tags = common.unique(tag_grid() + common.UNICODE_PROBES + ["ARB", "Ar", "AR_sa", "ar_", "_ar", "-ar", "ar--",
                                                               "İar", "aR-x", "mey", "MEY_mr", "ara", "arc", "arn"])
    return [
        common.corpus("const", [0], lambda _: ["const", "ARABIC_LANGUAGE_CODES", sorted(ARABIC_LANGUAGE_CODES)]),
        common.corpus("is_arabic_lang", tags, lambda t: ["is_arabic_lang", t, common.call(is_arabic_lang, t)]),
    ]
