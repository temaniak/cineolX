# XL representative musical comparisons

October 6, 2026. This follows preliminary listening retention of the short
Hall / Hall example. It prepares four independent current-native/reference
comparisons on an actual musical source, without another DSP correction.

## Source and recipe

The input reuses the repository's local Reflexion web-demo asset
`deps/reflexion/web-demo/page/sounds/aphex-twin-tha-riff.flac`, described by
the demo as an SH-101 riff. The first four seconds are decoded with the
installed FFmpeg, resampled from 44.1 to 48 kHz, scaled to peak 0.2, given
10 ms input onset/end fades and copied from mono to both input channels.
Prepared source samples, original/prepared hashes, decoder version and exact
argv are retained privately in `source.json`. No source is downloaded or
committed; the dependency asset is unchanged.

Concert Hall, Plate, Room and CD Plate A use factory controls, Dynamic/Mod/
Decay Opt off, wet A/C, analog on, 0 dB input gain, 1,000 ms silent warmup,
48 kHz and 256-frame blocks. Captures last twelve seconds: four seconds of
music and eight seconds of stopped input. Reference preparation uses the
existing physical program/fader/toggle protocol. Native/reference clocks
remain independent. Each rendered input is sample-identical to the prepared
source followed by exact zeros. No reference WCS/state trajectory is injected.

## Small offline-tool extension and checks

`native-hall/tools/xl_sound_compare.cpp` adds a `file` fixture with
`--input-wav=PATH`. It intentionally accepts only the controlled canonical
stereo float32 RIFF format at 48 kHz, verifies file length/header/sample bounds
and finite values, and requires at least one second of stopped input. Reads,
buffer allocation and sample copying occur outside native processing.
Example argv is retained in the private campaign's `commands.json`.

Only file fixtures add `input_stop_s` to the existing metadata schema.
`script/analyze_xl_sound.py` uses that value plus 200 ms for stopped-tail
analysis instead of assuming the synthetic music fixture's two-second stop.
Legacy fixture paths and metadata keep their behavior.

Four negative checks reject wrong sample rate, a nonfinite sample, a source
longer than the allowed capture, and a missing file option before reference
setup/output-directory creation. A legacy three-second Concert Hall fixture
passes exact equality against the retained first 144,000 frames of input,
reference, native and static-WCS diagnostic recordings. This checks that the
offline extension preserves existing sound recipes. Verified temporary legacy
prefix WAVs and malformed inputs are removed; hashes and regeneration details
remain in `legacy-regression.json` and `negative-checks.json`.

All four musical renders finish with finite output, the existing peak guard,
zero processing allocations/releases and identical source samples on both paths.
No runtime, filters, converters, bank, plugin identifiers or installed bundles
change. CPU is not benchmarked for this offline fixture extension; preserved
production and previously measured source identities are verified separately.

## Numerical triage

Band RMS differences use the existing eight 100 Hz–15 kHz bands, before any
listening level matching. Envelope diagnostics use 50 ms windows active in
the reference, with the existing `max(peak - 40 dB, -90 dBFS)` floor policy.

| Program | Maximum band RMS difference | Active-envelope median | Active-envelope p95 | Maximum active-window difference |
| --- | ---: | ---: | ---: | ---: |
| Concert Hall | 2.4771% | 0.3137% | 1.5110% | 6.5277% |
| Plate | 0.6481% | 0.2730% | 0.9802% | 3.2294% |
| Room | 0.7388% | 0.3882% | 1.4397% | 5.9223% |
| CD Plate A | 0.4494% | 0.4199% | 1.5307% | 5.6900% |

All 32 measurable primary level bands are within the provisional 5% triage
criterion. Six paired decay bands have usable T20-derived fits, with maximum
duration difference 0.1428%; the other 26 remain unavailable and are not passes.
Upper-band floor/non-exponential/capture-margin rejections remain in the CSVs.
Envelope percentages are diagnostics, not a whole-algorithm error score or
proof of an audible defect. This musical input contains no independent stereo
source, percussion/voice material, Mod-on or Dynamic behavior coverage.

## Listening deliverables and limits

Four files under ignored `build/validation/xl-musical-listening-20261006/listening/`
contain six seconds of reference, 500 ms silence, then six seconds of current
native; each segment contains the four-second riff and two seconds of tail.
Names are `concert-hall-reference-then-xl.wav`, `plate-reference-then-xl.wav`,
`room-reference-then-xl.wav` and `cd-plate-a-reference-then-xl.wav`.

Per-variant scalar gains match RMS over the four input-active seconds to a
common −24 dBFS target, reduced if peak headroom requires it. Gains are about
9.7–11.4 dB here. There is no output EQ, time alignment or tail fade. Saved
sample counts, matched RMS, finite levels and peak headroom are verified;
`listening/manifest.json` records ordering, gains and hashes. Full twelve-second
raw recordings remain available for longer tails and later regression.

Reference means the pinned ROM emulator, not a new physical-unit capture.
These recordings use current source, not an assertion about an installed
plugin's version. The user subsequently reports that all four presented pairs
sound close; this preliminary outcome is recorded below. No new correction is
justified by these small band differences alone.

## Listener outcome

The user explicitly selected "The sound is close in all four pairs" for
Concert Hall, Plate, Room and CD Plate A. Provisionally retain the current
native behavior for these mode-0 musical examples and stop fitting their
already small baseline differences. Together with numerical triage, this is
practical evidence of close basic timbre/tail behavior in the presented material.

Scope remains the first four seconds of one mono synth riff and two further
seconds of audible tail per segment, factory controls, all toggles off and
current source. It does not accept all XL programs, independent stereo inputs,
longer unseen tails, Dynamic/Mod-on modes, automation or installed-host behavior.
The next bounded sound block is representative Dynamic and Mod behavior on
musical input, pursuing material differences rather than reopening this
accepted baseline comparison without new evidence.

## Provenance and cleanup

Private evidence: `build/validation/xl-musical-listening-20261006/` contains
source preparation, exact render/analysis commands, input/legacy guard results,
metrics, pairs, logs and `checkpoint.json`. HEAD remains
`c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; clean Reflexion remains
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. The unchanged v5 bank hash is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Only four new source comparisons are rendered. Keep their current raw evidence
and pending listening pairs; confirmed disposable negative/prefix WAVs have
already been removed. No prior baseline, user asset, bank or dependency is
deleted or modified.
