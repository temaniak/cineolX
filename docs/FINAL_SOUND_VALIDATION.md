# Original-224 desktop regression and acceptance checkpoint

October 5, 2026. Baseline: `0ebb668`. Reference dependency remains clean at
Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. JUCE: 8.0.14.
Scope: original-224 application/VST3/Standalone; no Daisy or XL sonic work.
Implementation/test checkpoint: `ce45a03`.

This report retains the earlier Windows regression checkpoint. The later
[native scan report](NATIVE_CONTROL_SCAN_VALIDATION.md) documents the integrated
stable-control clock and new macOS sound/CPU/processor/Spillover checks.

## Status

The direct-switch counter correction and the current validation tooling pass
regression. **The overall sound-accuracy objective is not yet complete.**
Precise control scheduling and normal-startup modulation-on tail equivalence
were open at this checkpoint; diagnostic alignment is not the shipped plugin.
Stable-control scheduling has since been corrected and verified, while startup
phase and within-call audio updates remain open. The preceding
[modulation](MODULATION_VALIDATION.md), [residual](RESIDUAL_SOUND_VALIDATION.md)
and [gain/timing](GAIN_TIMING_VALIDATION.md) reports distinguish these limits.

The new tooling removes the dependence on ignored local test sources. Another
computer can regenerate private evidence from its own supported ROM1–ROM5 and
continue from this branch. No private assets are part of the checkpoints.

## Current code and compatibility verification

- Windows x64 Release VST3 and Standalone built successfully.
- All three ROM-free ADC/DAC/controller CTests passed.
- Optional six-WCS ADC/DAC/controller source/timing tests passed.
- Six integer networks matched independent row-machine output, arithmetic,
  saturation and delay memory over 12,000 full-scale stereo frames and 47
  combined control settings each. Shared SRC/cached-control checks passed.
- The desktop controller check passed 1,080,000 processing passes with no
  allocation, including the new direct-switch counter-retention/index checks.
- Processor versus desktop engine was exact over all 36 directed switches,
  simultaneous predelay/mode changes and 48 kHz blocks.
- Processor checks passed for all six programs at 44.1/48/96 kHz, mono/stereo,
  128/511/20,000-frame blocks, offline/realtime equivalence, saved state,
  automation and low-latency dry/wet behavior. Audio allocations/releases: zero.
- Cached-bank restart passed with an isolated cache; existing parameter IDs,
  plugin IDs, session formats and version-1 bank ABI were retained.
- The original-only Spillover suite passed all 30 directed pairs at each of
  three sample rates, three block sizes, rapid retirement, early disable,
  mono/stereo common dry and low latency. Old tail + fresh network agreed
  with independent processors at 1/5/10 second fade durations, maximum error
  **7.45e-9**. No callback allocation or release was recorded.

The desktop-engine comparison now prepares its first selected program directly,
matching the processor's first activation. Starting it with an unrelated program
then switching was a different counter history once continuity was corrected.
Large independent Spillover reference processors are allocated before rendering
in the test harness: the former stack fixtures overflowed Windows' stack before
the test ran. These are test-fixture fixes, not new callback allocation.

Mixed 224/XL fixtures and hardware listening/realtime margin were not validated
in this original-only work. Musical strike/chord evidence from the preceding
ADC/controller reports remains available; this checkpoint adds a portable
`music` fixture but does not claim a new six-program musical/listening acceptance.

## Paired CPU and storage

`script/benchmark_sound.py` independently compiled baseline/current source in
Release with strict FP and x86 FTZ/DAZ. Five alternating run pairs, each with
three repetitions per case, covered six programs × three modes. Two seconds
of identical 48 kHz audio were timed per run, excluding preparation and warmup.
No other build/render job ran during measurement.

Summed per-case median processing-time ratio: **1.01676 (+1.68%)**. Individual
cases ranged from **0.99732 to 1.02866**. Non-switching output checksums agreed.
The new correction runs only at direct switches; the small steady-run difference
includes compiler layout/timing variation. No substantial increase was observed
in these workloads. This is a relative internal benchmark against `0ebb668`,
not the CPU percentage of Windows/DAW or a cumulative claim from all prior steps.

Desktop engine size remains **82,088 bytes**; default engine **57,040 bytes**;
prepared bank **123,460 payload bytes**, version 1. No per-sample storage was added.

## CPU while programs overlap

Because the user currently uses Spillover, its cost was measured separately
in the **actual processor callback**, excluding input generation, construction,
warmup and parameter/UI operations. Each of six adjacent program pairs had
five alternating single/overlap trials at 48 kHz, 128-frame blocks. The timed
window was 1.024 seconds inside an active 10-second overlap; both networks ran.

Median overlap/single ratios were **1.921–2.103**, summed ratio **1.995**.
Callback processing used **1.845–1.958%** of the audio duration for one network
and **3.686–3.941%** during overlap on this computer. These fractions are not
whole-DAW CPU utilization. The largest measured first switching callback was
**0.2094 ms** for a 2.667 ms audio block; this is an observed test value, not
a worst-case scheduling guarantee. New/delete counts were zero.

The roughly doubled overlap cost is the existing two-network transition design.
It is distinct from the +1.68% baseline/current comparison. No change to the
Spillover mixer, fade lengths, old-tail state or fresh-instance startup was made.

## Reproduce and continue

Configure with `CINEOL_BUILD_PLUGIN=ON` for the plugin targets below and supply
`NATIVE_HALL_ROM_DIR` privately. A tools-only configuration cannot build the
plugin checks or VST3/Standalone targets.

```sh
cmake --build build/sound-validation --config Release --target NativeHall224_VST3 NativeHall224_Standalone native_hall_plugin_check native_224_modulation_check native_224_sound_compare native_224_gain_timing_check
ctest --test-dir build/sound-validation/native-hall -C Release --output-on-failure
native_hall_plugin_check --bank build/sound-validation/native-hall/programs-v44.bank224
native_hall_plugin_check --spillover-224-check build/sound-validation/native-hall/programs-v44.bank224
native_hall_plugin_check --spillover-224-cpu build/sound-validation/native-hall/programs-v44.bank224
python script/benchmark_sound.py 0ebb668 build/sound-validation/native-hall/programs-v44.bank224 build/cpu-checkpoint
```

Do CPU runs separately from builds/tests/renders. Processor tests seed their own
temporary cache, never the user's cache. On this Windows machine private evidence
and logs are under `build/validation/modulation-20261005/` and
`build/validation/residual-20261005/`. Executable paths depend on the generator.

Next required work: derive and validate a bounded native model of the firmware's
unequal, signal/state-dependent control-call timing; keep the exact step laws.
Then rerun the normal/aligned all-six matrix over several seeds/start phases,
input levels and musical fixtures, followed by CPU and Spillover regression.
Investigate Chamber above the quantization floor using actual converter words/
clock origin if its bias persists. Do not close the sound-accuracy roadmap item
merely because compilation/tests pass or an aligned diagnostic looks close.
