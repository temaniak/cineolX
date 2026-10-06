# 224XL catalog sound validation

Date: October 5, 2026. Engine checkpoint:
`c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`.

All 22 additional 224XL algorithms are checked individually against the
original v8.21 firmware. The integer graph and local control oracles pass, but
the independent full-path sound renders retain program-specific differences.
Startup interpolation state explains a substantial part of some differences;
it does not explain every program or every output. These measurements do not
establish full sound acceptance or implement a production correction.

This extends the [first Concert Hall investigation](XL_SOUND_ACCURACY_HANDOFF.md).
Production DSP, prepared banks, import behavior, plugin parameters and installed
bundles remain at the engine checkpoint. Original-224 tools and fixtures are
preserved. The additions are offline validation tools and English documentation.

## Identity and measurement boundary

- Reference: original 224XL v8.21, all eleven SHA-256-validated SBC/NVS chips,
  pinned Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`, and the existing
  build-only scheduler compatibility export. The dependency checkout is clean.
- Native input bank: private version-4 `programs-v821-native-v4-final.bankxl`,
  SHA-256 `1330245bfafe90df553545d5b7807f631f6b95dfce06d610e45cd49fba7448c3`.
- Sound build: tools-only Release, macOS arm64, AppleClang
  21.0.0.21000334, `-ffp-contract=off`. Sound renders retain the default host
  floating-point environment; the separate CPU benchmark sets the desktop
  denormal policy explicitly.
- Audio: 48 kHz, wet stereo A/C by default, unity gain, Analog on/Dirt 0;
  native Runtime's 57-sample alignment. Selected A/C and B/D routing cases are
  separately labelled. Plugin host conversion, the additional original/XL
  alignment and AU/VST3 behavior are outside these renders.

Each reference boots normally for 16 simulated seconds and selects its actual
bank/program with LarcOperator. Factory faders and requested switches are set
through physical operator commands, verified and settled. Native Runtime
independently loads the bank and receives the matching logical control values.
It does not receive reference delay memory, counters, converter phase or graph
registers. Both paths then receive the same silence warmup and deterministic
input on independent clocks. Setup time is not replayed through native Runtime.

Inverse Room's factory level controls are muted in this bank. Its fixtures open
the physical level faders to 128 and record the calibrated logical values.
Gate fixtures set physical LF/MID STOP DECAY faders to 18 and REV STOP DLY to
10. These labelled recipes are not silent factory-default passes. Raw bank/WCS
differences caused by level overrides are not attributed to interpolation.

Private evidence is under `build/validation/xl-catalog-20261005/`. The matrix
plan and per-case run records retain firmware, bank, binary, analyzer, source,
compiler and dependency-export identities, complete commands and logs. WAVs,
banks, ROMs, WCS dumps and oracle logs must stay in ignored `build/`.

After 209 completed cases, two Desktop ROM files became cloud `dataless`
placeholders. Both processes blocked in the ROM loader's file read before
preparation or DSP. Their interrupted cases and one process-stack sample are
retained separately. A local private copy was recovered from the pre-existing
`build/xl-probe/v821-memory.bin` CPU-memory snapshot: every one of its eleven
ROM regions matches both the original campaign chip SHA-256 and the known
v8.21 firmware catalog. No bytes were inferred or substituted. Recovery
provenance is in `rom-local-provenance.json`; original Desktop files were not
changed. Completed cases preserve their actual original paths/commands. The
remaining cases use the identical chips under ignored `rom-private-local/`.

## Coverage

All 313 planned fixtures completed with zero renderer or analysis failures:

| Recipe | Count | Scope |
| --- | ---: | --- |
| Noise, seed 17, level 0.08, warmup 1000 ms, all modes off | 22 | All programs |
| Quiet noise, seed 991, level 0.02, warmup 250 ms | 22 | All programs |
| Other applicable switch combinations | 136 | Seven combinations for 19 reverbs; Mod only for three effects |
| Stereo impulse and synthetic percussion/stopped chord | 44 | Both inputs for all programs |
| Single-sided impulse, A/C and B/D | 32 | Three effects and five split programs, both input sides |
| Short STOP DECAY/STOP DLY, modes 0, 4 and 7 | 57 | All 19 reverbs |

Mode bits are 1 = Mod Enhancement, 2 = Decay Optimization and 4 = Dynamic
Decay. Captures are 12 seconds. Every mode-0 fixture also includes a separately
labelled static-reference-WCS diagnostic; mode-on fixtures do not inject WCS.
The orchestration uses two independent renderer processes. CPU measurement is
performed separately, after rendering and analysis have stopped.

There are 303 signal-bearing cases and ten unexcited cross-input split-output
cases. For all five split programs, left-only input excites A/C and right-only
input excites B/D; the opposite pair is silent or below the existing 1e-12
summed-energy observation threshold in both paths. These are separation checks,
not additional sound-match passes. Relative energy ratios below that threshold
are unavailable, including minute float/filter residues.

| Routing program | Left to A/C energy dB | Left to B/D energy dB | Right to A/C energy dB | Right to B/D energy dB |
| --- | ---: | ---: | ---: | ---: |
| Chorus & Echo | -1.233 | -1.233 | -0.973 | -0.973 |
| Resonant Chords | -0.015 | -0.015 | -0.019 | -0.019 |
| Multiband Delay | +1.704 | +1.704 | +0.588 | +0.588 |
| Hall / Hall | -0.137 | — | — | -0.215 |
| Plate / Plate | -1.001 | — | — | -0.434 |
| Plate / Hall | -0.407 | — | — | -0.304 |
| Plate / Chorus | -0.439 | — | — | -2.537 |
| Rich Split | -0.171 | — | — | -0.102 |

Plate / Chorus B/D uses effect response/envelope metrics. Its injected WCS
diagnostic has -0.978 dB energy difference, versus the independent native
-2.537 dB, under the right-only impulse. A/C's small decay-estimate difference
does not establish the accuracy of the chorus side.

The diagnostic freezes the reference's prepared WCS coefficients and addresses
in a separate native graph, with empty delay memory and independent clocks.
It is neither the plugin nor a before/after correction. A default Concert Hall
recipe check confirms the updated harness preserves the earlier primary input,
native and reference WAVs byte-for-byte; the earlier 127/256 block repeat and
16 Concert Hall fixtures remain available.

## Repeated firmware oracles

All existing XL oracles were rerun for the complete catalog:

| Oracle | Result and tested boundary |
| --- | --- |
| Graphs | All 22 pass; 72,000 frames per graph across three settings, 1,584,000 total; exact A-D integer outputs, arithmetic state, memory and saturations; signal, tail, dry and rational-clock checks |
| Controls | All 22 pass; 1,928 composed fader fixtures and 52 size/layout fixtures on the 12 applicable programs |
| Modulation | All 22 inspected; 79,162 exact transitions on 17 interpolation programs, including 32 Chorus positions per program; five programs have no interpolation controller |
| Dynamics | All 19 reverbs pass; 27,117 slow/fast transitions across the four Dynamic/Optimization combinations; finite native audio and distinct switch behavior; the three effects have no reverb dynamics |

The tracked processing regions report zero C++ allocations/releases. Local
oracle equality does not imply equal full-path converter timing, physical
startup phase or firmware scheduling. The graph oracle's incidental timings
are not used as CPU measurements.

## Per-program baseline

The table uses seed-17 noise, mode 0, A/C. Energy is total native/reference
energy difference in dB, with no normalization. Decay columns are mean absolute
differences of extrapolated decay estimates, on the **same usable band set**
within each native/injected-diagnostic pair. Band sets differ between programs;
these are exploratory measurements, not a shared acceptance score.

Indices are the existing zero-based XL catalog indices. A dash means no usable
paired decay estimate, not zero difference. WCS rows count differing raw-bank
versus prepared-reference interpolation rows, not live native WCS after controls.

| Index | Algorithm | Energy dB | Common bands | Native decay difference % | Injected WCS diagnostic % | Differing interpolation rows |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 0 | Concert Hall | -0.443 | 8 | 11.61 | 1.53 | 4 |
| 1 | Bright Hall | -0.974 | 7 | 7.33 | 0.70 | 4 |
| 2 | Dark Hall | -1.196 | 8 | 14.00 | 1.34 | 8 |
| 3 | Plate | -0.937 | 8 | 6.53 | 0.30 | 2 |
| 4 | Room | -0.555 | 8 | 14.63 | 0.89 | 4 |
| 5 | Rich Chamber | -0.321 | 6 | 5.17 | 0.25 | 2 |
| 6 | Small Room | -1.023 | 0 | — | — | 8 |
| 7 | Chamber | -0.870 | 8 | 3.98 | 0.87 | 4 |
| 8 | Dark Chamber | -0.128 | 7 | 4.05 | 0.89 | 2 |
| 9 | Inverse Room | -1.066 | 0 | — | — | 0 |
| 10 | Small Plate | -0.806 | 7 | 6.89 | 0.37 | 2 |
| 11 | CD Plate A | -1.797 | 6 | 2.84 | 0.77 | 18 |
| 12 | CD Plate B | -0.983 | 7 | 1.73 | 0.81 | 16 |
| 13 | Rich Plate | -0.785 | 8 | 0.18 | 0.18 | 0 |
| 14 | Chorus & Echo | -2.101 | 0 | — | — | 12 |
| 15 | Resonant Chords | -0.225 | 0 | — | — | 0 |
| 16 | Multiband Delay | -1.431 | 0 | — | — | 0 |
| 17 | Hall / Hall | -1.084 | 8 | 13.06 | 2.12 | 4 |
| 18 | Plate / Plate | -1.457 | 8 | 15.29 | 0.67 | 4 |
| 19 | Plate / Hall | -0.818 | 8 | 12.76 | 0.29 | 4 |
| 20 | Plate / Chorus | -0.578 | 8 | 0.78 | 0.78 | 10 |
| 21 | Rich Split | -0.576 | 8 | 0.46 | 0.46 | 0 |

The baseline figure is retained privately as `matrix/catalog-baseline.svg` and
`matrix/catalog-baseline.png`. Its display labels are one-based 01-22.

All 22 physical mode-0 preparations show global modulation index 59 at select,
input start and render end. The 17 interpolation graphs' imported seeds range
from 103 to 333. This is a repeated startup difference under this specific
physical recipe. Programs without interpolation also expose the global 59
record, but it is not evidence of an active interpolation controller there.
The native `bank_seed` record is not a live native-state observation.

Many reverbs' decay differences become smaller in the injected diagnostic.
Rich Plate and Rich Split have no interpolation controller and retain their
energy differences. Plate / Chorus's A/C decay estimates do not change under
the diagnostic despite differing interpolation rows; its other output pair
must be assessed separately. There is no justification for applying one
Concert Hall adjustment indiscriminately to all 22 algorithms.

## Fit limits and effect metrics

Bands are 100-250, 250-500, 500-1000, 1-2k, 2-4k, 4-8k, 8-12k, 12-15k and
15-20 kHz. The final band is a spectral-image diagnostic, excluded from decay
summaries. Fits use post-input Schroeder energy and the -5..-25 dB slope,
extrapolated to T60. Requirements remain R-squared >= 0.95, negative slope,
at least 100 samples, 1.2 seconds of capture margin, and final-second energy
below -40 dB relative to post-input energy. Both paths must qualify. There is
no noise-floor subtraction or threshold relaxation.

Small Room's primary noise baseline fails floor and/or exponential-fit guards.
Inverse Room, Chorus & Echo, Resonant Chords, Multiband Delay and all Dynamic
Decay cases deliberately do not receive an exponential decay score. Their
response/envelope measurements are retained instead. Quiet noise, impulses
and music can also lack usable fits; an absent fit is not a sound pass. The
earlier Concert Hall 30-second captures document persistent floor limitations.

The final analysis policy is `xl-catalog-v2`. It also omits exponential scores
for Plate / Chorus B/D and for Rich Split's auxiliary output, whose child-specific
exponential acceptance target is not established here. An apparently good
mathematical fit on the chorus output is not evidence that a reverb decay target
applies. Pairs containing either auxiliary channel use response metrics instead.
The energy-ratio guard uses the existing 1e-12 summed-energy observation floor;
near-silent ratios become unavailable, not accepted zero errors.

After all renders completed, the original v1 analysis files were preserved in
each case's `analysis-v1/` and bound by `matrix/analysis-v1-manifest.json`. All
313 cases were reanalyzed with the explicit v2 policy, without rerendering or
changing any WAV. The original renderer/analyzer identity remains in the
campaign plan and run records; `analysis-v2-provenance.json` separately binds
the final policy/source, commands, results and changed fields. The initial
16 Concert Hall fixtures retain their original analysis. Known 3-second T60,
silence/floor rejection, a known gain ratio, negligible-energy handling,
route applicability, onset and impulse-candidate checks validate the analyzer.

Every case retains per-channel energy, peak, thresholded onset, peak time,
10/50/90% energy times, final-second RMS and 50 ms envelopes. Impulse peak
candidates use 1 ms bins and are not automatically identified echoes. For the
three effects, the catalog summarizer also retains post-input spectral peak
candidates for noise and music. They are not an acceptance test for resonant
pitch, echo delay or modulation speed. Numerical targets and human listening
for those properties remain open.

The stereo-impulse recipe has a left impulse at sample 0 and a right impulse
at sample 480. Its thresholded onset measurements show distinct effect
differences. Thresholds are -60 dB relative to each channel's peak with an
absolute 1e-7 floor; these times are not a blanket delay/latency acceptance
metric, and both input impulses contribute to the selected output pair.

| Effect | Native A/C onset ms | Reference A/C onset ms | Injected diagnostic A/C onset ms |
| --- | --- | --- | --- |
| Chorus & Echo | 30.833 / 30.833 | 25.042 / 25.042 | 25.500 / 25.500 |
| Resonant Chords | 100.542 / 8.896 | 100.417 / 8.771 | 100.542 / 8.896 |
| Multiband Delay | 406.688 / 279.042 | 406.562 / 278.938 | 406.688 / 279.042 |

Chorus & Echo's independent native onset is about 5.79 ms later in this
fixture; the injected diagnostic reduces but does not remove the difference.
The other two effects retain approximately 0.10-0.125 ms onset differences
without an interpolation controller. Their echo/pitch behavior still needs
specific numerical and listening targets.

Reference procedure-entry counts vary with mode and program; disabled-routine
early returns still count. They are not committed WCS-update counts. Full-path
independent clocks continue to use the bank's nominal native controller rates,
so exact local control step laws do not establish schedule equality.

## Desktop Runtime CPU baseline

The separate Release benchmark completed **550 measurements**: 22 programs,
five workloads and five repeats. Host: Apple M1 Pro, MacBookPro18,3, eight
physical cores; ARM FPCR FZ bit 24 set, `-ffp-contract=off`, 48 kHz and block
128. Select/preparation, one-second warmup and CSV writes are untimed. Each
timed run processes two seconds of audio, with signal for the first second.
Workload order reverses on alternate repeats. Level controls are opened to
128 for audible benchmarking and applicable modes are enabled as labelled.

Values below are median elapsed processing time / audio time, multiplied by
100. They include scheduler preemption and the harness's checksum/peak work;
they are not CPU hardware counters or individual block-deadline measurements.
The benchmark ran after renderers, oracles, analyzers and figure generation
completed. Other system activity was uncontrolled. Raw timings and all repeat
ranges are retained in `cpu.csv`, `cpu-summary.csv/json` and `cpu-provenance.json`.

| Index | Algorithm | Single modes off % | Single modes on % | Same-program tail % | Resonant Chords tail % | Control edges % |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 0 | Concert Hall | 2.28 | 2.30 | 4.64 | 4.11 | 2.29 |
| 1 | Bright Hall | 2.24 | 2.25 | 4.56 | 4.10 | 2.26 |
| 2 | Dark Hall | 2.31 | 2.30 | 4.62 | 4.12 | 2.26 |
| 3 | Plate | 2.35 | 2.35 | 4.72 | 4.15 | 2.38 |
| 4 | Room | 2.27 | 2.27 | 4.61 | 4.12 | 2.27 |
| 5 | Rich Chamber | 2.52 | 2.53 | 5.14 | 4.37 | 2.54 |
| 6 | Small Room | 2.26 | 2.26 | 4.59 | 4.14 | 2.27 |
| 7 | Chamber | 2.25 | 2.19 | 4.58 | 4.21 | 2.23 |
| 8 | Dark Chamber | 2.58 | 2.60 | 5.17 | 4.54 | 2.61 |
| 9 | Inverse Room | 2.57 | 2.60 | 5.19 | 4.42 | 2.64 |
| 10 | Small Plate | 2.36 | 2.35 | 4.77 | 4.17 | 2.37 |
| 11 | CD Plate A | 2.33 | 2.34 | 4.70 | 4.21 | 2.37 |
| 12 | CD Plate B | 2.47 | 2.49 | 5.02 | 4.38 | 2.50 |
| 13 | Rich Plate | 2.62 | 2.60 | 5.44 | 4.67 | 2.60 |
| 14 | Chorus & Echo | 2.39 | 2.38 | 4.80 | 4.27 | 2.47 |
| 15 | Resonant Chords | 1.84 | 1.84 | 3.65 | 3.70 | 1.84 |
| 16 | Multiband Delay | 2.12 | 2.15 | 4.26 | 4.02 | 2.16 |
| 17 | Hall / Hall | 2.30 | 2.30 | 4.74 | 4.28 | 2.37 |
| 18 | Plate / Plate | 2.27 | 2.25 | 4.60 | 4.17 | 2.29 |
| 19 | Plate / Hall | 2.26 | 2.30 | 4.60 | 4.19 | 2.28 |
| 20 | Plate / Chorus | 2.46 | 2.44 | 4.95 | 4.35 | 2.62 |
| 21 | Rich Split | 2.51 | 2.49 | 4.99 | 4.37 | 2.48 |

All 110 program/workload groups have identical output checksum and peak across
their five repeats, finite output and zero tracked allocations/releases during
warmup, processing and control edges. Single modes-on medians span 1.84-2.60%;
same-program overlap medians span 3.65-5.44%. The largest individual elapsed
measurement is 7.07%; medians must not hide that timing spread.

Control edges alter resolved cell 0 by eight raw steps every 128 samples. Tail
workloads warm the retiring engine with signal, then feed it silence while the
current graph receives signal. They approximate two active Runtime engines;
plugin retirement fades, parameter smoothing, host sample-rate conversion and
wrapper overhead are not included. Resonant Chords is the designated cross-graph
tail fixture, not an asserted worst-case retiring graph. No MCU CPU margin,
full-plugin Spillover deadline or paired before/after improvement is established.

Constructing two Runtime instances and their graph tuples requests **9,402,512
bytes** of persistent storage; `sizeof(Bank)` is **5,828,152 bytes**. These figures
exclude allocator overhead, raw bank-file/input buffers, UI and host storage;
they are not total process RSS. Processing remains allocation-free in the
tracked regions. No production memory layout was changed.

## Portable reproduction

Use privately held original v8.21 ROMs and the recorded v4 bank, and a fresh
ignored output directory. The renderer refuses to overwrite a completed case;
the matrix runner resumes only matching recipes and renderer/analyzer/source
identities. It retains the original runner identity for completed cases when
only orchestration code changes, and allows ROM storage relocation only when
the complete chip hash map and every other identity/argument match. Rendering
has a 180-second wall-time limit; timeout is a recorded failure, not a sound
pass. Interrupted orchestration/file-read cases are archived under
`matrix/interrupted/` and excluded from results.

The finalized campaign retains its original v1 analyzer identity plus separately
bound v2 post-analysis. Running the current v2 matrix runner against that old
directory is intentionally rejected before replacing its plan; use a fresh
directory for a new campaign. A resume-identity check confirms the completed
plan remains unchanged on that rejection.

```sh
cmake -S . -B build/xl-sound-validation -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON
cmake --build build/xl-sound-validation --parallel 2 --target cineol_xl_sound_compare cineol_xl_cpu_benchmark cineol_xl_graphs_check cineol_xl_controls_check cineol_xl_modulation_check cineol_xl_dynamics_check
XL_TOOLS=build/xl-sound-validation/native-hall
XL_ROM_DIRECTORY=/private/path/to/224XL-v8.21
XL_BANK=/private/path/to/programs-v821-native-v4.bankxl
XL_EVIDENCE=build/validation/xl-catalog-new
"$XL_TOOLS/cineol_xl_graphs_check" "$XL_ROM_DIRECTORY" --all
"$XL_TOOLS/cineol_xl_controls_check" "$XL_ROM_DIRECTORY" "$XL_BANK"
"$XL_TOOLS/cineol_xl_modulation_check" "$XL_ROM_DIRECTORY"
"$XL_TOOLS/cineol_xl_dynamics_check" "$XL_ROM_DIRECTORY" "$XL_BANK"
python3 script/compare_xl_catalog.py "$XL_ROM_DIRECTORY" "$XL_BANK" "$XL_EVIDENCE/matrix" --scope all --jobs 2
python3 script/summarize_xl_catalog.py "$XL_EVIDENCE/matrix" --require-complete
MPLCONFIGDIR=build/validation/matplotlib-cache python3 script/plot_xl_catalog.py "$XL_EVIDENCE/matrix"
"$XL_TOOLS/cineol_xl_cpu_benchmark" "$XL_BANK" "$XL_EVIDENCE/cpu.csv"
```

NumPy is required for analysis and matplotlib for figures. Some generators add
a `Release` directory and Windows adds `.exe`. Record the new machine's compiler,
FP environment, bank, ROM and tool identities; do not infer paired performance
changes from a different host or recipe.

## Remaining production work

The evidence supports building a physical XL startup/key oracle across all 17
interpolation programs, including actual recompile, Mod on/off repetitions,
Decay/Dynamic keys and non-default Chorus/Size. Establish reset/retention rules
and a compatible v4-bank representation before changing Runtime or import.
Original-224 phase rules are not evidence for XL.

A proposed correction must pass that failing oracle, preserve these identical
baseline inputs/control recipes, and receive a paired Release CPU/memory and
control-edge/Spillover measurement. Converter boundaries, quantization and
mode-dependent scheduling need independent evidence. Host 44.1/96 kHz,
full plugin validation, matched listening and comparison with a physical unit
are not established by this campaign. No DSP correction is shipped here.
