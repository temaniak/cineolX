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
Working branch: `codex/original-224-sound-accuracy`. Baseline checkpoint:
`0ebb668`. The user explicitly authorized pushing checkpoint commits to this
repository; do not create a second remote or force-push. The branch is published.

From a clean checkout on another computer, fetch/switch to that branch and
read this record. Current checkpoint history:

| Commit | Saved step |
| --- | --- |
| `0ebb668` | Completed DAC, ADC and initial decay/scan work plus first handoff record |
| `ce45a03` | Direct-switch modulation counters, ROM oracle/test tools and modulation report |
| `4a359f4` | Phase-aware all-six/Chamber residual measurements |
| `484bcc3` | Dry/Wet, gain and overlap behavior report |
| `2677d7c` | Final regression/CPU/Spillover results and current open work |
| `bdbc724` | Exact local modulation procedure/write timing model and portable oracle |
| `0417cf1` | Stable-control scheduler integrated and ROM-validated; see the native scan report |

Reflexion pin: `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.
JUCE pin: `8.0.14`. Inspect `dependencies.json` and build paths before changes.

The user's original Windows ROM location is `C:\Users\User\Desktop\lex`,
under `224 v4_4`. The macOS continuation used a separate private local ROM
directory. Another computer needs its own path to the same supported ROM1-ROM5.
Do not copy these assets into Git.

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

At `0ebb668` the six-program decay-only test had mean broad-band T20
error 0.36%, maximum 2.92%; with modulation enabled improvements were uneven.
Incremental decay/scan CPU cost was +0.91% versus the previous ADC/DAC engine.
See the reports for exact fixtures, exclusions and limitations.

## Remaining working stages

| Stage | Status | Required result/report |
| --- | --- | --- |
| 1. Modulation phase and scheduling | Stable-control scheduler integrated and verified; startup/compiler phase and within-call audio updates remain open | [Native scan report](NATIVE_CONTROL_SCAN_VALIDATION.md): 260,102 free-running events exact, local timing oracles and desktop adapter passed. [Modulation checkpoint](MODULATION_VALIDATION.md): direct switches retain three global counters. |
| 2. Remaining tails/frequency differences | Cause-isolation checkpoint complete; residuals retained | [Residual report](RESIDUAL_SOUND_VALIDATION.md): 18 all-six/mode runs plus four Chamber level/seed probes. Frozen phase and quiet floors matter; no compensating EQ. |
| 3. Dry/Wet and Input Gain | Verified | [Gain/timing report](GAIN_TIMING_VALIDATION.md): six-program dry/mix, 24 gain cases; behavior retained. |
| 4. Final regression/acceptance | Regression passed; full sonic acceptance open | [Regression checkpoint](FINAL_SOUND_VALIDATION.md): Windows builds, CPU +1.68% versus baseline, original-only Spillover verified separately. |

A stage report must state its status honestly: completed correction,
investigation with a retained approximation, or unresolved blocker. Include
baseline revision, changed files, exact test commands/fixtures, measured
results, sonic/session effects and the next action. Keep this table current
and create a Git checkpoint for each stage. Additional work may be needed if
final verification establishes another meaningful defect.

The current required next step is startup/program-load phase and within-call
coefficient application. The [WCS audio boundary report](WCS_AUDIO_BOUNDARY_VALIDATION.md)
isolates write arbitration from coefficient payload timing. No blanket
claim of identical modulation-on tails is supported. The normal-startup matrix
must remain separate from offline aligned diagnostics. The user's Spillover
setting adds two wet networks temporarily; isolate a single program for sound
measurements, then test the overlap and its CPU separately.

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

`DesktopHall` now uses a native event clock for prepared original banks:
eighteen nonuniform modulation calls, nine transfer reads and a level-controller
call. Timing follows independently validated state/signal-dependent laws and
WCS grants. Legacy single-Hall profiles retain their nominal clock.
Checked-in tools now provide this investigation: `native_224_modulation_check`,
`native_224_sound_compare`, `script/analyze_sound.py` (NumPy only), and
`script/benchmark_sound.py`. The sound tool renders normal native and a clearly
labelled diagnostic aligned variant separately. ROM1–ROM5 are SHA-256 checked.
No ignored diagnostic source is required on another computer.

The foundation checkpoint added `native_224_control_timing_check` and the native laws
in `desktop/control_timing224.hpp`. On macOS arm64, 576 cases checked 238,967
complete calls and 328,567 WCS writes with zero instruction-cost, whole-call
or per-write timing mismatches. See [timing foundation](CONTROL_TIMING_VALIDATION.md)
for the initial bus-history exclusions and local-oracle limits. That checkpoint
did not change the audio processor. The subsequent
[native scan report](NATIVE_CONTROL_SCAN_VALIDATION.md) covers integration,
11,242 exact local level calls, 260,102 exact free-running scan events,
separate left/right held detectors, 360,000 adapter passes, thirty paired sound
fixtures, CPU and processor/Spillover regression. Future scan events receive no
actual call-entry clocks or states after their initial alignment.

The normal modulation-on noise matrix improved mean absolute broad-band T20
error from 4.84% to 4.54%, with uneven results across seeds/programs. Frozen
startup phases and sparse accepted music fits still limit acceptance. No EQ
or feedback retuning was added. Read the report before making another change.

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
