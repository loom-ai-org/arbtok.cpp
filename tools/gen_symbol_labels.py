"""Write data/unicode_symbol2label.json from arbtok.unicode_symbol2label.

The JSON object holds UNICODE_SYMBOL_TO_LABEL with its keys in the dict's
order. Before it writes the file, the script reads the JSON text back and
stops unless the result equals the Python dict, key order included.

Usage, with the reference interpreter:
    python -I tools/gen_symbol_labels.py data/unicode_symbol2label.json
"""
import json
import sys

from arbtok.unicode_symbol2label import UNICODE_SYMBOL_TO_LABEL


def main(path):
    text = json.dumps(UNICODE_SYMBOL_TO_LABEL, ensure_ascii=False, indent=1) + "\n"
    back = json.loads(text)
    if back != UNICODE_SYMBOL_TO_LABEL or list(back.items()) != list(UNICODE_SYMBOL_TO_LABEL.items()):
        raise SystemExit("the JSON text does not read back as UNICODE_SYMBOL_TO_LABEL")
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print(f"{path}: {len(UNICODE_SYMBOL_TO_LABEL)} entries in, {len(back)} entries out", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1])
