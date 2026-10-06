# Native XL Mod work and WCS timing

October 6, 2026. This connects the native Mod branch-cost law to the checked
[WCS access clock](XL_WCS_CLOCK_VALIDATION.md) in an offline local oracle.
Native48 still uses nominal controller rates. This is preparation for its
coupled scan, not a shipped sound correction or a complete independent clock.

`desktop/control_timing_xl.hpp` calculates Mod instruction work from typed
profile/state fields. Each emitted byte asks `WcsTimingXL` for its own READY
completion and commit time. Neither measured WCS waits nor measured write
instruction durations are returned to the model. No CPU/opcode interpreter,
ROM reads, processing allocation, I/O or blocking operation is introduced.

The reference physically selects all 22 v8.21 programs and exercises Mod
off/on plus Chorus raw values 2, 130 and 250 where available. The 104 fixtures
contain 20,898 completed Mod calls, 70,377 Mod writes, 70,665 predicted bus
grants and 1,226 observed interrupt spans. Work durations, individual write
starts/completions, row/lane/value payloads, commits and bus grant/XACK times
have zero errors. The native computation has zero tracked new/delete calls.
Nineteen requests precede the local clock's quiet-pass initialization and are
explicitly excluded from grant comparison; they are not silently counted as
passes. A separate historical observed-wait oracle also passes 20,894 calls
and 70,260 writes after promotion of its shared cost law to the native header.
Its different windows are not a paired old/new sound comparison.

The oracle observes profile/state at each Mod entry, serial IRQ spans and a
quiet row-zero origin. These are labelled local inputs. It infers the settled
pair/RESET state from a quiet complete pass instead of copying the reference
flip-flops. Native48 must eventually generate its own controller entries,
interrupt chronology and initial/program/key phase. Arithmetic effects of
displaced fetches and operand holds are also separate open requirements.

The actual preceding source was frozen under
`build/validation/xl-timed-modulation-20261006/baseline/`. Source/binary hashes,
exact commands, exit codes and 104 per-fixture results are retained in the
private `predicted-all-run.json`, `historical-all-run.json` and
`predicted-summary.json`. Both runs exited zero. Reference remains pinned
Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`; the v5 bank and existing
audio implementation are unchanged. Release macOS arm64, C++20 and
`-ffp-contract=off` were used. No new whole-plugin CPU, sound-matrix, installed
host or listening claim follows from this component check.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_timed_modulation_check cineol_xl_modulation_timing_check -j2
build/xl-sound-validation/native-hall/cineol_xl_timed_modulation_check /private/path/to/v8.21-roms
build/xl-sound-validation/native-hall/cineol_xl_modulation_timing_check /private/path/to/v8.21-roms
```
