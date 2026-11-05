"""Goldens of arbtok.textnorm.

The first two corpora hold the part that phase 1 ports, is_arabic_lang, which
dialects calls. The other corpora hold the rest of the module: the constants and
the named bundles, the bundled tables, describe(), the checks the config classes
make, normalize_asr and normalize_for_tts.

A config is written as a JSON object that holds only the fields that differ from
the class default, in the order of the dataclass fields. A bool is true or false,
a tuple of strings is a list, the number_forms of a TtsNorm is a list of [value, word] pairs and a
lexicon is a list of [spelling, term] pairs in the sorted order the config holds.
A lexicon that is exactly one of the bundled ones is written as a reference:
"@canonical" for canonical_term_lexicon(), "@asr:<name>" for
bundled_asr_lexicon(name), and "@tts:<name>:1" or "@tts:<name>:0" for
bundled_tts_lexicon(name, pointed).

The texts are:
- every string literal of the sdist tests;
- the gold sentences, diacritized and raw;
- the generated texts of generated_texts(), one family per rule;
- the calendar texts of calendar_texts(): leap days of century years and holiday
  names;
- the calls the sdist test suite makes, recorded by record_calls.py in
  tests/data/textnorm_calls.jsonl.gz, replayed as they were made. A recorded call
  that speaks or reads numbers in a language that is not Arabic is left out.

All texts go through the named bundles of normalize_asr and through the default
config of normalize_for_tts with lang "ar". The other configs read a sample: the
generated texts, the calendar texts and SAMPLE_SIZE other texts drawn with
random.Random(SEED). The
number parser makes those configs slow, so the sample keeps the dump short. The
lects of LECTS read the sample under the default config and the dialect_numbers
configs. "en" and "fr" read a smaller sample under the configs that neither speak
nor read numbers, because the C++ port has the Arabic number words only; "en"
also reads the Arabic-only rules, which raise ValueError.

The codes of LANGUAGES read the generated and calendar texts under the configs that speak or
read numbers, with and without leave_unspeakable_numbers. They are codes that
arbtok counts as Arabic and the reference libraries do not, codes those libraries
reject, and the Arabic lect codes. A record is kept unless the reference reached
code that the port does not hold (tools/goldens/routes.py), and the corpus states
how many records it leaves out for that reason.

Each corpus states its expected count from its factor lists before its records
are made, and the dumper compares that count with the records it writes.
"""
import dataclasses
import functools
import gzip
import json
import multiprocessing
import pathlib
import random
import sys

from arbtok import textnorm as tn
from arbtok.textnorm import ARABIC_LANGUAGE_CODES, is_arabic_lang

from . import common
from . import routes
from .dialects import tag_grid

SEED = 3
SAMPLE_SIZE = 700
SMALL_SAMPLE = 300
LECTS = ("ar-SA", "ar-EG", "arz", "apc", "ary", "acw", "mey-MR")
FOREIGN = ("en", "fr")
LANGUAGES = ("ur", "mey-MR", "ar_SA", "xx", "ara", "AR", "arz", "apc", "afb", "ayl", "acw", "ars", "arb", "ary")
TUPLE_FIELDS = ("phone_shapes", "phone_regions", "phone_prefixes", "identifier_words", "month_names")
WORKERS = 4
CALLS = pathlib.Path(__file__).resolve().parent.parent.parent / "tests" / "data" / "textnorm_calls.jsonl.gz"


# ── Config encoding ─────────────────────────────────────────────────────────

@functools.lru_cache(maxsize=None)
def term_names():
    return tuple(sorted(p.stem for p in (common.package_dir() / "data" / "term_lexicons").glob("*.tsv")))


@functools.lru_cache(maxsize=None)
def lexicon_references():
    """Reference string -> the lexicon tuple a config holds for it."""
    out = {"@canonical": tn._as_lexicon(tn.canonical_term_lexicon())}
    for name in term_names():
        if not name.endswith("-cldr"):
            out[f"@asr:{name}"] = tn._as_lexicon(tn.bundled_asr_lexicon(name))
        for pointed in (1, 0):
            out[f"@tts:{name}:{pointed}"] = tn._as_lexicon(tn.bundled_tts_lexicon(name, pointed=bool(pointed)))
    return out


def encode_lexicon(entries):
    for ref, lexicon in lexicon_references().items():
        if entries == lexicon:
            return ref
    return [list(e) for e in entries]


def encode_config(config):
    out = {}
    for f in dataclasses.fields(config):
        value = getattr(config, f.name)
        if value == f.default:
            continue
        if f.name == "lexicon":
            out[f.name] = encode_lexicon(value)
        elif f.name == "number_forms" and isinstance(config, tn.TtsNorm):
            out[f.name] = [[v, w] for v, w in value]
        elif f.name in TUPLE_FIELDS:
            out[f.name] = list(value)
        else:
            out[f.name] = value
    return out


def decode_config(cls, encoded):
    fields = {}
    for name, value in encoded.items():
        if name == "lexicon":
            fields[name] = lexicon_references()[value] if isinstance(value, str) else [tuple(e) for e in value]
        elif name == "number_forms" and cls is tn.TtsNorm:
            fields[name] = [tuple(e) for e in value]
        elif name in TUPLE_FIELDS:
            fields[name] = tuple(value)
        else:
            fields[name] = value
    config = cls(**fields)
    if encode_config(config) != encoded:
        raise SystemExit(f"config encoding does not round-trip: {encoded}")
    return config


@functools.lru_cache(maxsize=None)
def decoded(kind, key):
    cls = tn.AsrNorm if kind == "asr" else tn.TtsNorm
    return decode_config(cls, json.loads(key))


def key(encoded):
    return common.dumps(encoded)


# ── Configs ─────────────────────────────────────────────────────────────────

BUNDLES = ("TRUTH_CHECK", "CER_STRIP", "CER_NORM", "CER_MARKS_FIRST", "CER_NORM_MARKS_FIRST",
           "INTELLIGIBILITY_GATE", "CER_CANONICAL")
GULF_MONTHS = ("يناير", "فبراير", "مارتش", "أبريل", "مي", "جون", "جولاي", "أوقست", "سبتمبر", "أكتوبر", "نوفمبر",
               "ديسمبر")
NUMBER_FORMS = {100: "مية", 2: "ثنين", 15: "خمستاش"}


def bool_fields(cls):
    return [f.name for f in dataclasses.fields(cls) if f.type in (bool, "bool")]


def asr_bundles():
    return [encode_config(getattr(tn, name)) for name in BUNDLES]


def asr_configs():
    """The AsrNorm configs that read the sample: each flag alone, each flag with
    collapse_whitespace, every flag, and CER_NORM with the cars-sa lexicon."""
    flags = bool_fields(tn.AsrNorm)
    out = [encode_config(tn.AsrNorm(**{f: True})) for f in flags]
    out += [encode_config(tn.AsrNorm(**{f: True, "collapse_whitespace": True})) for f in flags
            if f != "collapse_whitespace"]
    out.append(encode_config(tn.AsrNorm(**{f: True for f in flags})))
    out.append(encode_config(tn.CER_NORM.with_lexicon(tn.bundled_asr_lexicon("cars-sa"))))
    return unique_dicts(out)


def unique_dicts(items):
    seen, out = set(), []
    for item in items:
        k = key(item)
        if k not in seen:
            seen.add(k)
            out.append(item)
    return out


AGENT = dict(speak_percent=True, keep_code_digits=True, phone_regions=tn.ARAB_PHONE_REGIONS, long_digit_runs=True,
             identifier_words=tn.IDENTIFIER_WORDS, cardinal_numbers=True, space_fused_hundreds=True,
             spell_out_plates=True, spell_out_codes=True, spoken_forms=False, canonical_unicode=False)
TTS_TUPLE_VALUES = dict(phone_shapes=(r"05\d{8}", r"9665\d{8}"), phone_regions=tn.ARAB_PHONE_REGIONS,
                        phone_prefixes=(r"05\d{8}", r"\+?9665\d{8}", r"800\d{7}"),
                        identifier_words=tn.IDENTIFIER_WORDS, month_names=GULF_MONTHS)


def tts_single(defaults_on):
    """Each field set apart from its base: the class defaults, or with defaults_on
    False the defaults with spoken_forms, canonical_unicode and drop_false_starts off.
    A bool is flipped; a tuple field and number_forms take a value of their own."""
    base = {} if defaults_on else dict(spoken_forms=False, canonical_unicode=False, drop_false_starts=False)
    default = tn.TtsNorm(**base)
    out = []
    for f in dataclasses.fields(tn.TtsNorm):
        if f.name == "lexicon":
            continue
        fields = dict(base)
        if f.name in TTS_TUPLE_VALUES:
            fields[f.name] = TTS_TUPLE_VALUES[f.name]
        elif f.name == "number_forms":
            fields[f.name] = NUMBER_FORMS
        else:
            fields[f.name] = not getattr(default, f.name)
        out.append(encode_config(tn.TtsNorm(**fields)))
    return out


def tts_agent_configs():
    agent = tn.TtsNorm(**AGENT)
    out = [encode_config(agent)]
    for change in (dict(colloquial_clock=True), dict(colloquial_years=True), dict(colloquial_units=True),
                   dict(oblique_numbers=True), dict(dialect_numbers=True), dict(month_names=GULF_MONTHS),
                   dict(plain_orthography=True), dict(leave_unspeakable_numbers=True),
                   dict(colloquial_clock=True, colloquial_years=True, colloquial_units=True, oblique_numbers=True,
                        dialect_numbers=True, month_names=GULF_MONTHS, plain_orthography=True)):
        out.append(encode_config(dataclasses.replace(agent, **change)))
    out.append(encode_config(agent.with_number_forms(NUMBER_FORMS)))
    out.append(encode_config(agent.with_lexicon(tn.bundled_tts_lexicon("cars-sa", pointed=True))))
    out.append(encode_config(agent.with_lexicon(tn.bundled_tts_lexicon("common-en", pointed=True))))
    out.append(encode_config(tn.TtsNorm(spell_out_codes=True).with_lexicon(tn.bundled_tts_lexicon("cars-sa"))))
    return out


def tts_configs():
    return unique_dicts(tts_single(False) + tts_single(True) + tts_agent_configs())


def tts_dialect_configs():
    agent = tn.TtsNorm(**AGENT)
    return [encode_config(tn.TtsNorm()), encode_config(tn.TtsNorm(dialect_numbers=True)),
            encode_config(dataclasses.replace(agent, dialect_numbers=True)),
            encode_config(dataclasses.replace(agent, dialect_numbers=True, colloquial_clock=True,
                                              number_forms=tuple(sorted(NUMBER_FORMS.items()))))]


#: The TtsNorm fields whose rules speak a number: spell_out_codes speaks the decimal of 1.5T.
NUMBER_TTS_FIELDS = ("spoken_forms", "cardinal_numbers", "dialect_numbers", "number_forms", "spell_out_codes")


def speaks_numbers(kind, encoded):
    if kind == "asr":
        return bool(encoded.get("spoken_numbers_to_digits"))
    config = decoded("tts", key(encoded))
    return any(getattr(config, f) for f in NUMBER_TTS_FIELDS)


def arabic_only(encoded):
    config = decoded("tts", key(encoded))
    return any(getattr(config, f) for f in tn._ARABIC_ONLY)


# ── Generated texts ─────────────────────────────────────────────────────────

def generated_texts():
    """Texts for each rule family of textnorm and util, in a fixed order."""
    out = []
    # clocks
    for t in ("7:30", "07:30", "19:30", "0:00", "12:00", "23:59", "24:00", "9:5", "2:1", "3:10", "13:00 pm",
              "12 pm", "12 am", "10am", "10 AM", "4:15 م", "9:30 م", "4:30 العصر", "8:15 ص", "8 ص ب", "ص ب 1234",
              "الساعة 9.30", "الساعة 12.50 ريال", "سعر الساعة 12.50 ريال", "الساعة 10 ص", "الساعة 3 العصر",
              "الساعة 12 pm", "الساعة 14", "الساعة 8 ودقيقة 15", "الساعة 8 و15 دقيقة", "الساعة 3 انه دقيقة 45",
              "١٠:٣٠", "الساعه ٧:٠٠ م", "15h01", "19h30", "9H05", "الموعد 7:30pm غدا", "14:00 و 15:30",
              "الساعة الثامنة والربع", "الساعة الثامنة والنصف", "الساعة الثامنة إلا ربعاً", "الساعة الواحدة إلا ثلثاً",
              "الساعة الثانية عشرة إلا ربعاً مساءً", "الساعة السادسة مساء ودقيقة خمسة واربعين", "الساعة ثمانية",
              "الساعة ثمانية الاف", "الساعة السابعة مساءن", "الساعة السابعة صباحنا", "الساعة الحادية عشرة",
              "الساعة عشرة عشرة", "الساعة الثامنة تماماً", "الساعة الثامنة يوم الخميس", "الساعة الأولى من العلاج",
              "الساعة ثلاثة العصر", "الساعة سبعة المغرب", "الساعة عشرة بالليل", "الساعة ستة الصبح",
              "الساعة اثنا عشر ليلا", "بالساعة خمسة ونص", "والساعة اربعة وثلث", "الساعة السابعة مساء انه دقيقة خمسة عشر",
              "الساعة تسعة وخمسين", "الساعة ثمانية وخمسين", "الساعة الثالثة ودقيقتان", "الساعة الثالثة والدقيقة"):
        out.append(t)
    # dates
    for t in ("10/6/2026", "31/12/30", "1/2", "30/6", "يوم 3/4", "بتاريخ 5/6", "18/9/2026", "15/9/1447هـ",
              "15/9/1447 هـ", "15/9/1447", "2.1.1400", "10-06-2026", "10.06.2026", "2026-06-10", "2026/6/10",
              "2026-02-30", "29/2/2023", "29/2/2024", "1/1/1899", "1/1/2100", "10/6/2026م", "10/6/2026 م",
              "٢٠٢٦/٠٦/١٠", "١٠/٦/٢٠٢٦", "18 أيلول 2026", "18 سبتمبر 2026", "18 من سبتمبر", "الثامن عشر من سبتمبر",
              "الأول من يناير", "الحادي والعشرين من مارس", "جون 2026", "شهر جون", "نهاية جون 2026", "15 مي", "15 مي 2025",
              "كانون الثاني 2026", "تشرين الاول", "5 اب", "من مارس إلى مايو", "مارس المهنة", "مارس 2026", "ومارس",
              "15 يونيو 2026", "15 يونيه", "يوليه", "أغسطس اغسطس", "اكتوبر 2025", "موديل عشرين ستة وعشرين",
              "سنة عشرين عشرين", "عام عشرين خمسة وعشرين", "في عشرين تسعة وتسعين", "موديل 2026/2027",
              "موديل 2019/2020", "موديل 2026 و 2025", "و 2050 كيلو", "موديل 2026", "عام 2025", "مدير عام 2025",
              "في عام 2030", "بالعام 2021", "موديلات 2024", "سنة 2026 ريال"):
        out.append(t)
    # percent
    for t in ("4.5%", "%4.5", "10 %", "٪10", "١٠٪", "a%20b", "?q=%20", "50 في المية", "خمسة وعشرين في المية",
              "مية بالمية", "خمس مية بالمية", "بالمية", "شربت في المية", "25 بالمئة", "100%"):
        out.append(t)
    # units
    for t in ("100km", "100 km", "100KM", "120km/h", "100كم", "100 كم", "5 كم من البيت", "كامري 2024 كم؟",
              "عندي 3 كم سؤال", "MG5 كم صافي", "5 kg", "5kg", "3 mg", "3 MG", "2 EV", "150 hp", "1,200 ر.س",
              "1200ر.س", "500 SAR", "SR 50", "SAR 50", "50 KM", "وزنها 5 كيلو", "المسافة 5 كيلو", "5 كيلو في الساعة",
              "5 كيلومتر", "5 كيلومترات", "100 cm", "25ºC", "25°C", "5 GB", "3 L", "10 m", "1.5 l", "45,000ريال",
              "24h", "90s", "100kms", "520i", "M340i", "7abibi", "3ala", "10،", "١٠٠ كم"):
        out.append(t)
    # codes
    for t in ("X5", "GV70", "BMW", "GMC", "MG", "TOYOTA", "LAND ROVER DEFENDER", "iX3", "iX 3", "i4", "i7", "520i",
              "330e", "320d", "740Li", "M340i", "MG 4", "MG 4,500", "MG 4 EV", "BMW 5 SERIES", "BMW 5 سيريز",
              "F-150", "CX-5", "GV-70", "SAR-50", "COVID-19", "1.5T", "1.5T's", "BMW's", "X5’s", "L809UPZ3V361",
              "JTDKB20U793456789", "jtdkb20u793456789", "JTDKB20U7-93456789", "JTD KB20U 793456789",
              "رقم الهيكل ينتهي بـ ABC1234", "رقم الهيكل L457L680 100 ألف", "الشاصي: L457L680", "VIN 1HGCM826",
              "بالـBMW", "للـNissan", "بـABC", "وBMW", "والX5", "A 4", "A4", "15 S", "Class 5 C", "Q50", "LX600",
              "اتش1", "أتش1", "H1", "اكس فايف", "جي في سيفن زيرو", "اكستري", "كيو فيفتي", "بي فور", "او تو",
              "اكس 5", "التو", "بي ام دبليو", "بي ام دبليو اكس فايف", "تويوتا كامري", "لاند كروزر"):
        out.append(t)
    # plates
    for t in ("أ ب ج ١٢٣٤", "أ ب ج 1234", "ا ب ج1234", "أ ب ج - 1234", "(أ ب ج ١٢٣٤)", "هـ ر س 567", "ح ص ب 1234",
              "أ ب ج ١٢٣٤ و د ر س ٥٦٧٨", "1234 أ ب ج", "١٢٣ ر س ع", "د أ ب ج 1234", "س ع د", "الباقة أ 1450 ريال",
              "و 1500 ريال", "اللوحة XTR 6477", "لوحة سيارتك 6477 XTR", "لوحتها: abc 12", "اللوحة رقم ABD 1",
              "الف باء جيم 1234", "سين عين دال 55", "ألف باء جيم خمسة"):
        out.append(t)
    # phone numbers, one or more per region
    phones = ["+966 55 123 4567", "00966551234567", "0551234567", "055 123 4567", "966551234567", "920012345",
              "8001234567", "800 123 4567", "+971 50 123 4567", "0501234567", "+965 5000 1234", "+974 3312 3456",
              "+973 3600 1234", "+968 9212 3456", "+967 712 345 678", "+964 790 123 4567", "+962 7 9123 4567",
              "+961 3 123 456", "+963 944 567 890", "+970 599 123 456", "+20 100 123 4567", "01001234567",
              "+249 91 123 1234", "+218 91 2345678", "+216 20 123 456", "+213 551 23 45 67", "+212 650-123456",
              "+222 22 12 34 56", "+252 61 123 4567", "+253 77 83 10 01", "+269 321 23 45", "0100 123 4567 3",
              "رقم الجوال 0551234567", "جوالي: 055 123 4567", "رقمي 5551234", "الكود 4729", "الكود: 4729",
              "برقم الحجز 3401", "رقم الحجز 4 8 2 1 3", "0 5 5 3 1 7 9 2 4 5", "1234567890", "12345678901",
              "1000000000", "+12345", "+1234", "64008 45", "10/10/2026 123", "12 05 2026", "٠٥٥١٢٣٤٥٦٧",
              "٠٥٥ ١٢٣ ٤٥٦٧", "055-123-4567", "1.234.567", "رقم الطلب 123", "رمز التحقق 4521", "الموحد 920001234",
              "phone 0551234567", "otp 1234", "PIN: 9876"]
    out += phones
    # numbers and number words
    for t in ("الطابق 3", "الفئة 5", "بالطابق 12", "للطابق 1", "الطابق 0", "الطابق 100", "الطابق ٣", "طابق 3",
              "الطابق الثالث", "الفئة الخامسة", "5 سنوات", "8 سيارات", "3 ساعات", "10 أيام", "11 سنة", "3 سنوات",
              "200000", "200,000", "مئتين ألف", "1,500", "1,234,567", "12,5", "1,0000", "1,2,3", "055,123,4567",
              "123.456,78", "0.5", "3.14", "1e5", "٣٫٥", "١٬٢٣٤", "٢٠٢٦", "12345€", "5ft", "10kg", "3/3", "1/2",
              "3/4", "0/0", "-5", "+5", "5+", "²", "①", "፩", "١٢٣", "۱۲۳", "ثلاثمئة", "ثلاثمائة", "خمسمية",
              "اربعماية الف", "ست مية او عشرة الف", "مليون وست مية او عشرة الف", "الف او خمسمية", "ثلاثة او اربعة",
              "كباية ماية", "شربت ماية", "اشتريت خمسة ماية", "الف وميتين", "اثنين وميه", "بخمسميه الف",
              "الف وسبعماية", "ماية الف", "ميتين وماية", "تسعمية وماية", "الف وماية", "الساعة وحدة", "مرة وحدة", "وحدة", "اطناش", "اتناش", "واتناش", "لاطناش",
              "ثلثمية", "تالاف", "التامنة", "التالتة", "احد عشر", "اثنا عشر", "واثنتا عشرة", "خمسة وأربعون ألف",
              "الكفرات الاربعة الف وستمية", "ونص", "ربع ساعة", "نص ساعة", "خمسة ونص", "عشرة فاصلة خمسة",
              "1500 فاصلة 5 0", "1500.50", "1500.00", "واحد اثنين ثلاثة", "مليار", "تريليون", "999999999999999999",
              "12345678901234567890", "0.000001", "1.0", "007", "00", "0"):
        out.append(t)
    # spoken variants, dialect spellings, clitics, markup, controls
    for t in ("في أمان الله", "فمانيلا", "فمان الله", "وفامان الله", "في مانيلا", "فى امان الله", "هذي", "معاك", "كده",
              "ايوا", "لسا", "بكرا", "حابب", "وهذي", "فمعاك", "و الله", "قال له", "يعملوا لك", "عبد الله", "وعبد الرحمن",
              "ان شا الله", "الحمد لله", "يا رب", "ما في", "[laughter] نعم", "<foreign> hello </foreign>",
              "[french: bonjour] مرحبا", "(noise) اه", "[00:12] كلام", "<english>ok</english>", "[sound effect]",
              "[ضحك] ها", "<lang en> hi", "[?]", "[foreign word]", "مر​حبا", "‏مرحبا‎", "﻿نعم",
              "؜نعم", "مـــرحبا", "الـ السيارة", "الـ الـسيارة", "بـ بنت", "سيـ سيارة", "كتـ كتاب",
              "وMINI", "والBMW", "وام جي", "Test-Drive", "M-Sport", "ڤولفو", "پيجو", "گاز", "چاي", "ڨاز",
              "الصلوٰة", "علىٰ", "الكتٰب", "هٰذا", "لٰكن", "أٔ", "ؤ", "ئ", "آ", "إسلام",
              "آمن", "ٱلحمد", "مدرسة", "مستشفى", "سؤال", "شيء", "ماء", "بناء", "ABC abc", "Hello, World!",
              "I'm Dr. Prof. ok", "he's gonna", "Dr. Smith", "Sr. García", "Mme Curie"):
        out.append(t)
    return common.unique(out)


def calendar_texts():
    """Leap days of century years, and holiday names alone and beside a clock."""
    out = ["29/2/1900", "29/2/2000", "29/2/2100", "1900-02-29", "2000-02-29", "2100-02-29", "28/2/1900", "1/3/2100"]
    for word in ("المولد", "حانوكا", "عاشوراء", "نوروز", "رمضان", "ديوالي"):
        out += [word, f"الساعة 8 {word}", f"الساعة ثمانية {word}", f"الساعة الثامنة {word}",
                f"الساعة الثانية عشرة {word}", f"الساعة الخامسة {word} صباحا", f"الساعة الثامنة والربع {word}",
                f"{word} الساعة 8"]
    return out


@functools.lru_cache(maxsize=None)
def sample_pool():
    """The texts that the seeded samples draw from: the sdist test strings, the gold
    sentences and the generated texts. The calendar texts are not in the pool, so
    they do not change what a sample draws."""
    texts = common.test_strings()
    texts += [s for _, s in common.gold_sentences()]
    texts += [s for _, s in common.gold_sentences("raw")]
    texts += generated_texts()
    return tuple(common.unique(t for t in texts if isinstance(t, str)))


@functools.lru_cache(maxsize=None)
def all_texts():
    return tuple(common.unique(sample_pool() + tuple(calendar_texts())))


@functools.lru_cache(maxsize=None)
def sample_texts(size=SAMPLE_SIZE):
    generated = generated_texts()
    rest = [t for t in sample_pool() if t not in set(generated)]
    rng = random.Random(SEED * 1000 + size)
    head = generated + calendar_texts()
    return tuple(common.unique(head + sorted(rng.sample(rest, min(size, len(rest))))))


@functools.lru_cache(maxsize=None)
def small_sample():
    rest = list(sample_pool())
    rng = random.Random(SEED * 7)
    return tuple(sorted(rng.sample(rest, min(SMALL_SAMPLE, len(rest)))))


def recorded_calls():
    if not CALLS.is_file():
        raise SystemExit(f"{CALLS} is missing: run tools/goldens/record_calls.py first")
    with gzip.open(CALLS, "rt", encoding="utf-8") as fh:
        return [json.loads(line) for line in fh if line.strip()]


# ── Records ─────────────────────────────────────────────────────────────────

def record(item):
    kind, text, lang, k = item
    if kind == "asr":
        return ["normalize_asr", text, json.loads(k), lang, common.call(tn.normalize_asr, text, decoded("asr", k),
                                                                         lang=lang)]
    return ["normalize_for_tts", text, lang, json.loads(k), common.call(tn.normalize_for_tts, text, lang,
                                                                         decoded("tts", k))]


def run(items, fn=record):
    with multiprocessing.get_context("fork").Pool(WORKERS) as pool:
        return pool.map(fn, items, chunksize=64)


def routed(item):
    """record(item) when the reference reached only code the port holds, else None."""
    route, out = routes.route_of(lambda: record(item))
    return None if route == routes.MODULE else out


def kept(name, items, fn):
    """The corpus of the records of items that fn keeps, and the count it leaves out."""
    routes.install()
    out = run(items, fn)
    records = [r for r in out if r is not None]
    left_out = len(out) - len(records)
    print(f"  {name}: {len(items)} in, {left_out} left out as reaching code the port does not hold", file=sys.stderr)
    return counted(name, len(items) - left_out, records)


def language_items():
    tts = common.unique(key(encode_config(c)) for c in (
        tn.TtsNorm(), tn.TtsNorm(cardinal_numbers=True), tn.TtsNorm(cardinal_numbers=True, leave_unspeakable_numbers=True),
        tn.TtsNorm(dialect_numbers=True, leave_unspeakable_numbers=True),
        dataclasses.replace(tn.TtsNorm(**AGENT), leave_unspeakable_numbers=True)))
    asr = common.unique(key(encode_config(c)) for c in (tn.AsrNorm(spoken_numbers_to_digits=True), tn.CER_CANONICAL))
    texts = generated_texts() + calendar_texts()
    return ([("tts", t, lang, c) for lang in LANGUAGES for c in tts for t in texts]
            + [("asr", t, lang, c) for lang in LANGUAGES for c in asr for t in texts])


def pairs(d):
    return [[k, v] for k, v in d.items()]


def table(fn, *args):
    out = common.call(fn, *args)
    return out if isinstance(out, dict) and "err" in out and len(out) == 1 else pairs(out)


def counted(name, expected, records):
    """A corpus whose expected count comes from its factor lists, not from its records."""
    return name, expected, records


def asr_items():
    bundles = [key(c) for c in asr_bundles()]
    others = [key(c) for c in asr_configs() if key(c) not in set(bundles)]
    texts, sample, small = all_texts(), sample_texts(), small_sample()
    items = [("asr", t, "ar", c) for c in bundles for t in texts]
    items += [("asr", t, "ar", c) for c in others for t in sample]
    plain = [c for c in others if not speaks_numbers("asr", json.loads(c))]
    items += [("asr", t, lang, c) for lang in FOREIGN for c in plain for t in small]
    expected = len(bundles) * len(texts) + len(others) * len(sample) + len(FOREIGN) * len(plain) * len(small)
    calls = [c for c in recorded_calls() if c[0] == "normalize_asr"]
    replay = [("asr", c[1], c[3], key(c[2])) for c in calls if replayable("asr", c[3], c[2])]
    return items + replay, expected + len(calls) - dropped("asr", calls)


def tts_items():
    default = key(encode_config(tn.TtsNorm()))
    configs = [key(c) for c in tts_configs() if key(c) != default]
    dialect = [key(c) for c in tts_dialect_configs()]
    texts, sample, small = all_texts(), sample_texts(), small_sample()
    items = [("tts", t, "ar", default) for t in texts]
    items += [("tts", t, "ar", c) for c in configs for t in sample]
    items += [("tts", t, lang, c) for lang in LECTS for c in dialect for t in sample]
    plain = [c for c in configs if not speaks_numbers("tts", json.loads(c)) and not arabic_only(json.loads(c))]
    foreign_err = [c for c in configs if arabic_only(json.loads(c))]
    items += [("tts", t, lang, c) for lang in FOREIGN for c in plain for t in small]
    items += [("tts", t, "en", c) for c in foreign_err for t in small[:20]]
    expected = (len(texts) + len(configs) * len(sample) + len(LECTS) * len(dialect) * len(sample)
                + len(FOREIGN) * len(plain) * len(small) + len(foreign_err) * min(20, len(small)))
    calls = [c for c in recorded_calls() if c[0] == "normalize_for_tts"]
    replay = [("tts", c[1], c[2], key(c[3])) for c in calls if replayable("tts", c[2], c[3])]
    return items + replay, expected + len(calls) - dropped("tts", calls)


def replayable(kind, lang, encoded):
    """A recorded call is replayed unless it speaks or reads numbers in a language that is not Arabic,
    which the C++ port does not have."""
    return is_arabic_lang(lang) or not speaks_numbers(kind, encoded)


def dropped(kind, calls):
    """The recorded calls of kind that replayable() leaves out, counted on their own."""
    n = 0
    for c in calls:
        lang, encoded = (c[3], c[2]) if kind == "asr" else (c[2], c[3])
        if not is_arabic_lang(lang) and speaks_numbers(kind, encoded):
            n += 1
    return n


def constants():
    values = [("ASR_NORM_VERSION", tn.ASR_NORM_VERSION), ("TTS_NORM_VERSION", tn.TTS_NORM_VERSION),
              ("IDENTIFIER_WORDS", list(tn.IDENTIFIER_WORDS)), ("ARAB_PHONE_REGIONS", list(tn.ARAB_PHONE_REGIONS))]
    values += [(name, encode_config(getattr(tn, name))) for name in BUNDLES]
    return [["const", name, value] for name, value in values]


def tables():
    names = list(term_names()) + ["x", "en", "ar-SA", "fr", "units", "cars-sa.tsv", ""]
    langs = ["ar", "ar-SA", "arz", "ar_SA", "en", "fr", "x", ""]
    recs = [["spelled_codes", lang, table(tn.spelled_codes, lang)] for lang in langs]
    recs += [["cldr_units", lang, table(tn.cldr_units, lang)] for lang in langs]
    recs += [["bundled_asr_lexicon", name, table(tn.bundled_asr_lexicon, name)] for name in names]
    recs += [["bundled_tts_lexicon", name, pointed, table(tn.bundled_tts_lexicon, name, pointed)]
             for name in names for pointed in (True, False)]
    groups = [["cars-sa", "common-en", "spoken-sa"], ["cars-sa"], ["spoken-sa", "cars-sa"], [],
              ["common-en", "x"], ["units-cldr"], ["currencies-cldr", "cars-sa"]]
    recs += [["canonical_term_lexicon", g, table(tn.canonical_term_lexicon, tuple(g))] for g in groups]
    return recs, len(langs) * 2 + len(names) * 3 + len(groups)


def describes(asr, tts):
    recs = [["AsrNorm.describe", json.loads(c), common.call(decoded("asr", c).describe)] for c in asr]
    recs += [["TtsNorm.describe", json.loads(c), common.call(decoded("tts", c).describe)] for c in tts]
    return recs


def validation():
    lexicons = [[], [["BMW", "بي ام"]], [["", "x"]], [["  ", "x"]], [["\t", "x"]], [["a", ""]], [["a"]],
                [["a", "b", "c"]], [["a", 1]], [["b", "2"], ["a", "1"]], [["a", "1"], ["a", "2"]], [[" a ", "b"]],
                [["Bmw", "x"], ["BMW", "y"]], [["X5", "إِكْسْ فَيْفْ"], ["x5", "اكس"]]]
    forms = [[], [[100, "مية"]], [[2, " ثنين "]], [[-1, "x"]], [[True, "x"]], [[False, "x"]], [[1, ""]],
             [[1, "  "]], [[1, 5]], [[2, "a"], [2, "b"]], [[0, "صفر"], [10, "عشرة"]], [["1", "x"]]]
    months = [list(GULF_MONTHS), list(GULF_MONTHS[:11]), list(GULF_MONTHS) + ["x"], list(GULF_MONTHS[:11]) + [""],
              list(GULF_MONTHS[:11]) + [" "], []]

    def config_or_err(fn):
        out = common.call(fn)
        return out if isinstance(out, dict) else encode_config(out)

    recs = [["AsrNorm.with_lexicon", e, config_or_err(lambda e=e: tn.AsrNorm().with_lexicon([tuple(x) for x in e]))]
            for e in lexicons]
    recs += [["TtsNorm.with_lexicon", e, config_or_err(lambda e=e: tn.TtsNorm().with_lexicon([tuple(x) for x in e]))]
             for e in lexicons]
    recs += [["TtsNorm.with_number_forms", f,
              config_or_err(lambda f=f: tn.TtsNorm().with_number_forms([tuple(x) for x in f]))] for f in forms]
    recs += [["TtsNorm", {"month_names": m}, config_or_err(lambda m=m: tn.TtsNorm(month_names=m))] for m in months]
    return recs, 2 * len(lexicons) + len(forms) + len(months)


def corpora():
    tags = common.unique(tag_grid() + common.UNICODE_PROBES + ["ARB", "Ar", "AR_sa", "ar_", "_ar", "-ar", "ar--",
                                                               "İar", "aR-x", "mey", "MEY_mr", "ara", "arc", "arn"])
    out = [
        common.corpus("const", [0], lambda _: ["const", "ARABIC_LANGUAGE_CODES", sorted(ARABIC_LANGUAGE_CODES)]),
        common.corpus("is_arabic_lang", tags, lambda t: ["is_arabic_lang", t, common.call(is_arabic_lang, t)]),
    ]
    const = constants()
    out.append(counted("constants", 4 + len(BUNDLES), const))
    recs, n = tables()
    out.append(counted("tables", n, recs))
    asr, n_asr = asr_items()
    tts, n_tts = tts_items()
    asr_keys = common.unique(c for _, _, _, c in asr)
    tts_keys = common.unique(c for _, _, _, c in tts)
    out.append(counted("describe", len(asr_keys) + len(tts_keys), describes(asr_keys, tts_keys)))
    recs, n = validation()
    out.append(counted("validation", n, recs))
    out.append(counted("normalize_asr", n_asr, run(asr)))
    out.append(counted("normalize_for_tts", n_tts, run(tts)))
    out.append(kept("languages", language_items(), routed))
    return out
