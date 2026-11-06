"""Goldens of arbtok.plugin.

ArbtokG2PPlugin runs under a set of constructor keyword sets: the defaults (the
fusion diacritizer, the pausal register), the full register, the lattice
diacritizer, nativize, arabizi, stress and diacritize off, no stem or closed-class
lexicon, three varieties, and three lattice sets: the full register, no stem or
closed-class lexicon, and ar-EG with no closed-class lexicon. One instance serves
all the calls of a keyword set, in record order, as scriptconv keeps one engine
per variety.

The texts are every 12th Arabic sdist test string, every sdist test string that
holds both a Latin and an Arabic letter, the first 60 that hold a Latin letter
and no Arabic one, and EXTRA_TEXTS: Latin words with a curly apostrophe, and
ar-EG sentences whose closed-class words change the reading. transcribe runs the
defaults over every text, also with the Arabizi hint true and false, each other
keyword set over every 4th text, and the lattice sets over EXTRA_TEXTS too.
normalize runs the defaults over every Arabic text and three other keyword
sets over every 4th one. transcribe_word runs the words of every
400th line of tests/ar_test.txt, every 100th closed-class lexicon word, every
10th spec word exception and Latin words, alone and with orthographic neighbours and a
WordContext lang. The constructor corpus checks the attributes that each keyword
set gives, and the register that is refused.
"""
from arbtok import plugin
from arbtok.translit import is_latin
from orthography2ipa import WordContext

from . import common

CONFIGS = [{}, {"register": "full"}, {"fusion": False}, {"nativize": False}, {"arabizi": False}, {"stress": False},
           {"diacritize": False}, {"lexicon": None, "dialect_lexicon": False}, {"lang": "ar-EG"},
           {"lang": "ar-SA-x-najd"}, {"lang": "ar-MA"}]
LATTICE_CONFIGS = [{"fusion": False, "register": "full"}, {"fusion": False, "lexicon": None, "dialect_lexicon": False},
                   {"fusion": False, "lang": "ar-EG", "dialect_lexicon": False}]
EXTRA_TEXTS = ["it’s", "don’t wait", "قال it’s مهم", "ازيك يا صاحبي", "انا مش فاهم ليه كده", "ده مش كويس خالص"]
NORMALIZE_CONFIGS = [{}, {"register": "full"}, {"fusion": False}, {"diacritize": False}]
WORD_CONFIGS = [{}, {"register": "full"}, {"stress": False}, {"nativize": False}, {"lang": "ar-EG"}]
INIT_CONFIGS = CONFIGS + LATTICE_CONFIGS + [
    {"register": "bad"}, {"register": ""}, {"register": "full", "pausal": True}, {"pausal": False}, {"lang": "xx"},
    {"lang": "en"}, {"lang": "ar_SA"}, {"lang": "arz"}, {"lang": None}, {"lexicon": "", "fusion": False}]
ATTRIBUTES = ["lang", "fusion", "diacritize", "stress", "lexicon", "dialect_lexicon", "nativize", "arabizi", "pausal"]
CONTEXTS = [None, {}, {"prev_word": "فِي"}, {"next_word": "الْبَيْت"}, {"prev_word": "مِنْ", "next_word": "رَبِّهِمْ"},
            {"lang": "ar-EG"}, {"lang": "xx"}, {"prev_word": "و", "lang": "ar-SA-x-najd"},
            {"prev_word": "", "next_word": ""}]

_INSTANCES = {}


def instance(config):
    key = common.dumps(config)
    if key not in _INSTANCES:
        _INSTANCES[key] = plugin.ArbtokG2PPlugin(**config)
    return _INSTANCES[key]


def context(spec):
    return None if spec is None else WordContext(**spec)


def has_arabic(s):
    return any("؀" <= c <= "ۿ" for c in s)


def texts():
    strings = common.test_strings()
    arabic = common.arabic_test_strings()[::12]
    mixed = [s for s in strings if is_latin(s) and has_arabic(s)]
    latin = [s for s in strings if is_latin(s) and not has_arabic(s)][:60]
    return common.unique(arabic + mixed + latin + EXTRA_TEXTS)


def words():
    out = common.corpus_words(400) + common.lexicon_words()[::100] + common.spec_exception_words()[::10]
    out += ["meeting", "Google", "BMW", "7abibi", "model", "الـcharger", "", "،", "كتاب.", "ٱسْمُ", "مِنْ"]
    return common.unique(out)


def init_record(config):
    def build():
        p = plugin.ArbtokG2PPlugin(**config)
        return [getattr(p, name) for name in ATTRIBUTES]
    return ["init", config, common.call(build)]


def corpora():
    ts = texts()
    arabic_ts = [t for t in ts if has_arabic(t)]
    transcribe_inputs = [(c, t, None) for c in CONFIGS for t in (ts if c == {} else ts[::4])]
    transcribe_inputs += [(c, t, None) for c in LATTICE_CONFIGS for t in common.unique(ts[::4] + EXTRA_TEXTS)]
    transcribe_inputs += [({}, t, hint) for hint in (True, False) for t in ts if is_latin(t)]
    ws = words()
    word_inputs = [(c, w, ctx) for c in WORD_CONFIGS for i, w in enumerate(ws)
                   for ctx in [CONTEXTS[0], CONTEXTS[1 + i % (len(CONTEXTS) - 1)]]]
    return [
        common.corpus("constants", [0], lambda _: ["const", "PUNCT_STRIP", plugin.PUNCT_STRIP,
                                                   plugin.ArbtokG2PPlugin().language_codes]),
        common.corpus("init", INIT_CONFIGS, init_record),
        common.corpus("normalize", [(c, t) for c in NORMALIZE_CONFIGS
                                     for t in (arabic_ts if c == {} else arabic_ts[::4])],
                      lambda a: ["normalize", a[0], a[1], common.call(instance(a[0]).normalize, a[1])]),
        common.corpus("transcribe", transcribe_inputs,
                      lambda a: ["transcribe", a[0], a[1], a[2], common.call(instance(a[0]).transcribe, a[1], a[2])]),
        common.corpus("transcribe_word", word_inputs,
                      lambda a: ["transcribe_word", a[0], a[1], a[2],
                                 common.call(instance(a[0]).transcribe_word, a[1], context(a[2]))]),
    ]
