# Development guidance

- The primary repository is https://github.com/temaniak/cineolX. Reflexion is an external dependency under `deps/reflexion`.
- Preserve audio safety: no allocation, file/network I/O, locks or waits in audio processing.
- Keep DSP, logical controls and hardware I/O separate.
- Keep the plugin identifiers and parameter IDs stable for existing DAW sessions.
- Only original Lexicon 224 v4.4 ROM1-ROM5 are supported.
- Never commit ROMs, banks, captures, firmware images or private hardware adapters.
- Inspect build paths and dependency pins before large changes.
- Keep dependency checkouts clean; apply compatibility patches to build-only exports.
- Explain changes to sound or control behavior. MCU builds do not prove realtime CPU margin.
- Keep repository documentation in English.

## Sound-accuracy priority

- The main objective is sound as close as practical to the original: frequency balance, tail shape/duration and response to signal and controls, with acceptable realtime CPU cost.
- Prefer short, evidence-driven correction blocks followed by independent before/after sound comparisons. Investigate controller details only as far as needed to explain and correct a measured sound mismatch; avoid open-ended component equivalence work.
- Keep tail validation explicit: compare input-silent intervals, fall/retrigger timing, decay where measurable, and residual floors. Passing internal state checks alone does not accept the sound.
- For level-dependent or time-varying tails, treat short-window percentage differences as diagnostics rather than proven implementation defects. Account for the original's variation across levels and controller/modulation phases; prioritize frequency-dependent decay, envelope character, stereo behavior and listening over sample-identical tails.
- If exact matching is not practical after a bounded investigation, document the remaining differences and prepare level-matched listening comparisons for user evaluation. Never claim listening acceptance without an actual listening review.
- Reuse existing measurements and fixtures. Batch CPU measurements after an assembled correction, rather than after every helper change, unless the user requests a measurement or a concrete performance regression needs diagnosis.

## Generated-material cleanup

- After each completed validation block, and when disk space is low, review generated caches and test artifacts for obsolete material that will not be needed again.
- Delete only confirmed disposable material: redundant copies, superseded intermediate outputs, and abandoned experiments whose useful results and reproduction details have been retained. Preserve any artifact whose future use is uncertain.
- Keep active regression inputs/reference outputs, current before/after comparisons, unique evidence, listening pairs still awaiting review, and the sources needed to reproduce accepted results.
- Preserve reports, metrics, logs and provenance. Record removed archive paths and any retained identical replacement so historical evidence can still be located.
- Do not delete user recordings, source assets, ROMs, banks, firmware, hardware adapters, dependency resources or unrelated files as cache cleanup. Never clean or perform file I/O inside audio processing.
- Reuse existing fixtures and avoid accumulating repeated full WAV campaigns when a narrow check or saved metrics suffice.
