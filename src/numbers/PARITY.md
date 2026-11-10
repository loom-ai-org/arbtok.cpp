# arbtok_numbers parity

`arbtok_numbers` holds the parts of three Python libraries that arbtok 0.0.2a74 calls, for Arabic. For each
call it gives the same bytes as the reference, and the same Python exception class where the reference raises.

## Reference

| Package | Version |
| --- | --- |
| arbtok | 0.0.2a74 |
| ovos-number-parser | 0.24.3a1 |
| ovos-date-parser | 0.35.7a1 |
| unicode-rbnf | 2.4.1 |
| orthography2ipa | 7.122.20a2 |
| chronologia | 0.31.7a2 |
| dateparser | 1.5.0 |
| Python | 3.12.13 |

Build the reference interpreter with uv:

```sh
unset VIRTUAL_ENV
uv venv --python 3.12.13 venv
uv pip install --python venv/bin/python --prerelease=allow arbtok==0.0.2a74 orthography2ipa==7.122.20a2 \
    linguatag==0.1.5a1 langcodes==3.5.1 ovos-spec-tools==1.17.1a1 ovos-number-parser==0.24.3a1 \
    ovos-date-parser==0.35.7a1 unicode-rbnf==2.4.1 chronologia==0.31.7a2 dateparser==1.5.0 \
    onnxruntime==1.31.1 numpy==2.5.4
```

The dumpers stop when an installed version differs from `PINNED` or `RUNTIME` in `tools/goldens/common.py`.

Dump the three numbers goldens with that interpreter, from the repository root:

```sh
venv/bin/python -I tools/goldens/numbers_ar.py build tests/goldens/numbers_calls.jsonl.gz tests/goldens/numbers_ar.jsonl.gz
venv/bin/python -I tools/goldens/dates_ar.py tests/goldens/numbers_calls.jsonl.gz data/numbers/holidays_ar.json \
    tests/goldens/numbers_dates_ar.jsonl.gz
venv/bin/python -I tools/goldens/numbers_dispatch.py tests/goldens/numbers_dispatch.jsonl.gz
```

The results of these records do not come from orthography2ipa. `numbers_ar` takes its lect codes from `arbtok.dialects`, which calls orthography2ipa, so a registry change can change the input grid of `numbers_ar`. `tools/dump_model_goldens.sh --check` runs the three dumpers and fails when one of them stops with an error or writes other records than the committed file.

## Ported functions

| C++ | Python |
| --- | --- |
| `ovos_number_parser::pronounce_number` | `ovos_number_parser.pronounce_number` |
| `ovos_number_parser::pronounce_ordinal` | `ovos_number_parser.pronounce_ordinal` |
| `ovos_number_parser::pronounce_fraction` | `ovos_number_parser.pronounce_fraction` |
| `ovos_number_parser::extract_number` | `ovos_number_parser.extract_number` |
| `ovos_number_parser::numbers_to_digits` | `ovos_number_parser.numbers_to_digits` |
| `ovos_number_parser::is_numeric` | `ovos_number_parser.util.is_numeric` |
| `ovos_date_parser::nice_time` | `ovos_date_parser.nice_time` |
| `ovos_date_parser::nice_date` | `ovos_date_parser.nice_date` |
| `ovos_date_parser::extract_datetime` | `ovos_date_parser.extract_datetime` |
| `unicode_rbnf::RbnfEngine` | `unicode_rbnf.RbnfEngine.for_language`, `format_number` |

Each entry point routes a language code as the reference does, test by test in the reference order. The Arabic
code reads the codes the reference sends to its Arabic module: for the number functions each code that
`numbers_ar.resolve_ar_lang` names an Arabic variety for, for `nice_time` and `extract_datetime` the codes that
start with `ar`, and for `nice_date` the codes whose lower-case primary subtag is `ar`. A code that the reference
reads with the module of another language, with the date formats of another language, with the unicode-rbnf
rules of a language or with dateparser raises `NotPortedError`. That class stands for no Python exception, so
an `except Exception` of arbtok does not catch it. A code that the reference rejects gives what the reference
gives: `NotImplementedError("Unsupported language: ...")` from the speakers, `extract_number`, `nice_time` and
`nice_date`, and from `numbers_to_digits` the text with its whitespace collapsed.

## Not ported

- chronologia, which `extract_datetime` asks about a text that names a holiday. `data/numbers/holidays_ar.json`
  holds the reference answer for each holiday text arbtok sends: a holiday word alone, and
  `الساعة <hour> <word>`, for lang `ar` at the anchor 2000-01-01. Any other text that names a holiday raises
  `NotPortedError`.
- The unicode-rbnf rules. arbtok asks for a cardinal only for a word that holds a digit that is not a decimal
  digit, which `decimal.Decimal` rejects with InvalidOperation. For `ar`, `format_number` raises
  InvalidOperation for such a text and `NotPortedError` for a text that `Decimal` accepts. For another language
  it raises `NotPortedError`. Where the rules of a language have no ordinal rule set, the reference
  `pronounce_ordinal` raises NotImplementedError and the port raises `NotPortedError`.
- The other languages of the three libraries.

## Data

`data/numbers/*.json` holds the module tables of the reference by their Python names, written by
`data/numbers/gen_data.py`. `src/numbers/unicode_tables.inc` holds the character properties of the reference
interpreter, written by `data/numbers/gen_unicode_tables.py`. The library compiles the data files in.

## Goldens

`tools/goldens/numbers_ar.py` and `tools/goldens/dates_ar.py` write `tests/goldens/numbers_ar.jsonl.gz` and
`tests/goldens/numbers_dates_ar.jsonl.gz` from the reference. Their inputs are the calls arbtok's own test suite
makes into the libraries, recorded in `tests/goldens/numbers_calls.jsonl.gz`, and generated grids.
`numbers_ar.py record` records the calls with pytest. It needs pandas, pyarrow and network access to the
Hugging Face Hub: without them the live test of `tests/test_iqraeval_benchmark.py` skips, and 39 calls are missing.
`tools/goldens/numbers_dispatch.py` writes `tests/goldens/numbers_dispatch.jsonl.gz`: 11 calls of the entry
points for each of 1894 language tags, each with the route that `tools/goldens/routes.py` saw the reference
take. A record of route `arabic` or `rejected` holds the reference result, and a record of route `module` must
raise `NotPortedError` in the port.

The first line of each file is a header with the reference versions and the record count of each corpus.
`numbers_parity` fails a file that has no header, and a file that holds a different number of records than its
header gives, so an empty or a truncated golden fails. With `ARBTOK_PARITY_STRIDE` above 1, as in the sanitizer
build, it checks every n-th record and still counts every record of the file.

| Golden | Records | ctest target |
| --- | --- | --- |
| `numbers_ar.jsonl.gz` | 184111 | `numbers_ar_parity` |
| `numbers_dates_ar.jsonl.gz` | 55038 | `numbers_dates_ar_parity` |
| `numbers_dispatch.jsonl.gz` | 20834: arabic 9722, module 6644, rejected 4468 | `numbers_dispatch_parity` |

Each golden has a mutation control.

| Mutation | Result |
| --- | --- |
| `names_a_holiday` returns false (`dates_ar.cpp`) | numbers_dates_ar: 4 of 55038; textnorm: 312 of 398131 |
| `not_ported` of the number functions raises `NotImplementedError` | numbers_dispatch: 3733 of 20834; exceptions: 2 of 16 calls |
| `extract_datetime` sends a code such as `ara` or `arz` to another module, where the reference reads every code that starts with `ar` | numbers_dispatch: 52 of 20834 |
