# XL Hall tail: bounded review and listening handoff

October 6, 2026. The objective is practical sound similarity. This checkpoint
bounds the Hall / Hall startup investigation and prepares listening evidence;
it does not introduce or accept another controller or sound correction.

## Independent checks

Four additional Hall / Hall cases reuse the retained live-parameter pilot,
with Dynamic on, Mod/Optimization off, physical STOP 18/18 and STOP DLY 10,
wet A/C, analog on, 48 kHz and 256-frame blocks. Each changes one variable
from music seed 17, amplitude 0.08 and 1,000 ms silent warmup: 257 ms warmup,
2,000 ms warmup, seed 991, or amplitude 0.04. Captures are three seconds,
sufficient for the first two stopped-input intervals. The retained canonical
twelve-second case supplies its existing data. This is sensitivity analysis,
not a paired before/after improvement claim.

All four new renders complete with finite output, the renderer's peak guard
and zero tracked processing allocations/releases. Native and reference run
independent clocks; no observed controller state, timing or coefficient replay
enters native processing. Frequency bands retain the existing 100 Hz–15 kHz
policy; spectral images remain separate. Dynamic T20 fits remain unavailable.

The first input burst ends at 166.667 ms. Six complete 50 ms envelope windows
inside the silent interval through 500 ms are active in each case. Percentage
errors below measure RMS amplitude, using the retained reference activity
threshold `max(peak - 40 dB, -90 dBFS)`.

| Case | Maximum band RMS error | First-tail median | First-tail p95 | First-tail maximum |
| --- | ---: | ---: | ---: | ---: |
| Retained canonical, 1,000 ms | 1.6506% | 13.8534% | 30.4842% | 32.6359% |
| 257 ms warmup | 0.8262% | 7.5605% | 29.2419% | 34.4636% |
| 2,000 ms warmup | 0.1170% | 0.9700% | 4.8752% | 5.9294% |
| Seed 991 | 2.0292% | 15.8915% | 32.1244% | 33.4380% |
| Amplitude 0.04 | 0.3340% | 5.6046% | 35.9616% | 43.6075% |

The 2,000 ms case's smaller first-tail error does not establish a correction:
the engine is unchanged, both clocks advance further, and its full active
envelope maximum is still 40.5942%. The 257 ms case's second-tail maximum is
28.9331%. Final stopped tails have only two or three active windows in the
new cases, insufficient for general decay acceptance.

Restore chronology also changes with signal and phase. The canonical reference
restores during the first burst, while the pilot waits until the next burst.
With amplitude 0.04, the pilot restores during the first burst while the
reference first clears its stop flag during the second burst. These are event
observations, not proof of complete instruction timing. A universal startup
offset or detector-retention heuristic is not justified by these results.
Further component equivalence work is deferred; the next decision is whether
these retained tail differences are audible and material in listening.

## Interpretation of percentage differences

The user's subsequent question distinguishes nonlinear/time-varying behavior
from an implementation defect. The table's numerical "error" means deviation
from this particular independently clocked reference render; it does not prove
a faulty algorithm. Dynamic control is level-dependent and retains detector,
average and scan state even with Mod off, as in these cases. Modulation is an
additional source of variation in Mod-on cases, not the explanation for the
Mod-off fixtures here.

A deterministic algorithm can repeat its tail with identical complete initial
state and input. Independent controller history does not guarantee that state.
Short-window RMS deviations must therefore be interpreted against the
original's own variation across signal levels and scan phases. These four
cases establish sensitivity but do not establish either a defect or acceptable
equivalence. Future acceptance prioritizes frequency-dependent decay/envelope
character, stereo behavior, threshold response across levels and listening;
no sample-identical tail requirement is imposed for practical sound similarity.

## Reference variability comparison

The next bounded step reuses the canonical, 257 ms and 2,000 ms recordings.
All three input prefixes are sample-identical over the common three seconds:
seed 17, amplitude 0.08, the same physical controls/setup and bank. Only silent
warmup duration differs. The seed-991 and amplitude-0.04 recordings are excluded
from this comparison because each lacks a corresponding phase ensemble.
The current-source canonical recording is included alongside the private pilot.

The original's first-tail RMS spread across these three warmups reaches
**3.4977 dB** in a 50 ms window, equivalent to a 33.15% drop relative to the
louder original run. The prior 32.64% canonical native/reference difference
is therefore comparable in scale to the original's own observed variation.
This is a three-sample empirical range, not a confidence interval, distribution
estimate or automatic acceptance threshold.

| Native variant | First-tail windows inside observed original range | Largest excursion outside that range |
| --- | ---: | ---: |
| Current source, 1,000 ms | 5 / 6 | 0.0307 dB |
| Private pilot, 1,000 ms | 4 / 6 | 0.0389 dB |
| Private pilot, 257 ms | 5 / 6 | 0.3047 dB |
| Private pilot, 2,000 ms | 5 / 6 | 0.0357 dB |

Over the first tail's complete windows, integrated current-native levels fall
inside the observed original range in all eight 100 Hz–15 kHz bands. The
largest pilot band excursion is 0.0375 dB. Over the whole common three-second
capture, the largest current-native band excursion is 0.0004 dB, versus an
original band spread reaching 0.1365 dB. These are raw-level comparisons with
no signal alignment or level normalization. They support phase-dependent
variation as an explanation for this example's first-tail percentages;
they do not establish identical nonlinear control behavior.

Stereo balance and side/mid energy ratio of the current-source first tail are
inside the observed original ranges. Its correlation coefficient lies 0.0019
outside the three-run range. Over the first two seconds the current correlation
excursion is 0.00014 and side/mid excursion 0.00134 dB. These descriptive metrics
do not establish audible stereo equivalence.

The conclusion is deliberately limited to the first tail at this signal/level.
Across 38 windows active in every original run, the current source's largest
range excursion is still **1.1717 dB at 700 ms**, after the second input burst.
The canonical pilot reaches **0.4236 dB at 750 ms**. A warmup change thus does
not dispose of every residual. Listening of the existing clip is the next
acceptance input; broader claims at other levels, Mod-on or other programs
remain open. No DSP correction is justified by the first-tail percentages alone.

Evidence is under the private review's `variability/`: `results.json`,
`first-tail-windows.csv` and `reference-versus-native.png`.
`compare_reference_variability.py` verifies sample-identical inputs, matching
fixture identities and freshly measured RMS against saved envelope CSVs, then
compares envelope, band and stereo features. The plot uses saved metric rows.
No new renders, WAV files, CPU benchmarks or runtime edits are made; auditory
evaluation remains pending actual listener feedback.

## Listening deliverables

Two short files are prepared under ignored
`build/validation/xl-hall-tail-review-20261006/listening/`:

- `hall-dynamic-original-current-pilot.wav`: three seconds of ROM reference,
  500 ms silence, three seconds of the current native source, 500 ms silence,
  then three seconds of the private coupled pilot; ten seconds total.
- `hall-static-original-current.wav`: six seconds of reference and six seconds
  of current native source, separated by 500 ms silence; Dynamic/Mod/Opt off.

Each variant uses one scalar gain to match RMS over its first two seconds to
−24 dBFS, reduced if peak headroom requires it. There is no EQ, time alignment,
fade or separate normalization of tail intervals. Raw source WAVs are retained.
For the dynamic triplet, source input/reference files are byte-identical between
current-native and pilot campaigns. Output sample counts, finite levels, peak
headroom and matched RMS are verified after writing. Ordering, time ranges,
gains and source/output hashes are in `listening/manifest.json`.

The reference is the pinned ROM emulator, not a new physical-unit recording.
Listening has **not** been performed. The private pilot is not integrated in
production or installed; current-source renders do not assert installed-bundle
identity. Static listening clips check tone/tail separately from the Dynamic
startup issue. Listening approval of this example would not accept every XL
mode or algorithm.

## Reproduction, scope and cleanup

HEAD remains `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion remains
clean at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. The unchanged v5 bank hash
is `3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The private review directory retains `run_review.py`, exact argv/exit codes in
`commands.json`, firmware/renderer identity, `summarize_review.py`, raw metrics,
logs and `checkpoint.json`. Run the runner once into fresh case directories,
then run the summarizer; existing render fixtures are protected against overwrite.

No runtime, DSP, filter, converter, bank, dependency or installed plugin changes
are made. CPU is not rerun for offline analysis/listening preparation; previous
CPU results do not cover the newer queued pilot. No new all-catalog, Mod-on,
Spillover, host or original-224 acceptance is claimed. Generated-material review
retains the four unique comparison cases and two pending listening clips;
no disposable intermediate WAV copies were created or deleted.

## Listener's digital-trail observation

The user subsequently reported a digital-sounding trail after the tails.
This is actual preliminary listening feedback; subjective equivalence has
not been accepted. Its exact timing/character has not yet been specified.
A bounded inspection of the existing file finds a low-level tonal residual
in both the ROM reference and native sources, with the input exactly zero
after two seconds. Over 2.5–3 seconds, raw stereo RMS is −94.83 dBFS for the
reference, −95.04 dBFS for current native and −94.99 dBFS for the private pilot.
The residual remains near −95 dBFS in the final second of the twelve-second
sources. The late spectra have prominent peaks near 280 and 560 Hz.

The listening file's whole-variant scalar gains are approximately +26.3 dB.
They raise this late residual to approximately −68.5/−68.8/−68.7 dBFS,
making it more exposed. The saved listening samples exactly match the source
prefixes multiplied by the recorded gains and rounded to float32. Preparation
adds no other processing or residual. Fixed-point/DAC quantization is a
plausible mechanism given the integer feedback arithmetic and DAC truncation,
but no isolated causality experiment or physical-unit comparison is performed.
This finding identifies the measured late residue, not every possible sound
the listener may describe as a digital trail.

`inspect_residual.py` and `residual-inspection.json` retain this check.
No new audio files, noise suppression or DSP changes are introduced.

## Preliminary listener outcome

The user subsequently reported: "I did not hear a huge difference in the
material." This is preliminary subjective evidence for the presented short
test material. It supports stopping first-tail percentage fitting here and
temporarily retaining the current Hall / Hall behavior. It does not assert
inaudibility, a quantified listening threshold, preference between variants,
or acceptance of every XL algorithm/mode or longer musical program material.

The first-tail investigation is now bounded: no further controller expansion
or corrective EQ is justified by this clip alone. The practical next priority
is short comparisons of representative remaining algorithms and listening on
musical sources. Reuse the prepared pairs and raw evidence before generating
additional captures; investigate only repeatable, material differences.
