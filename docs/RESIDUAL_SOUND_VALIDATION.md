# Original-224 residual frequency and tail measurements

October 5, 2026. Baseline/source: `0ebb668` plus the checked-in diagnostic
tools from [modulation validation](MODULATION_VALIDATION.md), implementation
checkpoint `ce45a03`. Reference: Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, original v4.4.
Status: cause-isolation checkpoint; full sonic acceptance remains open.

The later [native scan report](NATIVE_CONTROL_SCAN_VALIDATION.md) adds the
stable-control event clock and thirty paired normal-startup sound fixtures.
The phase/floor limitations identified here still apply.

## Method and result

`native_224_sound_compare` checks all five ROM hashes and renders a selected
program with explicit mode, seed, input level, warmup and controls. It writes
unscaled normal native, full-firmware reference and **diagnostic aligned**
WAVs, with fixture metadata. Alignment copies only the 17-byte decay state
and modulation state at input start, not network delay memory or converter
clock origin. No ROM, bank, WCS or WAV is checked in. No EQ, filter or tail
coefficient change was made in this step.

The main matrix uses 48 kHz, A/C wet-only output, six programs, modes 0/2/3
(both off / decay only / both on), seed 17, a 0.2-second stereo noise burst
at amplitude 0.08, 1-second rendered silent warmup, 12-second recording,
Bass/Mid 17/14, Crossover 5, Treble 23, Depth 21, Diffusion 1 and each
program's minimum predelay. Firmware boot/program preparation is explicit
in `reference224.hpp` and the tool; it differs from the earlier private fixture.

The NumPy-only analyzer uses a 4097-tap Hann-windowed sinc bandpass, reverse
integrated stereo energy after 0.3 seconds, and a -5..-25 dB fit extrapolated
to 60 dB. Fits require both R² >= 0.95, enough samples and completion at least
1.2 seconds before the render ends. Rejected fits remain in CSV. These are
T20-based estimates, not measured physical-hardware RT60 or listening scores.

## Initial phase is a material variable

Disabling modulation freezes the interpolation taps at the phase reached
during firmware boot/loading. Different valid frozen phases can change the
high-band decay strongly even though static logical controls match.

Maximum accepted broad-band T20 difference in this new preparation schedule:

| Program | Both off, normal | Both off, aligned diagnostic | Both on, normal | Both on, aligned diagnostic |
| --- | ---: | ---: | ---: | ---: |
| Small Hall B | 54.11% | 1.82% | 7.24% | 3.11% |
| Vocal Plate | 19.00% | 0.35% | 8.16% | 4.09% |
| Large Hall B | 56.35% | 0.99% | 14.39% | 4.26% |
| Acoustic Chamber | 9.93% | 10.10% | 11.36% | 3.20% |
| Percussion Plate A | 17.94% | 0.71% | 15.22% | 4.38% |
| Small Hall A | 43.72% | 2.44% | 19.78% | 6.51% |

These large unaligned static differences are **not a regression from the
counter correction**: these renders contain no program switches after native
preparation, and the corrected code is executed only at direct switches.
They expose the dependence on firmware preparation/frozen tap phase that a
single prior startup fixture did not cover. Aligned figures are not shipped
plugin improvements, and must never replace normal figures in acceptance.

With aligned static taps, maximum integrated broad-band energy error across
all six programs is below 0.006 dB. The remaining quiet-tail estimate can
still differ, especially Chamber. The independent ADC/DAC tests also passed
again: exact ADC words, input waveform peak error 7.75e-15, and DAC waveform
peak error 3.21e-7 across four networks/all channels. There is no established
new analog-bandwidth defect to justify a compensating filter change.

## Chamber level, seed and quiet-band sensitivity

Additional static Chamber runs keep the same controls and nominal phase
alignment while changing noise level or the seed/warmup:

| Amplitude / seed / warmup | Accepted broad bands | Normal max T20 error | Aligned max T20 error |
| --- | ---: | ---: | ---: |
| 0.02 / 17 / 1000 ms | 0 | Rejected | Rejected |
| 0.08 / 17 / 1000 ms | 7 | 9.93% | 10.10% |
| 0.32 / 17 / 1000 ms | 7 | 8.84% | 0.28% |
| 0.80 / 17 / 1000 ms | 7 | 7.88% | 0.86% |
| 0.08 / 73 / 1047 ms | 7 | 5.07% | 5.42% |

At 0.80, AIN pre-emphasis can clip; its aligned broad-band energy discrepancy
reaches 0.56 dB. Do not interpret that run as a validated linear response.
At 0.02 the long quiet floor fails the fit criteria rather than establishing
a very long tail. The stronger, non-overload 0.32 run greatly reduces the
Chamber residual. This supports quantization/excitation/converter-clock origin
as contributors; it does **not** prove that every Chamber residual is only
noise or that a timing defect is impossible.

The narrow 9–10.24 kHz band is analyzed separately and is excluded from the
seven-band maxima. Several static Hall/Chamber fits have R² around 0.70–0.84;
they fail acceptance even when integrated energy agrees. Keep rejected bands
visible. A slope from that tail floor is unsuitable for tuning the filters.

## Reproduction and next action

```sh
cmake --build build/sound-validation --config Release --target native_224_sound_compare
native_224_sound_compare "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/chamber 3 0 noise 17 .08 1000
python script/analyze_sound.py build/chamber
```

Modes are bit 0 = enhancement, bit 1 = decay optimization. The optional five
controls are `BASS MID TREBLE DEPTH PREDELAY_MS`; Crossover/Diffusion remain
5/1 in this tool. The `music` fixture provides left/right strikes and a chord
with the same seed/level controls. Use several seeds and initial phases before
claiming a phase-independent tail improvement. No listening judgement was
made from these numeric renders.

Next: improve the independently traced native control schedule; measure an
ensemble of phases/excitations with normal plugin startup. If Chamber retains
a repeatable bias above the quantization floor, capture actual reference ADC
words and converter clock origin to separate excitation from integer feedback.
Do not retune native coefficients before that isolation. Private data on this
computer is `build/validation/residual-20261005/`, including all 18 main runs,
four Chamber probes, metadata and accepted/rejected `bands.csv` files.
