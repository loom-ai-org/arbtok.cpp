"""Goldens of the front end of a phoonnx arbtok voice.

tests/data/phoonnx_calls.jsonl.gz holds what tools/goldens/record_phoonnx.py
recorded from phoonnx and scriptconv: for each input text of a voice, the
ArbtokG2PPlugin constructor keywords and the text of each transcribe call, with
the phoneme lists and token ids that phoonnx made of the results. This dumper
replays the calls on the reference arbtok, with one ArbtokG2PPlugin per voice
and register, as scriptconv keeps one, in record order.

Each record is ["voice", lang, model, text, [[keywords, call text], ...],
outputs, phonemes, ids]. outputs are the transcribe results. phonemes is what
phoonnx's BasePhonemizer.phonemize_lazy makes of them: each result with every
"(...)" run removed, split into code points, and the empty ones left out. ids
is what the tokenizer of the recorded voice makes of each phoneme list: a
phoneme outside the voice's phoneme_id_map is dropped, a blank goes before
each phoneme and after the last one, and BOS and EOS go around the list.

Where a replayed result equals the recorded one, the phonemes and ids of the
record must equal what phoonnx gave, so a change of the phoonnx rules fails the
dump. Where it does not (a CPU of another class runs other kernels), the dump
writes its own result and counts the record.
"""
import gzip
import json
import pathlib
import re
import sys

from arbtok.plugin import ArbtokG2PPlugin

from . import common

CALLS = pathlib.Path(__file__).resolve().parent.parent.parent / "tests" / "data" / "phoonnx_calls.jsonl.gz"
_LANG_FLAG = re.compile(r"\([^)]+\)")


def recorded():
    with gzip.open(CALLS, "rt", encoding="utf-8") as fh:
        lines = [json.loads(line) for line in fh]
    header, records = lines[0], lines[1:]
    if len(records) != header["records"]:
        raise SystemExit(f"{CALLS}: the header gives {header['records']} records, the file holds {len(records)}")
    return header, records


def phonemes_of(outputs):
    lists = [list(_LANG_FLAG.sub("", out)) for out in outputs]
    return [p for p in lists if p]


def ids_of(phonemes, id_map):
    out = []
    for sentence in phonemes:
        known = [id_map[p] for p in sentence if p in id_map]
        body = [id_map["_"]]
        for i in known:
            body += [i, id_map["_"]]
        out.append([id_map["^"]] + body + [id_map["$"]])
    return out


def corpora():
    header, records = recorded()
    id_map = header["phoneme_id_map"]
    engines = {}
    differ = [0]

    def replay(record):
        key = (record["lang"], record["model"])
        outputs = []
        for keywords, text, _ in record["calls"]:
            engine_key = key + (common.dumps(keywords),)
            if engine_key not in engines:
                engines[engine_key] = ArbtokG2PPlugin(**keywords)
            outputs.append(common.call(engines[engine_key].transcribe, text))
        if any(not isinstance(o, str) for o in outputs):
            raise SystemExit(f"the replay raised on {record['text']!r}: {outputs}")
        phonemes = phonemes_of(outputs)
        ids = ids_of(phonemes, id_map)
        if outputs == [out for _, _, out in record["calls"]]:
            if phonemes != record["phonemes"] or ids != record["ids"]:
                raise SystemExit(f"the phoonnx rules of this dumper do not give what phoonnx gave on "
                                 f"{record['text']!r}")
        else:
            differ[0] += 1
        return ["voice", record["lang"], record["model"], record["text"],
                [[keywords, text] for keywords, text, _ in record["calls"]], outputs, phonemes, ids]

    out = [common.corpus("voice", records, replay)]
    print(f"  phoonnx: {differ[0]} of {len(records)} replayed records differ from the recording", file=sys.stderr)
    return out
