# Original-224 desktop DAC reconstruction

This records the completed output step. The subsequent
[input/ADC validation](INPUT_ADC_VALIDATION.md) addresses the input limitations
reported here and provides current full-path measurements.

Measured on October 5, 2026, against Reflexion commit
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. The baseline desktop engine is
repository commit `7e39baffa967e09bfa52884e5e6bba0d99007190`.

## Scope and resulting sound

The original-224 desktop path now reconstructs each DAC channel from its
converter capture events through the original continuous-time AOUT circuit,
then samples that circuit at 48 kHz. This replaces the output interpolation
FIR and the 48 kHz first-order-hold approximation. No firmware CPU runs in
the audio callback. The input filter, input SRC, ADC conversion, integer
networks and tail controllers retain their existing behavior.

This removes the output boundary's artificial 4–8 kHz lift and its additional
roll-off above 9 kHz. The four converter phases now produce their reference
relative timing, so the wet frequency balance and stereo phase change.
The desktop uses a fixed 38-frame transport in place of the old output FIR's
nominal 37.5-frame delay. Reported latency and the dry/bypass delay stay at
70 samples at the internal 48 kHz rate. This does not assert identical wet
arrival times or dry/wet interference with the old version or Reflexion.

The implementation lives in [dac22448.hpp](../native-hall/desktop/dac22448.hpp)
and [engine22448.hpp](../native-hall/desktop/engine22448.hpp). An output policy
in the shared engine allows the desktop to select this circuit. The default
`Engine48` retains the previous output path and matched a frozen pre-change
engine bit for bit over 1,152,000 stereo frames, including rapid program and
control changes. No Daisy source, configuration, build or hardware work was
performed. XL audio behavior is outside this change. Plugin identifiers and
parameter IDs are unchanged.

## Timing and realtime bounds

The native pass rate is 20,480 Hz. All event times use the original-224 row
clock and converter propagation offset from the independent timing model.
The warmed FPC capture rows are:

| Native network | A | B | C | D |
| --- | ---: | ---: | ---: | ---: |
| Hall | 62 | 39 | 112 | 89 |
| Plate | 57 | 109 | 109 | 57 |
| Chamber | 78 | 32 | 109 | 55 |
| Room | 62 | 39 | 112 | 89 |

Rows are relative to each 100-row pass; captures can cross a pass boundary.
Closely spaced Hall and Chamber requests wait in the FPC latch. Simply adding
a constant delay to each WR_DA row gives incorrect capture phases. The test
checks these periodic phases against all six independently loaded WCS images.
The firmware's cold-start converter transient is not emulated.

Preparation computes all circuit exponentials, including the tiny fast pole,
for the 300 fractional clock phases. Processing uses seven modal sections,
two queued events per channel and bounded relative time counters. There are
no processing allocations, I/O, locks, waits or transcendental evaluations.
Program switches clear old circuit state and queued events while preserving
the free-running host/native clock.

## Output measurements

The isolated comparison feeds identical native DAC values into the new
reconstruction and Reflexion's double-precision continuous-time `Output`.
It excludes the input boundary and controller-state differences.

- Four networks, four channels, seeded noise followed by silence, and tones
  at 100 Hz, 1, 4, 6, 8, 9, 9.6 and 10 kHz were tested at 48 kHz.
- Maximum absolute waveform error was `3.21e-7`. Every case passed a relative
  RMS error limit of −80 dB and an absolute peak limit of `3e-5`.
- Maximum measured tone gain error was `0.000073 dB`; phase error was
  `0.000664 degrees`. The previous output-only gain errors were +0.353 dB
  at 4 kHz, +1.499 dB at 8 kHz and −9.265 dB at 10 kHz.
- Same-stream 12-second tails from all six native programs were reconstructed
  by both circuits. Across eight bands from 100 Hz to 10.24 kHz, maximum
  relative difference in the T20-based decay estimate was `0.000020%`.
  This establishes output reconstruction accuracy; it does not establish
  complete native-versus-firmware tail equivalence.
- One million frames of reset and all-phase topology switching passed with
  queue assertions enabled in Release and zero allocations.

Decay estimates use reverse-integrated band energy and a linear fit between
−5 and −25 dB, extrapolated to 60 dB. The 0.2-second excitation is excluded;
the fit starts at 0.3 seconds. These are offline decay estimates, not a
measurement of physical hardware or a perceptual listening result.

## Remaining full-path differences

The matched full-firmware Hall B renders use the current six-program bank,
100% wet A/C routing and separately enabled modulation/decay controllers.
The new DAC does not remove the input boundary's frequency loss: isolated
input error remains −0.186 dB at 4 kHz, −0.782 dB at 8 kHz, −6.848 dB at
9.6 kHz and −11.853 dB at 10 kHz.

With both controllers disabled, full-render integrated energy at 4–8 kHz
changes from +0.375 dB to −0.379 dB relative to Reflexion. At 9–10.24 kHz
it changes from −2.519 dB to −2.764 dB. Removing an inaccurate output lift
also removes its accidental compensation for input loss; this step does
not improve every full-path band. The input filter/SRC is the next boundary
to investigate. No compensating EQ was added.

Controller initial state remains a separate cause of tail differences. In
the decay-only render, late-tail RMS is `2.32177e-5` versus Reflexion's
`3.4728e-5`. Diagnostic alignment of the initial decay state gives
`3.47910e-5`; this alignment is an offline experiment and was not applied
to the plugin. Controller rates, initial state and level-dependent timing
still require dedicated work before claiming identical tail lengths.

## Desktop CPU and memory

Release x64 MSVC 19.44 builds use strict floating-point evaluation. Each
of six programs was tested with controllers off, both controllers on, and
analog bypass: 18 paired cases, five repeats per implementation per case,
alternating run order. Each timed render processes two seconds of 48 kHz
stereo excitation/tail after an untimed warmup. The old engine is frozen
from the baseline commit. CPU comparisons use the median of each case;
no compilation or other validation jobs run concurrently with timing.

The final summed median processing-time ratio was `1.00157` (+0.16%). Individual
case ratios ranged from −1.66% to +3.18%. No material increase was observed
in this internal engine benchmark. This measures DSP processing time on
this desktop, not total DAW/UI CPU or realtime margin on other machines.

Engine storage changes from 57,040 to 63,544 bytes (+6,504 bytes per desktop
original-224 engine). The new DAC stage occupies 17,288 bytes. All processing
and rapid-switching checks recorded zero allocations. The default engine
remains 57,040 bytes.

## Reproduction and local evidence

Windows x64 VST3 and Standalone builds passed. The desktop processor checks
passed for all six programs at 44.1, 48 and 96 kHz: mono/stereo, session state,
automation, 128/511/20,000-frame blocks, low-latency dry/wet behavior and
36 directed program switches. Processor callbacks recorded zero allocations
and releases. Empty-ROM startup and cached-bank restart checks passed. The
optional mixed 224/XL quick-preset check was skipped in the original-224-only
fixture run; no XL sonic validation was performed.

The portable test can run without private ROMs:

```sh
cmake --build build/plugin --config Release --target native_224_dac_check
ctest --test-dir build/plugin/native-hall -C Release -R native_224_dac_boundary --output-on-failure
```

To also check FPC phases, configure `NATIVE_HALL_ROM_DIR` with the original
224 v4.4 ROM directory, build the same target, then run
`native_224_dac_check` with the generated `programs-v44.bank224` path. The
test uses its accompanying `.0.wcs` through `.5.wcs` fixtures. The desktop
`build_plugin.sh --check` workflow now includes this check; `--compare` uses
the current six-program bank and desktop engine instead of the old single
Hall profile.

Private diagnostic data and scripts remain ignored under
`build/validation/event-dac-20261005/`: `output-tones.csv`,
`same-dac-tail-bands.csv`, `full-comparison-bands.csv`, `cpu.csv`,
`measurements.json`, paired raw tails, full-firmware WAVs and test logs.
The prior read-only diagnosis is in
`build/validation/filter-dac-20261005/REPORT.md`. ROMs, banks, WCS captures
and audio renders must not be committed.
