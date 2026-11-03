# Clean-mode algorithm shortlist — 2026-11-02

Status: seven-program selection approved by the user on November 2. The separate [Clean A/B prototype](CLEAN_AB_PROTOTYPE_2026-11-02.md) is now implemented; main-plugin DSP and sound defaults are unchanged.

## Scope and method

Native-only screening of all six implemented original-224 reverbs and all 19 XL reverb-containing programs, including split programs. The three XL non-reverb effects are excluded. This is a bounded noise-candidate screen, not a physical-unit comparison, a proof of the noise mechanism, or listening acceptance.

Current working-tree source and existing private banks were used. Public source snapshots, hashes, exact bank paths/hashes, compiler identity and working diff are in `build/validation/clean-shortlist-20261102/`. Bank files were read without alteration.

At 48 kHz, warm up with one second of silence, apply 0.5 s of deterministic independent stereo uniform noise, then process exactly 30 s of zero input. Two input recipes are peak 0.08/seed17 and peak 0.008/seed991. Each is measured with Mod Enhancement and Decay Optimization both off, then both on. Dynamic Decay remains off. Dirt and Space are zero; wet only; unity input gain.

Original-224 uses common plugin default faders, not separate historical factory presets: bass17, mid14, crossover5, treble23, depth21, diffusion1, minimum predelay; hidden rate2/depth1/optimization5. XL uses factory faders with all level-kind slots set to128, A/C ordinary output routing and A/B split routing. These settings do not exhaust control-dependent noise behavior.

Statistics windows are active input and 2–4, 10–12, 20–22 and 28–30 seconds after input stop. Final AC RMS subtracts each channel's own mean and pools channel powers. Total RMS and peaks are also retained. -360 dBFS is the analysis reporting floor, not a measured noise level. Values are direct, without loudness normalization, dithering, gate or output fade. A decaying tail must not automatically be called noise; later-window trends and prior evidence support candidate selection.

100 recipes completed with 500 statistics rows, all finite. The per-channel calculation supersedes the preliminary pooled-mean diagnostic. No WAV campaign was generated and no listening test was performed.

## Proposed first A/B set

| Model | Program | Highest late AC RMS observed | Selection reason |
| --- | --- | ---: | --- |
| 224 | Large Concert Hall B | -98.66 dBFS | Requested Large Concert Hall B; core Hall B timbre control. |
| 224 | Small Concert Hall A | -92.71 dBFS | Highest observed original-224 late AC residual; strongly mode/history dependent. |
| 224 | Percussion Plate A | -94.64 dBFS | Highest consistently elevated original-224 late AC residual across all four recipes. |
| 224XL | Concert Hall | -94.77 dBFS | Requested principal XL hall; representative modulated residual. |
| 224XL | Bright Hall | -92.73 dBFS | Requested bright hall; high residual in one quiet/modulated recipe. |
| 224XL | Dark Hall | -98.64 dBFS | Requested dark hall; darker timbre control, not a worst-noise winner. |
| 224XL | CD Plate B | -90.46 dBFS | Highest and consistently elevated XL late AC residual in this screening. |

These seven balance the four explicitly requested halls with two original-224 noise candidates and the strongest XL candidate. Plate / Plate is a reserve XL case: high residual, but a split engine adds routing and two-child complexity to a first prototype. CD Plate A is another reserve for a later Dynamic-specific test; its previously heard short-tail grain is distinct from the stationary-floor screen. Hall / Hall is not selected on prior evidence alone: this current recipe reaches the AC reporting floor, despite the older control-dependent tonal example.

## Full bounded screen

Range below is min/max across the four recipes at 28–30 s after input stop, not the device's universal noise specification.

| Model | Program | Lowest late AC RMS | Highest late AC RMS | Largest absolute change from 20–22 s to 28–30 s |
| --- | --- | ---: | ---: | ---: |
| 224 | Small Hall A | below reporting floor | -92.71 dBFS | 8.56 dB |
| 224 | Percussion Plate A | -95.03 dBFS | -94.64 dBFS | 0.05 dB |
| 224 | Vocal Plate | -95.83 dBFS | -95.34 dBFS | 0.07 dB |
| 224 | Acoustic Chamber | -96.04 dBFS | -95.80 dBFS | 0.38 dB |
| 224 | Small Hall B | -101.89 dBFS | -98.66 dBFS | 0.09 dB |
| 224 | Large Hall B | -101.89 dBFS | -98.66 dBFS | 0.15 dB |
| 224XL | CD Plate B | -91.48 dBFS | -90.46 dBFS | 0.48 dB |
| 224XL | Plate / Plate | -93.17 dBFS | -91.63 dBFS | 0.62 dB |
| 224XL | Bright Hall | below reporting floor | -92.73 dBFS | 0.75 dB |
| 224XL | Small Plate | -94.43 dBFS | -93.23 dBFS | 0.10 dB |
| 224XL | Inverse Room | -95.68 dBFS | -93.75 dBFS | 0.01 dB |
| 224XL | CD Plate A | -94.62 dBFS | -93.78 dBFS | 2.79 dB |
| 224XL | Small Room | -104.77 dBFS | -94.06 dBFS | 10.67 dB |
| 224XL | Room | -102.52 dBFS | -94.24 dBFS | 0.60 dB |
| 224XL | Plate | -95.02 dBFS | -94.31 dBFS | 0.19 dB |
| 224XL | Concert Hall | -100.36 dBFS | -94.77 dBFS | 0.76 dB |
| 224XL | Rich Split | -98.55 dBFS | -96.77 dBFS | 0.02 dB |
| 224XL | Rich Chamber | -116.69 dBFS | -96.86 dBFS | 1.87 dB |
| 224XL | Dark Chamber | -113.94 dBFS | -97.81 dBFS | 0.86 dB |
| 224XL | Plate / Hall | -99.84 dBFS | -98.22 dBFS | 0.25 dB |
| 224XL | Dark Hall | -105.55 dBFS | -98.64 dBFS | 0.23 dB |
| 224XL | Plate / Chorus | -111.05 dBFS | -98.72 dBFS | 0.01 dB |
| 224XL | Chamber | -99.26 dBFS | -98.75 dBFS | 0.00 dB |
| 224XL | Rich Plate | -125.02 dBFS | -115.10 dBFS | 1.66 dB |
| 224XL | Hall / Hall | below reporting floor | below reporting floor | unavailable |

## Reproduction and evidence

Final metric CSV: `build/validation/clean-shortlist-20261102/metrics-per-channel.csv`. Source: `probe_per_channel.cpp`. Identity/recipe: `provenance.json`; public headers/core snapshot: `source-snapshot/`. The superseded pooled-mean source, binary and CSV are small retained diagnostics; they are not used in the proposal. No existing captures or fixtures were deleted.

```sh
c++ -std=c++20 -O3 -ffp-contract=off -Ibuild/validation/clean-shortlist-20261102/source-snapshot build/validation/clean-shortlist-20261102/probe_per_channel.cpp build/validation/clean-shortlist-20261102/source-snapshot/native-hall/core/hall.cpp -o /tmp/cineol-clean-shortlist-probe
/tmp/cineol-clean-shortlist-probe build/clock-experiment/cache/programs-v44-import-v1.bank224 build/validation/hidden-controls-fix-20261102/preview-cache/programs-v821-native-v5.bankxl > /tmp/cineol-clean-shortlist-metrics.csv
```

The user explicitly requested algorithm approval before modifying DSP or creating the minimal A/B application. Approval was subsequently given; implementation and validation are recorded in the linked prototype report. The proposed future A/B keeps original clocks, coefficients and converter filtering; increasing bandwidth or reusing the 48 kHz float Hall is not part of this proposal.
