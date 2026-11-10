"""Goldens of arbtok.o2i_plugins, and of orthography2ipa's G2P with them.

Each plugin class is called directly: language_codes and PATHS; the rescorers
that ArbtokRescorers gives for every supported lect; ArbtokDiacritizer.normalize
over Arabic test strings with the default path, "lattice", "fusion", and two
values it refuses (" Fusion " and "rawi"), with the message of each refusal; and
ArbtokSandhi.apply over the stressed words of tokenizer sentences with each
pause pattern, and over a grid of words with stress marks inside and at the
ends, and with two marks.

The g2p corpus runs orthography2ipa.G2P(lang, plugins=...).transcribe(text),
which reaches the plugins through their entry points, named "arbtok". The
plugin sets are all three stages, and each stage alone. All three run over
every Arabic sdist test string under "ar"; each stage alone over every 4th of
them; and a grid of the supported lects runs 12 test strings under each lect
with all three stages and 4 with each stage alone. One G2P serves each
(lang, plugins) pair, and one ArbtokDiacritizer serves each path.

The pipeline corpus runs G2P(lang, pipeline=pipeline(lang, path)).transcribe(text):
the lattice path over every Arabic sdist test string under "ar", the fusion
path over every 4th of them, and 4 test strings under each supported lect. The
mark corpus calls the mark step of pipeline(lang) alone, and the keep_letters
corpus calls _keep_letters over pairs of a written word and a proposed one.
"""
from arbtok import o2i_plugins
from arbtok.lattice import word_ipa
from arbtok.tokenizer import Sentence
from orthography2ipa import G2P

from . import common

PATHS = [None, "fusion", " Fusion ", "lattice", "rawi"]
STAGES = ["normalize", "rescore", "sandhi"]
PLUGIN_SETS = [{s: ["arbtok"] for s in STAGES}] + [{s: ["arbtok"]} for s in STAGES]
GRID_WORDS = [("ˈmin", "مِن"), ("ˈrabbihim", "رَبِّهِمْ"), ("maˈdiːnatun", "مَدِينَةٌ"), ("ˈqaːdˤin", "قَاضٍ"),
              ("kiˈtaːban", "كِتَابًا"), ("ˌʔal", "ال"), ("ˈ", "ـ"), ("", ""), ("ˈfiː", "فِي"), ("albajtˈ", "الْبَيْت"),
              ("ˈmuʔminun", "مُؤْمِنٌ"), ("ˈkataba", "كَتَبَ"), ("min", "مِنْ"), ("ˈbajtika", "بَيْتِكَ"),
              ("ˌkaˈtaba", "كَتَبَ"), ("ˈkaˌtaba", "كَتَبَ")]

#: Pairs of a written word and a proposed marking: a hamza on the same letter, a letter swap, an extra
#: letter, a mark alone, an empty word, and a hamza on one letter with a swap of another in the same word.
KEEP_LETTERS = [("ماده", "مَادَّة"), ("علي", "عَلَى"), ("اكل", "أَكَلَ"), ("كتب", "كَتَبَ"), ("كتب", "كتبوا"),
                ("قرا", "قَرَأَ"), ("", "َ"), ("ة", "ه"), ("هذه", "هَٰذِهِ"), ("الى", "إِلَى"), ("مدرسه", "مَدْرَسَةٌ"),
                ("سماء", "سَمَاءٌ"), ("سما", "سَمَاءٌ"), ("ا", "آ"), ("ي", "ى"), ("كتب", ""), ("", ""), ("ab", "ab"),
                ("ﻻ", "لَا"), ("é", "é"), ("اكله", "أَكَلَة"), ("الى", "إِلَي")]

_G2P = {}
_DIACRITIZERS = {}
_PIPELINES = {}


def diacritizer(path):
    if path not in _DIACRITIZERS:
        _DIACRITIZERS[path] = o2i_plugins.ArbtokDiacritizer() if path is None else o2i_plugins.ArbtokDiacritizer(path)
    return _DIACRITIZERS[path]


def normalize(args):
    path, text, lang = args
    return ["normalize", path, text, lang, common.call(lambda: diacritizer(path).normalize(text, lang))]


def refused(path):
    try:
        o2i_plugins.ArbtokDiacritizer(path)
    except ValueError as exc:
        return ["refused", path, str(exc)]
    return ["refused", path, None]


def pipeline_g2p(lang, path):
    if (lang, path) not in _PIPELINES:
        _PIPELINES[(lang, path)] = G2P(lang, pipeline=o2i_plugins.pipeline(lang, path))
    return _PIPELINES[(lang, path)]


def transcribe_pipeline(args):
    lang, path, text = args
    return ["pipeline", lang, path, text, common.call(lambda: pipeline_g2p(lang, path).transcribe(text))]


def mark(args):
    lang, text = args
    return ["mark", lang, text, common.call(lambda: pipeline_g2p(lang, "lattice").pipeline.mark(text))]


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
    normalize_inputs = [(path, t, lang) for i, t in enumerate(arabic[::10])
                        for path, lang in [(None, "ar"), (PATHS[1 + i % (len(PATHS) - 1)], ["ar", "arz-EG", "apc"][i % 3])]]
    pipeline_inputs = [("ar", "lattice", t) for t in arabic] + [("ar", "fusion", t) for t in arabic[::4]]
    pipeline_inputs += [(lect, "lattice", t) for i, lect in enumerate(lects) for t in short[i * 4:(i + 1) * 4]]
    pipeline_inputs += [("ar", path, t) for path in PATHS[2::2] for t in arabic[:2]]
    mark_inputs = [(lang, t) for i, t in enumerate(arabic[::3]) for lang in [["ar", "arz-EG", "apc"][i % 3]]]
    mark_inputs += [("ar", t) for t in ["", " ", "كتب  الولد", "كتب\tالولد", "hello كتب", " كتب ", "ﻻ"]]
    return [
        common.corpus("constants", [0], lambda _: ["const", list(o2i_plugins.ArbtokDiacritizer.PATHS),
                                                   o2i_plugins.__all__]),
        common.corpus("language_codes", [diacritizer(None), rescorers, sandhi],
                      lambda p: ["language_codes", type(p).__name__, p.language_codes]),
        common.corpus("rescorers", lects, lambda lang: [
            "rescorers", lang, [type(r).__name__ for r in rescorers.rescorers(lang)]]),
        common.corpus("normalize", normalize_inputs, normalize),
        common.corpus("refused", PATHS[2::2] + ["", "LATTICE", "lattice "], refused),
        common.corpus("sandhi", sandhi_inputs(arabic), lambda a: [
            "sandhi", a[0], a[1], a[2], a[3], common.call(sandhi.apply, a[0], a[1], a[2], a[3])]),
        common.corpus("g2p", g2p_inputs, transcribe),
        common.corpus("pipeline", pipeline_inputs, transcribe_pipeline),
        common.corpus("mark", mark_inputs, mark),
        common.corpus("keep_letters", KEEP_LETTERS, lambda a: [
            "keep_letters", a[0], a[1], common.call(o2i_plugins._keep_letters, a[0], a[1])]),
    ]
