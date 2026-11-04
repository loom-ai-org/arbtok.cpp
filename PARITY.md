# Parity with arbtok

arbtok.cpp gives the same bytes as one release of the Python package for each name it ports. This file names that release, lists the whole public surface of the package with the status of each name, and records what the port depends on and where it differs.

## Reference

The goldens are dumped from these versions. Each golden file names them in its header. `tools/dump_goldens.py` pins the packages of the table and stops when an installed one has another version. The linguatag, langcodes and ovos-spec-tools versions are the ones that resolve with the first two under `uv --prerelease=allow`. linguatag and langcodes decide how a language tag resolves. Other versions can change the dialects records. The number and date parsers give the spoken numbers, dates and times of the textnorm and util records, and `src/numbers` ports their Arabic parts at these versions.

| Package | Version |
| --- | --- |
| arbtok | 0.0.2a71 (PyPI) |
| orthography2ipa | 7.114.16a1 |
| langcodes | 3.5.1 |
| linguatag | 0.1.5a1 |
| ovos-spec-tools | 1.17.1a1 |
| ovos-number-parser | 0.24.3a1 |
| ovos-date-parser | 0.35.7a1 |
| unicode-rbnf | 2.4.1 |
| chronologia | 0.31.7a2 |
| dateparser | 1.5.0 |
| Python | 3.12.13 (Unicode 15.0.0) |

The other packages that resolve with these under `uv --prerelease=allow` are: combo_lock 0.3.1, json_database 1.0.5a1, numpy 2.5.4, onnxruntime 1.31.1, ovos-config 3.9.0a2, ovos-utils 0.15.7a2, quebra-frases 0.3.8a1, regex 2026.9.29, and their own dependencies.

The gold sets come from the arbtok repository at commit `a95668cf097c5fcfb72b7ab75d7346579626e7a5`, whose `version.py` is 0.0.2a71. The other test inputs come from the `tests/` directory of the 0.0.2a71 sdist and from the data that the installed wheel bundles. `tests/data/textnorm_calls.jsonl.gz` holds the 35138 distinct textnorm and util calls that the sdist test suite makes. `tools/goldens/record_calls.py` records them. The sdist has no `tests/voice_agent.py`, `scripts/` or `data/`, so the recording run takes them from the arbtok repository at the gold commit.

orthography2ipa.cpp is the submodule `third_party/orthography2ipa.cpp`, pinned to `11da68ffe8101a3171061cf5eafdf3f376de48f8` on its `correct-port` branch. That commit holds parity with orthography2ipa 7.114.16a1 and linguatag 0.1.5a1.

## Public surface

`tools/inventory.py` reads the surface from the installed package with `ast` and writes the table below. It exits with 1 when a name marked as ported does not occur in its header.

arbtok has no command-line interface. Its only entry points are the three orthography2ipa plugins of `o2i_plugins` (normalize, rescore and sandhi), which phase 4 ports.

<!-- inventory:begin -->
362 public symbols, 262 ported.

The count covers 25 modules and the package itself: each module-level name, each public class member, and each name of the package `__all__`.

| Python name | C++ name | Status |
| --- | --- | --- |
| `arbtok.AsrNorm` | `arbtok::textnorm::AsrNorm` | ported |
| `arbtok.Lect` | `arbtok::dialects::Lect` | ported |
| `arbtok.TtsNorm` | `arbtok::textnorm::TtsNorm` | ported |
| `arbtok.__version__` | `arbtok::version::VERSION_STR` | ported |
| `arbtok.is_arabic_lang` | `arbtok::textnorm::is_arabic_lang` | ported |
| `arbtok.normalize_asr` | `arbtok::textnorm::normalize_asr` | ported |
| `arbtok.normalize_for_tts` | `arbtok::textnorm::normalize_for_tts` | ported |
| `arbtok.spec_for_lang` | `arbtok::dialects::spec_for_lang` | ported |
| `arbtok.supported_lects` | `arbtok::dialects::supported_lects` | ported |
| `arbtok.vocalize` |  | phase 2 |
| `_ensemble.DEFAULT_ENSEMBLE_ONNX` |  | phase 2 |
| `_ensemble.DEFAULT_VOCAB` |  | phase 2 |
| `_ensemble.EnsembleDiacritizer` |  | phase 2 |
| `_ensemble.EnsembleDiacritizer.classes` |  | phase 2 |
| `_ensemble.EnsembleDiacritizer.logits` |  | phase 2 |
| `_ensemble.EnsembleDiacritizer.decode` |  | phase 2 |
| `_ensemble.EnsembleDiacritizer.diacritize` |  | phase 2 |
| `_ensemble.get_ensemble` |  | phase 2 |
| `arabizi.ARABIZI_DIGITS` | `arbtok::arabizi::ARABIZI_DIGITS` | ported |
| `arabizi.is_arabizi` | `arbtok::arabizi::is_arabizi` | ported |
| `arabizi.arabizi_covers` | `arbtok::arabizi::arabizi_covers` | ported |
| `arabizi.to_arabic_skeleton` | `arbtok::arabizi::to_arabic_skeleton` | ported |
| `constants.FATHA` | `arbtok::constants::FATHA` | ported |
| `constants.DAMMA` | `arbtok::constants::DAMMA` | ported |
| `constants.KASRA` | `arbtok::constants::KASRA` | ported |
| `constants.SHADDA` | `arbtok::constants::SHADDA` | ported |
| `constants.SUKUN` | `arbtok::constants::SUKUN` | ported |
| `constants.TANWIN_FATH` | `arbtok::constants::TANWIN_FATH` | ported |
| `constants.TANWIN_DAMM` | `arbtok::constants::TANWIN_DAMM` | ported |
| `constants.TANWIN_KASR` | `arbtok::constants::TANWIN_KASR` | ported |
| `constants.DAGGER_ALIF` | `arbtok::constants::DAGGER_ALIF` | ported |
| `constants.MADD` | `arbtok::constants::MADD` | ported |
| `constants.HAMZA` | `arbtok::constants::HAMZA` | ported |
| `constants.ALEF_MADDA` | `arbtok::constants::ALEF_MADDA` | ported |
| `constants.ALEF_HAMZA_ABOVE` | `arbtok::constants::ALEF_HAMZA_ABOVE` | ported |
| `constants.WAW_HAMZA` | `arbtok::constants::WAW_HAMZA` | ported |
| `constants.ALEF_HAMZA_BELOW` | `arbtok::constants::ALEF_HAMZA_BELOW` | ported |
| `constants.YA_HAMZA` | `arbtok::constants::YA_HAMZA` | ported |
| `constants.ALIF` | `arbtok::constants::ALIF` | ported |
| `constants.TA_MARBUTA` | `arbtok::constants::TA_MARBUTA` | ported |
| `constants.WAW` | `arbtok::constants::WAW` | ported |
| `constants.ALIF_MAKSURA` | `arbtok::constants::ALIF_MAKSURA` | ported |
| `constants.YA` | `arbtok::constants::YA` | ported |
| `constants.LAM` | `arbtok::constants::LAM` | ported |
| `constants.HAMZAT_AL_WASL` | `arbtok::constants::HAMZAT_AL_WASL` | ported |
| `constants.ALEF_VARIANTS` | `arbtok::constants::ALEF_VARIANTS` | ported |
| `constants.ARABIC_PUNCT` | `arbtok::constants::ARABIC_PUNCT` | ported |
| `constants.PUNCT` | `arbtok::constants::PUNCT` | ported |
| `constants.SUN_LETTERS` | `arbtok::constants::SUN_LETTERS` | ported |
| `constants.CLITIC_BASES` | `arbtok::constants::CLITIC_BASES` | ported |
| `constants.B` | `arbtok::constants::B` | ported |
| `constants.R` | `arbtok::constants::R` | ported |
| `constants.Z` | `arbtok::constants::Z` | ported |
| `constants.S` | `arbtok::constants::S` | ported |
| `constants.F` | `arbtok::constants::F` | ported |
| `constants.Q` | `arbtok::constants::Q` | ported |
| `constants.K` | `arbtok::constants::K` | ported |
| `constants.M` | `arbtok::constants::M` | ported |
| `constants.N` | `arbtok::constants::N` | ported |
| `constants.H` | `arbtok::constants::H` | ported |
| `constants.T` | `arbtok::constants::T` | ported |
| `constants.DJ` | `arbtok::constants::DJ` | ported |
| `constants.X` | `arbtok::constants::X` | ported |
| `constants.D` | `arbtok::constants::D` | ported |
| `diacritize.strip_marks` |  | phase 2 |
| `diacritize.repair_skeleton` |  | phase 2 |
| `diacritize.PROVENANCE` |  | phase 2 |
| `diacritize.DiacritizedWord` |  | phase 2 |
| `diacritize.DiacritizedWord.surface` |  | phase 2 |
| `diacritize.DiacritizedWord.output` |  | phase 2 |
| `diacritize.DiacritizedWord.provenance` |  | phase 2 |
| `diacritize.DiacritizedWord.lect_constrained` |  | phase 2 |
| `diacritize.DiacritizedWord.is_failure` |  | phase 2 |
| `diacritize.DiacritizedText` |  | phase 2 |
| `diacritize.DiacritizedText.text` |  | phase 2 |
| `diacritize.DiacritizedText.words` |  | phase 2 |
| `diacritize.DiacritizedText.by_provenance` |  | phase 2 |
| `diacritize.LatticeDiacritizer` |  | phase 2 |
| `diacritize.LatticeDiacritizer.diacritizer` |  | phase 2 |
| `diacritize.LatticeDiacritizer.diacritize_word` |  | phase 2 |
| `diacritize.LatticeDiacritizer.diacritize_word_record` |  | phase 2 |
| `diacritize.LatticeDiacritizer.diacritize` |  | phase 2 |
| `diacritize.LatticeDiacritizer.diacritize_text` |  | phase 2 |
| `diacritize.diacritize` |  | phase 2 |
| `dialect_lexicon.lexicon_dir` |  | phase 2 |
| `dialect_lexicon.lexicon_file` |  | phase 2 |
| `dialect_lexicon.available_lects` |  | phase 2 |
| `dialect_lexicon.parse` |  | phase 2 |
| `dialect_lexicon.DialectLexicon` |  | phase 2 |
| `dialect_lexicon.DialectLexicon.entries` |  | phase 2 |
| `dialect_lexicon.DialectLexicon.get` |  | phase 2 |
| `dialects.THEH` | `arbtok::dialects::THEH` | ported |
| `dialects.THAL` | `arbtok::dialects::THAL` | ported |
| `dialects.ZAH` | `arbtok::dialects::ZAH` | ported |
| `dialects.ARABIC_TO_IPA_CONSONANTS` | `arbtok::dialects::ARABIC_TO_IPA_CONSONANTS` | ported |
| `dialects.DIACRITIC_TO_IPA` | `arbtok::dialects::DIACRITIC_TO_IPA` | ported |
| `dialects.TANWIN_TO_IPA` | `arbtok::dialects::TANWIN_TO_IPA` | ported |
| `dialects.VOWEL_MAP` | `arbtok::dialects::VOWEL_MAP` | ported |
| `dialects.consonant_map` | `arbtok::dialects::consonant_map` | ported |
| `dialects.consonant_ipa` | `arbtok::dialects::consonant_ipa` | ported |
| `dialects.DEFAULT_LANG` | `arbtok::dialects::DEFAULT_LANG` | ported |
| `dialects.spec_for_lang` | `arbtok::dialects::spec_for_lang` | ported |
| `dialects.lect_code` | `arbtok::dialects::lect_code` | ported |
| `dialects.Lect` | `arbtok::dialects::Lect` | ported |
| `dialects.Lect.code` | `arbtok::dialects::Lect::code` | ported |
| `dialects.Lect.tier` | `arbtok::dialects::Lect::tier` | ported |
| `dialects.supported_lects` | `arbtok::dialects::supported_lects` | ported |
| `dialects.WORD_EXCEPTIONS` | `arbtok::dialects::WORD_EXCEPTIONS` | ported |
| `donor_lexicon.BUNDLED_DONOR` |  | phase 2 |
| `donor_lexicon.bundled_path` |  | phase 2 |
| `donor_lexicon.ensure_registered` |  | phase 2 |
| `espeak_wrapper.TextChunks` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.RawPhonemizedChunks` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.PhonemizedChunks` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakError` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.ESPEAK_LANGS` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.get_lang` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.tashkeel` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.phonemize_string` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.phonemize_to_list` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.add_diacritics` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.phonemize` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.match_lang` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.remove_punctuation` |  | not ported: no phoonnx voice path calls it |
| `espeak_wrapper.EspeakPhonemizer.chunk_text` |  | not ported: no phoonnx voice path calls it |
| `fusion.logprobs` |  | phase 2 |
| `fusion.Hypothesis` |  | phase 2 |
| `fusion.FusionDiacritizer` |  | phase 2 |
| `fusion.FusionDiacritizer.diacritizer` |  | phase 2 |
| `fusion.FusionDiacritizer.diacritize` |  | phase 2 |
| `lattice.SunLetterRescorer` | `arbtok::lattice::SunLetterRescorer` | ported |
| `lattice.SunLetterRescorer.rescore` | `arbtok::lattice::SunLetterRescorer::rescore` | ported |
| `lattice.WaslRescorer` | `arbtok::lattice::WaslRescorer` | ported |
| `lattice.WaslRescorer.rescore` | `arbtok::lattice::WaslRescorer::rescore` | ported |
| `lattice.HamzaCarrierRescorer` | `arbtok::lattice::HamzaCarrierRescorer` | ported |
| `lattice.HamzaCarrierRescorer.rescore` | `arbtok::lattice::HamzaCarrierRescorer::rescore` | ported |
| `lattice.ConsonantalGlideRescorer` | `arbtok::lattice::ConsonantalGlideRescorer` | ported |
| `lattice.ConsonantalGlideRescorer.rescore` | `arbtok::lattice::ConsonantalGlideRescorer::rescore` | ported |
| `lattice.AccusativeAlifRescorer` | `arbtok::lattice::AccusativeAlifRescorer` | ported |
| `lattice.AccusativeAlifRescorer.rescore` | `arbtok::lattice::AccusativeAlifRescorer::rescore` | ported |
| `lattice.DEFAULT_RESCORERS` | `arbtok::lattice::default_rescorers()` | ported |
| `lattice.DEFAULT_LANG` | `arbtok::lattice::DEFAULT_LANG` | ported |
| `lattice.word_lattice` | `arbtok::lattice::word_lattice` | ported |
| `lattice.word_ipa` | `arbtok::lattice::word_ipa` | ported |
| `lattice.spec_word_exception` | `arbtok::lattice::spec_word_exception` | ported |
| `lattice.defers_to_cascade` | `arbtok::lattice::defers_to_cascade` | ported |
| `lexicon.LexiconUnavailable` |  | phase 2 |
| `lexicon.DEFAULT_LEXICON` |  | phase 2 |
| `lexicon.CACHE_DIR` |  | phase 2 |
| `lexicon.resolve_source` |  | phase 2 |
| `lexicon.AMBIGUOUS_SHARE` |  | phase 2 |
| `lexicon.MIN_CONTEXT_WORDS` |  | phase 2 |
| `lexicon.parse` |  | phase 2 |
| `lexicon.parse_shares` |  | phase 2 |
| `lexicon.StemLexicon` |  | phase 2 |
| `lexicon.StemLexicon.entries` |  | phase 2 |
| `lexicon.StemLexicon.get` |  | phase 2 |
| `lexicon.StemLexicon.ambiguous` |  | phase 2 |
| `nisba.NOT_NISBA` | `arbtok::nisba::NOT_NISBA` | ported |
| `nisba.restore_nisba` | `arbtok::nisba::restore_nisba` | ported |
| `o2i_plugins.ArbtokDiacritizer` |  | phase 4 |
| `o2i_plugins.ArbtokDiacritizer.PATHS` |  | phase 4 |
| `o2i_plugins.ArbtokDiacritizer.normalize` |  | phase 4 |
| `o2i_plugins.ArbtokDiacritizer.language_codes` |  | phase 4 |
| `o2i_plugins.ArbtokRescorers` |  | phase 4 |
| `o2i_plugins.ArbtokRescorers.rescorers` |  | phase 4 |
| `o2i_plugins.ArbtokRescorers.language_codes` |  | phase 4 |
| `o2i_plugins.ArbtokSandhi` |  | phase 4 |
| `o2i_plugins.ArbtokSandhi.apply` |  | phase 4 |
| `o2i_plugins.ArbtokSandhi.language_codes` |  | phase 4 |
| `orthography.ORTHOGRAPHIC_MARKS` | `arbtok::orthography::ORTHOGRAPHIC_MARKS` | ported |
| `orthography.MADDA` | `arbtok::orthography::MADDA` | ported |
| `orthography.VOCALIC_MARKS` | `arbtok::orthography::VOCALIC_MARKS` | ported |
| `orthography.source_marks` | `arbtok::orthography::source_marks` | ported |
| `orthography.allowed_classes` | `arbtok::orthography::allowed_classes` | ported |
| `orthography.pinned_class` | `arbtok::orthography::pinned_class` | ported |
| `orthography.project_class` | `arbtok::orthography::project_class` | ported |
| `plugin.PUNCT_STRIP` |  | phase 4 |
| `plugin.ArbtokG2PPlugin` |  | phase 4 |
| `plugin.ArbtokG2PPlugin.language_codes` |  | phase 4 |
| `plugin.ArbtokG2PPlugin.normalize` |  | phase 4 |
| `plugin.ArbtokG2PPlugin.transcribe` |  | phase 4 |
| `plugin.ArbtokG2PPlugin.transcribe_word` |  | phase 4 |
| `sandhi.NUN_ASSIMILATION` | `arbtok::sandhi::NUN_ASSIMILATION` | ported |
| `sandhi.apply_cross_word` | `arbtok::sandhi::apply_cross_word` | ported |
| `stress.stress_ipa` | `arbtok::stress::stress_ipa` | ported |
| `stress.stress_words` | `arbtok::stress::stress_words` | ported |
| `tashkeel.TashkeelError` | `arbtok::tashkeel::TashkeelError` | ported |
| `tashkeel.TashkeelDiacritizer` |  | phase 2 |
| `tashkeel.TashkeelDiacritizer.diacritize` |  | phase 2 |
| `textnorm.AsrNorm` | `arbtok::textnorm::AsrNorm` | ported |
| `textnorm.AsrNorm.nfc` | `arbtok::textnorm::AsrNorm::nfc` | ported |
| `textnorm.AsrNorm.strip_event_markup` | `arbtok::textnorm::AsrNorm::strip_event_markup` | ported |
| `textnorm.AsrNorm.strip_controls` | `arbtok::textnorm::AsrNorm::strip_controls` | ported |
| `textnorm.AsrNorm.punctuation_before_marks` | `arbtok::textnorm::AsrNorm::punctuation_before_marks` | ported |
| `textnorm.AsrNorm.strip_harakat` | `arbtok::textnorm::AsrNorm::strip_harakat` | ported |
| `textnorm.AsrNorm.strip_extended_marks` | `arbtok::textnorm::AsrNorm::strip_extended_marks` | ported |
| `textnorm.AsrNorm.strip_quranic_marks` | `arbtok::textnorm::AsrNorm::strip_quranic_marks` | ported |
| `textnorm.AsrNorm.strip_tatweel` | `arbtok::textnorm::AsrNorm::strip_tatweel` | ported |
| `textnorm.AsrNorm.split_term_proclitics` | `arbtok::textnorm::AsrNorm::split_term_proclitics` | ported |
| `textnorm.AsrNorm.fix_asr_errors` | `arbtok::textnorm::AsrNorm::fix_asr_errors` | ported |
| `textnorm.AsrNorm.unify_spoken_variants` | `arbtok::textnorm::AsrNorm::unify_spoken_variants` | ported |
| `textnorm.AsrNorm.unify_dialect_spellings` | `arbtok::textnorm::AsrNorm::unify_dialect_spellings` | ported |
| `textnorm.AsrNorm.number_forms` | `arbtok::textnorm::AsrNorm::number_forms` | ported |
| `textnorm.AsrNorm.spoken_numbers_to_digits` | `arbtok::textnorm::AsrNorm::spoken_numbers_to_digits` | ported |
| `textnorm.AsrNorm.join_dictated_digits` | `arbtok::textnorm::AsrNorm::join_dictated_digits` | ported |
| `textnorm.AsrNorm.spell_codes` | `arbtok::textnorm::AsrNorm::spell_codes` | ported |
| `textnorm.AsrNorm.unify_alef` | `arbtok::textnorm::AsrNorm::unify_alef` | ported |
| `textnorm.AsrNorm.unify_ta_marbuta` | `arbtok::textnorm::AsrNorm::unify_ta_marbuta` | ported |
| `textnorm.AsrNorm.unify_alef_maqsura` | `arbtok::textnorm::AsrNorm::unify_alef_maqsura` | ported |
| `textnorm.AsrNorm.unify_hamza_carriers` | `arbtok::textnorm::AsrNorm::unify_hamza_carriers` | ported |
| `textnorm.AsrNorm.unify_loan_letters` | `arbtok::textnorm::AsrNorm::unify_loan_letters` | ported |
| `textnorm.AsrNorm.fold_digits` | `arbtok::textnorm::AsrNorm::fold_digits` | ported |
| `textnorm.AsrNorm.fold_case` | `arbtok::textnorm::AsrNorm::fold_case` | ported |
| `textnorm.AsrNorm.arabic_block_only` | `arbtok::textnorm::AsrNorm::arabic_block_only` | ported |
| `textnorm.AsrNorm.blank_punctuation` | `arbtok::textnorm::AsrNorm::blank_punctuation` | ported |
| `textnorm.AsrNorm.drop_word_final_hamza` | `arbtok::textnorm::AsrNorm::drop_word_final_hamza` | ported |
| `textnorm.AsrNorm.join_clitics` | `arbtok::textnorm::AsrNorm::join_clitics` | ported |
| `textnorm.AsrNorm.collapse_whitespace` | `arbtok::textnorm::AsrNorm::collapse_whitespace` | ported |
| `textnorm.AsrNorm.lexicon` | `arbtok::textnorm::AsrNorm::lexicon` | ported |
| `textnorm.AsrNorm.with_lexicon` | `arbtok::textnorm::AsrNorm::with_lexicon` | ported |
| `textnorm.AsrNorm.describe` | `arbtok::textnorm::AsrNorm::describe` | ported |
| `textnorm.IDENTIFIER_WORDS` | `arbtok::textnorm::IDENTIFIER_WORDS` | ported |
| `textnorm.ASR_NORM_VERSION` | `arbtok::textnorm::ASR_NORM_VERSION` | ported |
| `textnorm.normalize_asr` | `arbtok::textnorm::normalize_asr` | ported |
| `textnorm.TRUTH_CHECK` | `arbtok::textnorm::TRUTH_CHECK` | ported |
| `textnorm.CER_STRIP` | `arbtok::textnorm::CER_STRIP` | ported |
| `textnorm.CER_NORM` | `arbtok::textnorm::CER_NORM` | ported |
| `textnorm.CER_MARKS_FIRST` | `arbtok::textnorm::CER_MARKS_FIRST` | ported |
| `textnorm.CER_NORM_MARKS_FIRST` | `arbtok::textnorm::CER_NORM_MARKS_FIRST` | ported |
| `textnorm.INTELLIGIBILITY_GATE` | `arbtok::textnorm::INTELLIGIBILITY_GATE` | ported |
| `textnorm.spelled_codes` | `arbtok::textnorm::spelled_codes` | ported |
| `textnorm.cldr_units` | `arbtok::textnorm::cldr_units` | ported |
| `textnorm.bundled_asr_lexicon` | `arbtok::textnorm::bundled_asr_lexicon` | ported |
| `textnorm.bundled_tts_lexicon` | `arbtok::textnorm::bundled_tts_lexicon` | ported |
| `textnorm.canonical_term_lexicon` | `arbtok::textnorm::canonical_term_lexicon` | ported |
| `textnorm.CER_CANONICAL` | `arbtok::textnorm::CER_CANONICAL` | ported |
| `textnorm.ARAB_PHONE_REGIONS` | `arbtok::textnorm::ARAB_PHONE_REGIONS` | ported |
| `textnorm.ARABIC_LANGUAGE_CODES` | `arbtok::textnorm::ARABIC_LANGUAGE_CODES` | ported |
| `textnorm.is_arabic_lang` | `arbtok::textnorm::is_arabic_lang` | ported |
| `textnorm.TtsNorm` | `arbtok::textnorm::TtsNorm` | ported |
| `textnorm.TtsNorm.strip_controls` | `arbtok::textnorm::TtsNorm::strip_controls` | ported |
| `textnorm.TtsNorm.spell_out_plates` | `arbtok::textnorm::TtsNorm::spell_out_plates` | ported |
| `textnorm.TtsNorm.spell_out_codes` | `arbtok::textnorm::TtsNorm::spell_out_codes` | ported |
| `textnorm.TtsNorm.speak_percent` | `arbtok::textnorm::TtsNorm::speak_percent` | ported |
| `textnorm.TtsNorm.keep_code_digits` | `arbtok::textnorm::TtsNorm::keep_code_digits` | ported |
| `textnorm.TtsNorm.phone_shapes` | `arbtok::textnorm::TtsNorm::phone_shapes` | ported |
| `textnorm.TtsNorm.phone_regions` | `arbtok::textnorm::TtsNorm::phone_regions` | ported |
| `textnorm.TtsNorm.long_digit_runs` | `arbtok::textnorm::TtsNorm::long_digit_runs` | ported |
| `textnorm.TtsNorm.phone_prefixes` | `arbtok::textnorm::TtsNorm::phone_prefixes` | ported |
| `textnorm.TtsNorm.identifier_words` | `arbtok::textnorm::TtsNorm::identifier_words` | ported |
| `textnorm.TtsNorm.cardinal_numbers` | `arbtok::textnorm::TtsNorm::cardinal_numbers` | ported |
| `textnorm.TtsNorm.colloquial_clock` | `arbtok::textnorm::TtsNorm::colloquial_clock` | ported |
| `textnorm.TtsNorm.colloquial_years` | `arbtok::textnorm::TtsNorm::colloquial_years` | ported |
| `textnorm.TtsNorm.colloquial_units` | `arbtok::textnorm::TtsNorm::colloquial_units` | ported |
| `textnorm.TtsNorm.leave_unspeakable_numbers` | `arbtok::textnorm::TtsNorm::leave_unspeakable_numbers` | ported |
| `textnorm.TtsNorm.oblique_numbers` | `arbtok::textnorm::TtsNorm::oblique_numbers` | ported |
| `textnorm.TtsNorm.space_fused_hundreds` | `arbtok::textnorm::TtsNorm::space_fused_hundreds` | ported |
| `textnorm.TtsNorm.dialect_numbers` | `arbtok::textnorm::TtsNorm::dialect_numbers` | ported |
| `textnorm.TtsNorm.number_forms` | `arbtok::textnorm::TtsNorm::number_forms` | ported |
| `textnorm.TtsNorm.month_names` | `arbtok::textnorm::TtsNorm::month_names` | ported |
| `textnorm.TtsNorm.spoken_forms` | `arbtok::textnorm::TtsNorm::spoken_forms` | ported |
| `textnorm.TtsNorm.canonical_unicode` | `arbtok::textnorm::TtsNorm::canonical_unicode` | ported |
| `textnorm.TtsNorm.drop_false_starts` | `arbtok::textnorm::TtsNorm::drop_false_starts` | ported |
| `textnorm.TtsNorm.plain_orthography` | `arbtok::textnorm::TtsNorm::plain_orthography` | ported |
| `textnorm.TtsNorm.lexicon` | `arbtok::textnorm::TtsNorm::lexicon` | ported |
| `textnorm.TtsNorm.with_lexicon` | `arbtok::textnorm::TtsNorm::with_lexicon` | ported |
| `textnorm.TtsNorm.with_number_forms` | `arbtok::textnorm::TtsNorm::with_number_forms` | ported |
| `textnorm.TtsNorm.describe` | `arbtok::textnorm::TtsNorm::describe` | ported |
| `textnorm.TTS_NORM_VERSION` | `arbtok::textnorm::TTS_NORM_VERSION` | ported |
| `textnorm.normalize_for_tts` | `arbtok::textnorm::normalize_for_tts` | ported |
| `tokenizer.elide_silent_alif` | `arbtok::tokenizer::elide_silent_alif` | ported |
| `tokenizer.normalize_unicode` | `arbtok::tokenizer::normalize_unicode` | ported |
| `tokenizer.CharToken` | `arbtok::tokenizer::CharToken` | ported |
| `tokenizer.CharToken.surface` | `arbtok::tokenizer::CharToken::surface` | ported |
| `tokenizer.CharToken.char_idx` | `arbtok::tokenizer::CharToken::char_idx` | ported |
| `tokenizer.CharToken.prev_token` | `arbtok::tokenizer::CharToken::prev_token` | ported |
| `tokenizer.CharToken.next_token` | `arbtok::tokenizer::CharToken::next_token` | ported |
| `tokenizer.CharToken.word` | `arbtok::tokenizer::CharToken::word` | ported |
| `tokenizer.CharToken.is_first_char` | `arbtok::tokenizer::CharToken::is_first_char` | ported |
| `tokenizer.CharToken.is_first_word` | `arbtok::tokenizer::CharToken::is_first_word` | ported |
| `tokenizer.CharToken.is_last_char` | `arbtok::tokenizer::CharToken::is_last_char` | ported |
| `tokenizer.CharToken.is_last_word` | `arbtok::tokenizer::CharToken::is_last_word` | ported |
| `tokenizer.CharToken.is_punct` | `arbtok::tokenizer::CharToken::is_punct` | ported |
| `tokenizer.CharToken.lang` | `arbtok::tokenizer::CharToken::lang` | ported |
| `tokenizer.CharToken.normalized` | `arbtok::tokenizer::CharToken::normalized` | ported |
| `tokenizer.CharToken.is_vowel` | `arbtok::tokenizer::CharToken::is_vowel` | ported |
| `tokenizer.CharToken.has_diacritic` | `arbtok::tokenizer::CharToken::has_diacritic` | ported |
| `tokenizer.CharToken.has_shada` | `arbtok::tokenizer::CharToken::has_shada` | ported |
| `tokenizer.CharToken.has_sukun` | `arbtok::tokenizer::CharToken::has_sukun` | ported |
| `tokenizer.CharToken.has_vowel_mark` | `arbtok::tokenizer::CharToken::has_vowel_mark` | ported |
| `tokenizer.CharToken.is_sun` | `arbtok::tokenizer::CharToken::is_sun` | ported |
| `tokenizer.CharToken.is_moon` | `arbtok::tokenizer::CharToken::is_moon` | ported |
| `tokenizer.CharToken.is_silent` | `arbtok::tokenizer::CharToken::is_silent` | ported |
| `tokenizer.CharToken.is_tanwin` | `arbtok::tokenizer::CharToken::is_tanwin` | ported |
| `tokenizer.CharToken.ipa` | `arbtok::tokenizer::CharToken::ipa` | ported |
| `tokenizer.WordToken` | `arbtok::tokenizer::WordToken` | ported |
| `tokenizer.WordToken.surface` | `arbtok::tokenizer::WordToken::surface` | ported |
| `tokenizer.WordToken.word_idx` | `arbtok::tokenizer::WordToken::word_idx` | ported |
| `tokenizer.WordToken.prev_word` | `arbtok::tokenizer::WordToken::prev_word` | ported |
| `tokenizer.WordToken.next_word` | `arbtok::tokenizer::WordToken::next_word` | ported |
| `tokenizer.WordToken.lang` | `arbtok::tokenizer::WordToken::lang` | ported |
| `tokenizer.WordToken.is_punct` | `arbtok::tokenizer::WordToken::is_punct` | ported |
| `tokenizer.WordToken.is_first_word` | `arbtok::tokenizer::WordToken::is_first_word` | ported |
| `tokenizer.WordToken.is_last_word` | `arbtok::tokenizer::WordToken::is_last_word` | ported |
| `tokenizer.WordToken.end_with_vowel` | `arbtok::tokenizer::WordToken::end_with_vowel` | ported |
| `tokenizer.WordToken.normalized` | `arbtok::tokenizer::WordToken::normalized` | ported |
| `tokenizer.WordToken.tokens` | `arbtok::tokenizer::WordToken::tokens` | ported |
| `tokenizer.WordToken.has_definite_article` | `arbtok::tokenizer::WordToken::has_definite_article` | ported |
| `tokenizer.WordToken.is_proclitic` | `arbtok::tokenizer::WordToken::is_proclitic` | ported |
| `tokenizer.WordToken.is_sun` | `arbtok::tokenizer::WordToken::is_sun` | ported |
| `tokenizer.WordToken.is_reference_register` | `arbtok::tokenizer::WordToken::is_reference_register` | ported |
| `tokenizer.WordToken.ipa` | `arbtok::tokenizer::WordToken::ipa` | ported |
| `tokenizer.Sentence` | `arbtok::tokenizer::Sentence` | ported |
| `tokenizer.Sentence.surface` | `arbtok::tokenizer::Sentence::surface` | ported |
| `tokenizer.Sentence.lang` | `arbtok::tokenizer::Sentence::lang` | ported |
| `tokenizer.Sentence.stress` | `arbtok::tokenizer::Sentence::stress` | ported |
| `tokenizer.Sentence.pausal` | `arbtok::tokenizer::Sentence::pausal` | ported |
| `tokenizer.Sentence.normalized` | `arbtok::tokenizer::Sentence::normalized` | ported |
| `tokenizer.Sentence.tokens` | `arbtok::tokenizer::Sentence::tokens` | ported |
| `tokenizer.Sentence.ipa` | `arbtok::tokenizer::Sentence::ipa` | ported |
| `translit.DONOR_LANG` |  | phase 4 |
| `translit.DONOR_BY_SCRIPT` |  | phase 4 |
| `translit.SEGMENT_MAP` |  | phase 4 |
| `translit.nativization_table` |  | phase 4 |
| `translit.ESTABLISHED_LOANS` |  | phase 4 |
| `translit.is_latin` |  | phase 4 |
| `translit.segment_ipa` |  | phase 4 |
| `translit.guest_script` |  | phase 4 |
| `translit.nativize` |  | phase 4 |
| `translit.transliterate` |  | phase 4 |
| `unicode_symbol2label.UNICODE_SYMBOL_TO_LABEL` | `arbtok::unicode_symbol2label::items(), get()` | ported |
| `util.LOG` |  | not ported: the Python logger; the port writes no log |
| `util.match_lang` | `arbtok::util::match_lang` | ported |
| `util.CONTRACTIONS` | `arbtok::util::CONTRACTIONS` | ported |
| `util.TITLES` | `arbtok::util::TITLES` | ported |
| `util.UNITS` | `arbtok::util::UNITS` | ported |
| `util.resolve_thousands_or_decimal` | `arbtok::util::resolve_thousands_or_decimal` | ported |
| `util.pronounce_date` | `arbtok::util::pronounce_date` | ported |
| `util.pronounce_time` | `arbtok::util::pronounce_time` | ported |
| `util.is_fraction` | `arbtok::util::is_fraction` | ported |
| `util.normalize` | `arbtok::util::normalize` | ported |
| `version.VERSION_MAJOR` | `arbtok::version::VERSION_MAJOR` | ported |
| `version.VERSION_MINOR` | `arbtok::version::VERSION_MINOR` | ported |
| `version.VERSION_BUILD` | `arbtok::version::VERSION_BUILD` | ported |
| `version.VERSION_ALPHA` | `arbtok::version::VERSION_ALPHA` | ported |
| `version.VERSION_STR` | `arbtok::version::VERSION_STR` | ported |
| `waqf.FATHA` | `arbtok::waqf::FATHA` | ported |
| `waqf.DAMMA` | `arbtok::waqf::DAMMA` | ported |
| `waqf.KASRA` | `arbtok::waqf::KASRA` | ported |
| `waqf.SUKUN` | `arbtok::waqf::SUKUN` | ported |
| `waqf.SHADDA` | `arbtok::waqf::SHADDA` | ported |
| `waqf.TANWIN_FATH` | `arbtok::waqf::TANWIN_FATH` | ported |
| `waqf.TANWIN_DAMM` | `arbtok::waqf::TANWIN_DAMM` | ported |
| `waqf.TANWIN_KASR` | `arbtok::waqf::TANWIN_KASR` | ported |
| `waqf.TA_MARBUTA` | `arbtok::waqf::TA_MARBUTA` | ported |
| `waqf.SHORT_VOWELS` | `arbtok::waqf::SHORT_VOWELS` | ported |
| `waqf.DROPPED_TANWIN` | `arbtok::waqf::DROPPED_TANWIN` | ported |
| `waqf.KEPT` | `arbtok::waqf::KEPT` | ported |
| `waqf.LEXICAL_FINAL_VOWEL` | `arbtok::waqf::LEXICAL_FINAL_VOWEL` | ported |
| `waqf.pausal` | `arbtok::waqf::pausal` | ported |
<!-- inventory:end -->

## Goldens

A golden file is `tests/goldens/<module>.jsonl.gz`. Its first line is a header with the reference versions, the gold commit and the count per corpus. Each other line is one record, `[function, arguments..., result]`, in compact JSON. An exception is `{"err": "<Python class>"}`. A float is its Python `repr` string. The header holds the input count of each corpus. `tools/dump_goldens.py` prints the input count and the record count of each corpus. After it writes a file, it counts the records in the file again. It stops when that count is not the sum of the input counts. `arbtok_parity` fails when a file does not hold the record count of its header.

| Module | Corpora | Records |
| --- | --- | --- |
| arabizi | constants 1, is_arabizi 35040, arabizi_covers 11680, to_arabic_skeleton 11680 | 58401 |
| constants | constants 42 | 42 |
| dialects | constants 14, spec_for_lang 30882, lect_code 30881, o2i_calls 44036, supported_lects 1, arabic_codes 1, dialect_index 1, is_arabic_code 7671, core_subtags 30880, consonant_map 58, consonant_ipa 2610, spec 58 | 147093 |
| lattice | constants 2, defers_to_cascade 5186, word_ipa 16616, word_lattice 2770, spec_word_exception 3832 | 28406 |
| nisba | constants 1, restore_nisba 9581 | 9582 |
| orthography | constants 5, allowed_classes 354, pinned_class 177, project_class 17374, source_marks 21567 | 39477 |
| pyre | patterns 230, finditer 219420, fullmatch 43984, match 52344, search 19629 | 335607 |
| sandhi | constants 1, apply_cross_word 30725, pausal 3465, has_article 394, is_bare_wasl_alif 394 | 34979 |
| stress | first_segment_len 6077, stress_ipa 190275, stress_words 6078 | 202430 |
| tashkeel | messages 54 | 54 |
| textnorm | const 1, is_arabic_lang 20860, constants 11, tables 65, describe 227, validation 46, normalize_asr 185506, normalize_for_tts 127569 | 334285 |
| tokenizer | normalize_unicode 7761, elide_silent_alif 7761, reorder_diacritics 7761, sentence 21624, words 5406 | 50313 |
| unicode_symbol2label | items 4171, size 1, lookup 57 | 4229 |
| util | constants 3, normalize 14070, match_lang 646, resolve_thousands_or_decimal 123, is_fraction 41, pronounce_date 3256, pronounce_time 216 | 18355 |
| version | constants 5 | 5 |
| waqf | constants 13, pausal 26936 | 26949 |

The inputs are the string literals of the sdist tests, the gold sentences, the bundled lexicon words, the word exceptions of every Arabic spec, a set of Unicode probes, and generated grids. The docstring of each `tools/goldens/<module>.py` names the inputs of that module.

The `o2i_calls` corpus records each call that `dialects` makes into orthography2ipa (`resolve`, `resolves_exactly`) and into langcodes (`closest_match`), with the arguments that arbtok passes. The C++ resolution layer is checked on the inputs it receives.

The `pyre` records check the regex engine of the port against Python `re`. They hold each compiled pattern of `textnorm` and `util`, the patterns the two modules compile at call time, and the pattern strings they pass to `re` functions, with the result of `finditer`, `fullmatch`, `match` at a position and `search` up to an end position. Offsets are in code points.

Each golden suite has a mutation control: one rule is broken in the C++, and the suite must fail. The table gives the mismatches that each mutation causes.

| Suite | Mutation | Mismatches |
| --- | --- | --- |
| constants | KASRA holds the fatha | 1 of 42 |
| orthography | pinned_class gives 0 for no class, not -1 | 8186 of 39477 |
| tashkeel | TashkeelError names another Python class | 54 of 54 |
| nisba | the shortest nisba skeleton is 4 letters | 42 of 9582 |
| waqf | phrase_final_only is inverted | 3564 of 26949 |
| arabizi | the digit 9 does not mark Arabizi | 166 of 58401 |
| unicode_symbol2label | items() gives the symbol as its own label | 4179 of 4229 |
| textnorm | mey is not an Arabic language | 4854 of 334285 |
| textnorm | ماية before 100, not before the thousands word, is the hundred | 6 of 334285 |
| textnorm | ماية after و is the hundred after a value from 100, not from 1000 | 8 of 334285 |
| textnorm | a spoken date refuses the last day of a month | 93 of 334285 |
| util | match_lang refuses a language at distance 10 | 4 of 18355 |
| pyre | `\D` matches the decimal digits | 953 of 335607 |
| dialects | `split("-x-")[-1]` keeps the hyphen | 2656 of 147093, in each of the 8 shards |
| stress | a negative proclitic_onset cuts one code point late | 6742 of 202430 |
| sandhi | the pausal form applies when pausal is False | 310 of 34979 |
| lattice | a kept alternative costs at least 0.5, not 1.0 | 87 of 28406 |
| tokenizer | a kasra before a shadda is not reordered | 1154 of 50313 |
| version | VERSION_ALPHA is 70 | 1 of 5 |

## Python semantics that the port keeps

- The `str` methods (`lower`, `isalpha`, `isdigit`, `isspace` and the others the rules call) use tables that `tools/gen_unicode_tables.py` generates from the reference interpreter, so they follow Unicode 15.0.0 as Python 3.12 does. `tools/gen_unicode_norm_tables.py` does the same for NFC and NFD.
- A regex alternation tries its alternatives in Python order. A pattern is never replaced by a longest-match lexer.
- `$` in a Python `re` pattern also matches before a final `"\n"`. The port reproduces this where a rule ends in `$`.
- Slices with a negative or an out-of-range index follow Python slice rules.
- The goldens write a Python `set` sorted, because its iteration order follows the hash seed. A `dict` keeps its insertion order. A dict whose order comes from a spec file is written as sorted items.
- A float is written in Python `repr` form, and the build sets `-ffp-contract=off`, so that a fused multiply-add cannot change a result.
- `warnings.warn` becomes a call to the handler that `arbtok::set_warning_handler` installs. With no handler, a warning is dropped.
- `tools/gen_textnorm_tables.py` writes each module-level pattern, string and table of `textnorm` and `util` as the imported module holds it. A name that the module defines twice, as `_MONTH_NAMES`, has the value of its second definition. The patterns run on `src/pyre.cpp`, which follows Python `re` for the syntax and the flags that these patterns use.
- A `functools.lru_cache` of `textnorm` becomes a cache per thread. A cache changes only the speed.
- An `except Exception` of `textnorm` or `util` catches the `arbtok::Error` classes of the port, except `arbtok::NotImplementedError`. The port raises that class where Python speaks a number in a language other than Arabic, so the gap stays visible and does not become a different result.

## Data loading

`data/unicode_symbol2label.json` holds the table that `unicode_symbol2label.py` defines inline. `tools/gen_symbol_labels.py` writes it from the reference package and reads it back to check that the round trip gives the same dict, in the same order. `tests/data/rawi_v2.vocab.json` is a verbatim copy of the vocabulary file that the wheel bundles. The orthography goldens use it as an input.

The library finds its data directory in this order: `arbtok::set_data_dir()`, the `ARBTOK_DATA_DIR` environment variable, the `share/arbtok` directory of the install that holds the library or the program, and the install location compiled into the library. The orthography2ipa specs come from the orthography2ipa.cpp install.

## orthography2ipa.cpp gaps

These are the places where orthography2ipa.cpp at the pinned commit has no equivalent of a Python name that arbtok calls, or gives a different result. arbtok.cpp does not patch the submodule.

| Python name | Gap | What arbtok.cpp does |
| --- | --- | --- |
| `orthography2ipa.resolves_exactly` | no C++ function | `arbtok::dialects::detail::resolves_exactly` ports `_resolve_code` with `allow_nearest=False`, on the submodule's `src/registry_tables.inc` |
| `langcodes.closest_match` | no C++ function | `arbtok::dialects::detail::closest_match` ports langcodes 3.5.1 on orthography2ipa.cpp's `langcodes::tag_distance` |
| `orthography2ipa.phonetok._expand_arabic_gemination` | internal to the library, not in a header | `src/gemination.cpp` ports it |
| `orthography2ipa.rescorer` | `rescorer.hpp` uses `AllophoneRule` but does not include its declaration | `lattice.hpp` includes `orthography2ipa.hpp` first |
| `LanguageSpec.iso639_3` | a JSON `null` loads as the string `"null"` | `dialects` reads `""` and `"null"` as None |
| `orthography2ipa.registry.UnknownVariantWarning` | `resolve` issues no warning | none needed: arbtok records only `UserWarning` itself, not its subclasses |
| `orthography2ipa.resolve` | no result cache. A nearest-match resolve costs about 43 ms | `dialects` caches `lect_code` and `is_arabic_code` per tag, as the Python `lru_cache` does |

## Expected divergences

`tests/parity/expected_divergences.tsv` lists each golden record that differs because of a gap above. Each entry gives the golden line, the C++ line that the record gives, and the fix it waits on. A listed record that gives another C++ line is a mismatch. A listed record that starts to match Python fails the suite. A listed record that is not in the goldens also fails it, so the list cannot go stale. The list holds no entry: every golden record gives the bytes of Python.

## espeak_wrapper and plugin

`espeak_wrapper.py` is not ported. No phoonnx voice path calls it:

- phoonnx at `origin/dev` `f185255` has no reference to `espeak_wrapper`. Its eSpeak path is `scriptconv.phonemizers.mul.EspeakPhonemizer` (`phoonnx/engines/neutts.py:277`).
- phoonnx gets its Arabic phonemizer from the scriptconv registry (`phoonnx/config.py:570` and `:604`). scriptconv at `origin/dev` `bc7d842` maps Arabic to `ArbtokPhonemizer`, which imports `arbtok.plugin.ArbtokG2PPlugin` (`phonemizers/ar.py:245`) and `arbtok.spec_for_lang` (`phonemizers/ar.py:267`). scriptconv has no reference to `espeak_wrapper`.
- No module of arbtok imports `espeak_wrapper`, and no package in the reference environment imports it.

`plugin.py` holds `ArbtokG2PPlugin`, the grapheme-to-phoneme class that scriptconv calls. It has no OVOS plugin glue. It is on the phoonnx voice path, so phase 4 ports it.

## Paths the goldens do not reach

- No golden input makes `spec_for_lang` issue a `UserWarning`, so the warning path of `dialects` is not checked against Python. The C++ issues the warning through the handler in the same case as Python.
- The sanitizer build checks every 20th record of each suite and every 160th dialects record.
- The number words, ordinals, dates and times of `src/numbers` are the Arabic ones. A `textnorm` or `util` call that speaks or reads a number in another language raises `arbtok::NotImplementedError`. The dumper leaves out the 19 recorded calls of that kind (1 ASR call, 18 TTS calls), and it keeps a recorded `util` call only when its language is Arabic or its text has no digit and no word that `float()` reads.
- The `pyre` records of a call-time pattern give the C++ the pattern text from the golden. They check the engine on that pattern, not that the C++ builds the same text. The `textnorm` and `util` records check the built patterns end to end.
- The 9351 distinct texts of the recorded calls run only with the config and the language of their call. A TTS config that turns on a rule of `_ARABIC_ONLY` runs with "en" on 20 texts.
- `util.LOG` is not ported. The Python logs a line where a date or a time does not parse. The port gives the same result and writes no log.
