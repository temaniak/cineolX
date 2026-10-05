# Original-224 native desktop control scan

October 5, 2026. Baseline: `bdbc724` on
`codex/original-224-sound-accuracy`. Reference: original Lexicon 224 v4.4
ROM1-ROM5, Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.
Host: macOS arm64, AppleClang Release with strict FP.

## Result and scope

The desktop bank engine now uses a native event clock instead of evenly
spacing eighteen modulation calls at the bank's nominal rate. The clock
accounts for modulation branches, the separate high/conditional-low input
reads, nine stable panel comparisons, level/history/peak release work,
diffusion coefficient updates and WCS access waits. Its full sequence matches
the independently executed firmware after one initial alignment per case.

This is a completed timing correction for stable controls, with a retained
audio approximation. It does not reproduce firmware boot/program compilation
or apply individual WCS bytes within the fixed audio graph. Normal-startup
sound comparisons remain separate from aligned diagnostics; overall sonic
acceptance is still open.

No EQ, analog filter, feedback coefficient table, plugin identifier, parameter
ID or bank layout was changed. Banks remain version 1 with a 123,460-byte
payload. Reported/dry/bypass latency remains unchanged. No Daisy build or
dependency checkout modification was made.

## Implementation

- `desktop/control_scan224.hpp` stores native controller stages and an absolute
  2.048 MHz clock. It contains no CPU registers, opcodes or ROM memory.
- `desktop/level_timing224.hpp` models the level-controller costs, including
  its integer divide/multiply branches and up to twelve coefficient writes.
  Coefficient templates are ordinary ROM reads, not WCS reads or bus waits.
- `desktop/scan_timing224.hpp` describes stable panel comparison costs and
  the longer transfer-read gap in the one-tap plate programs.
- `desktop/control_timing224.hpp` retains the shared WCS protect-pair clock.
  It now skips stretches where protect/reset inputs cannot change the pair
  flip-flop. Falling/rising edges, displaced fetches and reset boundaries are
  still processed. This reduces work without changing grants or write waits.
- `desktop/controllers224.hpp` connects the clock to `DesktopHall`. It samples
  high and low bytes at their separate edges. A read can cross from one input
  channel to the other. Transfer visibility follows fetch/execute/capture,
  rather than switching immediately at rows 0 and 50.

Headroom holds are separate for left and right. The engine passes two five-bit
detector masks to the desktop core; the portable core's compile-time policy
continues receiving their original union. Right capture belongs to row 4;
left capture at row -46 supplies the next pass. The adapter resolves previous
pass control events before running the next audio pass, without delaying audio
or adding a sample to the advertised latency.

The shipped virtual panel uses canonical settled pot comparisons: each of six
cached readings is two ADC codes below the current value, with released switch
banks. Offline tests instead capture the actual stable panel context once.
Live parameter setters remain immediate. Direct program switches retain the
previously validated modulation counters and decay/history state, then restart
the scan for the new network. Spillover slots keep independent clocks.

Legacy single-Hall profiles retain their prepared nominal scan because their
format does not describe the full six-program context. The portable `Hall`
processing schedule remains unchanged.

## Independent clock and state verification

The ROM tools validate all five input chips by SHA-256. They never run in an
audio callback. Private firmware listings, ROMs, banks, WCS files and captures
are excluded from Git.

| Check | Coverage | Result |
| --- | --- | --- |
| Local modulation duration/write oracle | 576 cases; 238,967 calls; 328,567 writes | Zero instruction, whole-call or individual-write mismatches |
| Local level duration/write/state oracle | 168 cases; 11,242 calls | Zero instruction, whole-call, write or final-state mismatches |
| Free-running steady scan | 72 cases; 141,833 events | Zero clock, controller-state or input-byte mismatches |
| Free-running changing-input scan | 24 cases; 118,269 events | Zero clock, controller-state or input-byte mismatches |
| Desktop streaming adapter | 360,000 passes across six programs with mode changes | Exact agreement with the event clock; zero allocations |

The local modulation matrix is the same six-program/six-depth/four-mode/four-
amplitude test documented in [the timing foundation](CONTROL_TIMING_VALIDATION.md).
It was repeated after the sparse grant-clock optimization.

The local level matrix uses means 0/1/7/16/24/25/31, all four modes and all six
programs. Each case starts loud and then falls to silence, exercising held
peaks, history, release, diffusion dividers and coefficient updates. Its oracle
receives actual entry state/time and level word at each call. The first two
calls are excluded from bus-history assertions; instruction/state checks remain.
Half-gain and remote/display-bank transitions are outside this test.

The full scan uses Bass/Mid 3/15, Depth 35, minimum predelay, all six programs
and four modes. Clock, controller state and panel context are captured only at
the first modulation entry of a complete scan. All future events and states
are then generated before the reference executes them; there is no subsequent
clock/state alignment. The steady cases use ADC mantissas 0/128/2047 for 800 ms.
The changing-input cases run for two seconds, cycling eight positive, negative
and silent mantissas every 20,000 CPU states, with different left/right phases
and detector masks. ADC loads and held-detector sampling have their independent
FPC boundaries. Compared events include both input-byte reads and the level word.

All seven ADC/DAC/controller/Gain CTests passed. The VST3 and Standalone built
and were ad-hoc signed/verified on macOS. The processor suite passed sample
rate/block-size/state/mono checks with callback `new=0, delete=0`. The original-
224 Spillover suite passed all thirty directed pairs at 44.1/48/96 kHz,
1/5/10-second tails, early disable, rapid changes and low-latency/dry mixing.
Maximum independent-reference overlap error was `7.45058e-09`.

## Normal-startup sound comparison

`script/compare_sound_checkpoint.py` compiles separate frozen baseline and
current engines and runs thirty paired 12-second fixtures. Each pair's input
and full-firmware reference WAVs must be byte-identical. The matrix comprises
six programs with noise seed 17/amplitude 0.08/modes 0,2,3/1000 ms warmup;
six additional mode-3 noise cases with seed 224/amplitude 0.12/1000 ms warmup;
and six mode-3 music cases with seed 73/amplitude 0.12/1047 ms warmup.
Other controls and fit criteria match [the residual report](RESIDUAL_SOUND_VALIDATION.md).

| Normal native fixture | Accepted broad-band fits, baseline/current | Mean absolute T20 error, baseline/current |
| --- | ---: | ---: |
| Noise 17, both off | 42 / 42 | 11.38% / 11.38% |
| Noise 17, decay only | 42 / 42 | 11.27% / 11.34% |
| Noise 17, both on | 42 / 42 | 4.62% / 4.00% |
| Noise 224, both on | 42 / 42 | 5.06% / 5.09% |
| Both modulation-on noise groups together | 84 / 84 | 4.84% / 4.54% |

Both-off native WAVs were byte-identical in all six programs. The modulation-
on improvement is uneven: in the seed-17 group, maximum Percussion Plate error
fell from 15.22% to 5.29%, while Chamber changed from 11.36% to 11.61%.
The seed-224 group did not show an aggregate improvement. Across both noise
groups the maximum changed from 19.78% to 19.72%. These results support the
clock correction, not a phase-independent tail-equivalence claim.

Only five baseline and four current music fits out of 42 broad bands passed
the fit criteria. Mean errors from those different subsets are unsuitable for
an improvement claim. All rejected fits remain in the private CSVs. Maximum
broad-band integrated music energy discrepancy was 1.43/1.40 dB. Neither aligned
variants nor rejected quiet-floor slopes replace the normal-startup results.

## CPU and storage

The final paired Release benchmark against `bdbc724` measured a summed-median
CPU ratio of **1.019531**: +1.95%. Eighteen program/mode workloads ranged from
+0.90% to +2.53%. Each case has fifteen timed samples from five alternating
baseline/current runs. Preparation and warmup are excluded; no build, render
or other test ran during measurement.

A separate paired run against the original desktop baseline `0ebb668` on this
same macOS host measured **+2.10%** in summed medians, with case ratios
+1.41% to +2.78%. Do not combine these numbers with earlier Windows percentages;
the machines and independently compiled checkpoints differ.

The benchmark now enables ARM64 FZ as well as x86 FTZ/DAZ, matching JUCE's
`ScopedNoDenormals` policy on the validation host. Both independently compiled
checkpoints use the same current measurement helper. Earlier macOS trial runs
without explicit ARM FZ are not the final acceptance numbers.

The event-clock optimization did not change any output checksum in the
eighteen workloads compared with the first integrated clock. Fixed
`DesktopEngine48` storage is 82,216 bytes versus 82,088 at the baseline:
128 extra bytes per engine, or 256 for two original-224 Spillover slots.
This is a desktop comparison; it establishes no Daisy realtime margin.

## Reproduction and remaining work

```sh
cmake --build build/sound-validation --config Release --target native_224_control_timing_check native_224_level_timing_check native_224_scan_timing_check native_224_controllers_check native_224_sound_compare
native_224_control_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/modulation-timing.csv
native_224_level_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/level-timing.csv
native_224_scan_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/scan-steady.csv steady
native_224_scan_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/scan-dynamic.csv dynamic
ctest --test-dir build/sound-validation/native-hall --output-on-failure -R "native_224_(adc|dac|controller|gain)"
python script/compare_sound_checkpoint.py bdbc724 "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/sound-checkpoint
python script/benchmark_sound.py bdbc724 build/sound-validation/native-hall/programs-v44.bank224 build/cpu-checkpoint
```

Run CPU benchmarks separately from builds, renders and other tests. Plugin
tests seed an isolated temporary cache; run their `--bank` and
`--spillover-224-check` invocations sequentially. Never seed the user's cache.

Remaining sound work is initial/program-load phase and within-call coefficient
application. The current audio graph applies coefficient groups at completed
calls on its 100-row pass boundary. The clock models WCS waits, but the graph
does not reproduce displaced fetches or operand-clock holds. A subsequent
[WCS audio boundary isolation](WCS_AUDIO_BOUNDARY_VALIDATION.md) found these
write-side effects audio-invariant in all six stable stock graphs, with
53,382 grants and 160,146 held edges. Coefficient payload timing remains a
separate question. Compiler/predelay
transition timing and remote/cartridge mode changes are outside this stable-
control model. Keep phase ensembles and normal startup distinct from diagnostics;
do not tune feedback or EQ to compensate for one valid frozen tap phase.

Private evidence is under `build/validation/scheduler-20261005/`: public tool
CSVs, `sound-matrix`, processor/Spillover logs and paired CPU exports.
