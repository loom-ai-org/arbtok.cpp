"""Goldens of arbtok.translit.

The words are every n-th row of the bundled donor lexicons, the Latin words of
the sdist test strings, the established loans in several spellings, a few
Cyrillic, Greek and mixed-script words, and the Unicode probes. The IPA inputs
are every n-th reading of the donor lexicons, the readings that the English G2P
gives for the words, the keys and values of the nativisation tables, and the
phoneme inventory of every supported lect.

transliterate runs each word under a grid of the supported lects: every word
under ar, ar-EG and ar-SA-x-najd, and each other lect on a rotating share of the
words. It runs with the donor that the script gives, with strict, and with an
explicit donor (en-GB, fr-FR, ru, an unknown code). nativize and the private
_project run on the same grid. nativization_table runs for every supported lect
and for tags that resolve through the parent chain. A table is written by name:
the constants corpus writes the items of each named table.
"""
from arbtok import translit
from arbtok.donor_lexicon import ensure_registered
from orthography2ipa import G2P

from . import common

TABLE_NAMES = ["SEGMENT_MAP", "_EGYPTIAN_MAP", "_LEVANTINE_MAP", "_DEFAULT_MAP", "_MAGHREBI_MAP", "_RAISED"]
EXTRA_TAGS = ["ar-KW", "ar-LB", "ar-SY", "ar-JO", "ar-PS", "ary", "arq", "aeb", "ar-MA", "ar-DZ", "ar-TN", "ar_SA",
              "ar-SA", "AR-eg", "arb", "en", "en-GB", "fr", "xx", "", "ar-Latn", "mey", "ar-x-gulf", "ar-x-peninsular",
              "apc", "ars-SA", "arz-EG", "ar-001", "und"]
LOAN_SPELLINGS = ["model", "Model", "MODEL", " video ", "Vidéo", "automatic", "AUTOMATIC", "automatique", "modèle",
                  "video\t", "Mödel"]
FOREIGN_WORDS = ["Москва", "привет", "Αθήνα", "καλημέρα", "Zürich", "café", "naïve", "façade", "ÉCOLE", "Straße",
                 "İstanbul", "ﬁle", "Ωmega", "aβc", "abcمرحبا", "مرحباabc", "x", "7abibi", "3ala", "don't", "e-mail",
                 "O'Neil", "BMW", "GPS", "OK", "ok", "WiFi", "iPhone", "5G", "COVID-19", "x86", "ŋ", "ʃa", ""]
DONORS = [None, "en-GB", "en", "fr-FR", "ru", "el", "xx-YY"]


def name_of(table):
    return next(n for n in TABLE_NAMES if getattr(translit, n) is table)


def donor_ipas(limit_per_file=600):
    """The readings of the bundled donor lexicons, every n-th row."""
    out = []
    for path in sorted((common.package_dir() / "data" / "donor_lexicons").glob("*.tsv")):
        rows = [line.split("\t") for line in path.read_text(encoding="utf-8").splitlines()
                if line and not line.startswith("#")]
        rows = [r for r in rows if len(r) >= 2]
        step = max(1, len(rows) // limit_per_file)
        out += [r[-1] if path.name.endswith(".sources.tsv") else r[1] for r in rows[::step]]
    return common.unique(out)


def latin_test_words():
    return common.unique(w for w in common.words_of(common.test_strings()) if translit.is_latin(w))


def words():
    return common.unique(common.donor_words(700) + latin_test_words()[:600] + LOAN_SPELLINGS + FOREIGN_WORDS)


def english_readings(ws):
    ensure_registered("en-GB")
    g = G2P("en-GB")
    out = []
    for w in ws:
        try:
            out.append(g.transcribe_word(w))
        except Exception:
            pass
    return common.unique(x for x in out if x)


def grid(items, lects, main=("ar", "arz-EG", "ars-SA")):
    """Each item under the main lects, and under one other lect in turn."""
    others = [lect for lect in lects if lect not in main]
    out = []
    for i, item in enumerate(items):
        out += [(item, lect) for lect in main]
        out.append((item, others[i % len(others)]))
    return out


def corpora():
    lects = common.arabic_lects()
    ws = words()
    ipas = common.unique(donor_ipas() + english_readings(ws[:900]))
    table_strings = common.unique(s for n in TABLE_NAMES for kv in getattr(translit, n).items() for s in kv)
    inventory = common.unique(p for lect in lects for p in translit._targets(lect))
    segments = common.unique(s for ipa in ipas for s in translit.segment_ipa(ipa.replace("ˈ", "").replace("ˌ", "")))
    segments = common.unique(segments + table_strings + ["o", "oː", "e", "eː", "ɔ", "ø", "œ", "y", "ʁ", "ɜ", "ɐ",
                                                         "x", "q", "ħ", "ʔ", "ɣ", "ŋ", "ɲ", "ʎ", "ã", "ə̃", "pʰ"])
    strings = common.unique(common.test_strings() + ws + common.UNICODE_PROBES)
    script_probes = [chr(c) for c in list(range(0x0041, 0x0250, 3)) + list(range(0x0370, 0x0530, 3)) +
                     list(range(0x0600, 0x0700, 5)) + list(range(0x1E00, 0x2000, 7)) + list(range(0x3040, 0x30FF, 9)) +
                     [0x05D0, 0x0905, 0x0E01, 0x10A0, 0x13A0, 0x1D400, 0x1D6A8, 0xFB50, 0xFF21, 0x2C00, 0xA640]]

    const_records = [["const", "DONOR_LANG", translit.DONOR_LANG],
                     ["const", "DONOR_BY_SCRIPT", list(translit.DONOR_BY_SCRIPT.items())],
                     ["const", "ESTABLISHED_LOANS", [[k, list(v.items())] for k, v in translit.ESTABLISHED_LOANS.items()]],
                     ["const", "_TABLES", [[k, name_of(v)] for k, v in translit._TABLES.items()]],
                     ["const", "_BORROWABLE_TABLES", [name_of(t) for t in translit._BORROWABLE_TABLES]]]
    const_records += [["table", n, list(getattr(translit, n).items())] for n in TABLE_NAMES]

    translit_inputs = [(w, lect, donor, strict) for w, lect in grid(ws, lects)
                       for donor, strict in ((None, False), (None, True))]
    translit_inputs += [(w, lect, donor, False) for i, (w, lect) in enumerate(grid(ws[::5], lects))
                        for donor in [DONORS[1 + i % (len(DONORS) - 1)]]]

    def transliterate(args):
        w, lect, donor, strict = args
        return ["transliterate", w, lect, donor, strict, common.call(translit.transliterate, w, lect, donor, strict)]

    return [
        common.corpus("constants", const_records, lambda r: r),
        common.corpus("is_latin", strings, lambda s: ["is_latin", s, translit.is_latin(s)]),
        common.corpus("guest_script", strings + script_probes, lambda s: ["guest_script", s, translit.guest_script(s)]),
        common.corpus("segment_ipa", common.unique(ipas + table_strings + inventory + common.UNICODE_PROBES +
                                                   ["ŋk", "tʃʰ", "ãː", "aːː", "ːa", "ʰ", "eəʊ", "dʒʒ"]),
                      lambda s: ["segment_ipa", s, translit.segment_ipa(s)]),
        common.corpus("nativization_table", common.unique(lects + EXTRA_TAGS),
                      lambda lang: ["nativization_table", lang, common.call(lambda: name_of(
                          translit.nativization_table(lang)))]),
        common.corpus("targets", lects, lambda lang: ["_targets", lang, list(translit._targets(lang))]),
        common.corpus("established", [(w, d) for w in LOAN_SPELLINGS + ["", "  ", "models"]
                                      for d in ["en-GB", "en", "en-US", "fr-FR", "ru", ""]],
                      lambda a: ["_established", a[0], a[1], translit._established(a[0], a[1])]),
        common.corpus("project", grid(segments, lects),
                      lambda a: ["_project", a[0], a[1], translit._project(a[0], a[1])]),
        common.corpus("nativize", grid(ipas, lects),
                      lambda a: ["nativize", a[0], a[1], translit.nativize(a[0], a[1])]),
        common.corpus("transliterate", translit_inputs, transliterate),
    ]

