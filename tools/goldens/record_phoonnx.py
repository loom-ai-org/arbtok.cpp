"""Record the calls that a phoonnx arbtok voice makes into arbtok.

No published phoonnx voice uses arbtok, so the voice is the published phoonnx
Arabic voice OpenVoiceOS/phoonnx_ar_miro_espeak_V2 (miro_ar.json, 162 phoneme
ids) with its phoneme_type set to "arbtok", its lang_code set to the lect of
the input, its phonemizer_model set to the register, and add_diacritics off,
because arbtok restores the marks itself. phoonnx then phonemizes through
scriptconv's ArbtokPhonemizer, which calls
arbtok.plugin.ArbtokG2PPlugin(lang=spec_for_lang(lang), diacritize=True[,
register=...]).transcribe(chunk) once per chunk of each sentence.

The script runs the voice's own synthesis front end (TTSVoice._iter_synthesis_ids
with no ONNX session) over the inputs, and writes one JSON line per input that
reached arbtok:

    {"lang": <voice lang_code>, "model": <phonemizer_model>, "text": <input>,
     "calls": [[<constructor keywords>, <transcribe input>, <transcribe output>], ...],
     "phonemes": [[<phoneme>, ...], ...], "ids": [[<id>, ...], ...]}

phonemes and ids are what phoonnx gives for each sentence. The first line is a
header with the versions, the voice, the input count and the number of inputs
left out because they made no call into arbtok (an empty or all-punctuation
text) or because phoonnx raised before arbtok.

The inputs, in this order: every string literal of the sdist tests under "ar";
every gold sentence under its own lect; every 4th Arabic test string under "ar"
with the full register (phonemizer_model "i'rab"); and the first 10 Arabic test
strings of length 8 to 80 under each supported lect.

Run it with an interpreter that has phoonnx and the reference packages:
    python -I tools/goldens/record_phoonnx.py --sdist-tests <sdist>/tests --gold-dir <repo>/data \
        --voice-config miro_ar.json --out tests/data/phoonnx_calls.jsonl.gz
"""
import argparse
import gzip
import importlib.metadata
import io
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

from goldens import common  # noqa: E402

VOICE = "OpenVoiceOS/phoonnx_ar_miro_espeak_V2"
FULL = "i'rab"


def inputs():
    out = [("ar", "", s) for s in common.test_strings()]
    out += [(lect, "", s) for lect, s in common.gold_sentences()]
    arabic = common.arabic_test_strings()
    out += [("ar", FULL, s) for s in arabic[::4]]
    short = [s for s in arabic if 8 <= len(s) <= 80][:10]
    out += [(lect, "", s) for lect in common.arabic_lects() for s in short]
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdist-tests", required=True)
    parser.add_argument("--gold-dir", required=True)
    parser.add_argument("--voice-config", required=True, help=f"the voice config of {VOICE}")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    common.SOURCES["sdist_tests"] = args.sdist_tests
    common.SOURCES["gold_dir"] = args.gold_dir

    import arbtok.plugin as ap
    from phoonnx.config import Alphabet, SynthesisConfig, VoiceConfig
    from phoonnx.voice import TTSVoice

    calls = []
    keywords = {}
    init, transcribe = ap.ArbtokG2PPlugin.__init__, ap.ArbtokG2PPlugin.transcribe

    def recording_init(self, *a, **kw):
        if a:
            raise SystemExit(f"ArbtokG2PPlugin was called with positional arguments {a}")
        keywords[id(self)] = kw
        init(self, **kw)

    def recording_transcribe(self, text, arabizi=None):
        if arabizi is not None:
            raise SystemExit("ArbtokG2PPlugin.transcribe was called with an arabizi hint")
        out = transcribe(self, text)
        calls.append([keywords[id(self)], text, out])
        return out

    ap.ArbtokG2PPlugin.__init__ = recording_init
    ap.ArbtokG2PPlugin.transcribe = recording_transcribe

    base = json.loads(pathlib.Path(args.voice_config).read_text(encoding="utf-8"))
    voices = {}

    def voice(lang, model):
        if (lang, model) not in voices:
            config = dict(base, phoneme_type="arbtok", lang_code=lang, phonemizer_model=model)
            config["inference"] = dict(base.get("inference", {}), add_diacritics=False)
            vc = VoiceConfig.from_dict(config)

            class NoEngine:
                def configure(self, _config):
                    pass

            voices[lang, model] = TTSVoice(session=None, config=vc, adapter=NoEngine())
        return voices[lang, model]

    items = inputs()
    records, no_call, raised = [], 0, 0
    for lang, model, text in items:
        v = voice(lang, model)
        del calls[:]
        try:
            pairs = list(v._iter_synthesis_ids(text, SynthesisConfig(), Alphabet.GRAPHEMES, v.config.alphabet))
        except Exception as exc:
            print(f"phoonnx raised {type(exc).__name__} on {text!r} under {lang}", file=sys.stderr)
            raised += 1
            continue
        if not calls:
            no_call += 1
            continue
        records.append({"lang": lang, "model": model, "text": text, "calls": [list(c) for c in calls],
                        "phonemes": [list(p) for p, _ in pairs], "ids": [list(i) for _, i in pairs]})
    print(f"{len(items)} inputs: {len(records)} recorded, {no_call} made no call into arbtok, "
          f"{raised} raised in phoonnx", file=sys.stderr)
    if len(records) + no_call + raised != len(items):
        raise SystemExit("the inputs and the outcomes do not add up")
    header = {"versions": {name: importlib.metadata.version(name)
                           for name in ("phoonnx", "scriptconv", *common.PINNED)},
              "voice": VOICE, "phoneme_id_map": base["phoneme_id_map"], "inputs": len(items),
              "records": len(records), "no_call": no_call, "raised": raised}
    raw = io.BytesIO()
    with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as gz:
        for line in [header] + records:
            gz.write((common.dumps(line) + "\n").encode("utf-8"))
    pathlib.Path(args.out).write_bytes(raw.getvalue())
    with gzip.open(args.out, "rt", encoding="utf-8") as fh:
        written = sum(1 for _ in fh) - 1
    if written != len(records):
        raise SystemExit(f"{args.out}: {len(records)} records, {written} in the file")
    print(f"{args.out}: {written} records", file=sys.stderr)


if __name__ == "__main__":
    main()
