# Original 224 hidden controls: native implementation

Date: November 2, 2026. This completes the Rate, modulation Depth and
Optimization Response implementation requested after the
[manual/ROM audit](MANUAL_CONTROL_AUDIT_2026-11-02.md).
XL Chorus and its controller timing remain the next separate correction.

## Result and user-visible behavior

Original-224 programs now have a third control page with **Mod Rate**,
**Mod Depth** and **Opt Resp**, each on the original unitless 1-16 scale.
All six programs expose them, including Acoustic Chamber. Both physical
faders and draggable compact-screen values support automation and balanced
gestures. Help distinguishes modulation Depth from normal reverb Depth and
Diffusion, and Optimization Response from a level threshold.

| Event | Effective settings |
| --- | --- |
| Factory / legacy session | Rate 2, Mod Depth 1, Optimization Response 5 |
| Rate 2-16 | Divider = Rate - 1; Mod Depth resets to 1, step 4 |
| Rate 1 | Divider 1; Mod Depth resets to 2, step 8 |
| A subsequent Mod Depth edit | Step = 4 * Mod Depth, replacing the previous step |
| Either Mode Enhancement transition in an original program | All three hidden settings reset to factory |
| Optimization enable transition | Hidden settings retained |
| Same-program selection | Hidden settings retained |
| A different algorithm | Hidden settings reset to factory |
| Sound preset / DAW session recall | Saved effective settings restored together, without replaying reset side effects |

For example, Depth 8 then Rate 2 ends at Depth 1; Rate 2 then Depth 8 ends at
Depth 8. Rate 1 followed by Depth 8 also ends at step 32, not 64. The UI and
host parameter queries show the effective Depth immediately.

Lower Optimization Response values allow faster optimization changes where
the level controller uses this period. Controller state and long-decay rules
can override it, as before. Its encoded level-drop comparison remains fixed;
no new threshold in dB was introduced. Factory Optimization remains 5 for all
six supported v4.4 programs, preserving the measured ROM/bank defaults rather
than substituting the older manual's plate recommendation of 7.

## Implementation and compatibility

`plugin/Original224Parameters.hpp` owns a canonical packed atomic snapshot.
The hidden parameter setters apply order-dependent changes directly; Mod and
algorithm parameter hooks apply the reset rules. Parameter setters do not
allocate, call host listeners recursively, or access files/locks. Canonical
`getValue()` and audio snapshots are immediate. The existing message-thread
timer publishes only stale dependent APVTS/UI mirrors, without replaying the
parameter setters. This also permits setting Depth back to its former value
before a pending mirror update.

Audio reads the three settings together and checks their algorithm/Mod context
against the ordinary parameter snapshot. Preset/session transactions suspend
event coupling while restoring canonical values. Session saving explicitly
captures the canonical settings, including an immediate save after a Rate
edit and before deferred mirror publication.

New parameter IDs are `mod_rate`, `mod_depth`, `optimization_response`, appended
at host indices 72-74 with version hint 8. The previous 72 host parameters,
plugin identities and AU/VST3 identifiers keep their indices/IDs. Original
programs gain a third page; XL page layouts and algorithms are unchanged.

Sound presets use schema **6**. Schemas 1-5 remain accepted; a complete schema-5
preset still has 69 sound parameters. Missing hidden values in old presets or
sessions explicitly restore 2/1/5, including when loading into an instance
whose hidden settings were already changed. New presets will require this or
a later compatible build. The prepared bank format and payload are unchanged.

`core/Controls` carries effective period/step/response overrides; zero retains
the prepared profile default for existing portable/native callers. Modulation,
decay state transitions and the desktop cycle/write predictions consume the
same effective values. Desktop hidden edits retain delay/tap/counter/decay
state but cancel pending predictions made with old values and restart the
native scan boundary. Mod transitions retain their existing compiler-seed
behavior. No ROM execution was added to processing.

The DSP structures are shared with the portable core, but the new controls
are exposed in the desktop plugin. No Daisy CPU margin or deployment is claimed.

## Independent ROM regression

Baseline is `f545a0b412de6b72de7617b1e4aa755f10fd2343` plus the previously
completed, uncommitted GitHub fixes. Core/desktop/plugin source and the old
plugin-check executable were frozen before this change. Reference is unchanged
Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`, with SHA-validated
original 224 v4.4 chips. Dependencies remain clean; only the normal build-only
reference export is used.

`native_224_hidden_controls_check` performs real Shift+6/+7/+8 and Pre-delay
operations, rather than patching ROM control RAM. Its 180 fixtures cover all
six programs, both edit orders, Rate 1/2/16, Depth 1/8/16 and Optimization
1/5/16. Input/detector stimuli include excitation, stopped input and retrigger.

| Check | Frozen old implementation | Corrected native implementation |
| --- | ---: | ---: |
| Modulation calls observed | 148,131 | 148,131 |
| Modulation state mismatches | 104,611 | 0 |
| Modulation instruction-cost mismatches | 17,790 | 0 |
| Level-controller calls observed | 8,223 | 8,223 |
| Level-state mismatches | 70 | 0 |
| Level instruction-cost mismatches | 0 | 0 |

The old implementation exits 1 on the meaningful new oracle; the corrected
implementation exits 0. The old reference fixture uses the frozen source
without runtime overrides, not a copied bank with modified defaults.

These are **state-aligned diagnostics**: native modulation starts from an
observed ROM procedure entry, and level-controller checks use observed entry
state/level words. ROM wait states are separated from native instruction-cost
predictions. This establishes the effective transition laws and procedure
costs; it is not independent free-running audio or complete bus-phase equality.

## Sound and performance comparisons

Independent before/after native/reference renders cover 48 fixtures per
revision: six programs, four effective setting combinations and two seeds/
levels. Each render is eight seconds at 48 kHz: 0.5 seconds silent warmup,
two seconds of noise/tones/transient, then 5.5 seconds of stopped input.
Reference and native clocks remain independent; no controller state, delay
memory or reference phase is injected into native audio.

The setting combinations, written as Rate / Depth / Optimization with Depth
written after Rate, are 2/1/5, 1/16/1, 16/1/16 and 2/8/5. Reference metrics are
identical between revisions in all 48 fixtures. All 12 factory native metric
sets are unchanged, while all 36 edited-setting native renders change.
A separate 24-fixture frozen/current comparison hashes every float on all four
outputs over running and stopped input, across every program and all four Mod/
Optimization combinations. All factory sample hashes match.

The independent sound comparison retains the existing startup/phase and
converter differences. It does **not** establish universal sonic improvement:

| Diagnostic absolute RMS difference from reference, median dB | Before | After |
| --- | ---: | ---: |
| Running interval, 1.5-2.5 seconds | 0.517 | 0.387 |
| Initial stopped interval, 2.5-4 seconds | 0.999 | 0.763 |
| Later stopped interval, 4-8 seconds | 1.447 | 1.512 |

The largest later-window difference changes from 7.960 to 8.301 dB. These are
level/phase-sensitive diagnostic windows, not RT60 estimates or acceptance
limits; some cases worsen. The correction restores measured parameter/control
behavior, while full sonic acceptance remains open. Large Hall fast/deep and
slow/shallow raw before/after/reference files are retained, plus explicitly
level-matched listening pairs with gain/window metadata. No listening approval
is claimed.

Paired Release CPU uses frozen/current/current/frozen order, matching compiler,
host, FTZ and input recipes, with preparation excluded. The original-engine
matrix has six repeats per program/mode and 108 unchanged factory checksums.
Median relative core overhead is +0.71%, maximum +2.91% across the 18 workloads.
The plugin's mean single-engine cost is 2.200% -> 2.222% of one core; Spillover
overlap is 4.342% -> 4.378%. These small desktop measurements do not guarantee
realtime margin on other systems. Extreme current settings are measured
separately; they are not a paired before/after performance conclusion.

An additional current-only plugin stress measurement applies Rate/Depth/
Optimization endpoint edits inside every timed 128-frame block, including
Spillover transitions. Mean single/overlap costs are 2.165% / 4.244%; the largest
observed first overlap block is 0.137 ms. Setter/callback allocations and
releases remain zero. This control-edge workload restarts pending predictions;
it is a separate diagnostic, not evidence of an optimization over steady sound.

The desktop original engine grows from 42,240 to 42,256 bytes (+16 bytes).
ROM banks, XL graph storage and the prepared-bank ABI remain unchanged.

## Plugin and processing validation

- `--hidden-check`: all six programs, every Rate position, special Rate 1,
  edit ordering, both Mod edges, Optimization retention, same/different
  programs, canonical host values, immediate session saves, preset/session
  recall, schema-5 migration, full/compact bindings and moving controls.
  A further directed-switch check compares non-default hidden-control plugin
  audio sample-for-sample with the desktop engine through the host transport.
- Full dual-bank `--banks`: all 28 algorithms, stable legacy indices, file/
  bank/quick presets, recall audio, Spillover, 44.1/48/96 kHz, mono/stereo,
  block sizes including 1/127/128/511/512/20,000 as applicable, and finite output.
- `--display-check`, `--help-check`: full/compact operation, bidirectional value
  gestures and help on 127 pages / 1,129 active screen bindings.
- `--bypass-check` and `--clock-experiment-check`: both engines and the previous
  bypass/Space/Dirt/quad regressions; 108 moving clock/Dirt/link cases pass.
- Existing original-controller test: startup, held detectors, sparse peaks,
  program continuity and 1,080,000 control passes pass.
- Callback allocation/release instrumentation reports zero. The direct new
  parameter setters are additionally instrumented with audio tracking and
  report zero allocation/release.

Two older test expectations were updated for the intentional third page and
schema 6. No failing DSP check was relaxed. AU/VST3/Standalone Release bundles
were rebuilt for arm64 and x86_64 and passed strict ad-hoc signature checks.
The updated editor screenshot was rendered and visually reviewed at 1104x696.
Bundles are built locally, not installed or published; no DAW listening or
physical hardware acceptance is claimed.

## Reproduction and evidence

Ignored evidence is retained in `build/validation/hidden-controls-fix-20261102/`:
frozen baseline, ROM command/hash provenance, per-fixture CSVs and summaries,
plugin/build logs, artifact hashes, screenshot, audio sources/results and CPU
measurements. Private ROM/bank locations appear only in ignored provenance.
The generated-material review removed only four byte-identical duplicate
input/reference WAV copies; `cleanup.json` records each retained replacement
and hash. Raw native revisions and listening pairs remain available.

```sh
cmake --build build/plugin --parallel 2 --target \
  native_hall_plugin_check native_224_hidden_controls_check \
  NativeHall224_AU NativeHall224_VST3 NativeHall224_Standalone
build/plugin/native-hall/native_224_hidden_controls_check \
  ORIGINAL_ROM_DIRECTORY ORIGINAL_BANK OUTPUT_CSV
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check \
  --hidden-check ORIGINAL_BANK XL_BANK
```

The offline audio setup initially exceeded Reflexion's fixed coroutine frame
size after optimization. Applying `clang::optnone` to that **test setup coroutine**
keeps physical input operations unchanged and fits the existing allocator;
the render engines remain optimized. No dependency or production allocator
was changed. The failed log is retained, not counted as a passing render.
