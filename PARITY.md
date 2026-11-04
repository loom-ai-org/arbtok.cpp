# Parity with arbtok

arbtok.cpp gives the same bytes as one release of the Python package for each name it ports. This file names that release, lists the whole public surface of the package with the status of each name, and records what the port depends on and where it differs.

## Reference

The goldens are dumped from these versions. Each golden file names them in its header. `tools/dump_goldens.py` pins the five packages and stops when an installed one has another version. The linguatag, langcodes and ovos-spec-tools versions are the ones that resolve with the first two under `uv --prerelease=allow`. linguatag and langcodes decide how a language tag resolves. Other versions can change the dialects records.

| Package | Version |
| --- | --- |
| arbtok | 0.0.2a71 (PyPI) |
| orthography2ipa | 7.114.16a1 |
| langcodes | 3.5.1 |
| linguatag | 0.1.5a1 |
| ovos-spec-tools | 1.17.1a1 |
| Python | 3.12.13 (Unicode 15.0.0) |

The other packages that resolve with these under `uv --prerelease=allow` are: chronologia 0.31.7a2, combo_lock 0.3.1, dateparser 1.5.0, json_database 1.0.5a1, numpy 2.5.4, onnxruntime 1.31.1, ovos-config 3.9.0a2, ovos-date-parser 0.35.7a1, ovos-number-parser 0.24.3a1, ovos-utils 0.15.7a2, quebra-frases 0.3.8a1, regex 2026.9.29, unicode-rbnf 2.4.1, and their own dependencies.

The gold sets come from the arbtok repository at commit `a95668cf097c5fcfb72b7ab75d7346579626e7a5`, whose `version.py` is 0.0.2a71. The other test inputs come from the `tests/` directory of the 0.0.2a71 sdist and from the data that the installed wheel bundles.

orthography2ipa.cpp is the submodule `third_party/orthography2ipa.cpp`, pinned to `5d2343ad564aa33ac6633011a6b0897354651ebb` on its `correct-port` branch. That commit holds parity with orthography2ipa 7.114.16a1.

## Public surface

`tools/inventory.py` reads the surface from the installed package with `ast` and writes the table below. It exits with 1 when a name marked as ported does not occur in its header.

arbtok has no command-line interface. Its only entry points are the three orthography2ipa plugins of `o2i_plugins` (normalize, rescore and sandhi), which phase 4 ports.

<!-- inventory:begin -->
362 public symbols, 228 ported.

The count covers 25 modules and the package itself: each module-level name, each public class member, and each name of the package `__all__`.

| Python name | C++ name | Status |
| --- | --- | --- |
| `arbtok.AsrNorm` |  | phase 3 |
| `arbtok.Lect` | `arbtok::dialects::Lect` | ported |
| `arbtok.TtsNorm` |  | phase 3 |
| `arbtok.__version__` | `arbtok::version::VERSION_STR` | ported |
| `arbtok.is_arabic_lang` | `arbtok::textnorm::is_arabic_lang` | ported |
| `arbtok.normalize_asr` |  | phase 3 |
| `arbtok.normalize_for_tts` |  | phase 3 |
| `arbtok.spec_for_lang` | `arbtok::dialects::spec_for_lang` | ported |
| `arbtok.supported_lects` | `arbtok::dialects::supported_lects` | ported |
| `arbtok.vocalize` | `arbtok::vocalize` | ported |
| `_ensemble.DEFAULT_ENSEMBLE_ONNX` | `arbtok::ensemble::default_ensemble_onnx()` | ported |
| `_ensemble.DEFAULT_VOCAB` | `arbtok::ensemble::default_vocab()` | ported |
| `_ensemble.EnsembleDiacritizer` | `arbtok::ensemble::EnsembleDiacritizer` | ported |
| `_ensemble.EnsembleDiacritizer.classes` | `arbtok::ensemble::EnsembleDiacritizer::classes` | ported |
| `_ensemble.EnsembleDiacritizer.logits` | `arbtok::ensemble::EnsembleDiacritizer::logits` | ported |
| `_ensemble.EnsembleDiacritizer.decode` | `arbtok::ensemble::EnsembleDiacritizer::decode` | ported |
| `_ensemble.EnsembleDiacritizer.diacritize` | `arbtok::ensemble::EnsembleDiacritizer::diacritize` | ported |
| `_ensemble.get_ensemble` | `arbtok::ensemble::get_ensemble` | ported |
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
| `diacritize.strip_marks` | `arbtok::diacritize::strip_marks` | ported |
| `diacritize.repair_skeleton` | `arbtok::diacritize::repair_skeleton` | ported |
| `diacritize.PROVENANCE` | `arbtok::diacritize::PROVENANCE` | ported |
| `diacritize.DiacritizedWord` | `arbtok::diacritize::DiacritizedWord` | ported |
| `diacritize.DiacritizedWord.surface` | `arbtok::diacritize::DiacritizedWord::surface` | ported |
| `diacritize.DiacritizedWord.output` | `arbtok::diacritize::DiacritizedWord::output` | ported |
| `diacritize.DiacritizedWord.provenance` | `arbtok::diacritize::DiacritizedWord::provenance` | ported |
| `diacritize.DiacritizedWord.lect_constrained` | `arbtok::diacritize::DiacritizedWord::lect_constrained` | ported |
| `diacritize.DiacritizedWord.is_failure` | `arbtok::diacritize::DiacritizedWord::is_failure` | ported |
| `diacritize.DiacritizedText` | `arbtok::diacritize::DiacritizedText` | ported |
| `diacritize.DiacritizedText.text` | `arbtok::diacritize::DiacritizedText::text` | ported |
| `diacritize.DiacritizedText.words` | `arbtok::diacritize::DiacritizedText::words` | ported |
| `diacritize.DiacritizedText.by_provenance` | `arbtok::diacritize::DiacritizedText::by_provenance` | ported |
| `diacritize.LatticeDiacritizer` | `arbtok::diacritize::LatticeDiacritizer` | ported |
| `diacritize.LatticeDiacritizer.diacritizer` | `arbtok::diacritize::LatticeDiacritizer::diacritizer` | ported |
| `diacritize.LatticeDiacritizer.diacritize_word` | `arbtok::diacritize::LatticeDiacritizer::diacritize_word` | ported |
| `diacritize.LatticeDiacritizer.diacritize_word_record` | `arbtok::diacritize::LatticeDiacritizer::diacritize_word_record` | ported |
| `diacritize.LatticeDiacritizer.diacritize` | `arbtok::diacritize::LatticeDiacritizer::diacritize` | ported |
| `diacritize.LatticeDiacritizer.diacritize_text` | `arbtok::diacritize::LatticeDiacritizer::diacritize_text` | ported |
| `diacritize.diacritize` | `arbtok::diacritize::diacritize` | ported |
| `dialect_lexicon.lexicon_dir` | `arbtok::dialect_lexicon::lexicon_dir` | ported |
| `dialect_lexicon.lexicon_file` | `arbtok::dialect_lexicon::lexicon_file` | ported |
| `dialect_lexicon.available_lects` | `arbtok::dialect_lexicon::available_lects` | ported |
| `dialect_lexicon.parse` | `arbtok::dialect_lexicon::parse` | ported |
| `dialect_lexicon.DialectLexicon` | `arbtok::dialect_lexicon::DialectLexicon` | ported |
| `dialect_lexicon.DialectLexicon.entries` | `arbtok::dialect_lexicon::DialectLexicon::entries` | ported |
| `dialect_lexicon.DialectLexicon.get` | `arbtok::dialect_lexicon::DialectLexicon::get` | ported |
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
| `donor_lexicon.BUNDLED_DONOR` | `arbtok::donor_lexicon::BUNDLED_DONOR` | ported |
| `donor_lexicon.bundled_path` | `arbtok::donor_lexicon::bundled_path` | ported |
| `donor_lexicon.ensure_registered` | `arbtok::donor_lexicon::ensure_registered` | ported |
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
| `fusion.logprobs` | `arbtok::fusion::logprobs` | ported |
| `fusion.Hypothesis` | `arbtok::fusion::Hypothesis` | ported |
| `fusion.FusionDiacritizer` | `arbtok::fusion::FusionDiacritizer` | ported |
| `fusion.FusionDiacritizer.diacritizer` | `arbtok::fusion::FusionDiacritizer::diacritizer` | ported |
| `fusion.FusionDiacritizer.diacritize` | `arbtok::fusion::FusionDiacritizer::diacritize` | ported |
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
| `lexicon.LexiconUnavailable` | `arbtok::lexicon::LexiconUnavailable` | ported |
| `lexicon.DEFAULT_LEXICON` | `arbtok::lexicon::DEFAULT_LEXICON` | ported |
| `lexicon.CACHE_DIR` | `arbtok::lexicon::cache_dir()` | ported |
| `lexicon.resolve_source` | `arbtok::lexicon::resolve_source` | ported |
| `lexicon.AMBIGUOUS_SHARE` | `arbtok::lexicon::AMBIGUOUS_SHARE` | ported |
| `lexicon.MIN_CONTEXT_WORDS` | `arbtok::lexicon::MIN_CONTEXT_WORDS` | ported |
| `lexicon.parse` | `arbtok::lexicon::parse` | ported |
| `lexicon.parse_shares` | `arbtok::lexicon::parse_shares` | ported |
| `lexicon.StemLexicon` | `arbtok::lexicon::StemLexicon` | ported |
| `lexicon.StemLexicon.entries` | `arbtok::lexicon::StemLexicon::entries` | ported |
| `lexicon.StemLexicon.get` | `arbtok::lexicon::StemLexicon::get` | ported |
| `lexicon.StemLexicon.ambiguous` | `arbtok::lexicon::StemLexicon::ambiguous` | ported |
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
| `tashkeel.TashkeelDiacritizer` | `arbtok::tashkeel::TashkeelDiacritizer` | ported |
| `tashkeel.TashkeelDiacritizer.diacritize` | `arbtok::tashkeel::TashkeelDiacritizer::diacritize` | ported |
| `textnorm.AsrNorm` |  | phase 3 |
| `textnorm.AsrNorm.nfc` |  | phase 3 |
| `textnorm.AsrNorm.strip_event_markup` |  | phase 3 |
| `textnorm.AsrNorm.strip_controls` |  | phase 3 |
| `textnorm.AsrNorm.punctuation_before_marks` |  | phase 3 |
| `textnorm.AsrNorm.strip_harakat` |  | phase 3 |
| `textnorm.AsrNorm.strip_extended_marks` |  | phase 3 |
| `textnorm.AsrNorm.strip_quranic_marks` |  | phase 3 |
| `textnorm.AsrNorm.strip_tatweel` |  | phase 3 |
| `textnorm.AsrNorm.split_term_proclitics` |  | phase 3 |
| `textnorm.AsrNorm.fix_asr_errors` |  | phase 3 |
| `textnorm.AsrNorm.unify_spoken_variants` |  | phase 3 |
| `textnorm.AsrNorm.unify_dialect_spellings` |  | phase 3 |
| `textnorm.AsrNorm.number_forms` |  | phase 3 |
| `textnorm.AsrNorm.spoken_numbers_to_digits` |  | phase 3 |
| `textnorm.AsrNorm.join_dictated_digits` |  | phase 3 |
| `textnorm.AsrNorm.spell_codes` |  | phase 3 |
| `textnorm.AsrNorm.unify_alef` |  | phase 3 |
| `textnorm.AsrNorm.unify_ta_marbuta` |  | phase 3 |
| `textnorm.AsrNorm.unify_alef_maqsura` |  | phase 3 |
| `textnorm.AsrNorm.unify_hamza_carriers` |  | phase 3 |
| `textnorm.AsrNorm.unify_loan_letters` |  | phase 3 |
| `textnorm.AsrNorm.fold_digits` |  | phase 3 |
| `textnorm.AsrNorm.fold_case` |  | phase 3 |
| `textnorm.AsrNorm.arabic_block_only` |  | phase 3 |
| `textnorm.AsrNorm.blank_punctuation` |  | phase 3 |
| `textnorm.AsrNorm.drop_word_final_hamza` |  | phase 3 |
| `textnorm.AsrNorm.join_clitics` |  | phase 3 |
| `textnorm.AsrNorm.collapse_whitespace` |  | phase 3 |
| `textnorm.AsrNorm.lexicon` |  | phase 3 |
| `textnorm.AsrNorm.with_lexicon` |  | phase 3 |
| `textnorm.AsrNorm.describe` |  | phase 3 |
| `textnorm.IDENTIFIER_WORDS` |  | phase 3 |
| `textnorm.ASR_NORM_VERSION` |  | phase 3 |
| `textnorm.normalize_asr` |  | phase 3 |
| `textnorm.TRUTH_CHECK` |  | phase 3 |
| `textnorm.CER_STRIP` |  | phase 3 |
| `textnorm.CER_NORM` |  | phase 3 |
| `textnorm.CER_MARKS_FIRST` |  | phase 3 |
| `textnorm.CER_NORM_MARKS_FIRST` |  | phase 3 |
| `textnorm.INTELLIGIBILITY_GATE` |  | phase 3 |
| `textnorm.spelled_codes` |  | phase 3 |
| `textnorm.cldr_units` |  | phase 3 |
| `textnorm.bundled_asr_lexicon` |  | phase 3 |
| `textnorm.bundled_tts_lexicon` |  | phase 3 |
| `textnorm.canonical_term_lexicon` |  | phase 3 |
| `textnorm.CER_CANONICAL` |  | phase 3 |
| `textnorm.ARAB_PHONE_REGIONS` |  | phase 3 |
| `textnorm.ARABIC_LANGUAGE_CODES` | `arbtok::textnorm::ARABIC_LANGUAGE_CODES` | ported |
| `textnorm.is_arabic_lang` | `arbtok::textnorm::is_arabic_lang` | ported |
| `textnorm.TtsNorm` |  | phase 3 |
| `textnorm.TtsNorm.strip_controls` |  | phase 3 |
| `textnorm.TtsNorm.spell_out_plates` |  | phase 3 |
| `textnorm.TtsNorm.spell_out_codes` |  | phase 3 |
| `textnorm.TtsNorm.speak_percent` |  | phase 3 |
| `textnorm.TtsNorm.keep_code_digits` |  | phase 3 |
| `textnorm.TtsNorm.phone_shapes` |  | phase 3 |
| `textnorm.TtsNorm.phone_regions` |  | phase 3 |
| `textnorm.TtsNorm.long_digit_runs` |  | phase 3 |
| `textnorm.TtsNorm.phone_prefixes` |  | phase 3 |
| `textnorm.TtsNorm.identifier_words` |  | phase 3 |
| `textnorm.TtsNorm.cardinal_numbers` |  | phase 3 |
| `textnorm.TtsNorm.colloquial_clock` |  | phase 3 |
| `textnorm.TtsNorm.colloquial_years` |  | phase 3 |
| `textnorm.TtsNorm.colloquial_units` |  | phase 3 |
| `textnorm.TtsNorm.leave_unspeakable_numbers` |  | phase 3 |
| `textnorm.TtsNorm.oblique_numbers` |  | phase 3 |
| `textnorm.TtsNorm.space_fused_hundreds` |  | phase 3 |
| `textnorm.TtsNorm.dialect_numbers` |  | phase 3 |
| `textnorm.TtsNorm.number_forms` |  | phase 3 |
| `textnorm.TtsNorm.month_names` |  | phase 3 |
| `textnorm.TtsNorm.spoken_forms` |  | phase 3 |
| `textnorm.TtsNorm.canonical_unicode` |  | phase 3 |
| `textnorm.TtsNorm.drop_false_starts` |  | phase 3 |
| `textnorm.TtsNorm.plain_orthography` |  | phase 3 |
| `textnorm.TtsNorm.lexicon` |  | phase 3 |
| `textnorm.TtsNorm.with_lexicon` |  | phase 3 |
| `textnorm.TtsNorm.with_number_forms` |  | phase 3 |
| `textnorm.TtsNorm.describe` |  | phase 3 |
| `textnorm.TTS_NORM_VERSION` |  | phase 3 |
| `textnorm.normalize_for_tts` |  | phase 3 |
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
| `util.LOG` |  | phase 3 |
| `util.match_lang` |  | phase 3 |
| `util.CONTRACTIONS` |  | phase 3 |
| `util.TITLES` |  | phase 3 |
| `util.UNITS` |  | phase 3 |
| `util.resolve_thousands_or_decimal` |  | phase 3 |
| `util.pronounce_date` |  | phase 3 |
| `util.pronounce_time` |  | phase 3 |
| `util.is_fraction` |  | phase 3 |
| `util.normalize` |  | phase 3 |
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
| diacritize | constants 3, provenance_flags 11, author_complete 3523, proclitic_complete 3523, lattice_errors 5, word 168550, text 1248, vocalize 129706 | 306569 |
| dialect_lexicon | lexicon_dir 1, available_lects 1, lexicon_file 103, parent_chain 103, entries 103, len 103, get 66933, contains 9562, parse 609, env 7 | 77525 |
| dialects | constants 14, spec_for_lang 30882, lect_code 30881, o2i_calls 44036, supported_lects 1, arabic_codes 1, dialect_index 1, is_arabic_code 7671, core_subtags 30880, consonant_map 58, consonant_ipa 2610, spec 58 | 147093 |
| donor_lexicon | constants 1, bundled_path_default 1, sequence 31 | 33 |
| fusion | logprobs 1842, has_arabic_letter 2054, hypothesis_repr 5, fusion 26044 | 29945 |
| lattice | constants 2, defers_to_cascade 5186, word_ipa 16616, word_lattice 2770, spec_word_exception 3832 | 28406 |
| lexicon | constants 3, parse 916, parse_shares 916, source 1, get 31654, ambiguous 6666, contains 1333, len 1, env 56 | 41546 |
| nisba | constants 1, restore_nisba 9581 | 9582 |
| orthography | constants 5, allowed_classes 354, pinned_class 177, project_class 17374, source_marks 21567 | 39477 |
| sandhi | constants 1, apply_cross_word 30725, pausal 3465, has_article 394, is_bare_wasl_alif 394 | 34979 |
| stress | first_segment_len 6077, stress_ipa 190275, stress_words 6078 | 202430 |
| tashkeel | messages 54, TashkeelDiacritizer 2960 | 3014 |
| textnorm | const 1, is_arabic_lang 20860 | 20861 |
| tokenizer | normalize_unicode 7761, elide_silent_alif 7761, reorder_diacritics 7761, sentence 21624, words 5406 | 50313 |
| unicode_symbol2label | items 4171, size 1, lookup 57 | 4229 |
| version | constants 5 | 5 |
| waqf | constants 13, pausal 26936 | 26949 |

The inputs are the string literals of the sdist tests, the gold sentences, the Arabic word of every n-th line of `tests/ar_test.txt` at the gold commit, the bundled lexicon words, the word exceptions of every Arabic spec, a set of Unicode probes, and generated grids. The docstring of each `tools/goldens/<module>.py` names the inputs of that module.

The vocalize corpus runs `arbtok.vocalize(text, lect)` end to end: the word of every 400th line of `tests/ar_test.txt` (2143 words) under each of the 58 supported lects, the gold sentences under their own lect, and the Arabic test strings under `ar`, 129706 records in all. The word and fusion corpora run the diacritizers of ten lects under several option sets.

The env records of the lexicon and dialect_lexicon suites run one call under a fresh root directory with its own environment variables and files (`tools/goldens/envcase.py`). The Python call runs in a child interpreter, because `arbtok.lexicon` and `huggingface_hub` read the environment at import. The C++ call runs in the test process with the same variables set.

The goldens do not hold the default stem lexicon, which is GPL-2.0-derived data, or any text that comes from it. The dumper runs every corpus a second time in a child interpreter. Its Hugging Face cache holds the same revision of the lexicon, with the header row and no entries. A record whose line differs between the two runs depends on the stem lexicon. The golden keeps the input items of that record. In place of each result item it holds `{"sha256": <hex>}`, where `<hex>` is the SHA-256 of the Python line. `arbtok_parity` writes the C++ line of the record and compares its SHA-256. A mismatch prints the C++ line and both digests. The header of the file gives the count of hashed records as `sha256_records`. The test fails when the file holds another count.

| Golden | Hashed records |
| --- | --- |
| lexicon | 2830: source 1, get 2586, ambiguous 91, contains 151, len 1 |
| diacritize | 14249: word 7304, text 372, vocalize 6573 |
| fusion | 5678: fusion 5678 |

The other 16 golden files hold no hashed record. The source record is one of the hashed records. It checks the revision, the SHA-256 of the file, and the digests of its parsed entries and shares.

`tests/goldens/ensemble.jsonl.gz` (32432 records) and `tests/goldens/ensemble_unicode.jsonl.gz` (44152 records) hold the goldens of the ensemble. `ensemble_parity` compares them, and the logits of 32306 records are bit-identical to those of onnxruntime in Python on the same machine.

The `o2i_calls` corpus records each call that `dialects` makes into orthography2ipa (`resolve`, `resolves_exactly`) and into langcodes (`closest_match`), with the arguments that arbtok passes. The C++ resolution layer is checked on the inputs it receives.

Each golden suite has a mutation control: one rule is broken in the C++, and the suite must fail. The table gives the mismatches that each mutation causes. The ensemble suites have their own mutation controls, which kill 12 of 14 mutants.

| Suite | Mutation | Mismatches |
| --- | --- | --- |
| constants | KASRA holds the fatha | 1 of 42 |
| orthography | pinned_class gives 0 for no class, not -1 | 8186 of 39477 |
| tashkeel | TashkeelError names another Python class | 54 of 3014 |
| tashkeel | TashkeelDiacritizer ignores pausal | 1284 of 3014 |
| nisba | the shortest nisba skeleton is 4 letters | 42 of 9582 |
| waqf | phrase_final_only is inverted | 3564 of 26949 |
| arabizi | the digit 9 does not mark Arabizi | 166 of 58401 |
| unicode_symbol2label | items() gives the symbol as its own label | 4179 of 4229 |
| textnorm | mey is not an Arabic language | 801 of 20861 |
| dialects | `split("-x-")[-1]` keeps the hyphen | 1678 in 5 of 8 shards |
| stress | a negative proclitic_onset cuts one code point late | 6742 of 202430 |
| sandhi | the pausal form applies when pausal is False | 310 of 34979 |
| lattice | a kept alternative costs at least 0.5, not 1.0 | 87 of 28406 |
| tokenizer | a kasra before a shadda is not reordered | 1154 of 50313 |
| version | VERSION_ALPHA is 70 | 1 of 5 |
| lexicon | an ambiguous spelling steps aside only in a sentence of more than MIN_CONTEXT_WORDS words | 1051 of 41546 |
| lexicon | the cache directory is `arbtok/lexicon` | 8 of 41546 |
| lexicon | the stem of one word (`كتاب`) gains a sukun, which changes only hashed records | 3 of 41546 |
| dialect_lexicon | the parent's file wins over the lect's own file | 14 of 77525 |
| donor_lexicon | ensure_registered replaces a caller's registration | 8 of 33 |
| diacritize | the article allows only its lam to be open | 3432 of 306569 |
| diacritize | a two-letter word with a bare proclitic counts as proclitic | 435 of 306569 |
| diacritize | a whitespace-only word gets a record | 11 of 306569 |
| diacritize | the stem of one word (`كتاب`) gains a sukun, which changes only hashed records | 36 of 306569 |
| fusion | logprobs sums the exps in order, not pairwise | 11301 of 29945 |
| fusion | logprobs uses the libm `std::exp` | 5027 of 29945 |
| fusion | the stem of one word (`كتاب`) gains a sukun, which changes only hashed records | 78 of 29945 |

Two fusion mutations survive. Equal log-probabilities in ascending index order give 0 mismatches, because no tie among the top classes occurs on the corpus. A lattice weight of the opposite sign gives 0 mismatches, because the lattice cost of every licensed reading is 0.0 in Python too: each slot's top candidate costs 0.0 in the specs of these lects (73992 n-best entries in the fusion goldens, 7959 slots in the lattice goldens).

## Model output and the CPU

The ensemble model is an int8 ONNX model. onnxruntime picks its MLAS kernels once per process, in `MlasPlatform::MlasPlatform()` (`onnxruntime/core/mlas/lib/platform.cpp` at tag v1.31.1), from CPUID and XGETBV. Different kernels give different logits for the same input. The C++ port and Python both run onnxruntime 1.31.1, so the C++ logits equal the Python logits only on the same kernels.

The CPU class names the checks of that function that change a kernel this model reaches, joined with `+`. The model runs MatMulInteger, DynamicQuantizeLSTM and DynamicQuantizeLinear, which reach the U8S8 and U8U8 GEMM kernels, the logistic and tanh kernels and the quantize kernels.

| Tier | platform.cpp line | Check |
| --- | --- | --- |
| avx | 501, 512 | CPUID.1:ECX bits 27 and 28 (AVX, OSXSAVE), XCR0 & 0x6 |
| avx2 | 546 | CPUID.1:ECX bit 12 (FMA3), CPUID.7.0:EBX bit 5 (AVX2) |
| avxvnni | 608 | CPUID.7.1:EAX bit 4 |
| avx512f | 623 | CPUID.7.0:EBX bit 16, XCR0 & 0xE0 |
| avx512core | 650 | CPUID.7.0:EBX bits 17, 30 and 31 (DQ, BW, VL) |
| avx512vnni | 665 | CPUID.7.0:ECX bit 11 |
| avxvnniint8 | 681 | CPUID.7.1:EDX bit 4 |
| amx | 704, 214 | CPUID.7.0:EDX bits 24 and 25, XCR0 bits 17 and 18, the ARCH_REQ_XCOMP_PERM request |

The checks from line 546 on are inside the avx2 check, and lines 650 and 665 are inside the avx512f check. A CPU without AVX is class `sse`. The F16C (line 585), hybrid-core (line 593) and AVX-NE-CONVERT (line 694) checks change only float16 kernels and the thread count. `tools/goldens/cpu_class.py` and `tests/cpu_class.hpp` compute the class from the same bits. The committed goldens come from an Intel i9-11900H, class `avx+avx2+avx512f+avx512core+avx512vnni`. qemu with `-cpu Haswell` is class `avx+avx2`.

A golden that holds model output names its CPU class in the header as `cpu_class`. `tools/dump_goldens.py` counts the onnxruntime session runs of each module, and writes the class into the header of each module that runs the model. `tools/goldens/ensemble.py` always writes it. The model goldens are tashkeel, diacritize, fusion and ensemble. The other goldens run no session. The Python reference gives other records for each model golden under `qemu-x86_64-static -cpu Haswell`, which is class `avx+avx2`:

| Golden | Records that differ under qemu Haswell |
| --- | --- |
| tashkeel | 8 of 3014 (the full corpus) |
| diacritize | 1 of 174: the word record of `كتاب` under `ar` without waqf |
| fusion | 21 of 80: logprobs 18 of 40, fusion 3 of 40 |
| ensemble | the logits of `كتاب` and `ونبذأ` |

The fusion logprobs records are model output too: their input rows are the logits of the model.

`arbtok_parity` and `ensemble_parity` compare the class in the header with the class of the CPU that runs the test. When they differ, the test fails before it compares a record. Its message names both classes and the command that dumps the model goldens.

`tools/dump_model_goldens.sh <dir>` builds the reference venv with uv, fetches the sdist and the arbtok repository at the gold commit, and copies `tests/goldens` to `<dir>`. Then it dumps each golden whose header names a class again on this CPU. Configure with `-DARBTOK_GOLDENS_DIR=<dir>` to run the tests on that directory. With `--check`, the script also dumps the other goldens and fails unless the records of each one are byte-identical to the committed file. It does the same for a model golden whose class is the class of this CPU. Every CI test job prints the runner CPU and dumps the model goldens before ctest. The gcc baseline job adds `--check`.

## Python semantics that the port keeps

- The `str` methods (`lower`, `isalpha`, `isdigit`, `isspace` and the others the rules call) use tables that `tools/gen_unicode_tables.py` generates from the reference interpreter, so they follow Unicode 15.0.0 as Python 3.12 does. `tools/gen_unicode_norm_tables.py` does the same for NFC and NFD.
- A regex alternation tries its alternatives in Python order. A pattern is never replaced by a longest-match lexer.
- `$` in a Python `re` pattern also matches before a final `"\n"`. The port reproduces this where a rule ends in `$`.
- Slices with a negative or an out-of-range index follow Python slice rules.
- The goldens write a Python `set` sorted, because its iteration order follows the hash seed. A `dict` keeps its insertion order. A dict whose order comes from a spec file is written as sorted items.
- A float is written in Python `repr` form, and the build sets `-ffp-contract=off`, so that a fused multiply-add cannot change a result.
- `fusion.logprobs` keeps the float32 arithmetic of numpy 2.5.4. The exp is numpy's own SIMD algorithm (the AVX2 and AVX-512F loops), which is not correctly rounded, and every NaN gives the positive quiet NaN. The sum is numpy's pairwise sum over all elements. The log is `math.log` in double, and the subtraction is in float32. `np.argsort(lp)[::-1]` is a stable ascending sort, NaN last, reversed. numpy sorts with x86-simd-sort, whose order of equal values the port does not reproduce in general; on the 59209 logit rows of the corpus, and on five sets of allowed classes, the two orders agree.
- `sum()` of floats is the compensated sum of Python 3.12 (`orthography2ipa::detail::PythonFloatSum`). `list.sort` is a stable sort, with `reverse=True` keeping the order of equal keys.
- `str.splitlines()` splits on every line boundary that Python knows. `int(str)` accepts Unicode digits, surrounding whitespace, a sign and single underscores between digits. `Path.read_text(encoding="utf-8")` is strict UTF-8 with the CPython error messages, and universal newlines. A path is written as `str(PurePosixPath(...))` writes it.
- `warnings.warn` becomes a call to the handler that `arbtok::set_warning_handler` installs. With no handler, a warning is dropped.

## Data loading

`data/unicode_symbol2label.json` holds the table that `unicode_symbol2label.py` defines inline. `tools/gen_symbol_labels.py` writes it from the reference package and reads it back to check that the round trip gives the same dict, in the same order. `data/models` holds the model and the vocabulary of the wheel, and `data/models/NOTICE` gives their digests and licence. The orthography goldens read the vocabulary from there. `data/lexicons` holds the closed-class lexicons and `data/donor_lexicons` the donor lexicons, each a byte copy of the file in the 0.0.2a71 wheel, with the README of its directory at the gold commit.

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
| `orthography2ipa.resolve` | orthography2ipa.cpp ports the lineage data of linguatag 0.1.3a1. The goldens use linguatag 0.1.5a1. In C++, `ar-TR`, `arb-TR`, `ara-TR`, `AR-TR` and `Ar-TR` give `ar`, and `apc-TR` gives `apc-TR-x-cukurova`. Python gives `apc-TR-x-cukurova` and `apc`. `afb-SA` gives `afb-SA-x-dawasir` in C++ and `afb` in Python. No golden record holds it, because `spec_for_lang` gives `afb` for it in both | 66 dialects records are listed in `tests/parity/expected_divergences.tsv`. The fix is loom-ai-org/orthography2ipa.cpp#52, which ports the linguatag 0.1.5a1 tables. A submodule re-pin follows it |
| `orthography2ipa.registry.UnknownVariantWarning` | `resolve` issues no warning | none needed: arbtok records only `UserWarning` itself, not its subclasses |
| `orthography2ipa.resolve` | no result cache. A nearest-match resolve costs about 43 ms | `dialects` caches `lect_code` and `is_arabic_code` per tag, as the Python `lru_cache` does |

## Expected divergences

`tests/parity/expected_divergences.tsv` lists each golden record that differs because of a gap above. Each entry gives the golden line, the C++ line that the record gives, and the fix it waits on. A listed record that gives another C++ line is a mismatch. A listed record that starts to match Python fails the suite. A listed record that is not in the goldens also fails it, so the list cannot go stale. The list holds the 66 dialects records of the `-TR` resolve gap and no other entry. The C++ line of each entry is the record that Python gives with linguatag 0.1.3a1.

## Stem lexicon

`lexicon.resolve_source` turns a source into a local file in three ways. A path is expanded as `Path.expanduser` does. An `http://` or `https://` URL is fetched with `curl` into `cache_dir()` under its last path segment, unless that file exists. An `hf://org/name/file` id is the file of the dataset's `main` revision in the Hugging Face cache, in the layout of huggingface_hub 2.4.0: `datasets--org--name/refs/main` holds the commit, and `snapshots/<commit>/<file>` holds the file. The cache directory is `HF_HUB_CACHE`, `HUGGINGFACE_HUB_CACHE`, `HF_HOME/hub`, or `$XDG_CACHE_HOME/huggingface/hub`, else `~/.cache/huggingface/hub`, with `~` and `$VAR` expanded. When the file is not in the cache and `HF_HUB_OFFLINE` is not set, `curl` reads the commit of `main` from the Hub API and downloads the file into the cache. With `HF_HUB_OFFLINE` set, a missing file is `LocalEntryNotFoundError`, which `StemLexicon` reports as `LexiconUnavailable`.

The default lexicon that the goldens use is revision `05a7734e5ff20a567820f7134e1414a4b2daaa87` of `TigreGotico/arabic-stem-lexicon`, with SHA-256 `2f6a66dbbbee86f2db873497463d6e93eb0fddc431b7fd002244d99699fb79ee` and 145854 entries. CI writes that revision into the cache before the tests.

## Known differences

These differences are not reached by the goldens, or are deliberate.

| Python | C++ |
| --- | --- |
| `hf_hub_download` asks the Hub for the newest `main` commit when the network is up, and stores a file as a blob with a symbolic link | a file in the cache is used without a request to the Hub, and a download is written into `snapshots/` directly. No token, `HF_ENDPOINT` or revision other than `main` is read |
| `urllib` and `huggingface_hub` raise their own error messages for a failed download | the `LexiconUnavailable` message says that `curl` could not fetch the URL, and `curl` writes its reason to stderr |
| `lexicon.CACHE_DIR` and the huggingface_hub constants are read once, at import | `cache_dir()` and the cache directory are read from the environment at each call |
| `int()` reads a count of any size | a count outside the range of `long long` is skipped as if `int()` had refused it |
| `validate_repo_id` matches the repo id with a Unicode `\w` regex | the check accepts ASCII letters, digits, `-`, `_`, `.` and every non-ASCII byte |
| `LexiconUnavailable` is a `RuntimeError` | an `arbtok::Error` whose `python_type()` is `"LexiconUnavailable"` |
| `np.argsort` orders equal values as x86-simd-sort does | equal values come in descending index order (see the Python semantics above) |
| numpy picks its exp loop by CPU; the replicated loop is the AVX2 and AVX-512F one | `np_expf` gives those bits on every CPU. On a CPU where numpy takes another loop, the Python result can differ |
| a diacritizer is a Python object that the GIL serializes | `diacritize::diacritize` holds a mutex over its cache, but an instance of `LatticeDiacritizer` or `FusionDiacritizer` is not safe to share between threads, and orthography2ipa.cpp keeps process-wide state without a lock |

## espeak_wrapper and plugin

`espeak_wrapper.py` is not ported. No phoonnx voice path calls it:

- phoonnx at `origin/dev` `f185255` has no reference to `espeak_wrapper`. Its eSpeak path is `scriptconv.phonemizers.mul.EspeakPhonemizer` (`phoonnx/engines/neutts.py:277`).
- phoonnx gets its Arabic phonemizer from the scriptconv registry (`phoonnx/config.py:570` and `:604`). scriptconv at `origin/dev` `bc7d842` maps Arabic to `ArbtokPhonemizer`, which imports `arbtok.plugin.ArbtokG2PPlugin` (`phonemizers/ar.py:245`) and `arbtok.spec_for_lang` (`phonemizers/ar.py:267`). scriptconv has no reference to `espeak_wrapper`.
- No module of arbtok imports `espeak_wrapper`, and no package in the reference environment imports it.

`plugin.py` holds `ArbtokG2PPlugin`, the grapheme-to-phoneme class that scriptconv calls. It has no OVOS plugin glue. It is on the phoonnx voice path, so phase 4 ports it.

## Paths the goldens do not reach

- No golden input makes `spec_for_lang` issue a `UserWarning`, so the warning path of `dialects` is not checked against Python. The C++ issues the warning through the handler in the same case as Python.
- The sanitizer build checks every 20th record of each suite and every 160th dialects and diacritize record. It checks every fusion record, because a fusion record can depend on the record before it.
- The goldens run with `HF_HUB_OFFLINE=1` and a filled cache. The network paths of `lexicon.resolve_source` (an `hf://` download, an `http(s)://` download) are not checked against Python.
