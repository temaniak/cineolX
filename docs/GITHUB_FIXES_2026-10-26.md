# GitHub fixes — October 26, 2026

Follow-up physical ROM comparison:
[REFLEXION_CHORUS_HIDDEN_CONTROLS_2026-10-26.md](REFLEXION_CHORUS_HIDDEN_CONTROLS_2026-10-26.md).
It resolves the earlier Chorus hypothesis and verifies all three original-224
hidden operations, including their Rate/Depth interaction and Optimization
response semantics. The sections below retain the preceding correction block.

## Result and scope

The primary fixes for issues #5, #4, #6/#7 and #2 are implemented, with
regressions covering the reported failures. Chorus (#8/#9) remains a separate
follow-up in `BUGFIX_TODO.md`, as requested. Original-224 hidden-control absence
(#3) was investigated; Linux (#1) is excluded.

Baseline: `f545a0b412de6b72de7617b1e4aa755f10fd2343` (`v1.0Alpha`). The existing
uncommitted AU idle-paint workaround and its tests were preserved. Work is local
and unreleased: no issue comments/closures, commits, version changes or installs
were made. Existing host parameter IDs, indices, preset schemas, plugin/bundle
identities and prepared-bank payload/version remain compatible.

## HF endpoint correction (#5)

The physical LARC input accepts 2–254. The reference transport rejects 255;
after a valid 254 move, attempting 255 leaves the logical record at 254. ROM
compiler observations at 252/253/254 establish the finite/open-filter boundary.
The previous editor offered 255 as an ordinary frequency maximum. The native
compiler interprets that logical sentinel by zeroing both filter
groups, which can mute a signal path. Both 254 and 255 had displayed `INF`.

`ProgramData::control_maximum()` now limits frequency faders to 254, and
`resolve_controls()` also resolves old preset/session/automation values of 255
to 254. Host parameters retain their shared -1..255 ranges; stored automation
is not rewritten. A single frequency-cell mask is calculated per control
resolution, outside the sample loop. The compiler's internal sentinel handling
and the bank data are unchanged. `INF` continues to mean an open filter; it
does not add Freeze or infinite tail behavior.

Measured four-output RMS at the affected endpoint, with independent frozen
baseline/current binaries and identical inputs:

| Program/control, raw request 255 | Before: input / silent tail RMS | After: input / silent tail RMS |
| --- | --- | --- |
| Concert Hall Treble | 0.00001136 / 0.00000938 | 0.006506 / 0.002577 |
| Concert Hall HF Bandwidth | 0.00001136 / 0.00000938 | 0.005590 / 0.002109 |
| CD Plate B HF Bandwidth | 0 / 0 | 0.013079 / 0.004510 |

All 52 tested non-255 endpoint cases and all ten hidden-control sensitivity
cases retain identical printed metrics. Only the 13 invalid frequency endpoint
cases change. Inputs: 48 kHz, seed-17 deterministic noise at 0.03 peak, 0 dB
gain, factory controls, Dynamic/Mod/Optimization off; two seconds silent
warmup, two seconds input, two seconds silence. RMS windows are seconds 3–4
and 4–5, respectively. Full data and the probe source remain in ignored
`build/validation/github-fixes-20261026/` and the preceding triage directory.

The permanent native regression also renders both 254 and a resolved 255 for
all 43 HF controls across the catalog. Their complete four-channel outputs,
including stopped-input intervals, must be identical, finite and retain an
audible input response. The old source fails this regression at its first
frequency control.

## Control availability (#4, #6, #7)

`control_active()` no longer reserves cells 6/7 for stop decay when a program
has no dynamics controller. This restores Chorus & Echo voice levels 1/2,
Resonant Chords pitches 1/2 and Multiband Delay times 1/2. Actual stop-decay
availability and the algorithms' dynamics remain unchanged.

The same helper now recognizes decay dependencies: a Mid value can feed the
neighboring LF compiler, and an LF value can feed a paired Mid coefficient.
This restores Inverse Room running Mid and both CD Plate running LF controls
without introducing new sound parameters. Previously hidden values already
affected DSP. Their help text and the earlier help audit are updated.

Nine explicit UI fixtures check physical faders and screen values, captions/
availability through the normal editor path, balanced host gestures, automation
updates and session recall. These fixtures do not derive their expectations
from the predicate under test. Genuine parked slots and the still-unimplemented
Rich Plate/Rich Split Chorus remain unavailable. The native eligibility test
fails on the old source; after the voice fix alone it advances to the missing
Inverse Mid assertion, demonstrating the two independent causes.

## Host bypass (#2)

The processor now overrides `processBlockBypassed()` and routes normal/bypassed
processing through one audio routine. The dry path retains the reported host
latency; Low latency keeps zero-delay dry audio. A host-rate dry ring is kept
warm and both directions crossfade over 10 ms. A host starting bypassed starts
fully dry, and mono input is duplicated to Front. Rear outputs fade to silence.

DSP keeps processing the original input while bypassed, including spillover
and converter histories, so resuming cannot replay a frozen tail. This means
steady bypass continues to consume DSP CPU. Buffer allocation occurs only in
`prepareToPlay`; the audio loop adds no locks, I/O, waits or heap work. The
normal fully active path skips the crossfade arithmetic. No host bypass ID or
new user parameter is introduced; JUCE retains its existing wrapper bypass ID.

The old processor fails the new bypass test on its mono passthrough. The
corrected test covers both engines at 44.1/48/96 kHz, blocks 1/127/512,
normal/Low latency, exact delayed impulses, quad dry alignment, crossfade
weight continuity and return to an independently always-running processor.
Tracked audio allocations/releases are zero. Additional dry storage is 3,184 /
2,048 / 5,504 bytes per instance at 44.1/48/96 kHz, plus buffer bookkeeping.

This validates the JUCE processor bypass path used by the VST3 wrapper. Cubase
is not installed here; acceptance in the reporter's Cubase session remains
open. A host that suspends or externally switches the entire plugin can still
have behavior outside this callback's control.

## Original-224 Rate, Depth and Threshold (#3)

The importer captures `modulation_period`, `modulation_hold`, `modulation_step`
and `decay_amount` into each original-224 program profile. `Hall` and the desktop
scan controller read those immutable defaults. Public `Controls` and the plugin
parameter layout expose enable switches, but no independent overrides. The
existing reverb Depth control describes distance/early-late balance, not
modulation depth. This is a runtime/control-model omission, rather than an
editor field hidden by `control_active()`.

The attached original-224 manual page documents Shift+7 and Pre-delay for an
alteration setting 1–16, normally 2, and Shift+8 for Decay Optimization settings
normally 5 for halls and 7 for plates. It describes how much/how quickly the
reverb parameters are altered. The separate level-drop test in
`DecayController::step()` uses a fixed 22 in encoded level units. The page does
not establish that Shift+8 adjusts that level threshold, and does not document
the claimed Shift+6 operation. Therefore no guessed Threshold/Depth parameter
was added. The TODO records physical v4.4 key verification and compatible
runtime overrides as follow-up work.

## Validation and reproducibility

Retained evidence: `build/validation/github-fixes-20261026/`. The original
bank/cache was read without modification, and plugin tests use isolated
temporary caches. XL input is the existing v8.21 version-5 bank with SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Reflexion remains pinned to `f68ea1d069fef4a5663201693bfdfa1c579ffd69`, JUCE to
8.0.14. Private reference chips are hash-validated by the existing loader and
were not copied into public source or artifacts.

Passed checks:

- All 22 programs: 428 physical frequency endpoint/compiler fixtures, including
  the 252/253/254/255 boundary; native control allocations/releases zero.
- Inverse Room: 84 composed physical-fader fixtures and four Size fixtures;
  CD Plate A/B: 80 composed fixtures each, including restored running controls.
- Native HF audio: 43 controls, two independent upper-end renders each; explicit
  eligibility regression, inactive-slot and unsupported-Chorus guards.
- Plugin full dual-bank regression: 28 programs at 44.1/48/96 kHz, preset/session
  migration, quick presets, UI, stereo/mono, large/varied blocks, original-224
  reference integration, Spillover 1/5/10 s and zero callback new/delete.
- Nine restored UI bindings and 627 active fader endpoints; help covers all
  28 programs, 121 pages and 1,087 active screen bindings.
- Bypass regression described above; 108 moving SPACE/DIRT/filter-link cases
  and four-output behavior continue to pass.
- AU, VST3 and Standalone Release builds; arm64/x86_64 binaries and strict
  ad-hoc bundle signatures verified. These are local build outputs, not installs.

Key commands (supply private bank/ROM paths locally):

```sh
cmake --build build/plugin --parallel 2 --target native_hall_plugin_check \
  cineol_xl_control_regression_check NativeHall224_AU NativeHall224_VST3 NativeHall224_Standalone
build/plugin/native-hall/cineol_xl_control_regression_check XL_BANK --hf
build/plugin/native-hall/cineol_xl_control_regression_check XL_BANK --access
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --banks ORIGINAL_BANK XL_BANK
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --bypass-check ORIGINAL_BANK XL_BANK
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --help-check ORIGINAL_BANK XL_BANK
build/validation/github-fixes-20261026/controls_oracle ROM_DIRECTORY XL_BANK --endpoints
build/validation/github-fixes-20261026/controls_oracle ROM_DIRECTORY XL_BANK 9
build/validation/github-fixes-20261026/controls_oracle ROM_DIRECTORY XL_BANK 11
build/validation/github-fixes-20261026/controls_oracle ROM_DIRECTORY XL_BANK 12
```

The `controls_oracle` executable is built from `tests/xl_controls_check.cpp`;
the normal CMake target is `cineol_xl_controls_check`. Its new `--endpoints`
option narrows the existing private-ROM oracle to physical frequency limits.
No human listening, physical Lexicon comparison, Cubase runtime, Windows runtime,
Intel runtime or Daisy CPU-margin acceptance is claimed.

## Assembled CPU and memory comparison

Measured after the corrections were assembled, with no concurrent agent builds
or regression runs: Apple Silicon arm64, Apple clang 21.0.0, Release/O3,
`-ffp-contract=off`, matching denormal policies. Baseline/current/current/baseline
order was used. The full-plugin original-224 benchmark retains five internal
repeats per program/mode/run. The native XL benchmark covers all 22 programs,
six workloads, three internal repeats per run, two runs per revision, 48 kHz
and 128-sample control blocks. Preparation and file writes are outside timing.

| Workload | Baseline audio CPU | Current audio CPU |
| --- | --- | --- |
| Original-224 plugin, single engine, mean of six program medians | 2.118% | 2.165% |
| Original-224 plugin, Spillover overlap, same aggregation | 4.229% | 4.251% |
| XL native, modes off, median across 22 program medians | 2.260% | 2.245% |
| XL native, moving decay controls, same aggregation | 2.285% | 2.266% |
| XL native, Mod/Size edges, same aggregation | 2.285% | 2.267% |

The full-plugin dry-delay/bypass work adds approximately 0.047 percentage points
in the single-engine measurement (2.2% relative) and 0.022 percentage points
during overlap (0.5% relative). XL workload median changes range from -0.5%
to -0.8% relative; these small differences are treated as run variation, not
a claimed optimization. All 792 native XL measured checksums per revision
match; allocation/release counts are zero. The prepared two-runtime XL storage
is unchanged at 65,314,352 bytes, with bank storage 5,828,152 bytes. The plugin's
additional dry ring is specified above.

Raw CSVs/logs, alternating commands, `measure_cpu.py`, `summarize_cpu.py` and
`cpu-summary.json` are retained in the evidence directory. These measurements
describe the tested host/workloads, not guaranteed realtime margin in every DAW.

Cleanup review: retained the frozen baseline headers/executable, current build
outputs, unique oracle/audio/CPU evidence and scripts required to reproduce it.
No historical evidence, caches, recordings, ROMs or other user assets were
removed during this correction block. No repeated full WAV campaign was made.
