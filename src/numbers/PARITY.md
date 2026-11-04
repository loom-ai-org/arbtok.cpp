# arbtok_numbers parity

`arbtok_numbers` holds the parts of three Python libraries that arbtok 0.0.2a71 calls, for Arabic. For each
call it gives the same bytes as the reference, and the same Python exception class where the reference raises.

## Reference

| Package | Version |
| --- | --- |
| arbtok | 0.0.2a71 |
| ovos-number-parser | 0.24.3a1 |
| ovos-date-parser | 0.35.7a1 |
| unicode-rbnf | 2.4.1 |
| orthography2ipa | 7.114.16a1 |
| chronologia | 0.31.7a2 |
| Python | 3.12.13 |

Build the reference interpreter with uv:

```sh
unset VIRTUAL_ENV
uv venv --python 3.12 venv
uv pip install --python venv/bin/python --prerelease=allow arbtok==0.0.2a71 orthography2ipa==7.114.16a1
```

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

The number functions cover each language code that `numbers_ar.resolve_ar_lang` names an Arabic variety for.
`nice_time` covers the codes that start with `ar`; `nice_date` and `extract_datetime` cover the codes whose
primary subtag is `ar`. Every other code raises NotImplementedError. For `numbers_to_digits`, a code that no
module of the reference reads numbers for gives the text with its whitespace collapsed, as the reference does.

## Not ported

- chronologia, which `extract_datetime` asks about a text that names a holiday. `data/numbers/holidays_ar.json`
  holds the reference answer for each holiday text arbtok sends: a holiday word alone, and
  `الساعة <hour> <word>`, for lang `ar` at the anchor 2000-01-01. Any other text that names a holiday raises
  NotImplementedError.
- The unicode-rbnf rules. arbtok asks for a cardinal only for a word that holds a digit that is not a decimal
  digit, which `decimal.Decimal` rejects with InvalidOperation. `format_number` raises InvalidOperation for such
  a text and NotImplementedError for a text that `Decimal` accepts.
- The other languages of the three libraries.

## Data

`data/numbers/*.json` holds the module tables of the reference by their Python names, written by
`data/numbers/gen_data.py`. `src/numbers/unicode_tables.inc` holds the character properties of the reference
interpreter, written by `data/numbers/gen_unicode_tables.py`. The library compiles the data files in.

## Goldens

`tools/goldens/numbers_ar.py` and `tools/goldens/dates_ar.py` write `tests/goldens/numbers_ar.jsonl.gz` and
`tests/goldens/numbers_dates_ar.jsonl.gz` from the reference. Their inputs are the calls arbtok's own test suite
makes into the libraries, recorded in `tests/goldens/numbers_calls.jsonl.gz`, and generated grids. The ctest
targets `numbers_ar_parity` and `numbers_dates_ar_parity` compare the port with them.
