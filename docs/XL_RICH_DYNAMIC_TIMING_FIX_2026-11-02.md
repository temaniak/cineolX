# Rich XL Dynamic Decay / Optimization timing correction

Date: November 2, 2026. This follows the Chorus availability/work-budget fix.
The user explicitly accepts small timing differences and does not require
mathematical identity. This block removes the large input/mode-dependent clock
error in **Rich Plate and Rich Split**, without broadening the sound change to
the other 20 XL programs or original 224.

## What changes

The old native path advanced independent fast/slow phase accumulators. Even
with the Chorus-dependent average budget, a complete gradual LF/MID fall or a
multi-step Optimization release happened instantly. ROM time spent compiling
coefficients never postponed the next poll. The resulting combined-mode clock
could remain near 92 Hz while the physical ROM reference ran near 70–73 Hz.

`RichControllerXL` now connects the previously checked native slow-entry,
eight-position LF/MID transition, decay compiler, post-ramp/Optimization,
headroom-display, nine-pass monitor and five-fast-call stages. Each stage's
work advances the following stage; the fixed-rate Rich correction is removed.
Chorus executes the zero-destination ROM branch, including the retained period
and random-divider work, rather than an averaged rate. Disabled Mod uses its
fixed delay. No interpolation destination, pitch LFO or extra delay is invented.

Native T&C grant timing supplies bounded pending coefficient writes. They become
visible progressively at DSP-pass boundaries, retaining LF/MID and feedback
commit separation. Fresh per-channel detector accumulation/read-clear and
monitor reads drive the native control laws. The small UART worker uses its
own character deadlines; it never sends bytes or performs I/O.

Host control requests coalesce into a fixed coherent snapshot and wait until
the current scan/compiler finishes. LF/MID, STOP, STOP DLY, Size and mode updates
preserve elapsed clock, current held level and in-flight tail state. Static
controls and Size are refreshed coherently, including Size-dependent decay
cache invalidation. Identical requests and cancelled pending values compile no
transaction. Program activation still resets the selected engine normally.

The work clock contains no ROM/opcode interpreter, reference event injection,
measured poll rates, locks, sleeps, allocation or file/network I/O. The private
October 6 native coupled pilot supplies the already validated semantic stage
assembly; its global traces/counters are removed. Diagnostics are bounded,
per-instance counters. Non-Rich storage uses an empty type. Bank v5 layout,
plugin/parameter IDs, presets and sessions are unchanged.

## Baseline and reference

HEAD is `f545a0b412de6b72de7617b1e4aa755f10fd2343`, branch
`codex/clock-experiment`, plus the preceding uncommitted fixes. Before this
block, core/desktop/plugin/tests and the plugin-check executable were frozen
under `build/validation/xl-dynamic-timing-fix-20261102/baseline/`, with hashes.
The independent audio baseline is compiled from that unmodified tree in a
separate namespace. A second old-header copy adds only poll counters for the
cadence regression; it is not used for audio or performance measurements.

Reflexion remains clean/pinned at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`
through the existing build-only export. Private XL v8.21 chips are hash checked.
The unchanged v5 bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Original-224 v4.4 support and hidden Rate/Depth/Optimization are preserved.

## Timing and behavior validation

The independent physical-ROM observations use a fresh boot/physical selection,
verified toggle operations and one-second seeded noise. New native tests use
independent clocks, a half-second silent prelude and the same noise level. The
24 cases cover both graphs, Mod-only/off/all-three/Dynamic+Optimization and
Chorus 0/128/248. Tolerance is 5%, with two-poll allowance for finite one-second
counts; this is deliberately not a phase-identity test.

| Slow cadence result | Previous Rich budget | Coupled Rich clock |
| --- | ---: | ---: |
| Cases outside tolerance | 12 / 24 | 0 / 24 |
| Median relative discrepancy | 7.18% | 1.33% |
| Maximum relative discrepancy | 31.49% | 4.34% |

With Dynamic Decay and Optimization on, the corrected rates are 45 Hz at low
Chorus and 73 Hz at high Chorus for both programs. The observed ROM ranges are
44–46 and 70–73 Hz. Mod off gives 44–46 Hz, also matching the observed workload
range instead of the old approximately 52 Hz. Fast calls pass the same tolerance.

`cineol_xl_rich_timing_check` also verifies real gradual falls (no direct jump
to Stop Decay), finite stopped/retriggered tails, live state/clock retention,
request cancellation/coalescing, latest-value commit and mode/Size changes.
`cineol_xl_rich_chorus_check` retains all 32 work-budget positions, upper-half
invariance, Mod-off invariance and audible-tail response coverage. The old
unused average-clock assertion has been replaced by actual engine cadence
checks, rather than testing a helper no longer used in processing.

## Independent sound comparison

Forty noise/tones cases cover both graphs, all eight modes at seed 17 and the
original level, plus Optimization/combined modes at seed 99 ten times quieter,
with low/high Chorus. Eight further chord/percussion cases supply stopped and
retriggered listening material. At 48 kHz, each capture has one silent second,
two input seconds and three stopped seconds, and observes all four outputs.
Both native runtimes receive only the same logical operator values and input;
no reference RAM, controller state, DSP memory, phase or event clock is copied.
All 12 saved reference PCM captures match the preceding block byte-for-byte.
Saved listening WAVs use A/C; the fixture CSV metrics cover all four outputs.

Diagnostic high-minus-low Chorus effect in the first stopped second, dB:

| Program | Enabled modes | ROM | Previous | Corrected |
| --- | --- | ---: | ---: | ---: |
| Rich Plate | Mod + Optimization | -0.020 | -0.031 | -0.021 |
| Rich Plate | Mod + Dynamic | -0.735 | -0.926 | -0.685 |
| Rich Plate | All three | -0.601 | -0.932 | -0.510 |
| Rich Split | Mod + Optimization | -0.022 | -0.025 | -0.020 |
| Rich Split | Mod + Dynamic | -0.237 | -0.270 | -0.262 |
| Rich Split | All three | -0.222 | -0.260 | -0.245 |

Across these six primary effects, mean absolute discrepancy falls from
0.101 to 0.032 dB and maximum from 0.331 to 0.091 dB. Independent saved-case
half-second tail windows, with gain matched from active input, have median
absolute level discrepancy 0.700 -> 0.387 dB. Tail-band diagnostics
125–500/500–2000/2000–8000 Hz improve in median from 1.309 to 0.849 dB.

Unfavorable cases remain in the report data: p95 window discrepancy is
8.042 -> 8.191 dB, and the all-band p95 is 27.695 -> 30.240 dB. Very quiet,
quantized residuals and weak bands make relative errors large. For example,
Rich Plate high-Chorus noise ends at reference RMS 5.79e-6, old 8.56e-6,
new 1.50e-6. Rich Split high-Chorus percussion retains a pre-existing residual
floor near 1.3e-5 versus reference 3.1e-6. These floors are explicitly retained;
no EQ, phase fitting or noise injection was used to improve a percentage.
These nonstationary/gated tails do not justify an exponential T20 pass. No
physical-unit, host-listening or mathematical sound-identity claim is made.

Thirty-six raw WAVs and 36 level-matched versions are retained for the 12 saved
input/control cases and their reference/previous/corrected variants. Listening
gains are common to both Chorus positions of a program/input/variant, preserving
the control's tail effect. They await actual listening acceptance.

## Runtime and delivery checks

All 28 plugin programs pass the dual-bank checks at 44.1/48/96 kHz and varied
blocks, state/mono/switching checks and zero callback new/delete. Original-224
hidden controls, XL fader endpoints, Spillover and preset/audio recall checks
pass, including same-algorithm tail retention and partial preset recall.
The local universal AU, VST3 and Standalone builds retain their identifiers.
They are built/strict-signature checked, not installed or published by this task.

Paired arm64 Release CPU uses O3, FP contraction off, desktop denormal policy,
untimed preparation/warmup, old/new/new/old ordering and two repeats per run.
There are 1,056 timed rows, all 22 programs and six workloads, including
same/different-program overlap and parameter/Mod/Chorus/Size edges. All tracked
processing, warmup and setters allocate/release nothing.

| Audio budget, median | Before | After |
| --- | ---: | ---: |
| Rich Plate, single active engine | 2.455% | 2.482% |
| Rich Split, single active engine | 2.390% | 2.394% |
| Rich Plate, two-engine overlap | 4.928% | 4.947% |
| Rich Split, two-engine overlap | 4.787% | 4.835% |

Catalog median relative changes across workloads range from -0.035% to +0.098%.
Two Runtime objects request 17,440 additional bytes: 65,314,704 -> 65,332,144.
The bank remains 5,828,152 bytes. Checksums change only for the two corrected
programs with active modes or parameter edges; all six workloads of the other
20 programs and the two rich all-off workloads match. Native overlap omits
plugin retirement fades/host conversion; desktop checks do not prove Daisy or
physical DAW CPU margin.

## Accepted approximation and reproduction

UART entry is rounded to semantic boundaries, coefficient visibility to a DSP
pass, and startup panel/title/physical-fader wrapper work remains approximate.
The other 20 XL programs retain their existing clocks. These are explicit limits
of this bounded correction; small remaining Rich cadence differences are allowed
by the user's request. Further work should follow a material audible mismatch,
rather than pursue exact peripheral equivalence.

Private evidence, commands, full CSV/JSON, source/binary hashes, source snapshots,
compiler logs and listening files are in
`build/validation/xl-dynamic-timing-fix-20261102/`. Raw `.f32` containers were
replaced by exact standard WAV wrappers after PCM equality verification;
`cleanup.json` retains old paths, replacements and hashes. No captured samples,
ROMs, banks, dependency resources or user recordings were discarded.

```sh
cmake --build build/plugin --config Release --target cineol_xl_rich_timing_check cineol_xl_rich_chorus_check native_hall_plugin_check
build/plugin/native-hall/cineol_xl_rich_timing_check "$XL_BANK"
build/plugin/native-hall/cineol_xl_rich_chorus_check "$XL_BANK"
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --banks "$ORIGINAL_BANK" "$XL_BANK"
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --preset-check "$ORIGINAL_BANK" "$XL_BANK"
```
