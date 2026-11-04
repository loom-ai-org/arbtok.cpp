"""The public surface of arbtok and the part of it that the C++ port holds.

The surface is read from the installed reference package with ast: every
public name that a module defines at module level, every public member of a
public class, and the __all__ of the package. For each name the table gives
the C++ name and a status. A ported name must occur in its header; the script
exits with 1 when one does not.

    python tools/inventory.py [--package-dir <arbtok dir>] [--parity PARITY.md]

With --parity, the table and the counts replace the text between the
inventory markers of that file.
"""
import argparse
import ast
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BEGIN, END = "<!-- inventory:begin -->", "<!-- inventory:end -->"

# Modules of the port: their C++ namespace is arbtok::<module> (or the one
# NAMESPACE gives), their header include/arbtok/<namespace>.hpp, and each name
# keeps its Python spelling unless RENAMED gives another.
PORTED = {"_ensemble", "arabizi", "constants", "diacritize", "dialect_lexicon", "dialects", "donor_lexicon", "fusion",
          "lattice", "lexicon", "nisba", "orthography", "sandhi", "stress", "tashkeel", "tokenizer",
          "unicode_symbol2label", "version", "waqf"}
NAMESPACE = {"_ensemble": "ensemble"}
RENAMED = {
    "_ensemble.DEFAULT_ENSEMBLE_ONNX": "default_ensemble_onnx()",
    "_ensemble.DEFAULT_VOCAB": "default_vocab()",
    "lattice.DEFAULT_RESCORERS": "default_rescorers()",
    "lexicon.CACHE_DIR": "cache_dir()",
    "unicode_symbol2label.UNICODE_SYMBOL_TO_LABEL": "items(), get()",
}
# Names of a ported module that a later phase ports, and names of the other
# modules that this phase ports.
LATER_IN_PORTED = {}
PORTED_ELSEWHERE = {"textnorm.ARABIC_LANGUAGE_CODES", "textnorm.is_arabic_lang"}
# The package __all__: the module each re-exported name comes from.
REEXPORTS = {"AsrNorm": "textnorm", "Lect": "dialects", "TtsNorm": "textnorm", "__version__": "version",
             "is_arabic_lang": "textnorm", "normalize_asr": "textnorm", "normalize_for_tts": "textnorm",
             "spec_for_lang": "dialects", "supported_lects": "dialects", "vocalize": "__init__"}
REEXPORT_TARGET = {"__version__": "VERSION_STR"}
# The functions that the package itself defines, which include/arbtok/arbtok.hpp holds.
PACKAGE_FUNCTIONS = {"vocalize"}
PHASE = {"__init__": 2, "_ensemble": 2, "diacritize": 2, "dialect_lexicon": 2, "donor_lexicon": 2, "fusion": 2,
         "lexicon": 2, "textnorm": 3, "util": 3, "o2i_plugins": 4, "plugin": 4, "translit": 4}
NOT_PORTED = {"espeak_wrapper": "not ported: no phoonnx voice path calls it"}


def public(name):
    return not name.startswith("_")


def class_members(node):
    out = []
    for item in node.body:
        if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)):
            out.append(item.name)
        elif isinstance(item, ast.AnnAssign) and isinstance(item.target, ast.Name):
            out.append(item.target.id)
        elif isinstance(item, ast.Assign):
            out += [t.id for t in item.targets if isinstance(t, ast.Name)]
    return [m for m in dict.fromkeys(out) if public(m)]


def module_surface(path):
    """(names, __all__) of one module: names are 'name' or 'Class.member', in source order."""
    names, all_names = [], None
    for node in ast.parse(path.read_text(encoding="utf-8")).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            names.append(node.name)
        elif isinstance(node, ast.ClassDef):
            if public(node.name):
                names.append(node.name)
                names += [f"{node.name}.{m}" for m in class_members(node)]
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    if target.id == "__all__":
                        all_names = [e.value for e in node.value.elts]
                    else:
                        names.append(target.id)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names.append(node.target.id)
    return [n for n in dict.fromkeys(names) if public(n.split(".")[0])], all_names


def header_has(module, cpp_name):
    header = ROOT / "include" / "arbtok" / f"{NAMESPACE.get(module, module)}.hpp"
    if not header.exists():
        return False
    text = header.read_text(encoding="utf-8")
    words = re.findall(r"[A-Za-z_][A-Za-z_0-9]*", cpp_name)
    return all(re.search(rf"\b{re.escape(w)}\b", text) for w in words)


def cpp_name(module, name):
    target = RENAMED.get(f"{module}.{name}", name.replace(".", "::"))
    return f"arbtok::{target}" if module == "arbtok" else f"arbtok::{NAMESPACE.get(module, module)}::{target}"


def rows(package_dir):
    out = []
    for path in sorted(package_dir.glob("*.py")):
        module = path.stem
        names, all_names = module_surface(path)
        if module == "__init__":
            for name in all_names:
                source = REEXPORTS[name]
                key = f"{source}.{name}"
                if source == "__init__" and name in PACKAGE_FUNCTIONS:
                    out.append(("arbtok", name, cpp_name("arbtok", name), "ported", "arbtok", name))
                elif source in PORTED or key in PORTED_ELSEWHERE:
                    target = REEXPORT_TARGET.get(name, name)
                    out.append(("arbtok", name, cpp_name(source, target), "ported", source, target))
                else:
                    out.append(("arbtok", name, "", f"phase {PHASE.get(source, 2)}", None, None))
            continue
        for name in names:
            key = f"{module}.{name}"
            if module in NOT_PORTED:
                out.append((module, name, "", NOT_PORTED[module], None, None))
            elif key in LATER_IN_PORTED:
                out.append((module, name, "", f"phase {LATER_IN_PORTED[key]}", None, None))
            elif module in PORTED or key in PORTED_ELSEWHERE:
                target = RENAMED.get(key, name.replace(".", "::"))
                out.append((module, name, cpp_name(module, name), "ported", module, target))
            else:
                out.append((module, name, "", f"phase {PHASE[module]}", None, None))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--package-dir", type=pathlib.Path)
    parser.add_argument("--parity", type=pathlib.Path)
    args = parser.parse_args()
    package_dir = args.package_dir
    if package_dir is None:
        import arbtok
        package_dir = pathlib.Path(arbtok.__file__).parent
    table = rows(package_dir)
    missing = [r for r in table if r[3] == "ported" and not header_has(r[4], r[5])]
    ported = sum(r[3] == "ported" for r in table)
    modules = len({r[0] for r in table} - {"arbtok"})
    lines = [f"{len(table)} public symbols, {ported} ported.", "",
             f"The count covers {modules} modules and the package itself: each module-level name, each public "
             "class member, and each name of the package `__all__`.", "",
             "| Python name | C++ name | Status |", "| --- | --- | --- |"]
    for module, name, cpp, status, _, _ in table:
        lines.append(f"| `{module}.{name}` | {f'`{cpp}`' if cpp else ''} | {status} |")
    text = "\n".join(lines)
    print(f"{len(table)} public symbols, {ported} ported, {len(missing)} ported names missing from their header",
          file=sys.stderr)
    for r in missing:
        print(f"  missing: {r[0]}.{r[1]} -> {r[2]}", file=sys.stderr)
    if args.parity:
        doc = args.parity.read_text(encoding="utf-8")
        head, rest = doc.split(BEGIN, 1)
        _, tail = rest.split(END, 1)
        args.parity.write_text(f"{head}{BEGIN}\n{text}\n{END}{tail}", encoding="utf-8")
    else:
        print(text)
    return 1 if missing else 0


sys.exit(main())
