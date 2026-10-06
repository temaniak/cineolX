# Native XL input converter validation

October 6, 2026. This input correction follows the validated
[event DAC checkpoint](XL_DAC_VALIDATION.md). It replaces the XL input
approximation with independently timed channel captures and the X/XL input
circuit. The full XL sound-accuracy goal remains open.

## Problem and resulting behavior

The previous input path sampled two analog filters at the host rate, then
applied a 64-tap rational downsampler. Both channels entered the network at
the same resampler boundary. Circuit bypass quantized the current host pins
instead of the independently held pins at each FPC load. Float circuit values
also determined ADC range and headroom comparators.

`Input48<graph>` now uses `EventAdc48`. It combines the reference's 4x sinc
interpolation, 192 kHz first-order hold and seven-mode input circuit into a
prepared 33-tap FIR plus modal tail. Double precision is retained through
ADC conversion and comparator decisions. The left and right analog holds
are at rows `53-rows` and `3`; their final loads are at `89-rows` and `39`.
The callback waits until both loads have occurred. These are periodic FPC
scan times, independently established for all 22 physical graph layouts.

Circuit bypass uses ideal held 48 kHz pins at the respective load times and
retains the existing symmetric, unranged `Engine48::adc(..., false)` law.
This bypass oracle does not claim equivalence to every reference GUI bypass
or coarse CPU overshoot behavior. Intermediate Dirt values still interpolate
the clean and bare words; input comparators continue to observe the clean
circuit. Their five-bit mask now enters Dynamics without float requantization.

The change can alter channel phase, bandwidth, ADC rounding/range transitions,
bypass words and dynamic-decay threshold observations. It adds no EQ or
feedback compensation. Program and parameter IDs, state/preset schema, the
v5 bank and reported dry/plugin latency remain unchanged. The 16-frame input
interpolation latency and existing output compatibility transport are explicit
host alignment components, not additional physical ADC delays.

Coefficient tables initialize only during preparation. Program activation
clears fixed state and reuses prepared tables. Processing contains no ROM/CPU
interpreter, allocation, I/O, locks or waits. A converter instance occupies
5,024 bytes; its 240,976-byte immutable coefficient table is shared. The
separate output-converter table remains 215,264 bytes.

## Independent baseline and converter evidence

The actual final-DAC/row-inline implementation was frozen before input edits
as 129 source files in
`build/validation/xl-adc-integration-20261006/baseline/manifest.json`.
It is independently compiled for before/after audio and CPU comparisons.
Extracting the old input into `Input48` was bit-exact over 2,112,000 stereo
frames across all 22 graphs, including controls, gain, dry/mix, Dirt and routing.
The separately frozen extracted policy supplies the failing-old oracle.

The same public ADC test built against that frozen policy fails Concert Hall's
startup impulse: peak circuit error `0.784188`, relative RMS error `0.658023`,
182 differing ADC words, 16 comparator masks, one bypass mismatch and 18,540
deadline mismatches. This is a signal/converter failure, not a bank rejection.
The complete frozen source trees, commands and failure logs are retained.

The independent reference is Reflexion's continuous-time input model and
voltage-domain ADC/comparator implementation. It receives the same host pins;
expected hold/load times are derived from its clock constants, not from native
state. The policy oracle checks circuit values, clean words, comparator masks,
held bypass pins/words and callback deadlines. Reference-only allocations are
excluded from native processing tracking.

All seven row-count families (`100, 102, 104, 105, 107, 108, 109`) pass the
component check: 5,010,412 ADC words/comparator masks match exactly. The actual
policy passes all 22 graphs and eleven signals each: 15,947,624 channel words,
with zero clean/bare/comparator/deadline errors. Signals include startup and
later impulses, threshold steps, noise followed by silence, and tones at
100/1,000/4,000/8,000/12,000/16,000/20,000 Hz. Maximum policy circuit error is
`2.40172e-14`; maximum relative RMS error is `1.82718e-13`. Fixed guards remain
peak `<1e-10` and RMS `<1e-8`. An additional 250,000 seeded quantizer/comparator
inputs and half-code ties with adjacent doubles pass. Native heap new/delete
counts are zero.

These checks validate the periodic input boundary, including its zero-state
startup convention. They do not establish the firmware's cold first scan,
CPU-displaced DSP fetches, controller IRQ scheduling or WCS write visibility.
Those remain separate whole-engine work.

## Integration evidence

The dual-bank plugin regression passes all 28 original/XL programs, state,
presets, faders, mono/stereo, 44.1/48/96 kHz, 128/511/20,000-frame blocks,
Low latency and Spillover. Callback heap new/delete counts are zero.
AU, VST3 and Standalone rebuilds pass signature verification; these bundles
are not installed or validated in a DAW host. Dependency checkouts remain clean.

All 22 integer graphs pass 72,000 input frames across three controls each.
Every emitted WR_DA word/mask, final A-D word, arithmetic state, delay memory
and saturation count matches the independent WCS machine: 4,464,000 DAC
request words/masks. The integrated audio path passes finite-output,
signal/tail, dry-latency and clock checks with no processing heap activity.

Six paired full-path fixtures use the actual frozen pre-ADC renderer and
current renderer with the same v5 bank. Input/reference WAVs and physical
fixture metadata are byte-identical. Physical program/control operations and
independent clocks remain enabled; no reference state or WCS is injected.
Each uses mode 0, seed 17, level 0.08, 1 s warmup, 12 s capture and
48 kHz/256-frame blocks.

| Primary fixture | Old energy error dB | New energy error dB | Old envelope error dB | New envelope error dB |
| --- | ---: | ---: | ---: | ---: |
| Concert Hall noise | -0.42356 | -0.00046 | 1.11391 | 0.50093 |
| Concert Hall music | -0.04908 | +0.00040 | 0.29171 | 0.82716 |
| Dark Hall left, A/C | -0.22189 | +0.06331 | 1.11475 | 1.71267 |
| Dark Hall left, B/D | -0.18970 | +0.05079 | 1.09699 | 1.72984 |
| Dark Hall right, A/C | -0.00706 | +0.46674 | 12.73266 | 17.00931 |
| Dark Hall right, B/D | -0.01172 | +0.42546 | 12.29182 | 16.47497 |

Concert Hall noise's eight common accepted T20 bands improve from 1.54928%
to 0.76844% mean absolute error. The other five pairs have no common accepted
T20 fits; they are unavailable, not passes. Envelope comparisons retain the
reference-active-window guard, with zero missing native windows. Noise's extra
active native windows decrease from 81 to 35; Dark Hall right retains nine
extra windows on each output pair. Music and impulse envelope errors worsen
in several cases, as the table records. This is mixed primary evidence, not a
claim of universal sound improvement or whole-engine identity. No fit or
acceptance threshold was relaxed.

The quiet paired Release CPU/storage measurement follows the finished builds,
oracles, renders and analyses: five alternating old/new rounds, 1,320 rows
covering all 22 programs and six workloads. Both independently compiled
binaries use matching FTZ/FZ, 48 kHz/128-frame blocks, 1 s untimed warmup and
2 s measured audio. Checksums/peaks are stable within each version and change
across the sonic correction. All processing/control-edge new/delete counts
are zero; external host activity remains uncontrolled.

| Workload | New median audio CPU range, % | New/old sum of matched medians |
| --- | ---: | ---: |
| Modes off | 1.49226-2.32651 | 0.99063 |
| Modes on | 1.49311-2.33421 | 0.99070 |
| Same-program overlap | 2.96969-4.70893 | 0.98785 |
| Resonant Chords overlap | 2.99051-3.94961 | 0.99869 |
| LF/MID control edges | 1.50319-2.33479 | 0.99067 |
| Mod/applicable Size edges | 1.50696-2.35083 | 0.99227 |

The aggregate matched-median cost ratio is `0.991926` (0.81% lower), a small
change within the limits of this desktop measurement. Per-group spread,
commands, checksums and binary/source hashes remain in the private CPU JSON.
Individual matched ratios range from `0.963011` to `1.023090`; some workloads
cost up to 2.31% more, so the aggregate is not an all-program speedup claim.
Requested storage for two prepared Runtime objects decreases from 8,079,136
to 6,457,216 bytes (-1,621,920); bank payload remains 5,828,152 bytes. Shared
ADC/DAC immutable tables occupy 240,976/215,264 bytes separately. Requested
heap storage is not RSS. These native workloads omit host conversion/plugin
retirement fades and arbitrary overlap pairs, and do not establish a complete
plugin worst case or Daisy realtime margin.

## Identities and reproduction

Reference remains Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69` through
the build-only export, with eleven hash-validated private v8.21 chips.
The unchanged v5 bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Builds are Release macOS arm64, AppleClang 21, `-ffp-contract=off`.
No ROMs, banks, captures or firmware images belong in Git.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_adc_check cineol_xl_graphs_check cineol_xl_sound_compare cineol_xl_cpu_benchmark -j2
ctest --test-dir build/xl-sound-validation/native-hall -R cineol_xl_adc --output-on-failure
build/xl-sound-validation/native-hall/cineol_xl_graphs_check build/validation/xl-catalog-20261005/rom-private-local --all
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --banks build/native-expansion-preview/cache/programs-v44-import-v1.bank224 build/validation/xl-corrections-20261005/programs-v821-native-v5-startup.bankxl
```

Private evidence lives under `build/validation/xl-adc-integration-20261006/`:
source/binary manifests, complete frozen baselines, extraction/failing-old
oracles, component/policy logs, full graph/plugin runs, format signatures,
paired sound recipes and CPU provenance. The historical startup-only 313-case
campaign has not yet been repeated with the integrated ADC/DAC path.

The next reproducible experiment is a physical-key, independently clocked
firmware trace of controller entry, serial IRQ, WCS write grant/commit and
displaced fetch row, beginning with Concert Hall Mod off/on and Decay Opt/
Dynamic Decay combinations. Compare the first divergent native event rather
than aggregate call rates alone. The local Mod branch-cost oracle and fixed
nominal clocks are retained baselines; injecting reference call timestamps
into native processing would be a labelled diagnostic, not a shipped scheduler.
