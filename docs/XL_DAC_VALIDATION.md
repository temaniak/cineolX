# Native XL event DAC validation

October 6, 2026. This is a bounded output-converter correction within the
ongoing [XL sound-accuracy work](XL_SOUND_CORRECTIONS.md). This report preserves
the output-only checkpoint. The subsequent
[input correction](XL_ADC_VALIDATION.md) now has independent all-22 policy and
full-path regression evidence; firmware scheduling and listening acceptance
remain separate completion gates.

The final output checkpoint includes the row-proxy inline correction. Its
five-round matched CPU cost is 6.84% below the pre-DAC implementation and
13.78% below the initial event DAC; Resonant Chords is 13-16% below pre-DAC.
The earlier +7.42%/44-46% figures below are preserved historical findings,
superseded for current performance by the final follow-up measurement.

## Problem and resulting behavior

The previous XL output path retained one final word per channel per network
pass, applied a 32-tap rational interpolation filter, and sampled the output
circuit with a 48 kHz first-order hold. It discarded intermediate writes to
the same channel. Dark Hall writes A+D at row 51 and C+D at row 104: both D
updates must reach the converter. Some programs never write B or D, and close
requests can wait behind an active conversion.

The native graph now emits every WR_DA row's bus word and channel mask. The
output policy implements the waiting-latch overwrite, normalization/capture
boundary and converter occupancy using bounded event state. It integrates
piecewise-constant DAC holds through the X/XL output circuit and samples that
circuit at host frame boundaries. No CPU, ROM execution, opcode dispatch or
per-row emulator runs in audio. The graph's integer arithmetic is unchanged.

Analog/Dirt bypass also receives the actual DAC holds, including repeated
updates to D, rather than the last word for an entire pass. Circuit bypass
retains its additional 32-frame input-delay alignment. Intermediate Dirt
values continue to interpolate circuit and bypass paths. Input gain, mix,
program/parameter identities and the reported latency are preserved.

The existing input adapter produces a native pass in the host frame at or
after its start. The DAC forecast includes one causal host frame; its fixed
output transport includes that frame within the previous graph-specific
55-57 sample compatibility delay. Runtime's maximum delay remains 57 samples.
That transport is host alignment, not an extra physical converter delay.

## Independent evidence and identities

Baseline production HEAD is `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86` with the
validated, uncommitted final startup correction. Before converter edits, 135
source files were frozen with individual hashes in
`build/validation/xl-converters-20261006/baseline/manifest.json`.
Both old/new renderers and CPU tools compile independently from their own
complete source trees. The independently frozen output-policy extraction was
bit-exact to the full baseline over 2,112,000 stereo frames, including controls,
gain, dry/mix, Dirt and routing; a second 137-file snapshot preserves it for the
old-policy oracle. Neither frozen tree is modified by this correction.

Reference dependency remains Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, through the build-only export.
The eleven private v8.21 chips remain hash-validated. The unchanged final v5
startup bank has SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Converter changes require no additional bank version or migration.
Builds are Release macOS arm64, AppleClang 21, `-ffp-contract=off`.

Physical program selection followed by a labelled offline FPC/WCS clone
established the periodic ADC reads/holds and every DAC request/capture for all
22 programs: 396 ADC and 738 DAC channel observations. The final phase probe
also rejects any zero-mask WR_DA omitted by native graph metadata; none occurred.
Cold startup and CPU-displaced fetches are explicitly excluded from that
periodic probe. The separate converter oracle covers cold startup directly.

`cineol_xl_dac_check` feeds graph-specific synthetic bus words to the native
output and, independently, clocks Reflexion's FPC one physical row at a time.
It compares captures and samples against Reflexion's double-precision
continuous-time output circuit. Native computation forecasts complete passes;
the reference advances only to the current physical time. This checks waiting
requests, overwrite order, repeated channels, normalization, capture
propagation and circuit response without injecting native state into reference.

All 65,536 DAC input words normalize identically. Each of 22 graphs passes
noise followed by silence and 100/1,000/8,000/15,000/20,000 Hz tones, both for
the component and its actual output policy with compatibility transport.
Each suite compares 6,336,000 four-channel frames and 12,251,700 capture events.
Maximum absolute circuit error is `3.26053e-7`; maximum relative RMS error is
`3.683e-7`. Fixed acceptance guards remain peak `<3e-5` and RMS `<1e-4`
(-80 dB), matching the original-224 converter check's numerical standard.
Every raw hold matches exactly. Tracked request/sample heap allocations and
releases are zero. The native converter instance is 488 bytes; its immutable
107,744-byte coefficient table is shared and initialized outside processing.

The same policy oracle built against the independently frozen old output fails
on Concert Hall noise: peak error `1.00284`, relative RMS error `0.76649`.
This is a waveform/circuit failure, not a bank rejection. The old test and its
failure log are retained in ignored evidence.

## Full-path verification checkpoint

The expanded integer graph oracle checks every emitted WR_DA bus word and mask
against the independent WCS machine, including intermediate D writes. All 22
graphs pass 72,000 input frames across three control settings each:
4,464,000 request words/masks are exact, as are final A-D words, arithmetic,
delay memory and saturation counts. Native audio remains finite with expected
signal/tails, dry latency and input clocks. Heap tracking reports zero calls.

The dual-bank plugin regression passes all 28 original/XL programs, fader
limits, state/presets, mono/stereo, 44.1/48/96 kHz, variable block sizes,
Low latency and Spillover. Callback heap new/delete counts are zero. AU, VST3
and Standalone builds are ad-hoc signed and verified, but are not installed or
tested in an AU/DAW host. Dependency checkouts remain clean.

Six full-path old/new primary pairs pass finite/heap checks and require
byte-identical input/reference WAVs and physical fixture metadata. The unchanged
v5 bank isolates the output correction. These are Concert Hall noise/music
and Dark Hall left/right impulses on A/C and B/D, with actual physical keys,
independent clocks, 1 s native warmup, 12 s captures and 48 kHz/256-frame blocks.
No WCS/state alignment diagnostic is enabled.

| Primary fixture | Old energy error dB | New energy error dB | Old envelope error dB | New envelope error dB |
| --- | ---: | ---: | ---: | ---: |
| Concert Hall noise | -0.40525 | -0.42356 | 1.11285 | 1.11393 |
| Concert Hall music | -0.04632 | -0.04908 | 0.29191 | 0.29156 |
| Dark Hall left, A/C | -0.21739 | -0.22187 | 1.11082 | 1.11332 |
| Dark Hall left, B/D | +0.06552 | -0.18969 | 6.98831 | 1.09922 |
| Dark Hall right, A/C | +0.00718 | -0.00709 | 12.79467 | 12.73006 |
| Dark Hall right, B/D | +1.53102 | -0.01176 | 13.62306 | 12.29027 |

Envelope error uses the existing reference-active-window policy; missing and
extra native windows remain separate in the paired JSON. This is mixed
full-path evidence, not universal improvement. Dark Hall's D response changes
substantially as the previously discarded update reaches its output. For
Concert Hall noise, the eight common accepted T20 bands' mean absolute error
changes from 1.52908% to 1.54928%; music's sole accepted band changes from
0.41945% to 0.42002%. All four impulse pairs have no common accepted T20 fits;
unavailable fits are not passes. No thresholds were relaxed and no EQ was added.

One level-matched Concert Hall music pair is prepared and readback-verified,
with source/output hashes and gains recorded. It has not been listened to and
does not constitute all-22 listening coverage. The all-catalog listening helper
correctly rejects this six-case subset; its failure is retained, rather than
weakening its all-22 coverage guard.

Evidence, commands and pre-launch binary/source identities are recorded under
`build/validation/xl-converters-20261006/`, including
`event-validation-runs.json`, `event-primary-runs.json`, the paired comparison
JSON, component/policy logs and independent baseline build records.
The paired Release CPU/storage check also completes: five alternating old/new
rounds, 1,320 rows covering 22 programs and six workloads, identical v5 banks,
48 kHz/128-frame blocks, 1 s untimed warmup and 2 s measured audio. Both binaries
use matching FTZ/FZ. Builds, renders and validation jobs finished before CPU
sampling; external host activity remains uncontrolled. Checksums are stable
within each version and change as expected across this sonic correction.
All timed/warmup/control processing reports heap new/delete `0/0`.

| Workload | New median audio CPU range, % | New/old sum of matched medians |
| --- | ---: | ---: |
| Modes off | 2.14835-2.52068 | 1.03088 |
| Modes on | 2.15114-2.53034 | 1.03199 |
| Same-program overlap | 4.33888-5.07389 | 1.03031 |
| Resonant Chords overlap | 4.68321-5.05224 | 1.21627 |
| LF/MID control edges | 2.15732-2.53379 | 1.03336 |
| Mod/applicable Size edges | 2.15837-2.54083 | 1.03452 |

Across all 132 workload/program medians, cost ratio is `1.07423` (+7.42%).
Resonant Chords is a material outlier: its individual workloads increase about
44-46%; the same-program overlap changes from 3.47855% to 5.07389% of audio
duration. Accurate captures therefore have a measured cost, not an unchanged-
CPU claim. Investigate the real-pole/modal update cost before broad performance
acceptance. These native measurements omit host conversion, plugin retirement
fades and arbitrary overlap pairs; they establish neither whole-plugin worst
case nor Daisy CPU margin.

Requested storage for two prepared Runtime objects decreases from 9,416,128 to
8,051,680 bytes (-1,364,448). Bank payload remains 5,828,152 bytes. The separately
shared immutable coefficient table is 107,744 bytes; requested heap storage is
not RSS. Per-run spread, hashes, commands and matched workload medians are
retained in `cpu-paired-event-dac/provenance.json` and `summary.json`.

The primary render pairs use actual program/control operations and independent
clocks, with identical input and reference WAV hashes required. Synthetic
converter equivalence does not prove whole-engine or physical-unit sound
identity. The old input FIR/circuit sampling, channel-specific ADC holds,
controller clocks, WCS write visibility and displaced DSP rows remain open.
The existing 313-case startup campaign is historical evidence; it has not yet
been rerun with this DAC correction. Routing expansion must include B/D for
the first 14 reverbs as well as the effects and split programs.

## Capture-state CPU follow-up

The initial event-DAC results above are preserved. The subsequent optimization
is recorded separately under `build/validation/xl-dac-optimization-20261006/`.
Its independent full-source baseline contains 127 files from the validated
initial event DAC. A first prototype omitted zero imaginary arithmetic for
four real poles: 2,112,000 full-runtime stereo frames were bit-exact, and a
two-round exploratory CPU pilot showed cost ratio 0.96523 to the initial DAC.

The current implementation also retains circuit state at DAC captures, evaluates
only the currently selected analog outputs, and retains all four channels'
capture histories. Compatibility delay is encoded in event timestamps rather
than precomputed analog frame storage: selecting a new output immediately sees
its correct delayed history. The bounded forecast queue holds 128 events.
Clocks rebase every 96 frames, avoiding a full forecast-queue shift every frame.
Dormant channels advance by one whole frame when needed; there is no unbounded
catch-up loop, transcendental processing, heap work or audio lock.

The shared immutable coefficient table grows from 107,744 to 215,264 bytes.
The converter instance grows to 1,848 bytes, while output frame storage is
removed. All seven poles and circuit coefficients are retained. Changing the
floating update order produces tiny rounding differences, not a new filter or
timing approximation. Against the independently frozen full initial runtime,
2,112,000 stereo frames with control/Mod/Size, gain, routing, mix and Dirt edges
stay within 8.9407e-8 peak difference (fixed guard 1e-6).

Four independent all-22 continuous-time suites pass: component/policy, each
with all outputs or repeatedly changing selected outputs. They retain exact
raw holds, 65,536-word normalization, all six signals and unchanged numerical
guards. The maximum circuit peak error is 3.38552e-7 including the additional
96,000-frame dormant/zero-mask/channel-recall fixture. Maximum relative RMS is
5.220e-7. Each dormant fixture contains 2,562 requests and 1,707 captures,
covering long gaps, pending-word overwrite and clock rebasing. Processing heap
new/delete remains 0/0.

Dual-bank plugin regression and rebuilt/signed AU, VST3 and Standalone pass
again. The six primary physical recipes are rerendered with byte-identical
input/reference data and the same mixed sound conclusions. Source/binary hashes
distinguish this implementation from the initial DAC; the new numerical runtime
check does not claim bit identity. The frozen real-pole-only prototype and its
earlier bit-identity result are retained separately.

The first five-round, three-version comparison completes with 1,980 rows:
pre-DAC, initial DAC and the capture-state prototype, identical v5 banks and
workloads. That prototype's matched median cost ratio is 0.96669 to the initial
DAC and 1.03585 to pre-DAC. Resonant Chords remains 38-43% above pre-DAC, so the
modal changes alone do not resolve the outlier. Those data remain historical
prototype measurements in `cpu-paired-final/`.

The later stream cleanup removes an unused four-word output copy and four
duplicate final DAC normalizations. Its full-runtime comparison is bit-exact
over another 2,112,000 stereo frames, and all-22 graph/request and dual-bank
plugin regressions pass. An exploratory single paired CPU round shows ratio
0.98850 to the capture-state prototype; it is not the final performance claim.

Disassembly identifies the larger regression: AppleClang 21 changes Resonant
Chords' pass from 52 calls to 99, including 49 outlined row-proxy calls.
Forcing only the small GraphExecutor row proxy inline restores 52 calls and
zero outlined proxy calls; DSP arithmetic is unchanged. The private candidate
is bit-exact over 2,112,000 full-runtime stereo frames and has exact checksums
for all 132 benchmark workloads. Its exploratory matched round costs 0.90536
of the stream implementation overall, about 0.606-0.624 for Resonant Chords.
Assembly, candidate source, hashes and pilot rows are retained separately.

The production row-proxy hint uses MSVC/GNU/Clang spellings and a plain-inline
fallback. Rebuilt AU/VST3/Standalone formats and fresh all-22 graph, dual-bank
plugin and six-pair primary checks precede the final five-round measurement.
All final checks complete for that exact production source: all 22 graphs,
4,464,000 exact per-row WR_DA words/masks, the 28-program dual-bank plugin
regression, six physical primary pairs and rebuilt/signed desktop formats.
Input/reference WAVs and fixture metadata remain byte-identical across each
primary pair. The tiny projection rounding differences retain the same mixed
sound conclusions, including the two slightly worsened usable T20 comparisons.
No whole-catalog sound acceptance is inferred from those six pairs.

The final five-round, three-version CPU campaign has 1,980 rows, matching v5
banks, Release/FTZ/FZ, untimed warmup and 48 kHz/128-frame workloads. All task
builds/oracles/renders/analysis finished before sampling; external host activity
remains uncontrolled. The sum of 132 matched medians has ratio 0.931566 to
pre-DAC (-6.84%) and 0.862195 to the initial event DAC (-13.78%). All processing
heap allocations/releases are zero, and each version's checksums are stable.

| Final workload | Audio CPU median range, % | Final/pre-DAC matched median ratio |
| --- | ---: | ---: |
| Modes off | 1.50774-2.32398 | 0.94248 |
| Modes on | 1.50048-2.33074 | 0.94162 |
| Same-program overlap | 3.01863-4.63133 | 0.93721 |
| Resonant Chords overlap | 2.99978-3.83239 | 0.90068 |
| LF/MID edges | 1.54225-2.32446 | 0.94367 |
| Mod/applicable Size edges | 1.51393-2.35259 | 0.94260 |

Resonant Chords' six individual cost ratios to pre-DAC are 0.840-0.868; its
former regression is resolved in this matched desktop workload. Two prepared
Runtime objects request 8,079,136 bytes versus pre-DAC's 9,416,128
(-1,336,992); the bank payload remains 5,828,152 bytes. The 215,264-byte immutable
table is shared and separate from requested heap storage. Native measurements
are not whole-plugin worst-case/RSS or Daisy CPU-margin claims.

Authoritative final CPU evidence is `cpu-paired-row-inline-final/`, with hashes,
commands, per-run spread and matched groups. Final graph/plugin run JSON and
`row-inline-primary-runs.json` identify their actual binaries and recipes.

## Isolated ADC prototype

`adc_xl48.hpp` and `xl_adc_check.cpp` are an unconnected periodic input prototype.
Native48 still uses its prior input path. The prototype derives the X/XL input
circuit independently, with a shared immutable 240,976-byte table, seven
host-rate modes and a 33-tap reconstruction at each separate ADC hold. Its
5,024-byte instance has fixed history/state storage. Conversion follows the
reference's voltage-domain range selection, rounding and comparator thresholds.
The callback deadline is the right final load at timing-ROM address 39, so
future bare input can be available when both completed words are emitted.

The standalone prototype passes all seven row-count/hold-phase families used
by the 22 physically phase-probed graphs: 100, 102, 104, 105, 107, 108 and 109.
Tests cover startup/late impulses, steps, noise/silence and seven tones up to
20 kHz. All 5,010,412 reconstructed words and comparator masks match the
independent double-precision continuous-time Input model; 250,000 seeded
range/code values plus half-code/adjacent values also match. Processing heap
new/delete is 0/0. The first compile failure was a test-local variable scope
error; it is retained, and the corrected prototype compiles/passes.

This does not yet establish an input correction in the plugin. Before
integration, freeze/extract the actual old input path, show its meaningful
failure against the new oracle, implement bare/Dirt and detector plumbing,
then repeat relevant full-path sound, CPU and plugin checks. Cold FPC scan
timing and CPU-displaced loads remain separate from the measured periodic law.

## Portable commands

```sh
cmake -S . -B build/xl-sound-validation -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON
cmake --build build/xl-sound-validation --parallel 2 --target cineol_xl_dac_check cineol_xl_converter_phase_check cineol_xl_graphs_check
build/xl-sound-validation/native-hall/cineol_xl_dac_check
build/xl-sound-validation/native-hall/cineol_xl_dac_check 22 --policy
build/xl-sound-validation/native-hall/cineol_xl_dac_check 22 --policy --selected
build/xl-sound-validation/native-hall/cineol_xl_converter_phase_check PRIVATE_XL_ROM_DIRECTORY build/validation/xl-converter-phases.csv
build/xl-sound-validation/native-hall/cineol_xl_graphs_check PRIVATE_XL_ROM_DIRECTORY --all
```

The first two checks require no ROM or bank. The phase and graph checks require
the user's private original v8.21 chips. ROMs, banks, WCS, captures and renders
stay in ignored output directories and never enter Git. No plugin installation,
AU-host validation, human listening or physical-unit comparison is claimed.
