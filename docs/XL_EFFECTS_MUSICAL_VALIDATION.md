# XL Inverse Room and effects musical checkpoint

October 6, 2026. Following positive listening feedback on Concert Hall Mod and
retention of CD Plate A Dynamic behavior, this bounded block adds four previously
unreviewed musical examples. Existing catalog metrics and synthetic listening
pairs remain available. Only these four musical comparisons are newly rendered;
the renderer, DSP, bank and dependencies are unchanged.

## Independent cases

Reuse the four-second peak-0.2 dual-mono demo riff from
[the baseline musical checkpoint](XL_MUSICAL_LISTENING_VALIDATION.md), SHA-256
`7fcbbf61a8cfd1e789843e0e4c796ed94426db2133572a61a2576200e7b6029d`.
Every capture is twelve seconds at 48 kHz, 256-frame blocks, analog on, wet A/C,
0 dB input gain, 1,000 ms warmup, with exactly zero input after four seconds.
Reference settings are operated physically and native/reference active controls
are audited. Native and reference clocks are independent; no phase, WCS or
controller-state injection is used.

- Inverse Room, index 9, mode 0: Dynamic, Mod and Decay Opt off, with level
  faders set to physical 128 using the existing `--audible-levels` recipe.
- Chorus & Echo, index 14, mode 1: Mod on to exercise chorus motion, factory
  controls; Dynamic and Decay Opt off. This extends the existing static mode-0
  catalog comparison rather than repeating it.
- Resonant Chords, index 15, and Multiband Delay, index 16, mode 0: factory
  controls with all toggles off.

All four renders pass finite-output/peak guards and report zero tracked native
processing allocations/releases. Saved inputs exactly equal the common prepared
riff followed by zeros; identical full inputs share existing immutable hard-linked
storage after byte-hash verification.

## Numerical diagnostics

| Case | Maximum primary band RMS deviation | Active-envelope median | Active-envelope p95 | Maximum active-window deviation |
| --- | ---: | ---: | ---: | ---: |
| Inverse Room | 0.4178% | 0.3677% | 2.7346% | 6.2731% |
| Chorus & Echo, Mod on | 8.5589% | 10.1690% | 52.8377% | 148.2253% |
| Resonant Chords | 0.3401% | 0.3758% | 1.6758% | 2.3336% |
| Multiband Delay | 0.7056% | 0.3835% | 3.5162% | 14.1649% |

Thirty-one of 32 primary level bands are within the provisional 5% triage limit.
Chorus & Echo's 500–1,000 Hz band differs by +0.7133 dB: raw whole-capture RMS is
−49.6980 dBFS reference / −48.9847 dBFS native. This is not a ratio confined to
the very quiet upper-band residual. Its independently running chorus histories
also show larger short-window envelope deviations. These do not establish an
implementation fault or estimate reference variability; do not compensate them
with EQ or phase alignment before a repeatable audible mismatch is established.

All 32 exponential-decay fits remain inapplicable for Inverse Room and the three
effects, not accepted zero errors. Response/envelope data and absolute band levels
are retained. Music onset/energy-percentile measurements are diagnostics, not
individual echo identities or resonant-note tuning tests. This block does not
establish all delay, pitch, routing or modulation-rate settings.

Final-second raw reference/native RMS is −95.940/−95.761 dBFS for Inverse Room,
−143.366/−135.013 for Chorus & Echo, −85.974/−85.971 for Resonant Chords and
−137.322/−121.631 for Multiband Delay. Quiet terminal ratios are kept separate
from active-response diagnostics. Capture/listening coverage does not imply that
every possible echo or resonance duration has been exhausted.

## Listening and next decision

Four verified 16.5-second clips are under ignored
`build/validation/xl-effects-musical-20261006/listening/`. Each contains eight
seconds of reference, 500 ms silence, then eight seconds of current native: four
seconds of riff and four seconds after input stops in each segment.

One scalar per full variant matches input-active RMS to −24 dBFS, reduced if
shared peak headroom requires it. Saved segments are exactly raw prefixes times
their recorded gains. No EQ, time alignment, denoising or tail fade is applied.
Reference/native gains are +2.3093/+2.3094 dB for Inverse Room,
+8.9086/+9.2624 for Chorus & Echo, −2.6108/−2.6103 for Resonant Chords and
+12.7261/+12.7275 for Multiband Delay. Peak checks and scalar RMS matching pass.

The user reviewed all four presented pairs and reports that every example is
satisfactory and sounds identical to their ear. Provisionally accept these four
saved musical examples at their exact recorded settings. Stop numerical fitting
for these cases, including the Chorus & Echo Mod-on envelope deviations; no
audible mismatch was reported. Do not turn this listening outcome into a claim
of sample identity, all-program/settings coverage or physical-unit validation.
Advance remaining representative coverage and final plugin integration checks.

## Provenance and cleanup

Private root: `build/validation/xl-effects-musical-20261006/`. Exact argv/exits,
controls, raw recordings, metrics, pair bounds/gains/hashes, input-sharing map and
checkpoint are retained. HEAD remains `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`,
clean Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`; unchanged v5 bank SHA is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The renderer and five production runtime source hashes match the accepted enabled
checkpoint; frozen CPU sources are preserved. No plugin is installed, no DSP
correction is introduced, and CPU is not repeated for this offline-only block.
Cleanup review retains all unique raw recordings and accepted listening fixtures; repeated
input storage is shared. No accepted earlier fixture is rerendered or deleted.
