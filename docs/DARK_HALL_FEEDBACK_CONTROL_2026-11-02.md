# Dark Hall feedback cause and continuous-control feasibility — November 2, 2026

Subsequent user decision: the experimental 0–120% range below was replaced by
a bounded **Stable → Original** variation, now integrated into the main plugin
as page 5, fader 3 of Dark Hall. See the
[implementation and new validation](CLEAN_ALL_ALGORITHMS_2026-11-02.md).
The investigation and measurements below remain historical evidence.

## Outcome and scope

The observed v8.21 runaway is a genuine small-signal instability of its
recursive delay/diffusion network at the tested long-decay settings. It is not
dependent on quantization noise, converter noise or overload. Noise or an input
burst excites an unstable mode; saturation eventually bounds its growth.

A continuous, common gain on the existing LF/Mid feedback contributions in both
channels can move that mode from decay through near-hold to growth. A private
fixed-point Clean prototype demonstrates this without changing the graph,
delay addresses, original filter coefficients or modulation. This investigation
does not add a GUI control or change the installed A/B plugin or production DSP.

This extends the [hardware/firmware investigation](DARK_HALL_RUNAWAY_2026-11-02.md).
The previous hardware testimony, version distinctions and finite test limits
still apply. The user specifically requested comparison of 8.21 and the earlier
8.1 family. Original v8.1 X, v8.1A XL and v8.21 XL snapshots were taken; the two
XL snapshots provide the controlled Mod/Opt-off mathematical comparison.

## Evidence for the cause

The reference snapshots boot the original, hash-validated ROMs in pinned
Reflexion, select Dark Hall V5 and set LF 7.5 s, Mid 5.7 s and Treble 9 kHz.
XL Dynamic Decay, Mod Enhancement and Decay Optimization are off. The original
test logs, actual page displays, controls and complete firmware identities are
retained in the previous firmware-comparison evidence. New snapshots retain
private WCS listings and control records. Firmware images are neither copied
nor edited.

Three independent descriptions agree on the unstable mode:

1. The prior full-ROM reference output has a dominant component near 298 Hz.
   Its spectral amplitude rises by approximately 2.9 dB/s before overload.
2. A diagnostic linear ARU recurrence, using the actual WCS graph and delays
   but excluding quantization, clipping, converters and controller modulation,
   also grows. Between seconds 10 and 30 the fitted rise is 2.86 dB/s. The
   equivalent earlier v8.1A graph decays instead. This is an analysis model,
   not a proposed floating-point replacement for the audio engine.
3. An independently assembled frequency-domain model follows each memory read
   to its most recent visible write, propagates the ARU's pipelined register/
   accumulator equations, and cuts the two cross-coupled feedback returns.
   Solving the resulting characteristic equation gives a pole at **298.213 Hz**
   with real growth exponent **+0.33113/s**, or **+2.876 dB/s**. The per-internal-
   sample pole radius is approximately 1.00001038, outside the unit circle.

At this frequency the two-return operator has modal gain about **1.113** for
v8.21 and **0.757** for v8.1A. These are eigenvalue magnitudes of the coupled
return operator, not a single physical multiplier or output gain. The v8.21
right-going branch supplies the excess gain; its effect interacts with the
left-going return and the LF/Mid feedback filtering. The earlier graph retains
substantial loss at the same physical frequency.

The versions differ in graph structure, memory layout and time scaling, not
just factory tone values. Earlier-tone matching in the preceding full-ROM
tests did not remove the v8.21 off/off runaway. Diagnostic section ablations
localize strong sensitivity to the first right feedback diffuser: removing it
changes the linear fitted slope from +2.86 to -3.04 dB/s; removing its left
counterpart alone still gives +2.53 dB/s. Such ablations are causal probes,
not accepted sonic corrections or proof that one particular ROM instruction
is erroneous. A single mistaken instruction or precise historical source-code
change has not been identified. The established cause is the unstable mode
of the revised coupled network, activated when decay feedback is high enough.

## Independent full-ROM intervention

`intervene.cpp` performs an explicitly labelled diagnostic intervention in the
running v8.21 **WCS**, after normal physical LARC setup. It reduces both Mid
feedback multipliers from 22/32 to 19/32, retaining both LF contributions,
all filters, graph/delays and input/output gains. This is not a normal LARC
operation or a modified firmware image. The harness checks each second that
the firmware has not overwritten the diagnostic multipliers.

With the same 0.2-second stereo noise burst (seed17, peak0.03), one-second
silent warmup, 48 kHz, analog I/O enabled and A/C outputs, the modified full
reference falls to -94.11 dBFS AC RMS at 10–11 s and approximately -103.04 dBFS
at 44–45 s, instead of growing into overload. No gate or output attenuation is
used. This directly confirms that reducing the internal return gain suppresses
the observed instability in the original running DSP implementation.

## Continuous fixed-point prototype

An isolated build-only export of the existing Clean engine adds a Q24 feedback
gain to the **four LF/Mid feedback products**, applying the same multiplier
to both channels. Original six-bit coefficients stay unchanged. The additional
gain multiplies the loaded product operand with a signed 64-bit intermediate;
it does not rescale external input, dry signal, preecho levels or final output.
It is not a second reverb engine or a crossfade between two algorithm versions.

The proposed control is **Feedback, 0–120%, default 100%**:

- 100% bypasses the new multiplication exactly and preserves the prior Clean
  sample stream, including its original instability where applicable.
- Lower settings reduce internal recirculation and shorten/stabilize tails.
- Higher settings increase sustain/growth until the existing arithmetic's
  nonlinear/overload behavior dominates.

The gain moves with a bounded slew of 20 gain units per second: a change of
1.0 takes approximately 50 ms. This provides fine continuous control rather
than quantizing the original six-bit feedback coefficient to slider steps.
Setters only update the target; they retain delay memory and controller state.

The actual 28-bit fixed-point prototype ran seven gain values (80/85/90/95/
100/105/120%) with Mod/Opt both off and both on: 14 cases, 60 seconds each,
the same finite problem settings and stopped-input fixture. All outputs remain
finite. At 100%, the maximum sample difference from the unmodified Clean engine
is **exactly zero** throughout both 60-second cases.

For Mod/Opt off, fits over seconds 10–30 illustrate the usable transition:

| Feedback | Measured envelope slope | Behavior in this fixture |
| --- | ---: | --- |
| 80% | -3.15 dB/s | Decays |
| 85% | -1.79 dB/s | Decays |
| 90% | -0.13 dB/s | Near the stability boundary |
| 95% | +1.39 dB/s | Slowly grows |
| 100% | +2.81 dB/s | Original growth |

The ideal static model's 298 Hz mode crosses its stability boundary near 89.81%.
That number is **not** a universal infinite-decay setting: quantization,
modulation, controls and other modes change the practical boundary. Fits after
overload are not growth-rate estimates. The seven-point test does not establish
monotonic sonic behavior or stability for every setting of the algorithm.

An additional moving-control check runs input followed by silence while
changing gain every 250 ms. The setter preserves delay memory exactly;
outputs remain finite; maximum Q24 change per host frame is 10,519, consistent
with the bounded slew. C++ allocation and release counts during processing and
parameter changes are both zero. Listening acceptance, host UI/automation,
session migration and realtime CPU measurements for a shipped control have
not been performed.

## Sonic decision

A single feedback slider is feasible and directly controls the cause of this
runaway. It controls the rate of decay/growth, not a separately regulated final
oscillation loudness. It necessarily changes tail length and can change the
balance of long-lived modes. The existing filters and modulation need not be
replaced to provide it. A fixed, precisely limited self-oscillation output
level would require an additional nonlinear regulation design and is outside
this experiment.

Keep 100% as the identity point if this control is added to the A/B interface.
It can be offered as a deliberate Dark Hall modification in either precision
path; the tested continuous prototype is the Clean path. Do not present the
proposed slider as an automatically stable mode at arbitrary LF/Mid/Treble/Size
settings. The production fidelity path, plugin parameter IDs and default sound
remain unchanged in this investigation.

## Private evidence and reproduction

Evidence root: `build/validation/dark-hall-cause-20261102/`.

- `snapshot.cpp`, per-version control/WCS/listing snapshots and logs.
- `linear.cpp`, `loop.py`, `loop-analysis.json`, baseline, gain and ablation CSVs.
- `intervene.cpp`, full-reference intervention CSV/log.
- `generated/sustain/`, fixed-point gain experiment; original export hash manifest.
- `feedback.cpp/.csv`, `feedback_edges.cpp`, edge-check and build logs.
- `provenance.json`, source/dependency/firmware/bank identities and final metrics.

The comparison and all intervention captures are private/ignored. No new WAV
campaign was generated; earlier active reference WAVs are reused for spectrum
analysis. Small ablation metrics and unique snapshots are retained as causal
evidence. No user recording, ROM, bank or dependency resource was removed.

```sh
# Reference snapshot: set 6 = XL v8.1A, 7 = XL v8.21.
build/validation/dark-hall-cause-20261102/snapshot \
  '/private/path/to/224XL v8_21' 7 OUTPUT 1 0 XL_BANK
build/validation/dark-hall-cause-20261102/linear \
  OUTPUT/wcs.csv LINEAR_CSV 30
python3 build/validation/dark-hall-cause-20261102/loop.py

# Diagnostic running-WCS intervention; never labels itself a physical capture.
build/validation/dark-hall-cause-20261102/intervene \
  '/private/path/to/224XL v8_21' INTERVENTION_CSV
build/validation/dark-hall-cause-20261102/feedback XL_BANK FEEDBACK_CSV
build/validation/dark-hall-cause-20261102/feedback_edges XL_BANK
```
