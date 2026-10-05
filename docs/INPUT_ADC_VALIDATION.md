# Original-224 desktop input and ADC validation

Measured on October 5, 2026, against pinned Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, original Lexicon 224 v4.4.
This follows the [desktop DAC reconstruction](EVENT_DAC_VALIDATION.md).

## Result and sound change

The desktop original-224 input now reproduces the reference's 4x windowed-sinc
interpolation, 192 kHz first-order hold, continuous-time AIN circuit and
separate left/right ADC hold times. It replaces the 48 kHz input circuit
approximation followed by the 64-tap downsampling FIR. No equalization or
controller-rate tuning was applied.

The old input systematically lost upper-band energy. The new input follows
the reference response through the native Nyquist region and reproduces its
folded response above Nyquist. The reference's analog anti-aliasing remains;
the additional native digital low-pass is removed. This can increase upper
frequency content, converter clipping and alias content at the same source
level, as expected from the reference boundary. ADC gain ranging, rounding
and detector comparisons now keep double precision until quantization.

The [implementation](../native-hall/desktop/adc22448.hpp) is selected only by
`DesktopEngine48`. The default engine keeps the old input/output boundaries
and matched the frozen pre-DAC implementation bit for bit over 1,152,000
stereo frames with rapid program/control changes. No Daisy source or build
was changed or run. No hardware CPU-margin claim is made. XL processing,
plugin identifiers, parameter IDs, session formats and bank formats retain
their existing behavior.

Normal reported latency and dry/bypass delay remain 70 internal 48 kHz
samples. Low-latency mode still reports zero and uses direct host-rate dry.
The corrected wet input timing and channel separation can change stereo
phase and dry/wet interference. The reference's firmware boot phase and
physical sample-and-hold hardware beyond its current model are not emulated.

## Cause isolation and timing

The prior investigation separated two causes: first-order interpolation on
the 48 kHz grid, and the native downsampling FIR's transition centered near
9.63 kHz. At 8 kHz the input error was mainly the interpolation boundary;
at 9.6 and 10 kHz the FIR added substantial attenuation.

The FPC completes the left conversion before RD_AD at row 0 and the right
before row 50. Its final loads are at rows 90 and 40. The reference samples
36 rows before each load, making the relevant hold phases −46 and +4 rows
relative to a native pass. These phases, channel order and completed words
were checked against all six independent WCS images after the cold scan
settled. The first audio boundary starts with the reference input's zero
circuit state; the FPC's power-up transient is not reproduced.

The 100-row pass takes 28,125,000 ticks; a host frame takes 12,000,000 ticks.
The hold origin is `timing_224.first_marker + converter_offset = 585805`.
The reference also subtracts the interpolator's nominal 16-frame latency
from the requested input time. Together with the interpolator's own delay,
this remains approximately 32 host frames of input transport.

First-order interpolation uses the next 192 kHz point in its last quarter
interval. Omitting this small precursor produces a measurable input error.
Preparation includes the precursor and the special first-sample condition.
All queried source times are sufficiently behind the host to use already
available samples.

## Bounded realtime implementation

The 192 kHz interpolator and circuit are combined analytically at preparation.
The finite part becomes 64 phase tables of 33 coefficients; the remaining
impulse tail becomes six modal recurrences at the 48 kHz host rate. A fixed
raw-sample history and fixed circuit-state history allow sampling at the ADC
hold times. Converter timing uses bounded relative integer counters.

Processing performs no allocations, I/O, locks, waits, exponentials or
coefficient preparation. Double precision removes numerical error before
the integer ADC boundary; the existing integer networks are unchanged.
Circuit coefficients are prepared outside audio, and firmware activation
resets already-prepared fixed storage. Input filtering continues through a
normal hard program switch, preserving the continuous input circuit.

## Measurements

ROM-free checks compare the actual input stage with independent Reflexion
`Input`, `convert`, `level_detectors` and timing code, using identical source
samples and hold instants. Tone fits use two-second renders, fitting the last
second without gain normalization.

| Frequency | Previous input gain error | New input gain error |
| --- | ---: | ---: |
| 1 kHz | −0.01172 dB | < 0.000001 dB |
| 4 kHz | −0.18640 dB | < 0.000001 dB |
| 8 kHz | −0.78151 dB | < 0.000001 dB |
| 9 kHz | −2.65353 dB | < 0.000001 dB |
| 9.6 kHz | −6.84821 dB | < 0.000001 dB |
| 10 kHz | −11.85334 dB | < 0.000001 dB |
| 12 kHz, folded | −84.60790 dB | < 0.000001 dB |

Across tones up to 20 kHz, measured gain and phase discrepancies were at
double-precision numerical scale (`1.28e-11 dB`, `8.85e-11 degrees`). This is
agreement with an offline circuit model, not physical-device accuracy.
Maximum waveform error over full-scale seeded noise was `7.75e-15`.

The permanent check covers clipping/level steps, shifted impulses (including
the precursor), noise/silence and eleven tones. All 1,146,880 reconstructed
ADC words matched the independent continuous-time reference exactly. Another
250,000 seeded converter values and 49,152 half-code/adjacent values checked
gain ranging and rounding. Detector masks matched as well. One million
frames with resets passed clock/history bounds with zero allocations.

All six programs were also rendered for 12 seconds with 0.2 seconds of
stereo noise, both with controllers disabled and with both enabled. The
desktop output matched an independent continuous-time-input pipeline bit
for bit over 6,912,000 stereo frames. That pipeline uses the same native
integer networks, controller initial state and clocks, and desktop output
reconstruction. Bandwise T20-based decay estimates therefore matched exactly.
This isolates input accuracy; it does not prove complete firmware-controller
equivalence or identical tails on physical hardware.

The full-firmware Hall B comparison uses left/right strikes and a chord,
100% wet A/C routing and separate controller modes. With both controllers
disabled, integrated spectral differences now are:

| Band | After the DAC step | After the input step |
| --- | ---: | ---: |
| 250–500 Hz | −0.04701 dB | −0.00066 dB |
| 4–8 kHz | −0.37859 dB | −0.00136 dB |
| 9–10.24 kHz | −2.76402 dB | +0.05904 dB |

Remaining full-firmware differences include controller initial state,
modulation phase, firmware scheduling and converter/host clock origin. In
the decay-only strike/chord render, late-tail RMS is `2.34502e-5` versus
`3.4728e-5` for Reflexion. Offline alignment of the 17-byte initial decay
state gives `3.46712e-5`. That alignment remains diagnostic and is not applied
to the plugin. High-band decay estimates from the quiet part of the short
musical render are sensitive to the integer tail floor and truncation;
they must not be interpreted as validated RT60 measurements.

A separate 12-second full-firmware Hall B render uses a 0.2-second stereo
noise burst followed by silence. With controllers disabled, accepted
T20-based decay estimates differ by at most **0.62%** across seven bands
from 100 Hz to 10.24 kHz. For example, the 4–8 kHz estimates are 2.176229 s
native and 2.176224 s reference. With both controllers enabled, the largest
accepted band difference is **11.21%** (250–500 Hz), confirming that this
stage does not resolve controller-dependent tail length.

These estimates reverse-integrate band energy after 0.3 seconds, fit −5 to
−25 dB and extrapolate to 60 dB. Acceptance requires both fits to have
`R² >= 0.95` and to finish before 10.5 seconds of the post-excitation window.
The separate 9–10.24 kHz sub-band fails the fit-quality check and is excluded
from those maxima. Its quiet-tail floor must not be treated as a long reverb
decay. `noise-tail-bands.csv` preserves accepted and rejected fits.

## Desktop CPU and memory

The baseline is the completed desktop DAC step before input changes. Its
frozen engine header has SHA-256
`13049fad3dc0883226ddcda90131bfa41b822696ab27a7894a779f78a8c78c8e`.
Release x64 MSVC 19.44 builds use strict floating-point evaluation. The final
benchmark enables FTZ/DAZ, matching `ScopedNoDenormals` in the audio callback.

Six programs × three modes (controllers off, both on, analog bypass) were
tested with five paired repeats, alternating implementation order. Each
timed run processes two seconds of 48 kHz stereo excitation/tail after an
untimed warmup. No compilation or other validation job ran during timing.

The summed median processing-time ratio was `1.02307`: **+2.31%**. Individual
cases ranged from **+1.00% to +3.76%**. This is a small measured increase for
the final double-precision implementation, not the large reduction suggested
by an earlier prototype benchmark with a different denormal setting. These
are internal DSP wall-time comparisons, not whole-DAW/UI CPU or hardware
realtime margin. Median processing times were about 35–38 ms per two seconds
of audio on this desktop.

Original-224 desktop engine storage increases from 63,544 to 82,024 bytes
(+18,480 bytes per engine); the input stage occupies 27,696 bytes. The default
engine remains 57,040 bytes. Processing and rapid-switch checks recorded
zero allocations and releases.

## Tests and reproduction

Windows x64 VST3 and Standalone builds passed. Processor checks passed for
all six programs at 44.1, 48 and 96 kHz, mono/stereo, 128/511/20,000-frame
blocks, state/automation, directed program switches and low-latency dry/wet
behavior. Cached-bank restart, existing core/network/control checks and both
ADC/DAC boundary tests passed. The mixed 224/XL quick-preset fixture remained
outside the original-224-only run.

```sh
cmake --build build/plugin --config Release --target native_224_adc_check
ctest --test-dir build/plugin/native-hall -C Release -R native_224_adc_boundary --output-on-failure
```

To check all FPC hold phases, configure `NATIVE_HALL_ROM_DIR` with original
224 v4.4 ROMs and run `native_224_adc_check` with the generated bank path;
its six `.wcs` fixtures must accompany it. `build_plugin.sh --check` now
includes this check. No ROM data is needed for the linear/converter check.

Private evidence stays ignored under `build/validation/event-adc-20261005/`:
`adc-check.log`, `input-tones.csv`, paired raw tails, `tail-bands.csv`,
`full-bank/`, `full-comparison-bands.csv`, `cpu.csv`, `measurements.json`,
`noise-bank/`, `noise-tail-bands.csv`,
frozen baseline headers, diagnostic sources and desktop test/build logs.
ROMs, banks, WCS images and audio captures must not be committed.

## Subsequent controller work

The controller state and measurements above describe the completed ADC step.
The subsequent [desktop controller validation](CONTROLLER_VALIDATION.md)
records startup-period and peak-sampling corrections, updated full-firmware
tail comparisons, remaining modulation-phase differences and incremental CPU
cost. The isolated ADC/linear-boundary results remain applicable.
