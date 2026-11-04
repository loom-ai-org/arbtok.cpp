"""Goldens of arbtok.tashkeel: TashkeelError.

TashkeelDiacritizer runs the bundled ensemble, which phase 2 ports, so its
goldens are not here. A record is ["TashkeelError", message, [str(error),
the class name, the names of its base classes]].
"""
from arbtok.tashkeel import TashkeelError

from . import common


def corpora():
    messages = common.unique(["", "boom", "model failed: شكل", "x" * 300] + common.UNICODE_PROBES)

    def record(message):
        e = TashkeelError(message)
        return ["TashkeelError", message, [str(e), type(e).__name__, [b.__name__ for b in type(e).__mro__[1:]]]]
    return [common.corpus("messages", messages, record)]
