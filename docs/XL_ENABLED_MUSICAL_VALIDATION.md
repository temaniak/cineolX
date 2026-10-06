# XL Dynamic and Mod musical checkpoint

October 6, 2026. After the listener found the four mode-0 musical examples
close, this bounded block compares two enabled-mode current-source examples.
No DSP correction is introduced and no earlier acceptance is broadened.

## Independent cases

Both cases reuse the exact four-second, peak-0.2 dual-mono SH-101 demo riff
prepared in the [baseline musical checkpoint](XL_MUSICAL_LISTENING_VALIDATION.md).
The source SHA-256 is
`7fcbbf61a8cfd1e789843e0e4c796ed94426db2133572a61a2576200e7b6029d`.
Capture duration is twelve seconds, with input exactly zero after four seconds;
48 kHz, 256-frame blocks, analog on, wet A/C, 0 dB gain and 1,000 ms warmup.

- CD Plate A: Dynamic on, Mod/Decay Opt off; physical STOP faders 18/18,
  stop-delay override request 10. CD Plate A uses shared STOP cells 6/7,
  recorded as 18/18; the native falling positions quantize to 16/16.
  Cells 12/13 remain 5/5 but are not the selected STOP pair for this profile.
  Delay cell 45 remains zero; the override request did not establish a changed
  physical delay. Native/reference control audits match the recorded values.
- Concert Hall: Mod on, Dynamic/Decay Opt off; factory controls. The matching
  switch record is 64. This is an independently clocked modulation comparison,
  not a phase-aligned waveform test.

The unchanged accepted offline renderer and bank are used. Reference settings
come from the existing physical program/fader/toggle recipe. No controller
snapshot, phase offset, event schedule or reference WCS is injected into native.
Both runs finish with finite output, the existing peak guard and zero tracked
processing allocations/releases. Input samples match the prepared source followed
by zeros. Byte-identical input WAVs share immutable hard-linked storage with
the prior canonical musical fixture after hash verification.

## Numerical diagnostics

These remain percentage deviations for particular independent histories;
they are not whole-algorithm similarity scores or proof of audible defects.

| Case | Maximum 100 Hz–15 kHz band RMS deviation | Active-envelope median | Active-envelope p95 | Maximum active-window deviation |
| --- | ---: | ---: | ---: | ---: |
| CD Plate A, Dynamic | 69.5149% | 5.1499% | 22.0847% | 32.2559% |
| Concert Hall, Mod | 7.3150% | 4.5818% | 25.8908% | 61.3395% |

CD Plate A's maximum band percentage is 8–12 kHz, where whole-capture
reference/native RMS is only −102.83/−98.25 dBFS. The 4–8 kHz band is
−92.21/−89.14 dBFS, and 12–15 kHz is −107.81/−103.52 dBFS. Below 4 kHz its
maximum band deviation is about 1.71%. The large upper-band ratios involve
very quiet energy and must not be described as a 69% overall sound difference.
Raw floor and transient data remain available, rather than being suppressed.

Concert Hall's largest band deviation is 100–250 Hz, +0.6132 dB. Of its eight
primary bands, two have valid paired T20-derived duration estimates, with maximum
deviation 9.4189%. Its other six fits remain unavailable. All eight Dynamic
case fits are inapplicable; those fourteen unavailable fits are not passes.
Different free-running modulation/controller histories remain a relevant
interpretation limit. These two cases do not estimate the original's phase
distribution or establish a systematic implementation fault.

## Listening and next decision

The two 12.5-second pairs are under ignored
`build/validation/xl-enabled-musical-20261006/listening/`:
`cd-plate-a-dynamic-reference-then-xl.wav` and
`concert-hall-mod-reference-then-xl.wav`.
Each contains six seconds of reference, 500 ms silence, then six seconds of
current native, covering four seconds of riff and two seconds of tail.

Whole-variant scalar gains match RMS over the four input-active seconds to
−24 dBFS, reduced if peak headroom requires it. Gains are approximately 11.47/
11.44 dB for Dynamic reference/native and 10.73/10.40 dB for Mod reference/native.
There is no output EQ, time alignment or tail fade. Saved sample counts, matched
RMS, finite levels and peak headroom pass. Raw files remain unchanged;
the manifest records ordering and source/output identities.

The listener reports a digital crunch during playing in both Dynamic variants.
This is a shared audible concern, not a reported native/reference difference;
After the bounded tail follow-up, the user provisionally retained the current
Dynamic behavior. The user also explicitly reports no audible difference in the
presented Concert Hall Mod pair. Provisionally accept this particular Mod-on
musical example and stop fitting its short-window numerical deviations.
Current-source renders
do not assert installed-plugin identity or physical-unit sound validation.
If a material difference is heard, the next step targets that specific mode
and tests reference variability or an existing bounded correction before
additional controller work. If the examples sound close, provisionally retain
them and advance representative coverage. No all-program or combined-mode
acceptance follows from these two cases.

## Bounded crunch inspection

The prepared input peaks at 0.2; raw reference/native peaks are 0.0876862/
0.0841106. The listening pair peaks at 0.3282844, with zero samples at or above
full scale. Both listening segments are exactly their raw float32 prefixes
multiplied by the recorded scalar gain. File clipping and additional listening
preparation distortion are excluded for this fixture.

A private native saturation probe reconstructs the unchanged CD Plate A runtime
recipe and reports zero core saturations during the music and tail. It reproduces
575,998 saved native frames exactly; the first two wrapper latency frames are
excluded. Reference internal saturation is not measured. The probe is supporting
evidence and does not establish the audible cause or physical-hardware behavior.

Input coloration and level-triggered coefficient transitions under the aggressive
short shared-STOP, delay-zero setup remain hypotheses. A four-second scalar-level-matched
dry riff, `listening/dry-riff-no-reverb.wav`, is provided to separate those causes;
dry-source listening was deferred by the user. The previously observed very late tonal
floor is a separate observation and does not explain crunch during playing by
itself. `crunch-preparation.json`, the private probe source and its compile/run
logs retain the measurements. No production DSP change, new full render or CPU
benchmark is added; only the short dry diagnostic WAV is generated.

The user deferred dry-source listening, then clarified that the concern is
rapid stair-like tail decay in Dynamic, heard on monitors. The following
[bounded STOP/tail comparison](XL_DYNAMIC_TAIL_VALIDATION.md) isolates Dynamic
off and a longer effective shared STOP. It corrects the earlier interpretation
of cells 12/13 and supports a Dynamic/STOP-dependent rapid decay, while keeping
exact quantization/aliasing causality open.
Both enabled-mode examples are now provisionally retained within their reviewed
scope. This does not accept every program, material, parameter setting or combined
mode, nor establish physical-unit identity. Advance representative coverage;
do not repeat these renders or CPU measurements without new evidence.

## Provenance and cleanup

Private root: `build/validation/xl-enabled-musical-20261006/`; it retains exact
argv, exit codes, source/mode settings, raw recordings, metrics, absolute band
levels, paired WAVs, script identities and `checkpoint.json`. HEAD is
`c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; clean Reflexion remains
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. The unchanged v5 bank hash is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The renderer's hash is verified against the accepted baseline musical checkpoint.
No runtime, tool, bank, dependency, plugin or installed bundle changes occur.
CPU is not repeated for render/listening preparation. Only two new reference/
native cases are produced; identical input storage is shared and pending
listening pairs/current evidence are retained.
