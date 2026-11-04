# LexIR listening render artifact investigation

**User follow-up:** the user listened, confirmed enabled Decay Opt as the artifact
source and requested all comparisons rebuilt with it off. The [Opt-off rebuild
and cleanup report](LEXIRS_OPT_OFF_REBUILD_2026-11-04.md) records the completed
85 renders/presets and removal of the old audio locally and on the VPS. The
diagnostic collection URL below is historical and now returns 404. The
investigative findings below retain their pre-feedback uncertainty; no universal
firmware defect is inferred from the user's listening diagnosis.

Date: November 4, 2026. Baseline: `30ac7d136899bfe51ee8f11ddc423324bd1e8a9c`.
This is an offline investigation of the user's reported digital-overload-like
sound, especially Concert Hall V7 after approximately 3 seconds, in both Original
and Clean and in other algorithms. No production DSP, plugin, installed bundle,
parameter ID, prepared bank or existing listening collection is changed.

## Findings and limits

- The published Concert Hall V7 can be reproduced within one 24-bit PCM step.
  Additional Plate V1, Room V1 and Rich Chamber V1 baselines are also checked
  against their published files. This is a deterministic render, rather than
  evidence of a streaming dropout or realtime underrun.
- All 85 published Cineol FLACs (71 Original, 14 Clean) contain finite samples
  and no sample at or beyond full scale. The highest file peak is about
  -2.14 dBFS; Concert Hall V7 peaks at about -7.85 dBFS. This rules out final
  file clipping, not every possible nonlinear mechanism inside the engine.
- A build-only direct-engine harness counted the graph's accumulator saturation
  events. All 42 tested renders report zero, including warmup. Coverage: Concert
  Hall V7 and Dark Hall V7, Original/Clean, modes 0/1/2/3 at ordinary level and
  mode 3 at input scales 1, 0.5, 0.25 and 0.125; plus Plate V1, Room V1 and Rich
  Chamber V1 with their published modes and relevant switch-off alternatives.
  Therefore ordinary graph accumulator overload is not supported in these
  cases. This is not an all-71 internal headroom certification.
- Concert Hall Original versus Clean raw-output RMS difference at 3–4.5 s is
  -52.32 dB relative to the output signal. Both have the same observed control
  transitions. Clean's finer arithmetic does not remove the shared control
  behavior; this strongly disfavors word rounding as the main reported effect.
- Decay Opt repeatedly changes the feedback reduction while the sustained
  chord is playing. In Concert V7 its amount covers 0–12, often advancing at
  about 33 ms intervals and resetting on level detector events. Room and Rich
  Chamber also exhibit feedback changes; disabling Opt removes those changes.
  Plate V1 already has Opt off, so Opt cannot explain every reported case.

The main current candidate is the audible behavior of time-varying Mod/Opt
and their control updates. It has not yet been established that this is the
specific glitch the user hears, or that every algorithm has the same cause.
No candidate switch-off render is accepted as a sound fix without listening.

## Independent firmware evidence

Reference: pinned Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`,
private known-set XL v8.21 ROMs, bank v5 SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The reference executes firmware offline; no firmware executes in native audio.
It is an emulator reference, not a new physical-unit recording.

`xl_sound_compare.cpp` boots, uses physical program/fader/toggle operations,
settles, warms for 3670 ms and processes the exact test source. For Concert V7,
physical overrides 0:176, 1:168, 6:76, 7:76, 10:72 resolve to the same logical
values in both paths; every active control is checked. Both modes 3 (Mod/Opt)
and 1 (Mod only) are rendered, A/C wet, Analog on, 0 dB input, independent
clocks, 48 kHz. Both native processing allocation/release counts are zero.

The independent firmware also repeatedly changes the Opt amount during these
chords and produces additional spectral energy between source partials. Thus
the existence of these changes is not, by itself, evidence of a Cineol defect.
Chronology is not identical: for example, a reference retrigger descends through
intermediate amounts while the current simple native controller restores more
abruptly. This is a retained timing limitation, not an accepted explanation of
the user's particular audible complaint.

A diagnostic spectral-spread proxy measures 350–2500 Hz energy more than 5 Hz
from the known input partials, with Hann windows and both chords included where
their tails overlap. It includes residual percussion, resonances and finite
window leakage; it is neither THD nor a perceptual score.

| Window | Cineol Mod+Opt | Cineol Mod only | Firmware Mod+Opt | Firmware Mod only |
| --- | ---: | ---: | ---: | ---: |
| 2.0–3.0 s | -14.55 dB | -19.27 dB | -15.94 dB | -19.85 dB |
| 3.5–4.4 s | -17.33 dB | -20.44 dB | -16.48 dB | -22.19 dB |

Opt changes this behavior in both implementations. Different modulation/control
phases preclude sample-identical comparisons; these values cannot establish
which version sounds acceptable.

## Comparison preparation confound

The initial impulse fit for Concert V7 selected mode 1, while the listening
export selected mode 3 on the musical source because it reduced a coarse
envelope-fit loss. That automatic choice enabled level-dependent Opt without
listening acceptance. An IR is a fixed convolution kernel and cannot reproduce
the running algorithm's ongoing level-dependent or modulated control behavior.
The existing collection must therefore be treated as fitted listening candidates,
not proof of algorithm equivalence. Do not silently disable Opt across all
presets: it changes decay behavior and does not address Opt-off cases such as
Plate V1.

## Reproduction and retained artifacts

Private root: `build/validation/lexir-hall-artifacts-20261104/`.

- `input.f32`: deterministic 48 kHz stereo source from
  `script/lexir_listening.py::fixture()`, four percussion events then chords at
  1.9 and 3.2 s, input stops at 4.5 s. Original source peak about 0.5025;
  sustained chord peaks about 0.116. Warmup is 176160 host frames.
- `trace.cpp`, `trace-wide.cpp`, `.job` recipes and `.csv` controller records;
  `matrix.json`, `additional-matrix.json` retain all 42 saturation results.
- `reference-compare` is compiled from the existing source with C++20, `-O2`
  and `-ffp-contract=off`. `reference-trace.cpp` is a build-only copy that records
  reference controller RAM after blocks without changing reference state.
- `rom-concert-mode3/`, `rom-traced-mode3/`, `rom-concert-mode1/`: raw independent
  renders, controls, fixture/bank identities and startup metadata. The traced
  recipe uses 48-frame blocks; ordinary comparisons use 256.
- `published-file-sanity.json`, `diagnostic-sanity.json`,
  `clean-difference.json`, `spectral-spread.json`, `concert-spectrogram.png`.
- `probe-additional.py` and `prepare-listening.py` reproduce the extra cases
  and diagnostic listening collection. `listening/provenance.json` records
  source hashes, scalar gains and reference latency removal.

Representative independent reference command, after compiling the existing tool:

```sh
build/validation/lexir-hall-artifacts-20261104/reference-compare \
  build/validation/xl-catalog-20261005/rom-private-local \
  build/clock-experiment/cache/programs-v821-native-v5.bankxl \
  build/validation/lexir-hall-artifacts-20261104/rom-new-mode3 \
  0 3 file 224821 1 3670 7 256 1 \
  --control=0:176 --control=1:168 --control=6:76 --control=7:76 \
  --control=10:72 \
  --input-wav=build/validation/lexir-hall-artifacts-20261104/input-short.wav
```

The prepared diagnostic collection has six cases and 23 variant entries:
Concert V7 control isolation, independent firmware, reduced input level, and
Plate/Room/Rich Chamber controls. Audio uses one RMS gain over 0–4.5 s to
-22 dBFS, 57-sample reference latency compensation and the first 335943 actual
samples (6.9988125 s), with no EQ, limiter or synthetic tail padding. The earlier
3.25–4.35 s high/low frequency ratio in `matrix.json` is retained only as an
exploratory proxy; it did not reliably identify the reported roughness.

Automatic approval review initially rejected deployment pending explicit consent.
The user subsequently approved this exact six-case diagnostic collection, and it
is now published on the existing private Listen service:
`https://45-128-204-208.sslip.io/listen/?collection=cineol-artifact-probe-2026-11-04`.
All 20 unique files were hash-verified on the server before activation. The
authenticated HTTPS manifest returns 200 and every FLAC range returns 206;
unauthenticated metadata returns 401. The original 71-case collection remains
available. Results are retained in the private root's `production-checks.json`.

No CPU benchmark or full plugin regression is needed for an offline-only
investigation with no production change. No listening acceptance, physical-unit
validation or universal cause/fix is claimed. Next have the user compare the
same 3–4.5 s interval in original/Clean/Opt-off/Mod-and-Opt-off, then compare
with the independent firmware, before choosing a bounded DSP correction.

Nineteen unselected intermediate matrix `.f32` files (97.4 MiB) were removed
after retaining recipes, logs and metrics. The additional matrix's raw `.f32`
files were converted to retained listening FLACs and removed. `cleanup.log`
records these paths and reproduction details; baseline/source assets, private
ROMs, prepared banks and unique reference captures are retained.
