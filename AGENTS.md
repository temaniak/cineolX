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
