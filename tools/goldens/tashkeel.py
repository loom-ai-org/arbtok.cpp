"""Goldens of arbtok.tashkeel.

A TashkeelError record is ["TashkeelError", message, [str(error), the class
name, the names of its base classes]]. A TashkeelDiacritizer record is
["TashkeelDiacritizer.diacritize", text, pausal, result] over Arabic test
strings, corpus words and Unicode probes.
"""
from arbtok.tashkeel import TashkeelError

from . import common


def corpora():
    messages = common.unique(["", "boom", "model failed: شكل", "x" * 300] + common.UNICODE_PROBES)

    def record(message):
        e = TashkeelError(message)
        return ["TashkeelError", message, [str(e), type(e).__name__, [b.__name__ for b in type(e).__mro__[1:]]]]
    return [common.corpus("messages", messages, record)] + diacritizer_corpora()


def diacritizer_corpora():
    from arbtok.tashkeel import TashkeelDiacritizer
    reader = TashkeelDiacritizer()
    texts = common.unique(common.arabic_test_strings()[:1000] + common.corpus_words(2000) + common.UNICODE_PROBES)
    cases = [(t, pausal) for t in texts for pausal in (False, True)]
    return [common.corpus("TashkeelDiacritizer", cases,
                          lambda p: ["TashkeelDiacritizer.diacritize", p[0], p[1],
                                     common.call(reader.diacritize, p[0], pausal=p[1])])]
