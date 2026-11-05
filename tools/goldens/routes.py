"""Which implementations of the reference number and date libraries a call reached.

The port holds the Arabic implementations of ovos-number-parser and
ovos-date-parser, the code of the two libraries that serves every language, and
the paths where the reference rejects a language. After install(), route_of(make)
runs make() and names the code it reached:

- MODULE: code that the port does not hold ran. That is the module of a language
  other than Arabic, the number engines those modules share, dateparser, chronologia as the date parser of cs, hr
  and sk, the unicode-rbnf rules of a language other than Arabic, the Arabic
  rules when they format a number, the date_time.json
  formats of another language, or chronologia as the holiday layer for a text that
  data/numbers/holidays_ar.json does not answer. The port raises NotPortedError
  for such a call.
- ARABIC: no such code ran, and code of the Arabic modules did: numbers_ar.py,
  dates_ar.py or the Arabic date_time.json formats. That includes a call that
  only asked numbers_ar.resolve_ar_lang about the language and then rejected it.
- REJECTED: neither ran.
"""
import datetime
import functools
import json
import pathlib
import re
import sys

import ovos_date_parser
import ovos_number_parser  # noqa: F401  its modules load before the watch starts
import unicode_rbnf
from ovos_date_parser import holidays

ARABIC, REJECTED, MODULE = "arabic", "rejected", "module"

_HOLIDAY_ANSWERS = json.loads((pathlib.Path(__file__).resolve().parents[2] / "data" / "numbers" /
                               "holidays_ar.json").read_text(encoding="utf-8"))["answers"]
_HOLIDAY_ANCHOR = datetime.datetime(2000, 1, 1)
_LANGUAGE_FILE = re.compile(r"(?:numbers|dates|eras|scoped)_(?P<lang>[a-z]+)\.py")
_TOOL = "arbtok goldens routes"
_reached = set()


#: The engines of ovos_number_parser.util that the modules of other languages read numbers with.
_ENGINES = ("NumberVocabulary", "RomanceNumberExtractor")


def _code_kind(code):
    path = pathlib.Path(code.co_filename)
    if path.parent.name == "ovos_number_parser" and path.name == "util.py":
        return MODULE if code.co_qualname.split(".")[0] in _ENGINES else None
    if path.parent.name in ("ovos_number_parser", "ovos_date_parser"):
        m = _LANGUAGE_FILE.fullmatch(path.name)
        if m:
            return ARABIC if m["lang"] == "ar" else MODULE
        return None
    if "dateparser" in path.parts:
        return MODULE
    return None


def _on_start(code, offset):
    kind = _code_kind(code)
    if kind is None:
        return sys.monitoring.DISABLE
    _reached.add(kind)
    return None


def _marks(kind_of, real):
    """real, which adds kind_of(its arguments) to the reached set when it returns."""
    @functools.wraps(real)
    def call(*args, **kwargs):
        out = real(*args, **kwargs)
        _reached.add(kind_of(*args, **kwargs))
        return out
    return call


def _module(*args, **kwargs):
    return MODULE


def _formats_of(dt, lang, *args, **kwargs):
    return ARABIC if lang.split("-")[0] == "ar" else MODULE


def _holiday_gate(real):
    """The holiday layer of the port answers only the texts of its table, for "ar" at its anchor."""
    @functools.wraps(real)
    def call(text, lang, anchorDate=None, *args, **kwargs):
        if text and holidays._names_a_holiday(text, "ar") and not (
                lang == "ar" and anchorDate == _HOLIDAY_ANCHOR and text in _HOLIDAY_ANSWERS):
            _reached.add(MODULE)
        return real(text, lang, anchorDate, *args, **kwargs)
    return call


def _rbnf_rules(real_for_language, real_format):
    """The port holds no unicode-rbnf rules. For Arabic it holds the check that rejects a
    number Decimal cannot read, which is the only call arbtok makes into those rules."""
    def for_language(language):
        engine = real_for_language(language)
        engine.arbtok_language = language
        return engine

    @functools.wraps(real_format)
    def format_number(self, *args, **kwargs):
        if getattr(self, "arbtok_language", None) != "ar":
            _reached.add(MODULE)
        out = real_format(self, *args, **kwargs)
        _reached.add(MODULE)
        return out
    return staticmethod(for_language), format_number


def install(*consumers):
    """Watch the reference libraries, once per process. consumers are the modules that
    imported extract_datetime by name before the call."""
    tool = sys.monitoring.PROFILER_ID
    if sys.monitoring.get_tool(tool) == _TOOL:
        return
    sys.monitoring.use_tool_id(tool, _TOOL)
    sys.monitoring.register_callback(tool, sys.monitoring.events.PY_START, _on_start)
    sys.monitoring.set_events(tool, sys.monitoring.events.PY_START)
    real = ovos_date_parser.extract_datetime
    gated = _holiday_gate(real)
    for module in (ovos_date_parser, *consumers):
        if getattr(module, "extract_datetime", None) is real:
            module.extract_datetime = gated
    ovos_date_parser._extract_datetime_chronologia = _marks(_module, ovos_date_parser._extract_datetime_chronologia)
    fmt = ovos_date_parser.date_time_format
    fmt.date_format = _marks(_formats_of, fmt.date_format)
    engine = unicode_rbnf.RbnfEngine
    engine.for_language, engine.format_number = _rbnf_rules(engine.for_language, engine.format_number)


def route_of(make):
    """make() and the route of the calls it made into the reference libraries."""
    _reached.clear()
    out = make()
    if MODULE in _reached:
        return MODULE, out
    return (ARABIC if ARABIC in _reached else REJECTED), out
