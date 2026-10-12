# Float Hall brightness correction — 2026-10-12

The first 48 kHz float Hall sounded too dark. The delay-unit conversion was
correct, but the interpolation and internal-filter conversion changed its
frequency-dependent decay substantially. This correction keeps the original
engine and A path unchanged. Listening decides acceptance; exact sonic
identity is not the objective of this prototype.

## Cause and correction

All bank coefficients were initially decoded as `integer / 32`, and delay
lengths were scaled by `48000 / 20480 = 75 / 32`. For example, a native
1140-sample delay remains 55.6640625 ms, or 2671.875 host frames. A native
one-sample delay is 48.828125 microseconds, or 2.34375 host frames.
There was no blanket error in the long-delay time conversion.

Two conversions nevertheless altered the sound:

- Linear fractional reads attenuated high frequencies at every traversal.
  At fraction 0.34375, one read loses 1.11027 dB at 8 kHz. Repeated reads
  inside feedback shortened and darkened the upper-frequency tail.
- Exponential pole remapping preserved the internal filter's pole time and
  DC gain, but did not preserve its full frequency response. That was an
  inadequate transfer of the original short recursive filter topology.

The corrected core uses six-tap, fifth-order Lagrange fractional reads,
with original bank coefficients and physically scaled short recursive delays.
The current-sample contribution in a 2.34375-frame recursive read is solved
analytically, avoiding unwritten delay storage. Both captured inputs are
published before short cross-channel input reads. Native integer-delay graph
results remain consistent with the independent real-valued ARU oracle.
The worst single-read loss through 8 kHz across all 32 fractional phases is
0.04724 dB. These reads approximate magnitude and phase; they are not ideal
fractional delays.

Two intermediate candidates were rejected. First-order allpass reads reduced
brightness loss, but literal rounded network gains produced late growth in a
maximum-decay, high-diffusion test. Recomputing every allpass complement as
`1 - g*g` removed that behavior but shortened some maximum tails dramatically.
The selected FIR candidate keeps all original coefficient values, including
rounded complements. It introduces no input/output filters or compensating EQ.

## Narrow before/after evidence

These measurements use shared **default controls**, A/C wet routing, and both
modes off. They are not a reconstruction of the user's unspecified parameter
values. The input is seeded independent stereo noise for 0.5 s, followed by
11.5 s of exact silence, after 1 s of warmup. A is the existing desktop engine,
including converters and circuit filters. Band comparisons use 0.1 s Hann
windows and a scalar match on active 500–2000 Hz energy.

| Diagnostic | First float B | Corrected float B |
| --- | ---: | ---: |
| Active 6–8 kHz energy relative to A | -5.36 dB | +0.05 dB |
| Tail 6–8 kHz energy, 0.7–1.5 s, relative to A | -16.40 dB | -0.21 dB |
| Active 4–6 kHz energy relative to A | -2.76 dB | -0.10 dB |
| Tail 4–6 kHz energy, 0.7–1.5 s, relative to A | -9.56 dB | -0.30 dB |

Decay diagnostics with accepted fit R² ≥ 0.95 give A/corrected B T60 values of
3.04/3.07 s at 500–1000 Hz, 2.69/2.70 s at 2–4 kHz,
2.27/2.26 s at 4–6 kHz, and 2.08/1.97 s at 6–8 kHz.
Short-window differences are diagnostics, not a requirement for exact matching.
The same noise fixture also covers Mode Enhancement alone and both modes on.

Maximum-decay checks retain 90 s of input-silent tail after a one-second drive.
With A/C routing and minimum diffusion, late/early energy is 0.13632 for A
and 0.142923 for the selected candidate. The old `< 0.1` test requirement
would reject the original itself, so the production check now requires bounded
finite decay instead of an artificially short tail. At maximum diffusion,
A has a higher quiet residual floor than B; this remains a listening question.
A separate 270 s candidate sweep covered four extreme configurations.

## Validation and listening

On macOS arm64, the corrected core, fractional-read check, plugin routing/state
check and ASan/UBSan check passed. Audio processing allocated no memory.
The plugin check verifies bit-identical A behavior, B routing and its 70-frame
pad, tail-preserving switching, dry/mono paths and host-rate bridging.
The corrected AU, VST3 and Standalone were built; the separate experimental
AU/VST3 were installed with previous copies retained, and AU validation passed. Windows compilation has not
been validated in this correction block.

Listening material uses an existing private/local musical fixture: 4 s of
input plus 8 s of silence. Each output has one constant gain matching the
active four seconds to a common RMS target. Sequential pairs contain 12 s of
the first version, a 0.5 s pause, then 12 s of corrected B. There is no EQ,
output filtering, output fade or fitted time alignment. No listening acceptance
has been claimed.

Private evidence and reproduction sources remain under
`build/validation/float-darkness-20261012/`: frozen `baseline.hpp`,
`baseline-source.json`, `probe.cpp`, `analyze.py`, per-mode `numbers.csv`,
`metrics.json` and `summary.txt`, extreme-tail probes, final check logs,
`music-probe.cpp`, `prepare-listening.py`, and `listening/manifest.json`.
The imported bank SHA-256 is
`15d14dcebbbafad35269e8159ddfedfb24ca9fdd203531b3d85cf094ce6e72ed`.
ROMs, banks and audio are excluded from Git.

The float path intentionally has wider bandwidth, different arithmetic and
quiet-tail behavior. Above the original passband it can sound brighter because
A still includes its circuit filters and converters. Control scanning and
modulation phases also remain approximate. Further adjustment should follow
listening evidence, rather than chasing 100% matching.

For background, Julius O. Smith's
[delay interpolation discussion](https://www.dsprelated.com/freebooks/pasp/Delay_Line_Interpolation.html)
explains why linear reads introduce frequency-dependent loss in feedback.
