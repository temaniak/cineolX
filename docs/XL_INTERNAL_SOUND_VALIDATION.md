# XL internal sound comparison

Date: October 6, 2026. Revision `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`
plus the retained uncommitted startup/ADC/DAC corrections. This report concerns
native desktop XL v8.21; it does not establish physical-unit or listening acceptance.

## Percentage policy

Measure independent native/reference paths with identical signals, controls,
gain and routing. Report RMS amplitude differences as
`100 * (sqrt(native_energy/reference_energy) - 1)`, rather than treating an
energy percentage as an amplitude percentage. Valid T20 slope estimates are
reported as `100 * (native/reference - 1)`. The provisional small-difference
criterion is 5% for each metric. Missing/rejected fits remain unavailable.
The 15–20 kHz spectral-image band is retained separately from the eight
100 Hz–15 kHz response bands. No EQ, normalization or noise suppression is
applied to measurement WAVs.

## Completed all-22 noise comparison

Two independent stereo 200 ms noise bursts per algorithm: seed 17/level 0.08/
1000 ms warmup and seed 991/level 0.02/250 ms warmup. Each capture is 12 seconds
at 48 kHz, block 256, wet A/C, Analog enabled, Dirt 0, factory controls and
Mod/Decay Opt/Dynamic Decay off. Inverse Room explicitly uses audible level
controls; it is not a silent factory-default pass.

All 44 render/analysis cases finish successfully with no native processing
allocations/releases. The maximum RMS-level difference across 352 response
bands is **0.3651%**; no level band exceeds 5%. There are 171 usable paired
decay fits, of which two exceed 5%: Room seed 17 at 1–2 kHz (**+5.7319%**),
and Dark Hall quiet seed 991 at 12–15 kHz (**−5.4616%**). The remaining 181
band fits are unavailable under the retained fit policy, including effects,
Inverse Room and floor/non-exponential/truncation rejections. They are not passes.

| Program | Maximum RMS-level difference, both signals | Usable decay bands / 16 | Maximum valid decay difference |
| --- | ---: | ---: | ---: |
| Concert Hall | 0.094% | 8 | 2.127% |
| Bright Hall | 0.106% | 13 | 3.149% |
| Dark Hall | 0.066% | 8 | 5.462% |
| Plate | 0.107% | 8 | 1.794% |
| Room | 0.305% | 5 | 5.732% |
| Rich Chamber | 0.276% | 12 | 0.544% |
| Small Room | 0.079% | 0 | fit rejected |
| Chamber | 0.051% | 11 | 0.996% |
| Dark Chamber | 0.086% | 10 | 2.113% |
| Inverse Room | 0.109% | 0 | not applicable |
| Small Plate | 0.226% | 8 | 0.175% |
| CD Plate A | 0.163% | 9 | 4.000% |
| CD Plate B | 0.127% | 8 | 1.808% |
| Rich Plate | 0.136% | 14 | 1.525% |
| Chorus & Echo | 0.047% | 0 | not applicable |
| Resonant Chords | 0.108% | 0 | not applicable |
| Multiband Delay | 0.365% | 0 | not applicable |
| Hall / Hall | 0.081% | 16 | 3.977% |
| Plate / Plate | 0.083% | 9 | 2.366% |
| Plate / Hall | 0.123% | 8 | 1.914% |
| Plate / Chorus | 0.093% | 16 | 2.977% |
| Rich Split | 0.036% | 8 | 1.532% |

These labels concern the exponential T20-derived decay estimate only; every
listed program has completed renders and measurable response-band levels.
The usable-band count of zero means no accepted decay fits, not zero decay
error. T20 is not applied to Inverse Room's reverse envelope or the three
non-reverb effects. Small Room is eligible for fitting, but every paired fit
fails the late-energy guard (last-second energy above -40 dB relative to
post-burst energy). Some seed-17 bands also fail the exponential-fit criterion;
most quiet seed-991 bands additionally fail the capture-margin guard. Their
unreliable fitted numbers are retained in the private CSV, excluded from decay
acceptance and not replaced with a zero or a pass. Its 0.079% figure compares
response-band RMS levels and does not establish decay-envelope agreement.

## Completed musical and low-bass comparison

All 22 mode-0 musical fixtures complete successfully. The largest response-band
RMS difference is **0.4316%**, with no 100 Hz–15 kHz band exceeding 5%.
The extra 20–60 Hz and 60–100 Hz measurements cover all 22 algorithms on both
seed-17 noise and music: all 88 low-bass bands are measurable, with maximum
absolute RMS difference **1.6938%** (Multiband Delay music, 20–60 Hz).
This extends the direct excess-bass check below the catalog's 100 Hz boundary.

For all 22 mode-0 seed-17 noise fixtures, the injected-reference-WCS diagnostic
WAV is byte-identical to ordinary native output. Thus the prepared coefficient
and address values do not cause an audible-frequency discrepancy in those cases.
No diagnostic settings are injected into production processing.

The 22 stereo impulse fixtures retain 24 response-band whole-capture differences
above 5%, with maximum **31.0635%**. Fixed two-second diagnostics reduce the
maximum to **6.46%**, but do not replace the full-capture measurements or establish
a pass. Relative errors from quiet quantized tails are labelled separately from
the musical/noise frequency-response evidence. `impulse-window-diagnostics.json`
retains all eight response bands at 1/2/4/12-second endpoints.

## Confirmed converter improvement and impulse limits

The six retained before/after ADC pairs have byte-identical inputs/reference
outputs and the same v5 bank. Concert Hall seed-17 noise previously differed
by approximately −12.4% RMS at 8–12 kHz and −25.4% at 12–15 kHz; the corrected
path differs by about +0.01% and −0.05% respectively. All eight response bands
in that current case are within 0.051%; valid decay differences are at most
2.128%. Its musical fixture is within 0.034% by band level. This establishes
removal of the measured high-frequency loss in those cases.

Dark Hall right-only level-0.08 impulse initially gives +61.419% at 100–250 Hz
when integrating the full 12 seconds. Injecting exact prepared reference WCS
settings produces **byte-identical native diagnostic audio**, ruling out a
coefficient discrepancy for this case. Over the first two seconds the same
band differs by −3.549%; the 8–12-second native/reference total RMS levels are
−98.731/−121.507 dBFS. The large whole-capture ratio includes a very quiet
late residual. Changing only warmup to 1250 ms reverses the full-band sign:
−37.782% at 100–250 Hz, with late native/reference levels −117.632/−98.333 dBFS.
The level-0.32 impulse is within 1.242% across all response bands, while the
quiet level-0.02 impulse retains larger relative errors. This is evidence of
phase/level-sensitive quantized residuals, not a confirmed broadband bass-gain
error. Full-capture outliers remain flagged; no compensation is shipped.

## Evidence and reproduction

Private evidence: `build/validation/xl-sound-focus-20261006/`. The current
renderer SHA-256 is
`f83ea83192516b9646055e81db870e0413455eb1a2ed1bca7b1c4947f1ffe5b5`;
the v5 bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Reflexion remains pinned to `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.
The source snapshot, physical-ROM hash map, renderer commands and individual
run/fit metadata are retained. Production audio sources match the ADC checkpoint;
controller timing components are not connected to production processing.

```sh
python3 script/compare_xl_catalog.py ROM_DIRECTORY XL_V5_BANK \
  build/validation/xl-sound-focus-20261006/catalog --scope baseline --jobs 2
python3 script/report_xl_percentages.py \
  build/validation/xl-sound-focus-20261006/baseline-percentages \
  build/validation/xl-sound-focus-20261006/catalog/p??-m0-noise*/
```

`baseline-plan.json` and each `run.json` contain the concrete private paths and
commands. `baseline-percentages/band-percentages.csv` contains signed percentages
and fit rejection reasons; `adc-before-after-percentages.csv` preserves the six
paired converter comparisons; `dark-hall-window-diagnostics.json` retains the
fixed-window/level/warmup diagnostics alongside unchanged whole-capture metrics.
`low-bass-percentages.csv` records the 20–100 Hz measurements. Reproduce with
`script/analyze_xl_low_bass.py OUTPUT_CSV CASE_DIRECTORY [...]`.
The percentage reporter also retains 50 ms block-envelope differences above
`max(reference_peak - 40 dB, -90 dBFS)` and absolute final-second RMS levels.
These envelope diagnostics have no automatic pass criterion and do not hide
the separately reported residual floors. Twenty-two level-matched music pairs
are prepared under `listening-pairs/`; listening has not been performed.

## Completed extended matrix and remaining discrepancies

All **313 planned cases** complete with zero renderer/analysis failures:
44 baseline noise, 22 music, 22 stereo impulse, 136 enabled-mode noise,
32 split/effect routing, and 57 stop-decay/control fixtures. Every render checks
finite output and zero native processing allocation/release calls. All 22
programs have current ADC/DAC full-path evidence; historical startup-only renders
are not substituted for this matrix.

| Fixture group | Cases | Maximum response-band RMS difference | Bands over 5% | Usable decay bands | Decay bands over 5% |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mode-0 noise, two signals | 44 | 0.3651% | 0 | 171 | 2 |
| Mode-0 music | 22 | 0.4316% | 0 | 10 | 2 |
| Stereo impulse | 22 | 31.0635% | 24 | 4 | 0 |
| Split/effect impulse routing | 32 | 31.0632% | 17 | 7 | 1 |
| Enabled-mode noise | 136 | 6.4987% | 6 | 377 | 23 |
| Stop controls, mode 0 | 19 | 0.3365% | 0 | 8 | 3 |
| Stop controls, mode 4 | 19 | 9.8950% | 25 | 0 | 0 |
| Stop controls, mode 7 | 19 | 8.0914% | 19 | 0 | 0 |

There are 2,504 primary response-band records: 2,424 measurable levels, 80
unmeasurable split-output bands, and **91 level differences over 5%**. Of 577
usable paired decay fits, **31 exceed 5%**; the largest is **+24.1570%** for
Hall / Hall, Mod on, 500–1000 Hz. The 1,927 unavailable decay bands include
dynamic/effect/non-exponential/floor/capture rejections and are not counted as
successful measurements. The 15–20 kHz image band and injected-WCS diagnostics
remain outside these response-band counts.

The threshold is provisional, and these are numerical triage results. The
mode-0 noise/music frequency response can be temporarily retained under the
small-difference criterion. The entire matrix is **not accepted as sound-identical**:
impulse residuals, Mod tail estimates and dynamic stop envelopes remain flagged.
The active 50 ms envelope diagnostics can be larger than the full-capture level
ratios; for example Small Plate mode-4 stop control has a p95 envelope difference
of about 89%. It is not hidden by its 7.49% maximum band-level difference.

## Bounded controller-rate diagnostic

Three extra mode-4 stop-control renders change only native slow/fast nominal
rates in separate private diagnostic banks to the reference's measured mean
procedure-entry rates. Inputs/reference WAVs stay byte-identical. This is an
observed-rate injection, not an independently derived clock or shipping fix.

| Program | Slow/fast native rates Hz, old → diagnostic | Max band RMS difference, old → diagnostic | Active envelope p95, old → diagnostic |
| --- | --- | --- | --- |
| Small Plate | 73.6/368.5 → 57.2/286.4 | 7.4872% → 5.9422% | 88.9914% → 88.8823% |
| CD Plate A | 61.6/308.0 → 99.7/498.2 | 8.5058% → 8.6472% | 26.8945% → 27.0225% |
| CD Plate B | 64.1/320.8 → 99.8/498.8 | 7.4460% → 7.4490% | 31.1865% → 31.2023% |

Changing only the average rates is insufficient and is rejected as a correction.
The next sound-focused experiment is to compare native/reference trigger,
LF/MID ramp and coefficient-write visibility for the retained CD Plate A
mode-4 stop fixture. Native `Dynamics::slow_step` currently collapses a bounded
fall sequence into one tick; its relation to the measured envelope discrepancy
must be established before changing production behavior. Complete controller
reproduction is not automatically required by these results.
The following [coefficient-trajectory isolation](XL_DYNAMIC_ENVELOPE_VALIDATION.md)
establishes that relationship for CD Plate A: observed coefficient replay reaches
0.0575% maximum band-level and 1.2953% active-envelope p95 error on unchanged
input/reference WAVs. A native gradual-transition/busy model is still required;
the replay is diagnostic evidence, not a shipped improvement.

The combined concrete commands/provenance are in `catalog/all-plan.json` and
the 313 `run.json` files. `all-percentages/band-percentages.csv` retains signed
errors and fit rejection reasons; `all-percentages/case-percentages.csv` adds
the envelope/floor diagnostics. `average-rate-diagnostic-runs.json` records
the three private-bank diagnostics, while `percentage-selfcheck.json` verifies
amplitude conversion, unavailable values, doubled-gain envelopes and silence.
Eleven extra targeted diagnostic renders are retained separately from the
313-case matrix; they do not expand its acceptance count.

Internal numerical results, built formats and prior CPU/heap validation do not
establish listened or DAW-host validation. No production DSP or normal user
cache is changed by this internal test/reporting pass. Further controller work
is justified only by a concrete audible/measurement discrepancy; the previous
component engineering is preserved.
