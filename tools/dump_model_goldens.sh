#!/bin/bash
# Dump the goldens of the model on this machine.
#
#   tools/dump_model_goldens.sh <out dir> [--check]
#
# A golden whose header names a "cpu_class" holds onnxruntime output of that
# CPU class. This script copies tests/goldens to <out dir> and dumps each of
# those goldens again there, on this CPU, with the reference versions of
# tools/goldens/common.py. Configure with -DARBTOK_GOLDENS_DIR=<out dir> to
# test against them. The re-dumped ensemble golden holds the raw logits of
# every record, so ensemble_parity checks the tolerance on all of them. The
# script fails when a re-dumped golden has other corpus counts than the
# committed one. --check also dumps every other golden that dump_goldens.py or
# ensemble_unicode.py writes, and fails unless its records are byte-identical
# to the committed file. It does the same for a model golden whose class is
# the class of this CPU. The numbers goldens come from their own scripts under
# tools/goldens, and --check names them without a dump. --check fails when
# it would skip any other golden.
#
# It needs uv, curl, the stem lexicon in the Hugging Face cache (see README),
# and about 3 GB in <out dir>.work for the reference venv and the sources.
set -euo pipefail
[ $# -ge 1 ] || { echo "usage: $0 <out dir> [--check]" >&2; exit 2; }
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$1"
out=$(cd "$1" && pwd)
work="$out.work"
check=${2:-}
mkdir -p "$work"

# The pins and the gold commit, read from the dumper without importing it.
eval "$(python3 - "$root/tools/goldens/common.py" <<'PY'
import ast, sys
values = {}
for node in ast.parse(open(sys.argv[1]).read()).body:
    if isinstance(node, (ast.Assign, ast.AnnAssign)):
        target = node.targets[0] if isinstance(node, ast.Assign) else node.target
        if isinstance(target, ast.Name) and target.id in ("PINNED", "RUNTIME", "GOLD_COMMIT"):
            values[target.id] = ast.literal_eval(node.value)
pins = values["PINNED"] | values["RUNTIME"]
print("pins='" + " ".join(f"{k}=={v}" for k, v in pins.items()) + "'")
print(f"arbtok_version={values['PINNED']['arbtok']}")
print(f"gold_commit={values['GOLD_COMMIT']}")
PY
)"
python_version=3.12.13

if [ ! -x "$work/ref/bin/python" ]; then
    (unset VIRTUAL_ENV; uv venv -q --python "$python_version" "$work/ref")
    VIRTUAL_ENV="$work/ref" uv pip install -q --prerelease=allow $pins
fi
py="$work/ref/bin/python"

# The sources go to a new directory, which is renamed to src only when both
# downloads are complete, so an interrupted run leaves no partial src.
if [ ! -d "$work/src" ]; then
    partial=$(mktemp -d "$work/src.partial.XXXXXX")
    mkdir -p "$partial/sdist" "$partial/repo"
    sdist=$(curl -sSf "https://pypi.org/pypi/arbtok/$arbtok_version/json" |
        python3 -c 'import json, sys; print(next(u["url"] for u in json.load(sys.stdin)["urls"] if u["packagetype"] == "sdist"))')
    curl -sSfL "$sdist" | tar xz -C "$partial/sdist" --strip-components=1
    curl -sSfL "https://codeload.github.com/TigreGotico/arbtok/tar.gz/$gold_commit" |
        tar xz -C "$partial/repo" --strip-components=1 --wildcards '*/data/gold_code_switched' '*/data/span_gold' '*/tests'
    mv "$partial" "$work/src"
fi

here=$("$py" -I "$root/tools/goldens/cpu_class.py")
echo "CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')"
echo "CPU class: $here"

cp "$root"/tests/goldens/*.jsonl.gz "$out/"
model=()
other=()
for f in "$root"/tests/goldens/*.jsonl.gz; do
    name=$(basename "$f" .jsonl.gz)
    # numbers_calls holds recorded calls and no header line.
    golden_class=$("$py" -I -c 'import gzip, json, sys; h = json.loads(gzip.open(sys.argv[1], "rt").readline()); print(h.get("cpu_class", "") if isinstance(h, dict) else "")' "$f")
    if [ -n "$golden_class" ]; then
        model+=("$name")
        echo "model golden $name: committed class $golden_class"
    else
        other+=("$name")
    fi
done

dump() {  # out dir, modules...
    local dir=$1; shift
    local mods=()
    for m in "$@"; do
        case $m in
            # The golden with the raw logits of every record is the one the
            # tests read. The one with digests is the form of the committed golden.
            ensemble) "$py" -I "$root/tools/goldens/ensemble.py" --tests-dir "$work/src/repo/tests" \
                          --out "$work/ensemble.digests.jsonl.gz" --full-logits-out "$dir/ensemble.jsonl.gz" ;;
            ensemble_unicode) "$py" -I "$root/tools/goldens/ensemble_unicode.py" "$dir/ensemble_unicode.jsonl.gz" ;;
            *) mods+=("$m") ;;
        esac
    done
    if [ ${#mods[@]} -gt 0 ]; then
        HF_HUB_OFFLINE=1 "$py" -I "$root/tools/dump_goldens.py" --out-dir "$dir" --sdist-tests "$work/src/sdist/tests" \
            --gold-dir "$work/src/repo/data" --corpus "$work/src/repo/tests/ar_test.txt" "${mods[@]}"
    fi
}

dump "$out" "${model[@]}"
# Each re-dump must name this CPU class and hold the corpus counts of the
# committed golden: a missing input file gives a smaller corpus, not an error.
"$py" -I - "$root/tests/goldens" "$out" "$here" "${model[@]}" <<'PY'
import gzip, json, sys

committed, dumped, here, names = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
bad = False
for name in names:
    want, got = (json.loads(gzip.open(f"{d}/{name}.jsonl.gz", "rt", encoding="utf-8").readline()) for d in (committed, dumped))
    if got.get("cpu_class") != here:
        print(f"{name}: the re-dump names class {got.get('cpu_class')}, this CPU is {here}", file=sys.stderr)
        bad = True
    for key in ("count", "corpora", "records"):
        if got.get(key) != want.get(key):
            print(f"{name}: the re-dump has {key} {got.get(key)}, the committed golden has {want.get(key)}", file=sys.stderr)
            bad = True
sys.exit(1 if bad else 0)
PY
echo "dumped ${#model[@]} model goldens for class $here into $out: ${model[*]}"

# The records of two goldens, compared without the gzip header.
same() { cmp -s <(gzip -dc "$1") <(gzip -dc "$2"); }

if [ "$check" = --check ]; then
    # The module list comes from importing the dumper, so any form of the
    # MODULES assignment gives the list that dump_goldens.py itself runs.
    modules=$("$py" -I - "$root/tools" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
import dump_goldens
print(" ".join(dump_goldens.MODULES))
PY
)
    dumpable=" ensemble_unicode $modules "
    checked=()
    skipped=()
    for m in "${other[@]}"; do
        if [[ $dumpable == *" $m "* ]]; then checked+=("$m"); else skipped+=("$m"); fi
    done
    # Only the numbers goldens come from other scripts. Any other golden that
    # this check would skip is an error, not a pass.
    not_dumped="numbers_ar numbers_calls numbers_dates_ar numbers_dispatch"
    if [ "$(printf '%s\n' "${skipped[@]}" | sort | xargs)" != "$not_dumped" ]; then
        echo "--check would skip ${skipped[*]}; only $not_dumped come from other scripts" >&2
        exit 1
    fi
    for m in "${skipped[@]}"; do echo "not dumped by this script: $m"; done
    mkdir -p "$work/check"
    dump "$work/check" "${checked[@]}"
    bad=0
    for m in "${checked[@]}"; do
        if same "$work/check/$m.jsonl.gz" "$root/tests/goldens/$m.jsonl.gz"; then
            echo "identical: $m"
        else
            echo "DIFFERS: $m"; bad=1
        fi
    done
    for m in "${model[@]}"; do
        golden_class=$("$py" -I -c 'import gzip, json, sys; print(json.loads(gzip.open(sys.argv[1], "rt").readline())["cpu_class"])' "$root/tests/goldens/$m.jsonl.gz")
        if [ "$golden_class" = "$here" ]; then
            dumped="$out/$m.jsonl.gz"
            [ "$m" = ensemble ] && dumped="$work/ensemble.digests.jsonl.gz"
            if same "$dumped" "$root/tests/goldens/$m.jsonl.gz"; then echo "identical: $m (class $here)"; else echo "DIFFERS: $m (class $here)"; bad=1; fi
        else
            echo "not compared: $m holds class $golden_class"
        fi
    done
    exit $bad
fi
