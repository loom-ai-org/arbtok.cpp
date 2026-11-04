"""Calls whose result depends on the environment and the file system.

A case names environment variables, files and directories under a fresh root
directory, and one call. The call runs in a child interpreter, because
arbtok.lexicon and huggingface_hub read the environment when they are
imported. A record is ["env", case, result]: every path in the case and in the
result is written with the root as "<ROOT>", and HOME is always <ROOT>/home.
A result that raises is {"err": "<Python class>"}, with "message" when the
case asks for the message.
"""
import json
import os
import subprocess
import sys
import tempfile

# The variables a case starts without, so that the caller's own cache
# settings do not reach the record.
CLEARED = ("XDG_CACHE_HOME", "HF_HOME", "HF_HUB_CACHE", "HUGGINGFACE_HUB_CACHE", "HF_HUB_OFFLINE")

CHILD = r"""
import json, sys
case = json.loads(sys.argv[1])
root = sys.argv[2]
def sub(v):
    return v.replace(root, "<ROOT>") if isinstance(v, str) else v
call = case["call"]
try:
    if call[0] == "cache_dir":
        from arbtok.lexicon import CACHE_DIR
        out = str(CACHE_DIR)
    elif call[0] == "resolve_source":
        from arbtok.lexicon import resolve_source
        out = str(resolve_source(call[1].replace("<ROOT>", root)))
    elif call[0] == "load":
        from arbtok.lexicon import StemLexicon
        lex = StemLexicon(call[1].replace("<ROOT>", root))
        out = [list(lex.entries.items()), [[k, repr(v)] for k, v in lex._shares.items()]]
    elif call[0] == "dialect_entries":
        from pathlib import Path
        from arbtok.dialect_lexicon import DialectLexicon
        out = list(DialectLexicon(call[1], data_dir=Path(call[2].replace("<ROOT>", root))).entries.items())
    else:
        raise SystemExit("unknown call " + call[0])
    result = json.loads(json.dumps(out).replace(json.dumps(root)[1:-1], "<ROOT>"))
except Exception as exc:
    result = {"err": type(exc).__name__}
    if case.get("message"):
        result["message"] = str(exc).replace(root, "<ROOT>")
print(json.dumps(result, ensure_ascii=False))
"""


def run(case):
    with tempfile.TemporaryDirectory() as root:
        root = os.path.realpath(root)
        os.makedirs(os.path.join(root, "home"))
        for rel in case.get("dirs", []):
            os.makedirs(os.path.join(root, rel), exist_ok=True)
        for rel, hexdata in case.get("files", {}).items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as fh:
                fh.write(bytes.fromhex(hexdata))
        env = {k: v for k, v in os.environ.items() if k not in CLEARED}
        env["HOME"] = os.path.join(root, "home")
        for k, v in case.get("env", {}).items():
            env[k] = v.replace("<ROOT>", root)
        proc = subprocess.run([sys.executable, "-I", "-c", CHILD, json.dumps(case), root], env=env, cwd=root,
                              capture_output=True, text=True, check=True)
        return ["env", case, json.loads(proc.stdout)]


def hexof(text):
    return text.encode("utf-8").hex() if isinstance(text, str) else text.hex()
