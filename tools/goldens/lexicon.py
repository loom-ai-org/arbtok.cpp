"""Goldens of arbtok.lexicon.

parse and parse_shares run over generated TSV texts: header rows, short rows,
blank lines, every line boundary str.splitlines() knows, keys that
normalize_unicode changes, repeated keys, and counts in every form int()
accepts or refuses. The default stem lexicon is read from the Hugging Face
cache: its records hold the SHA-256 of the file and of the parsed entries and
shares, not the entries, and StemLexicon.get runs over the corpus words and
the Arabic test words under several shares and sentence lengths. The env
records (see envcase.py) cover CACHE_DIR, resolve_source for a path, a URL and
an hf:// id, and the errors of a lexicon that cannot be read.
"""
import hashlib
import json
import random

from arbtok import lexicon as lx
from arbtok.diacritize import _has_arabic

from . import common, envcase

LINE_BREAKS = ["\n", "\r\n", "\r", "\v", "\f", "\x1c", "\x1d", "\x1e", "\x85", " ", " "]
KEYS = ["كتاب", "كِتَاب", "ٱلكتاب", "ﻻ", "مدرسة", "عربي", "آبل", "كَّ", "كَّ", "", " ", "key", "KEY",
        "abc", "آ", "أ", "أ", "١٢٣", "x\ty"]
COUNTS = ["0", "1", "3", "10", "134", "007", "1_000", "1__0", "_1", "1_", "+5", "-2", " 4 ", "\t9\n", "٣", "١٢",
          "۴", "५", "²", "1.5", "1e3", "", "x", "0x10", "  ", "+", "- 3", " 12 ", "99999999999"]


def tsv_texts():
    rng = random.Random(41)
    out = ["", "\n", "key\tstem\tcount\trunner_up\n", "a", "a\tb", "a\tb\n", "a\tb\tc", "a\tb\t1\t2", "a\tb\t1\t2\textra",
           "key\tx\t1\t2\nkey2\ty\t3\t4", "\tstem\t1\t2", "k\t\t1\t2", "k\tv\t0\t0", "k\tv\t-1\t1", "k\tv\t2\t-1",
           "k\tv1\t1\t2\nk\tv2\t3\t0", "k\tv\t1\t2\nk\tv\t5\t5"]
    for _ in range(900):
        lines = []
        for _ in range(rng.randint(1, 6)):
            parts = [rng.choice(KEYS)]
            for _ in range(rng.choice([0, 1, 1, 2, 3, 3, 3, 4])):
                parts.append(rng.choice(KEYS + COUNTS))
            if rng.random() < 0.6 and len(parts) >= 2:
                parts = parts[:2] + [rng.choice(COUNTS), rng.choice(COUNTS)]
            lines.append("\t".join(parts))
        out.append("".join(line + rng.choice(LINE_BREAKS) for line in lines)[:None if rng.random() < 0.7 else -1])
    return common.unique(out)


def digest(items):
    return hashlib.sha256(json.dumps(items, ensure_ascii=False, separators=(",", ":")).encode("utf-8")).hexdigest()


def source_record():
    path = lx.resolve_source(lx.DEFAULT_LEXICON)
    data = path.read_bytes()
    lex = lx.StemLexicon()
    entries = list(lex.entries.items())
    shares = [[k, repr(v)] for k, v in lex._shares.items()]
    return ["source", lx.DEFAULT_LEXICON, {"revision": path.parent.name, "sha256": hashlib.sha256(data).hexdigest(),
                                           "entries": len(entries), "entries_sha256": digest(entries),
                                           "shares": len(shares), "shares_sha256": digest(shares)}]


def lookup_words():
    words = common.corpus_words(400) + common.words_of(common.arabic_test_strings())
    words += ["", " ", "abc", "ك", "كتاب ", "كِتَاب"]
    return common.unique(words)


def env_cases():
    hub = "datasets--org--name"
    snap = {f"hub/{hub}/refs/main": envcase.hexof("abc123"),
            f"hub/{hub}/snapshots/abc123/dir/f.tsv": envcase.hexof("k\tv\n")}
    lexdir = "xdg/arbtok/lexicons"
    cases = [
        {"call": ["cache_dir"]},
        {"call": ["cache_dir"], "env": {"XDG_CACHE_HOME": "<ROOT>/xdg"}},
        {"call": ["cache_dir"], "env": {"XDG_CACHE_HOME": ""}},
        {"call": ["cache_dir"], "env": {"XDG_CACHE_HOME": "rel/cache"}},
    ]
    for src in ["<ROOT>/a.tsv", "~/a.tsv", "~", "~/", "rel/a.tsv", "", ".", "./x//y.tsv", "a/./b/", "//a/b", "///a",
                "a/../b", "/", "x.tsv/", "ftp://h/x.tsv", "HF://org/name/f.tsv", "hf:/org/name"]:
        cases.append({"call": ["resolve_source", src]})
    for src, files in [("https://example.invalid/d/x.tsv", {f"{lexdir}/x.tsv": "6b0976"}),
                       ("http://example.invalid/x.tsv?y=1", {f"{lexdir}/x.tsv?y=1": "6b0976"}),
                       ("https://example.invalid/d/", {f"{lexdir}/keep": ""}),
                       ("https://x.tsv", {f"{lexdir}/x.tsv": ""})]:
        cases.append({"call": ["resolve_source", src], "env": {"XDG_CACHE_HOME": "<ROOT>/xdg"}, "files": files})
    for env, prefix in [({"HF_HUB_CACHE": "<ROOT>/hub"}, ""), ({"HUGGINGFACE_HUB_CACHE": "<ROOT>/hub"}, ""),
                        ({"HF_HOME": "<ROOT>/hfh"}, "hfh/"), ({"XDG_CACHE_HOME": "<ROOT>/xc"}, "xc/huggingface/"),
                        ({}, "home/.cache/huggingface/")]:
        files = {prefix + k: v for k, v in snap.items()}
        env = dict(env, HF_HUB_OFFLINE="1")
        for src in ["hf://org/name/dir/f.tsv", "hf://org/name/dir/missing.tsv", "hf://other/name/f.tsv"]:
            cases.append({"call": ["resolve_source", src], "env": env, "files": files})
    hf_env = {"HF_HUB_CACHE": "<ROOT>/hub", "HF_HUB_OFFLINE": "1"}
    for src in ["hf://org", "hf://org/name", "hf://org/name/", "hf://org/name/dir//f.tsv"]:
        cases.append({"call": ["resolve_source", src], "env": hf_env, "files": snap})
    loads = {
        "ok.tsv": "key\tstem\tcount\trunner_up\nكتاب\tكِتَاب\t5\t1\r\nقلم\tقَلَم\t3\t3\rباب\tبَاب\n",
        "bad.tsv": b"k\tv\n\xff\xfe",
        "bad2.tsv": b"ab\xc3",
        "bad3.tsv": b"\xe0\x80\x80k",
        "bad4.tsv": b"k\xed\xa0\x80",
        "bad5.tsv": b"k\tv\n\xe2\x82",
        "bad6.tsv": b"\xf0\x9f\x98",
        "bom.tsv": "﻿k\tv\t1\t1\n",
    }
    for name, content in loads.items():
        cases.append({"call": ["load", f"<ROOT>/{name}"], "files": {name: envcase.hexof(content)}, "message": True})
    cases.append({"call": ["load", "<ROOT>/missing.tsv"], "message": True})
    cases.append({"call": ["load", "<ROOT>/d"], "dirs": ["d"], "message": True})
    cases.append({"call": ["load", "~/h.tsv"], "files": {"home/h.tsv": envcase.hexof("a\tb\n")}, "message": True})
    cases.append({"call": ["load", "hf://org/name/dir/f.tsv"], "env": hf_env, "files": snap, "message": True})
    return cases


def corpora():
    texts = tsv_texts()
    words = lookup_words()
    configs = [(lx.AMBIGUOUS_SHARE, None), (lx.AMBIGUOUS_SHARE, 2), (lx.AMBIGUOUS_SHARE, 3), (lx.AMBIGUOUS_SHARE, 7),
               (None, 5), (0.25, 3), (0.0, 3)]
    lexicons = {share: lx.StemLexicon(lx.DEFAULT_LEXICON, share) for share, _ in configs}
    gets = [(share, ctx, w) for share, ctx in configs for w in (words if ctx in (None, 3) else words[::4])]
    shares = [lx.AMBIGUOUS_SHARE, None, 0.25]

    def get(p):
        share, ctx, w = p
        return ["get", None if share is None else repr(share), ctx, w, lexicons[share].get(w, ctx)]

    cases = env_cases()
    return [
        common.corpus("constants", ["DEFAULT_LEXICON", "AMBIGUOUS_SHARE", "MIN_CONTEXT_WORDS"],
                      lambda n: ["const", n, repr(getattr(lx, n)) if n == "AMBIGUOUS_SHARE" else getattr(lx, n)]),
        common.corpus("parse", texts, lambda t: ["parse", t, list(lx.parse(t).items())]),
        common.corpus("parse_shares", texts, lambda t: ["parse_shares", t,
                                                        [[k, repr(v)] for k, v in lx.parse_shares(t).items()]]),
        common.corpus("source", [None], lambda _: source_record()),
        common.corpus("get", gets, get),
        common.corpus("ambiguous", [(s, w) for s in shares for w in words[::3]],
                      lambda p: ["ambiguous", None if p[0] is None else repr(p[0]), p[1], lexicons[p[0]].ambiguous(p[1])]),
        common.corpus("contains", words[::5], lambda w: ["contains", w, w in lexicons[lx.AMBIGUOUS_SHARE]]),
        common.corpus("len", [None], lambda _: ["len", len(lexicons[lx.AMBIGUOUS_SHARE])]),
        common.corpus("env", cases, envcase.run),
    ]
