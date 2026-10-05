# Native 224 sound comparison with Reflexion

Discussion: October 4–5, 2026. Measurements: October 4, 2026.

## User feedback

A listener reported that Cineol loaded successfully and worked, but sounded
slightly more boomy and less transparent than the original Reflexion build.
They heard the difference across the presets they tried, particularly in the
low and low-mid range. They considered the CPU usage of a Reflexion224 DEV
build acceptable and asked whether the original sound could be retained while
keeping Cineol's additional functionality.

The investigation was explicitly read-only: no DSP or plugin source changes
were requested or made during the discussion.

## Product objective

The plugin's purpose is to avoid full emulation and reproduce the Lexicon 224
algorithms as closely as possible with a native engine.

An initial suggestion that restoring the sound required the original emulator
was too categorical and was corrected during the discussion. Full emulation
is not a prerequisite for improving sonic similarity. The intended direction
is to retain native DSP and improve the audio boundary and control behavior.

## Investigation scope

- Cineol source at the time of the investigation: `023b80c`.
- Pinned upstream Reflexion reference: `f68ea1d`.
- Original Model 224, firmware v4.4; locally supplied ROMs and prepared bank.
- Full audio comparison: Large Concert Hall B at 48 kHz, 100% wet, outputs A/C,
  identical stereo input and no gain normalization.
- Factory controls: Bass 3.4 s, Mid 2.6 s, Crossover 540 Hz, Treble 4000 Hz,
  Depth 21, Predelay 24 ms and Diffusion 1.
- Cases covered Mod Enhancement and Decay Optimization on/off, plus analog
  bypass. The current native engine used the six-program bank.

The comparison used upstream source, not the listener's unidentified DEV
binary. Its results do not establish the behavior of every preset or the XL
engine. The listener's exact versions, settings and DAW routing were not
available during the discussion.

## Confirmed differences

### Audio boundary and upper-frequency response

Reflexion feeds interpolated input at 192 kHz into continuous-time circuit
states sampled at converter events. Its output follows event-timed DAC holds
through a continuous-time output filter.

Cineol uses the inherited circuit pole/residue values in a 48 kHz filter model,
with a 64-tap FIR downsampler to 20,480 Hz and a 32-tap FIR upsampler back to
48 kHz. The resampler cutoff factor is 0.94, giving a nominal transition center
of 9,625.6 Hz. The surrounding sampling and reconstruction therefore differ
even though the circuit parameters originate from Reflexion.

Relevant source:

- [Native resampler](../native-hall/core/rate48.hpp)
- [Native analog filters](../native-hall/core/analog48.hpp)
- [Native 48 kHz audio engine](../native-hall/core/engine48.hpp)
- [Reflexion analog I/O](../deps/reflexion/analog/analog_io.hpp)
- [Reflexion continuous-time filter model](../deps/reflexion/analog/analog.hpp)

A ROM-free sinusoidal measurement isolated the linear input and output
boundaries. The following values sum their native/reference fundamental
amplitude differences; they exclude the reverb network, quantization,
nonlinear gain ranging and controllers.

| Frequency | Native minus Reflexion |
|---:|---:|
| 100 Hz | +0.00013 dB |
| 250 Hz | +0.00048 dB |
| 500 Hz | +0.00201 dB |
| 1 kHz | +0.00982 dB |
| 4 kHz | +0.16650 dB |
| 6 kHz | +0.39305 dB |
| 8 kHz | +0.71764 dB |
| 9 kHz | −2.00539 dB |
| 9.6 kHz | −10.33584 dB |
| 10 kHz | −21.11868 dB |

The result shows reduced bandwidth at the very top, rather than a broad
treble reduction. Native is slightly stronger in the 4–8 kHz region. Reduced
upper bandwidth could contribute to the reported lack of transparency, but
that perceptual explanation has not been established by a listening test.

The full wet-only render retained the upper-band difference with both
controllers disabled: integrated spectral energy differed by −2.5191 dB at
9–10.24 kHz and +0.5503 dB at 6–8 kHz.

### Tail controllers and initial state

The native engine uses measured nominal controller frequencies and captured
initial state. Reflexion executes the firmware CPU, including its timing.
Signal-dependent scheduling and modulation phase are not fully reproduced.

Relevant source: [native modulation and decay control](../native-hall/core/hall.cpp)
and [decay controller](../native-hall/core/decay.hpp).

With controllers enabled, the comparison showed differences in tail envelope
and spectrum. In the decay-only case:

- Integrated 200–500 Hz energy was +0.2010 dB relative to Reflexion.
- Native tail RMS over 3–10 s was `2.32471e-5`, versus `3.4728e-5` for Reflexion.
- Diagnostic alignment of the native's 17-byte initial decay state changed its
  tail RMS to `3.48418e-5`.

This demonstrates a material effect from controller startup state. It does
not establish that native tails are consistently longer, louder or bassier.

### Dry/wet timing and gain

Below 100% wet, another difference becomes relevant. The original plugin mixes
the input-gained dry signal directly with wet at its 48 kHz processing rate.
Cineol's normal mode delays dry by 70 samples and applies Input Gain to the
reverb input only. Low latency uses direct dry instead.

These differences can alter dry/wet interference and balance. They do not
explain the upper-band difference measured at 100% wet. Analog/Dirt settings,
output routing and input levels must also match for a useful comparison.

## What was not established

A persistent low-mid boost or resonance was not found in the tested static
case. With both controllers disabled, integrated energy differed by only
−0.0257 dB at 200–500 Hz and +0.0041 dB at 500–1000 Hz.

The reported boominess could involve upper-band balance, controller-dependent
tail structure or dry/wet interaction. Its exact cause in the listener's
session remains unresolved. The report of a sonic difference is credible,
but the measurements do not justify calling it a proven bass resonance.

## Optimization and arithmetic checks

The current processing checks passed:

- Both SRC directions matched the frozen implementation from before the
  phase-counter optimization bit for bit over 1,000,000 input samples each.
- Cached control updates matched unconditional updates across all six programs,
  including tails, switches and repeated preparation.
- All six integer networks matched the independent row machine over 12,000
  full-scale stereo frames and 47 control settings per network, including
  arithmetic state, delay memory and saturation counts.

Reverting the tested phase-counter and control-cache optimizations would not
restore the original Reflexion audio path. These checks establish specific
equivalences, not complete sonic identity with Reflexion.

The existing Analog48 test compares float and double calculations under the
same 48 kHz first-order hold. It does not compare the complete event-timed
analog boundary. Passing that test never established a full audio null.

## Agreed direction for future work

Preserve the native engine and address measured differences:

1. **Audio boundary first.** Bring the combined frequency and phase response
   closer to Reflexion, including SRC, converter sampling and DAC
   reconstruction. Simply opening the FIR passband is insufficient: spectral
   images, aliasing and phase must also be evaluated.
2. **Tail controls.** Improve controller frequencies, initial state and response
   to signal level; evaluate modulation and decay behavior separately.
3. **Dry/wet behavior.** Compare relative timing and levels at matched settings.

Measure the CPU cost of greater accuracy separately. Keep processing bounded
and allocation-free, and explain any resulting sonic changes. This discussion
did not authorize or implement DSP changes.

## Subsequent desktop implementations

The following section records work after the initial investigation; the
measurements above describe the earlier audio path. On October 5, 2026,
the original-224 desktop output FIR/48 kHz hold approximation was replaced
with independently timed DAC captures and continuous-time reconstruction.
The [DAC validation report](EVENT_DAC_VALIDATION.md) records isolated output
accuracy, full-path limitations, decay estimates and desktop CPU cost.
The subsequent desktop input step replaced the 48 kHz AIN/downsampling
approximation with the reference interpolator/circuit response at independent
ADC hold times. See [input/ADC validation](INPUT_ADC_VALIDATION.md) for
converter-word agreement, six-program tails, full-firmware spectral results
and final CPU cost. The next desktop step corrects the decay startup period,
transfer-peak sampling and panel-scan cadence; see
[controller validation](CONTROLLER_VALIDATION.md). Modulation phase and precise
signal-dependent controller timing remain open. The next checkpoint validates
the complete modulation step law and retains its global counters on direct
desktop switches; see [modulation validation](MODULATION_VALIDATION.md).
[Residual sound measurements](RESIDUAL_SOUND_VALIDATION.md) separate frozen
phase/quiet-floor sensitivity from boundary response; [gain/timing](GAIN_TIMING_VALIDATION.md)
and [regression](FINAL_SOUND_VALIDATION.md) record Dry/Wet, Input Gain and
original-only Spillover results. Overall sonic acceptance remains open.
Daisy is deferred.

## Initial local diagnostic artifacts

The detailed investigation and private listening renders remain in the ignored
directory `build/validation/sound-review-20261004/`:

- `analysis.md`: detailed investigation report.
- `analog-src-tone-response.csv`: isolated linear-boundary measurements.
- `spectral-bands.txt`: integrated spectral comparisons.
- `bank-check.txt`: independent network-check results.
- `current/comparison.txt`: current program-bank render statistics.
- `current/reference-*.wav` and `current/native-*.wav`: matched audio renders.

Top-level WAVs in that directory use the legacy single-Hall profile; the
`current/` subdirectory contains the current program-bank comparison. ROMs,
banks and captures are private local diagnostics and must not be committed.
