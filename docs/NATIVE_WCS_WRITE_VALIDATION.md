# Original-224 native individual WCS writes

October 5, 2026. Desktop baseline: `0417cf1`. Cause-isolation checkpoint:
`512f7ba`. Branch: `codex/original-224-sound-accuracy`. Reference: original
Lexicon 224 v4.4 ROM1-ROM5 and Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Host: macOS arm64,
AppleClang Release with strict FP.

## Result and scope

Later [startup/key validation](ORIGINAL_224_STARTUP_VALIDATION.md) verifies
the bank/compiler seeds and corrects Mod-key seed resets. Its separate
compiler-start recipe explains the large historical frozen-phase differences;
the measurements in this report retain their original reference recipe.

The original-bank desktop engine now makes each predicted coefficient or
delay-address write visible at its target DSP fetch. It no longer waits for
the whole controller procedure to return and the next audio pass to begin.
This corrects the retained stable-control audio approximation in the
[native scan checkpoint](NATIVE_CONTROL_SCAN_VALIDATION.md).

The fixed native networks remain intact. No row interpreter, ROM execution,
allocation, I/O, lock, wait or extra audio sample was added to processing.
The portable engine and legacy single-Hall schedule retain their behavior.
Plugin identifiers, parameter IDs, reported/dry/bypass latency and version-1
bank layout remain unchanged: a 123,460-byte payload.

This is a completed individual-write correction for stable stock programs.
Native startup/program-load phase and compiler/predelay transition timing
remain approximations. Overall normal-startup sonic acceptance is still open.
No EQ, analog filter or feedback table was retuned. No Daisy build or dependency
checkout change was made.

## Implementation

`desktop/control_write224.hpp` describes a coefficient or address-low-byte
write. The modulation and level branch-cost models now predict its value and
target as well as its duration. The modulation routine writes the second
interpolation coefficient before the first; this order was verified against
actual CPU writes. Level writes retain the existing signed gain/complement
laws and program-specific loop rows.

`ControlScan224` emits the first fetch that can see each committed byte. The
reference's physical row numbering and CPU-state numbering have different
epochs; the free-running oracle maps them once at initial alignment, alongside
its existing first-entry PC/scan alignment. It receives no later actual clock,
state or write timing to generate the native sequence.

`desktop/control_write_queue224.hpp` holds at most twelve predicted writes.
Each target is fetched once per 100-row pass. Before rendering, the queue
selects its old or new value according to that fetch. After rendering, it
retains all values committed during the pass in write order. A commit after
the target's fetch therefore affects its next pass; multiple writes to one
target retain the final committed value. No per-row runtime check is inserted
into `core/program_networks.inc` or its arithmetic nodes.

Only bounded coefficient/address setters were added to `Hall`. Controller
descriptors, counters and decay/history still commit at completed procedures;
firmware RAM visibility during a procedure is not emulated. Their next-call
state matches the ROM. The desktop streaming adapter can predict writes early
enough from completed input holds without delaying audio. Direct switches
clear the old program's pending queue; Spillover slots retain independent
queues and scan clocks.

The [boundary-isolation report](WCS_AUDIO_BOUNDARY_VALIDATION.md) showed that
write-side displaced fetches and operand holds leave the six stable stock
graphs' audio invariant. Their protect-pair transitions remain in the grant
clock because they affect subsequent write waits. Coefficient payload timing
is the separate effect corrected here. WCS reads and compiler transitions are
outside that invariance result.

## Independent verification

| Check | Coverage | Result |
| --- | --- | --- |
| Local modulation timing/value/target oracle | 576 cases; 238,967 calls; 328,567 writes | Zero timing, target or value mismatches |
| Local level timing/value/target/state oracle | 168 cases; 11,242 calls | Zero timing, target, value or final-state mismatches |
| Free-running steady scan | 72 cases; 141,833 events; 109,845 writes | Zero clock, state, input-byte, payload or first-visible-fetch mismatches |
| Free-running changing-input scan | 24 cases; 118,269 events; 93,021 writes | Zero clock, state, input-byte, payload or first-visible-fetch mismatches |
| Fixed-graph write queue | 72,000 full-scale stereo passes; 216,000 mixed writes | Exact DAC words, ARU state and complete delay memory against row-by-row application |
| Desktop stream/control adapter | 360,000 passes across six programs with mode changes | Exact controller state and completed coefficient groups; zero allocations |

The local oracles retain their documented first-two-call bus-history exclusion;
payload assertions cover every observed write. The full-scan cases use only
their initial alignment, then generate future events and payloads before the
reference executes them. See the scan report for their signal/control matrix.

The queue test generates coefficient/address changes explicitly before, at
and after target fetches, including multiple changes in one pass and a full
twelve-entry queue. Its independent machine applies them row by row. The
native side selects values before rendering its fixed graph; it does not
reuse the reference's row execution or the queue's selection as its oracle.

All nine relevant ADC/DAC/controller/Gain/WCS CTests passed. VST3 and
Standalone built and were ad-hoc signed/verified. Processor checks passed
44.1/48/96 kHz, 128/511/20,000-sample blocks, state and mono cases with callback
`new=0, delete=0`. Original-224 Spillover checks passed all thirty directed
pairs, 1/5/10-second tails, early disable, rapid changes and low-latency/dry
mixing. The tests use isolated temporary caches and leave the user's cache alone.
The checker now rejects missing bank arguments or unsupported option/count
combinations before GUI/cache initialization, rather than silently running a
different default suite.

## Normal-startup sound matrix

Thirty paired fixtures use separately compiled frozen `0417cf1` and current
engines. Every pair has byte-identical input and full-firmware reference WAVs.
The same private bank snapshot is used throughout. The fixture and fitting
criteria match the previous scan report: noise 17/amplitude 0.08 in modes
0/2/3, noise 224/amplitude 0.12 in mode 3, and music 73/amplitude 0.12 in mode 3,
for all six programs and twelve seconds per fixture.

| Normal native fixture | Accepted broad bands, baseline/current | Mean absolute T20 error, baseline/current | Maximum, baseline/current |
| --- | ---: | ---: | ---: |
| Noise 17, both off | 42 / 42 | 11.38% / 11.38% | 56.35% / 56.35% |
| Noise 17, decay only | 42 / 42 | 11.34% / 11.26% | 56.10% / 56.10% |
| Noise 17, both on | 42 / 42 | 4.00% / 3.87% | 19.72% / 13.51% |
| Noise 224, both on | 42 / 42 | 5.09% / 4.98% | 18.64% / 18.86% |
| Both modulation-on noise groups | 84 / 84 | 4.54% / 4.43% | 19.72% / 18.86% |

All six both-off native WAVs are byte-identical. The modulation-on improvement
is uneven; the second seed's worst band became slightly worse. Maximum
broad-band integrated energy discrepancies are 0.39/0.40 dB for noise 17 and
0.37/0.36 dB for noise 224. This is a write-timing correction with modest
aggregate tail improvement, not a universal spectral or decay-equivalence claim.

Only four of 42 music bands passed in each checkpoint. They remain too sparse
for overall music-tail acceptance. Maximum integrated music-energy error is
1.40 dB in both, before rounding. All rejected fits remain in the private CSVs.

The aligned variant remains diagnostic. After externally restoring its
controller snapshot, the tool now discards old pending writes and restarts a
canonical scan. It does not copy reference CPU phase, converter timing or
delay memory. These aligned results must not replace the normal native matrix
or be compared as an unchanged alignment protocol with older checkpoints.

## CPU and storage

The paired Release benchmark against `0417cf1` measured a summed-median CPU
ratio of **1.008605**: **+0.86%** for individual-write visibility. Eighteen
program/mode workloads ranged from +0.02% to +1.75%. Each case has fifteen
timed samples from five alternating baseline/current runs. Preparation/warmup
are untimed. No build, render or test ran during measurement. Both independently
compiled checkpoints use the same helper and ARM64 FZ policy as the plugin.

A separate paired run against the original desktop baseline `0ebb668` measured
**1.032320**, or **+3.23%** overall, with case ratios +1.35% to +4.54%. These are
independent paired measurements; do not add rounded incremental percentages or
combine them with historical Windows measurements.

Fixed `DesktopEngine48` storage is 82,416 bytes: +200 bytes versus `0417cf1`,
or +328 versus `0ebb668`. Two original-224 Spillover slots add 400/656 fixed
bytes respectively. The CPU matrix measures one engine; functional Spillover
tests do not establish a separate overlap CPU budget. No MCU margin is inferred.

## Reproduction and remaining work

```sh
cmake -S . -B build/sound-validation -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON -DNATIVE_HALL_ROM_DIR="/private/path/224 v4_4" -DCMAKE_BUILD_TYPE=Release
cmake --build build/sound-validation --config Release --target native_224_control_timing_check native_224_level_timing_check native_224_scan_timing_check native_224_wcs_queue_check native_224_controllers_check native_224_sound_compare
ctest --test-dir build/sound-validation/native-hall --output-on-failure -R "native_224_(wcs|controller|gain|adc|dac)"
build/sound-validation/native-hall/native_224_control_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/modulation-writes.csv
build/sound-validation/native-hall/native_224_level_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/level-writes.csv
build/sound-validation/native-hall/native_224_scan_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/scan-writes.csv dynamic
python script/compare_sound_checkpoint.py 0417cf1 "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/sound-writes
python script/benchmark_sound.py 0417cf1 build/sound-validation/native-hall/programs-v44.bank224 build/cpu-writes
# In a separately configured plugin build, run these sequentially:
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --bank build/sound-validation/native-hall/programs-v44.bank224
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --spillover-224-check build/sound-validation/native-hall/programs-v44.bank224
```

Run CPU measurements separately from all builds, renders and tests. Keep the
bank fixed throughout paired runs. Run plugin `--bank` and
`--spillover-224-check` tests sequentially; their cache creation is not concurrent.

The next sound investigation is native initial/program-load phase versus the
firmware compiler's startup state. Preserve cached-bank/session compatibility
and separate normal phase ensembles from controlled alignment experiments.
Compiler/predelay retiming, half-gain and remote/display-bank transitions remain
outside the stable-control proof. Do not fit EQ or feedback to one frozen phase.

Private evidence is under `build/validation/scheduler-20261005/`:
`modulation-payload`, `level-payload`, `write-visibility-{steady,dynamic}`,
`write-queue-ctest`, `write-runtime-regression`, `write-plugin-check`,
`write-spillover-check`, `write-sound-matrix`, `write-cpu-fz` and
`write-cpu-original-fz`. ROMs, banks, WCS images, captures and binaries remain
excluded from Git.
