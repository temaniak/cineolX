# Rich Plate / Rich Split Chorus correction

Date: November 2, 2026. This block restores the two firmware-named controls
and their Chorus-dependent controller work budget. It does not add delay-tap
modulation absent from the observed v8.21 ROM, or claim complete XL scheduler
accuracy.

Subsequent correction: [Rich Dynamic/Optimization timing](XL_RICH_DYNAMIC_TIMING_FIX_2026-11-02.md)
replaces this block's average Rich clock with coupled native work and progressive
coefficient updates. The measurements below remain the historical first step.

## Evidence and implementation

The baseline is `f545a0b412de6b72de7617b1e4aa755f10fd2343` plus the existing
uncommitted GitHub/hidden-control fixes. A complete copy of the native core,
desktop, plugin and tests, its hashes, and the old plugin-check executable were
frozen before this block in `build/validation/rich-chorus-fix-20261102/baseline/`.
Reflexion remains pinned at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`;
the clean dependency is used through its existing build-only scheduler export.
The XL bank remains v5, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Original-224 v4.4 remains supported unchanged by this block. No bank migration,
new host parameter, plugin identity change, ROM execution in audio, installation
or release is introduced.

The [manual audit](MANUAL_CONTROL_AUDIT_2026-11-02.md) identifies Rich Plate
Chorus on printed page 4.22 and shared Rich Split Chorus on 4.34. The retained
manufacturer scan has a 224X cover and XL errata; it does not establish the
exact behavior of v8.21. The ROM uses 32 positions, displayed 00–97. Its
Plate/Chorus exception belongs to another program.

Fresh physical-key/fader probes confirm zero interpolation destinations and
zero modulation WCS writes in both rich programs. They also confirm that AD5C
still consumes work and changes the monitor cadence. There are two calls per
monitor pass, nine passes per slow scan. For zero destinations, enabled average
work is `1197 - 1006 / period + 35 / 8 + 75 / (8 * hold)` CPU states per call.
The disabled branch always takes 1127 states. Logical positions 0–15 set
periods 17–2; positions 16–31 use period 1 and change only the unused step.
The resulting 18-call work difference now adjusts both native control clocks.
Changing Chorus or Mod preserves the clock phase and evolving tail state.

The factory reference is the logical `ProgramData::chorus`, not the bank's
stored modulation period: Rich Split has factory Chorus 0 while its prepared
modulation period is 1. Using that period would anchor the correction to the
wrong factory cadence. Rich Plate's factory Chorus is 16.

UI eligibility now receives the graph identity explicitly. Rich Plate page 2,
slot 3 and Rich Split page 3, slot 3 are active, as is Mode Enhancement in both.
Inverse Room and unrelated zero-target controls retain their previous status.
Help distinguishes observed controller timing from moving delay taps, including
Rich Split's LR caption. Existing `xl_chorus` automation, gestures, factory reset
and state/preset storage are retained.

## Timing limits

The shipped change corrects the missing Chorus-dependent work, around the
existing imported factory nominal clock. It averages the divider work; it does
not implement complete independent serial IRQ, panel and feedback/decay compiler
scheduling. Absolute cadence still varies with input, Dynamic Decay and
Optimization work in the ROM. For example the fresh mode-1 low/high reference
slow rates are 52.690/93.735 Hz for Rich Plate and 52.663/93.329 Hz for Rich Split;
the corrected nominal model stays within 3% there. With both dynamic modes on,
the reference can instead run about 44–46/70–73 Hz, so that absolute timing gap
is still open. This distinction is intentional and is not sound-identity
acceptance.

The independent audio comparison uses actual physical selection/faders and
verified mode bits, a fresh 16-second boot per fixture, all four outputs at
48 kHz, one silent second, two input seconds and three stopped seconds. The
old runtime is compiled from the frozen source in a separate namespace and
receives only the same operator control values and input as the new runtime.
No reference controller/DSP state, RAM, write clock or modulation phase is
injected into either native runtime. Two seeds and levels exercise noise plus
sustained tones; additional independent chord and percussion fixtures are
retained for level-matched listening. Signal and half-second silent-window
metrics are diagnostic; these nonstationary tails are not accepted via T20 or
short-window percentage matching. No listening or physical-unit acceptance is
claimed.

## Completed validation

- New native regression: 32 Chorus positions match the independently verified
  AD5C cost law; the corrected nominal mode-1 cadence matches fresh physical
  ROM entry rates within 3%. Both rich tails change with low/high Chorus when
  Dynamic/Optimization is enabled, are identical throughout the upper half,
  and remain identical across Chorus with Mod off. Outputs stay finite.
- Independent ROM local timing: 2,012 Mod calls, zero cost/write errors; 2,409
  outer monitor passes, zero stage/level/gap errors. These component checks
  remain separate from the free-running native approximation.
- Fresh before/after audio: 40 noise/tones fixtures, seed 17 at the original
  level and seed 99 ten times quieter; all eight toggle combinations at the
  first level, Optimization/combined modes at the second. All 20 old native
  Chorus pairs are identical. All eight new Mod-off pairs remain identical.
  Eight additional independent chord/percussion fixtures supply listening
  comparisons, for 48 total reference/baseline/current fixtures.
- Primary stopped-window effect, high Chorus minus low Chorus, dB:

| Program | Enabled modes | ROM | Before | After |
| --- | --- | ---: | ---: | ---: |
| Rich Plate | Mod + Optimization | -0.020 | 0.000 | -0.031 |
| Rich Plate | Mod + Dynamic | -0.735 | 0.000 | -0.926 |
| Rich Plate | All three | -0.601 | 0.000 | -0.932 |
| Rich Split | Mod + Optimization | -0.022 | 0.000 | -0.025 |
| Rich Split | Mod + Dynamic | -0.237 | 0.000 | -0.270 |
| Rich Split | All three | -0.222 | 0.000 | -0.260 |

These are diagnostics for the missing control response, not an audio acceptance
threshold. Mean absolute effect discrepancy in these six primary cases changes
from 0.306 to 0.101 dB, with a remaining maximum of 0.331 dB. The Rich Plate
combined-mode overshoot is retained, not hidden by a fitted gain or discarded
fixture. Half-second windows and both levels are in `audio.csv`.

- UI: eleven issue-derived fader/screen bindings including the two rich Chorus
  controls pass gestures, automation and session recall; 641 active endpoints
  pass. Help covers all 28 programs, 127 pages and 1,131 active bindings. The
  physical-ROM display checks validate 130 Rich Plate and 261 Rich Split values
  with Size-dependent bounds. Shared-code original-224 hidden-control checks
  and the full dual-bank plugin regression pass, including 44.1/48/96 kHz,
  varied blocks, switching/state/mono and no callback new/delete.
- Paired arm64 Release CPU: O3, FP contraction off, desktop denormal policy,
  untimed preparation/warmup, old/new/new/old order, two repeats per run.
  There are 1,056 timed rows across 22 programs and six workloads. Catalog
  median relative changes range from -0.30% to +0.071%; zero allocations or
  releases, including control edges. Rich Plate single active runtime:
  2.489% before, 2.482% after; same-program overlap 4.988% / 4.984%.
  Rich Split: 2.407% / 2.426%, overlap 4.868% / 4.866%. These native measurements
  exclude plugin retirement fades/host conversion and do not prove Daisy or
  physical DAW CPU margin. Two-runtime prepared storage grows 352 bytes,
  65,314,352 to 65,314,704; the immutable bank is unchanged at 5,828,152 bytes.
  Benchmark checksums change only for the two rich programs' combined
  Chorus/Mod/Size edge workload. All factory and other-catalog checksums match.
- Local Release AU, VST3 and Standalone are universal arm64/x86_64 builds and
  pass strict deep signature verification. Runtime tests execute on arm64;
  host listening, Cubase acceptance and x86_64 runtime acceptance are not claimed.
  Both final Chorus pages are captured and visually inspected.

There are 36 raw WAVs and 36 level-matched WAVs for the 12 saved input/control
cases, with reference/before/after variants. Gains match active-input RMS and
are shared between low/high Chorus for each input/program/variant, preserving
the control-dependent tail difference. No listening acceptance is inferred
from generating them. `listening-gains.json` records gains and peaks.

## Reproduction and retained evidence

Public code checks:

```sh
cmake --build build/plugin --config Release --target cineol_xl_rich_chorus_check native_hall_plugin_check
build/plugin/native-hall/cineol_xl_rich_chorus_check "$XL_BANK"
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --limits-check "$ORIGINAL_BANK" "$XL_BANK"
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --help-check "$ORIGINAL_BANK" "$XL_BANK"
```

Private, ignored evidence is under `build/validation/rich-chorus-fix-20261102/`:
`work.cpp`, `work.csv`, `compare.cpp`, frozen `baseline_bridge.cpp`/object,
`audio.csv`, `listening.cpp`, listening captures, `cpu.cpp`, paired CPU outputs,
commands, build/check logs, source hashes and both editor screenshots. The
comparison adapters are offline-only and require the user's verified v8.21
chips and v5 bank. They are not distributed or included in plugin processing.
Raw `.f32` containers were replaced with exact IEEE-float WAV wrappers; PCM
equality was verified and `cleanup.json` maps each removed path to its identical
replacement and hash. No captured samples, dependency resource, ROM, bank or
source asset was discarded. Reuse these retained inputs and metrics for a full
scheduler correction.
