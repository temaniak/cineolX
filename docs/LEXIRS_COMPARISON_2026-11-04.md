# Little Devil 224XL IR matching — November 4, 2026

The [completed Opt-off rebuild](LEXIRS_OPT_OFF_REBUILD_2026-11-04.md) supersedes
the original listening export: after the user's listening diagnosis, all 71
Cineol renders, 14 Clean alternatives and 85 presets were rebuilt with Decay Opt
disabled. References and other controls are unchanged. Old comparison and
diagnostic audio was deleted locally and on the VPS at the user's request.

The subsequent [artifact investigation](LEXIRS_ARTIFACT_DIAGNOSIS_2026-11-04.md)
records the user's listening objection, unclipped-file and accumulator checks,
and a control-selection confound: Concert V7's export enabled Decay Opt after
the impulse fit had selected it off. These are listening candidates; numerical
fitting alone does not accept their sound. Original outputs are retained while
switch-isolation and independent firmware comparisons await evaluation.

## Scope and interpretation

The private library at `/Volumes/Something/LexIRs` contains 71 true-stereo
sets, covering 14 **224XL** algorithms. Each set has two stereo 96 kHz,
24-bit, eight-second WAVs: `.L` is the response of both outputs to the left
input; `.R` is the response of both outputs to the right input. All 142 WAVs
were inspected directly inside the four ZIPs. The existing extracted Halls
folder was not treated as another independent capture.

The README identifies Little Devil Studios and does not give capture control
values or firmware revision. Its installation directions were treated as
documentation; no system installation, account upload or redistribution was
performed. Original archives and user recordings remain untouched.

The [author's description](https://littledevilstudios.com.au/2015/03/lexicon-224xl-impulse-response-library/)
confirms an actual 224XL and a Lynx Aurora 16. It explicitly describes incomplete
chorus representation and unsuccessful capture of some nearly infinite tails.
The author's routing explanation and
[LiquidSonics' topology documentation](https://www.liquidsonics.com/knowledge-base/reverb-processing-topologies/)
agree on the four convolution paths used here.

This is evidence about these archived captures and the native XL engine. It
does not establish original-224 v4.4 accuracy, capture firmware identity, or
listening acceptance. A single static response cannot identify all
signal-dependent or time-varying behavior. Capture gain, excitation level,
modulation phase and any capture trimming remain unknown. Fitted settings are
useful candidates, not proven recovery of the operator's settings.

## Results and remaining differences

All 71 sets have candidate presets and level-matched files. Much of the library
is explained by the measured factory variations without changing the DSP.
Across 994 band/input observations, median absolute **relative band-balance**
difference is 0.062 dB. This describes seven wide integrated bands after gain
normalization, not fine spectral equality or an overall fidelity score.

There are 773 paired T20 fits after the stated quality/margin checks and
excluding inverse and the two long Rich V7 responses. Median absolute T20
difference is 3.82%, the 90th percentile 18.88%, and the largest 100.0%.
The worst accepted case is Dark Hall V1, left-driven 500–1000 Hz: 2.86 s in
the archive and 5.72 s in this native render. Dark Hall V1/V3, some high bands
of Rich Plate V8 and selected short rooms retain substantial decay differences.
These level/phase/controller-dependent fits are diagnostics, not proof of one
specific DSP defect. A good band-balance overlay does not accept the tail.

The largest overall fitting residuals are Small Room V3, Inverse Room V1/V2,
Rich Chamber V7, Rich Plate V7 and Dark Hall V5. Their unfavorable comparisons
are included. Rich V7 material falls away in the finite captured response while
native long-tail behavior persists; replacing that behavior with an output
fade would hide the mismatch. A bounded Dark Hall V5 test at Original, 50% and
Stable worsened the musical diagnostic as stability increased, so Original
was retained. This is not an acceptance of its remaining tail difference.

Bright Hall V1 matches factory V4 much better than factory V1 (four-path
diagnostic about 0.40 versus 2.83 for the named variation's best four-path
screen). Its preset therefore starts from V4. This suggests a naming/capture
anomaly; the actual operator settings are unconfirmed. Concert/Dark Hall V6
and Plate/Small Plate V5 also rank nearer V2; those variations differ in stop
settings which can be ambiguous in a static capture.

The requested stopped percussion/chords test also screens Optimization and
Dynamic changes. `impulse_fitted_mode` and `music_mode_selection` preserve
that distinction in each JSON. The final preset matches the B listening file.

Evidence under the ignored validation root:

- `paired-metrics.csv`: every band/input result, including unavailable pairs.
- `summary.json`: aggregate metrics and explicit listening-pending status.
- `band-balance.png`: independent impulse band-energy overlays for 14 V1 sets.
- `stopped-tails.png`: actual level-matched music after the input stops.
- `verification.json`: 142 decoded finite FLACs, 15 verified 29-second PCM24
  A/B WAVs, matching hashes and active-window RMS differences below 0.002 dB.
- `preset-check.log`: all 85 presets (71 primary + 14 Clean alternatives) loaded in the existing November 3
  processor build, all 74 public parameter values retained, finite callback
  output at 48 kHz / 256 frames. This is an offline processor check, not a DAW
  listening session or revalidation of the installed AU/VST3.

## Method

- Baseline checkout: `30ac7d136899bfe51ee8f11ddc423324bd1e8a9c` (1.1a).
  Existing user changes to CMake, NOTICE and Pitch Lab were preserved.
- Current private XL v5 prepared bank, original precision, Clean off, Dirt 0,
  Space 0, Original Dark Hall variation, front A/C outputs, 48 kHz.
  No DSP, plugin identifiers, parameter IDs or installed settings changed.
- Original 96 kHz audio was downsampled with a compensated 255-tap
  Kaiser-windowed sinc low-pass. No matching EQ or per-channel gain was added.
- Seven bands (100–250, 250–500, 500–1000, 1000–2000, 2000–4000,
  4000–8000 and 8000–12000 Hz), smoothed 2048-sample STFT envelopes,
  energy distribution, buildup and energy quantiles were compared.
- Noise-compensated Schroeder T20 estimates retain R², fitting bounds,
  rejection reasons and capture-margin checks. Inverse and nearly infinite
  responses require envelope interpretation; an available straight-line fit
  alone does not make them ordinary exponential decays.
- The pinned Reflexion catalog
  `deps/reflexion/web-demo/page/catalogs/eb3a7a765a703ece.json` supplies measured
  factory variation controls. Firmware SIZE record bytes are scratch values;
  their displayed size was mapped back through native control scales.
  Chorus/Diffusion candidates obey the plugin's 8/4-raw-step aliases.
- Every factory variation within each algorithm was screened over eight
  Mod/Optimization/Dynamic combinations, with independent left- and
  right-driven renders. A bounded control search refined useful candidates.
  The fitting score combines band balance, smoothed envelopes and buildup;
  it is **not a similarity percentage** or an acceptance threshold.
- Fit excitation: 0.5 FS impulse, one-second warmup. Native declared latency
  of 57 samples was compensated. No match-specific waveform shift was used.
- Validation uses a different 3.67-second warmup, both driven inputs and
  separate stereo percussion/chords. Musical input stops at 4.5 seconds;
  captures continue to 14 seconds. Early, late and final input-silent RMS
  windows remain in the per-case metrics.

The convolution reference uses
`outL = inL * LL + inR * RL`, `outR = inL * LR + inR * RR`.
Cineol musical files run the native algorithm continuously; they are not
convolutions of a fitted Cineol impulse.

## Listening package

Private outputs are under `build/validation/lexirs-20261104/listening/`.
`index.html` lists all sets, `settings.csv` contains displayed and raw faders,
and each set has:

- `A-Lexicon-IR.flac`: four-path convolution of the archived capture.
- `B-Cineol.flac`: running native Cineol XL at the matched settings.
- `Matched.cineol224`: a separate schema-7 preset; it is not installed into the
  user's preset bank. It requires an existing XL v8.21 import.
- `settings-and-metrics.json`: full settings, scalar gains, hashes,
  independent impulse measurements and input-silent residual windows.

The V1 set of every family and Bright Hall V4 also have `A-then-B.wav`:
14 seconds of A, one second of silence, 14 seconds of B. The same dry test is
`00-DRY.wav`. Playback is wet-only. Stereo active-window RMS is equalized by a
single gain per variant to at most −22 dBFS, reduced equally if either variant
requires peak headroom. No EQ, compression, gating or independent L/R
normalization is applied. Listening has **not** been performed by this run.

All 14 V1 examples additionally have `C-Cineol-Clean.flac` and
`Matched-Clean.cineol224`. These retain B's faders, switches and warmup while
enabling the already shipped higher-precision Clean path. Their active RMS is
matched separately to A. The Clean source export's 87 hashes were checked
against the current core/desktop sources before compilation; no source
mismatch was found. These alternatives expose the noise-floor difference
without changing modulation, filters or the unmatched envelope shapes.
The stopped-tail plot includes C. Aggregate fitting/T20 results above describe
the primary Original-precision B files, not C.

## Reproduction and retained evidence

```sh
mkdir -p build/validation/lexirs-20261104
clang++ -std=c++20 -O2 -ffp-contract=off native-hall/tools/lexir_render.cpp -o build/validation/lexirs-20261104/renderer
cp build/validation/lexirs-20261104/renderer build/validation/lexirs-20261104/renderer-display
build/validation/lexirs-20261104/renderer PRIVATE_XL_V5_BANK --describe > build/validation/lexirs-20261104/catalog.json
python3 script/lexir_match.py inventory
python3 script/lexir_match.py fit --bank PRIVATE_XL_V5_BANK
python3 script/lexir_listening.py identify --bank PRIVATE_XL_V5_BANK
python3 script/lexir_listening.py refine --bank PRIVATE_XL_V5_BANK
python3 script/lexir_listening.py export --bank PRIVATE_XL_V5_BANK
python3 script/lexir_report.py
```

Python requires NumPy; export also requires FFmpeg. Use a fresh output folder
or intentionally reuse the saved fitting records. The scripts accept
`--source`, `--output` and `--renderer`. No ROM execution is used by the
renderer. `provenance.json` records bank/archive/source/executable hashes.
Inventory, rejected fits, compact factory-feature caches, fitting histories
and logs remain under the same ignored validation root. Large duplicate
unpacked IRs and complete candidate-WAV campaigns were avoided.

The generated-material review retained the compact initial/blind fitting
histories, factory feature caches, final comparisons awaiting listening,
reports, commands and the isolated processor cache. No historical evidence,
source archive, recording, ROM or bank was deleted. The two primary renderer
builds differ only by the additional offline display mode; their sounding path
is the same. The separately labelled Clean renderer, its build command, source
and output hashes are retained beside `clean-listening.py` and
`clean-listening.json`. Pre-export floating-point residual metrics can fall below PCM24
resolution; use the decoded files and stopped-tail graph for digital silence.

This work changes offline tooling and evidence only, so no realtime CPU
comparison or DSP regression campaign is required. It does not claim new
realtime margin or a sonic correction to the shipped engine.
