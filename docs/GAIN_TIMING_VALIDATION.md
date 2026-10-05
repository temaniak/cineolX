# Original-224 desktop Dry/Wet and Input Gain checkpoint

October 5, 2026. Source baseline: `0ebb668`, followed by the direct-switch
counter correction. Desktop application only; no Daisy work.
Status: measured/tested, no behavior correction required by these fixtures.
Implementation/test checkpoint: `ce45a03`.

## Behavior and tests

The six-program `native_224_gain_timing_check` verifies the desktop engine:

- Dry output is sample-exact at **70 internal 48 kHz frames**, including with
  Input Gain at +12 dB. Input Gain applies to the wet excitation before AIN;
  it does not raise the dry signal.
- Settled 50% Mix follows linear dry/wet interpolation. Maximum absolute
  error is **1.321e-6** versus separately rendered dry and wet instances;
  residual Mix smoothing/float rounding accounts for this small difference.
- Six programs × gains -36/-12/0/+12 dB compare Input Gain with independent
  source scaling before AIN. Maximum integrated output-energy discrepancy
  is **0.00845 dB**. This is an energy comparison with ADC quantization,
  not a sample null or a claim of perfectly identical arbitrarily quiet tails.

The gain test uses 40,000 silent settling frames and two seconds of signal/
tail, with enhancement/optimization disabled and matching initial states.
Input is a low-level two-tone excitation; clipping response is covered by the
independent ADC boundary check, not inferred from this linear gain fixture.
Input Gain/Mix retain their approximately 10.4 ms smoothing time constant at
48 kHz. Parameter meanings, IDs, saved sessions and latency policy are unchanged.

## Timing limits

Reported normal transport remains 70 internal frames: approximately 32 input
transport frames plus the desktop output's fixed 38-frame transport. Wet audio
also has algorithmic predelay, separately timed ADC/DAC channels and the
analog circuits' frequency-dependent phase/group delay. Dry transport does
not reproduce those musical/circuit delays. Matching reported latency alone
does not establish a dry/wet phase null with either Reflexion or old versions.
The validated converter phases are in the [ADC](INPUT_ADC_VALIDATION.md) and
[DAC](EVENT_DAC_VALIDATION.md) reports.

The full processor suite checks 44.1/48/96 kHz, mono/stereo, irregular blocks
and normal/zero-latency modes. Low latency reports zero and uses direct
host-rate dry while leaving the wet engine running. At intermediate Mix,
different delay choices naturally change interference; this is the existing
documented behavior, not a new frequency-response correction.

## Spillover

The user currently switches programs with overlapping tails. During Spillover,
two independent networks contribute wet output and a common dry mixer prevents
duplicating the dry signal. The old network retains its controls/state; the
new network starts separately. Input crossfades over 5 ms, tail fade uses the
selected 1–10 second duration, and rapid/disabled overlaps retire within 20 ms.
These are application transition behavior, not the original single-network
firmware's reverb decay. Do not measure an individual algorithm's T20 across
a switch/overlap or interpret the chosen fade duration as original RT60.

The original-only processor modes `--spillover-224-check BANK` and
`--spillover-224-cpu BANK` allow verification without an XL fixture. They cover
independent old/new references, all 30 directed pairs at three sample rates,
block-size independence, dry mixing, rapid retirement and CPU during overlap.
Results are recorded in [final regression checkpoint](FINAL_SOUND_VALIDATION.md).

## Reproduction

```sh
cmake --build build/sound-validation --config Release --target native_224_gain_timing_check native_hall_plugin_check
native_224_gain_timing_check build/sound-validation/native-hall/programs-v44.bank224
native_hall_plugin_check --bank build/sound-validation/native-hall/programs-v44.bank224
native_hall_plugin_check --spillover-224-check build/sound-validation/native-hall/programs-v44.bank224
```

The bank-based gain test is also registered in CTest when a private original
ROM directory is configured. Processor tests create isolated temporary caches;
they never modify the user's normal bank or preset cache. No new sound,
gain, Mix or latency implementation was introduced by this checkpoint.
