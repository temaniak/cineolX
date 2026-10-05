# Original-224 desktop sound accuracy: continuation record

Updated: October 5, 2026. Read this file before continuing on another computer.
Each working stage has its own report, code/tests and Git checkpoint. Do not
rely on another agent's conversation history or ignored local diagnostics.

## Scope and constraints

- Desktop application/VST3/Standalone, original Lexicon 224 v4.4 programs only.
- Improve frequency response and decay lengths toward pinned Reflexion without
  a substantial CPU increase. Retain the efficient native runtime.
- Do not work on or build Daisy in this phase. Default portable behavior must
  remain compatible; no conclusions about MCU realtime margin are permitted.
- Keep plugin identifiers, parameter IDs and existing sessions/caches usable.
- No allocations, I/O, locks or waits in audio processing.
- Keep dependency checkouts clean; patch only ignored build exports.
- Repository documentation is English. User communication can be Russian.
- Never add ROMs, banks, WCS images, audio captures or private adapters to Git.

Primary repository: `https://github.com/temaniak/cineolX`.
Reflexion pin: `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.
JUCE pin: `8.0.14`. Inspect `dependencies.json` and build paths before changes.

The user's original ROM files are always under `C:\Users\User\Desktop\lex`,
in `224 v4_4` on this computer. Another computer needs its own private local
path to the same supported ROM1–ROM5. Do not copy these assets into Git.

## Completed starting point

The previous three desktop steps were completed and verified together:

| Step | Implementation | Evidence |
| --- | --- | --- |
| Output/DAC | Event-based DAC capture and continuous-time AOUT | [DAC report](EVENT_DAC_VALIDATION.md) |
| Input/ADC | Analytic interpolator/AIN response at independent ADC hold times | [ADC report](INPUT_ADC_VALIDATION.md) |
| Decay/scan | Correct startup period and sparse transfer-peak sampling | [Controller report](CONTROLLER_VALIDATION.md) |

The selected engine is `DesktopEngine48` in
`native-hall/desktop/engine22448.hpp`. Its integer/control core is
`DesktopHall`, selected through the third `Engine48WithOutput` template
argument. Ordinary `Engine48` keeps its previous boundaries and controls.
The prepared bank ABI remains version 1: 123,460 payload bytes.

At this checkpoint the six-program decay-only test had mean broad-band T20
error 0.36%, maximum 2.92%; with modulation enabled improvements were uneven.
Incremental decay/scan CPU cost was +0.91% versus the previous ADC/DAC engine.
See the reports for exact fixtures, exclusions and limitations.

## Remaining working stages

| Stage | Status | Required result/report |
| --- | --- | --- |
| 1. Modulation phase and scheduling | In progress | Explain startup/phase and signal-dependent clock differences; validate any correction without an excessive CPU cost. `MODULATION_VALIDATION.md` |
| 2. Remaining tails/frequency differences | Pending | Investigate Chamber and the 9–10 kHz edge; distinguish repeatable defects from initial phase, excitation and quiet-floor fit errors. `RESIDUAL_SOUND_VALIDATION.md` |
| 3. Dry/Wet and Input Gain | Pending | Measure relative delays/levels and response to input amplitude; change behavior only for established mismatches. `GAIN_TIMING_VALIDATION.md` |
| 4. Final regression/acceptance | Pending | Six programs, modes, controls, signal levels, musical fixtures, sample rates, switching/state and paired CPU. `FINAL_SOUND_VALIDATION.md` |

A stage report must state its status honestly: completed correction,
investigation with a retained approximation, or unresolved blocker. Include
baseline revision, changed files, exact test commands/fixtures, measured
results, sonic/session effects and the next action. Keep this table current
and create a Git checkpoint for each stage. Additional work may be needed if
final verification establishes another meaningful defect.

## Current modulation evidence and next action

The controller report contains an important diagnostic: aligning reference
decay bytes **and** modulation descriptors/index/dividers/coefficients/offsets
at input start reduced Hall B's maximum broad-band tail error from 13.80% to
1.94%. That alignment is only an offline experiment, not plugin behavior.
Do not claim it as a shipped improvement or tune a fixed phase to one clip.

Native `Hall::update_modulation()` matches ROM steps at the actual routine
clock for the importer's base controls. Its internal descriptors/dividers
come from a captured profile. `set_controls()` writes coefficient tables;
check whether this leaves the descriptor phase consistent after depth/mode
changes. ROM program loading resets its random index to 4; other dividers
can retain their previous phase. Compare the whole state and event ordering.

`DesktopHall` currently uses a uniform nominal scan clock: 18 modulation
routine calls and nine transfer reads per level-controller call. Actual ROM
instruction timing is state/signal dependent. Derive corrections from ROM
traces or independently validated native laws, not fitted tail lengths.
First make a checked-in, ROM-private validation harness so another computer
can reproduce this investigation without ignored local sources.

## Building and checking

Configure with CMake and original ROMs outside the repository. For a tools-only
run, no audio device or plugin installation is needed:

```sh
cmake -S . -B build/sound-validation -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON -DNATIVE_HALL_ROM_DIR="/private/path/224 v4_4"
cmake --build build/sound-validation --config Release --target native_224_bank native_224_bank_check native_224_adc_check native_224_dac_check native_224_controllers_check native_hall_compare
ctest --test-dir build/sound-validation/native-hall -C Release --output-on-failure
```

The bank/WCS files are generated privately from validated ROMs. Their paths
are in the build directory. The existing `native_hall_compare` is a Hall B
strike/chord fixture, not yet a complete six-program acceptance matrix.
Plugin checks use `native_hall_plugin_check --bank BANK` and
`cineol_rom_import_check --cached` with a **test-only** `CINEOL224_CACHE_DIR`.
Never overwrite the user's normal imported cache for a test.

On this Windows machine the tested desktop build is `build/windows-event-dac`.
Its readable patched reference export is
`build/validation/filter-dac-20261005/reference`; ordinary `build/deps/reflexion`
was inaccessible in this sandbox. The compiler flags include `/DWIN32
/D_WINDOWS /EHsc` plus that export include path. Inherited `PATH` and `Path`
duplicates caused MSBuild failures; normalize environment keys when launching
MSBuild. These are machine workarounds, not portable required paths.

Paired CPU benchmarks must use Release, untimed preparation/warmup, alternating
baseline/new runs, repeated medians, and FTZ/DAZ as in `processBlock`. Compare
equivalent workloads on the same machine. Old non-FTZ timings are not suitable
baselines. Use independently compiled frozen code when a shared core changes.

Ignored evidence on this machine is under `build/validation/`, including
`filter-dac-20261005`, `event-dac-20261005`, `event-adc-20261005` and
`controllers-20261005`. The public reports retain the important results.
Missing private renders on another computer are expected; regenerate them.
