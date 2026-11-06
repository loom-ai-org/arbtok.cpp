"""Goldens of arbtok.o2i_plugins, and of orthography2ipa's G2P with them.

Each plugin class is called directly: language_codes and PATHS; the rescorers
that ArbtokRescorers gives for every supported lect; ArbtokDiacritizer.normalize
over Arabic test strings under ARBTOK_DIACRITIZER unset, "fusion", " Fusion ",
"lattice" and a value it refuses; and ArbtokSandhi.apply over the stressed
words of tokenizer sentences with each pause pattern, and over a grid of
words with stress marks inside and at the ends, and with two marks.

The g2p corpus runs orthography2ipa.G2P(lang, plugins=...).transcribe(text),
which reaches the plugins through their entry points, named "arbtok". The
plugin sets are all three stages, and each stage alone. All three run over
every Arabic sdist test string under "ar"; each stage alone over every 4th of
them; and a grid of the supported lects runs 12 test strings under each lect
with all three stages and 4 with each stage alone. One G2P serves each
(lang, plugins) pair, and one ArbtokDiacritizer serves the normalize corpus.
"""
import os

from arbtok import o2i_plugins
from arbtok.lattice import word_ipa
from arbtok.tokenizer import Sentence
from orthography2ipa import G2P

from . import common

ENV = [None, "fusion", " Fusion ", "lattice", "rawi"]
STAGES = ["normalize", "rescore", "sandhi"]
PLUGIN_SETS = [{s: ["arbtok"] for s in STAGES}] + [{s: ["arbtok"]} for s in STAGES]
GRID_WORDS = [("ˈmin", "مِن"), ("ˈrabbihim", "رَبِّهِمْ"), ("maˈdiːnatun", "مَدِينَةٌ"), ("ˈqaːdˤin", "قَاضٍ"),
              ("kiˈtaːban", "كِتَابًا"), ("ˌʔal", "ال"), ("ˈ", "ـ"), ("", ""), ("ˈfiː", "فِي"), ("albajtˈ", "الْبَيْت"),
              ("ˈmuʔminun", "مُؤْمِنٌ"), ("ˈkataba", "كَتَبَ"), ("min", "مِنْ"), ("ˈbajtika", "بَيْتِكَ"),
              ("ˌkaˈtaba", "كَتَبَ"), ("ˈkaˌtaba", "كَتَبَ")]

_G2P = {}
_DIACRITIZER = o2i_plugins.ArbtokDiacritizer()


def with_env(value, fn, *args):
    old = os.environ.pop("ARBTOK_DIACRITIZER", None)
    if value is not None:
        os.environ["ARBTOK_DIACRITIZER"] = value
    try:
        return common.call(fn, *args)
    finally:
        os.environ.pop("ARBTOK_DIACRITIZER", None)
        if old is not None:
            os.environ["ARBTOK_DIACRITIZER"] = old


def g2p(lang, plugins):
    key = (lang, common.dumps(plugins))
    if key not in _G2P:
        _G2P[key] = G2P(lang, plugins=plugins)
    return _G2P[key]


def transcribe(args):
    lang, plugins, text = args
    return ["g2p", lang, plugins, text, common.call(lambda: g2p(lang, plugins).transcribe(text))]


def sentence_words(text, lang):
    """The stressed IPA and the surface of each word of a tokenizer sentence, and its pause flag."""
    tokens = Sentence(text, lang=lang).tokens
    words, surfaces, pausal = [], [], []
    for i, t in enumerate(tokens):
        if t.is_punct:
            if pausal:
                pausal[-1] = True
            continue
        words.append(common.call(word_ipa, t.surface, lang))
        surfaces.append(t.surface)
        pausal.append(i == len(tokens) - 1)
    return [w if isinstance(w, str) else "" for w in words], surfaces, pausal


def sandhi_inputs(arabic):
    out = []
    for i, text in enumerate(arabic[::12]):
        lang = ["ar", "arz-EG", "ars-SA", "apc"][i % 4]
        words, surfaces, pausal = sentence_words(text, lang)
        out.append((words, surfaces, pausal, lang))
        out.append((words, surfaces, [False] * len(words), lang))
    for n in (1, 2, 3):
        for start in range(len(GRID_WORDS)):
            combo = [GRID_WORDS[(start + k * 5) % len(GRID_WORDS)] for k in range(n)]
            for mask in range(2 ** n):
                out.append(([w for w, _ in combo], [s for _, s in combo], [bool(mask >> k & 1) for k in range(n)],
                            "ar"))
    return out


def corpora():
    lects = common.arabic_lects()
    arabic = common.arabic_test_strings()
    short = [s for s in arabic if 8 <= len(s) <= 80]
    g2p_inputs = [("ar", PLUGIN_SETS[0], t) for t in arabic]
    g2p_inputs += [("ar", plugins, t) for plugins in PLUGIN_SETS[1:] for t in arabic[::4]]
    g2p_inputs += [(lect, PLUGIN_SETS[0], t) for i, lect in enumerate(lects) for t in short[i * 12:(i + 1) * 12]]
    g2p_inputs += [(lect, plugins, t) for i, lect in enumerate(lects) for plugins in PLUGIN_SETS[1:]
                   for t in short[i * 4:(i + 1) * 4]]
    rescorers = o2i_plugins.ArbtokRescorers()
    sandhi = o2i_plugins.ArbtokSandhi()
    normalize_inputs = [(env, t, lang) for i, t in enumerate(arabic[::10])
                        for env, lang in [(None, "ar"), (ENV[1 + i % (len(ENV) - 1)], ["ar", "arz-EG", "apc"][i % 3])]]
    return [
        common.corpus("constants", [0], lambda _: ["const", list(o2i_plugins.ArbtokDiacritizer.PATHS),
                                                   o2i_plugins.__all__]),
        common.corpus("language_codes", [_DIACRITIZER, rescorers, sandhi],
                      lambda p: ["language_codes", type(p).__name__, p.language_codes]),
        common.corpus("rescorers", lects, lambda lang: [
            "rescorers", lang, [type(r).__name__ for r in rescorers.rescorers(lang)]]),
        common.corpus("normalize", normalize_inputs, lambda a: [
            "normalize", a[0], a[1], a[2], with_env(a[0], _DIACRITIZER.normalize, a[1], a[2])]),
        common.corpus("sandhi", sandhi_inputs(arabic), lambda a: [
            "sandhi", a[0], a[1], a[2], a[3], common.call(sandhi.apply, a[0], a[1], a[2], a[3])]),
        common.corpus("g2p", g2p_inputs, transcribe),
    ]
