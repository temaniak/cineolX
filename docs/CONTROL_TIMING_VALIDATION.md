# Original-224 native control-timing foundation

October 5, 2026. Baseline: `2677d7c`. Original Lexicon 224 v4.4 ROM1–ROM5;
Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Validation host: macOS arm64,
AppleClang Release, strict FP. See [continuation record](SOUND_ACCURACY_HANDOFF.md).

## Status and scope

The native modulation **procedure-duration and WCS-write timing model** is
validated. The complete control-call scheduler is still open. The model is
not connected to `DesktopHall::process`; application sound, CPU cost, state,
parameter identifiers and prepared-bank version remain unchanged by this
checkpoint. No Daisy code or dependency checkout was modified.

The earlier [modulation check](MODULATION_VALIDATION.md) proved state evolution
when native and ROM routines receive the same calls. This checkpoint proves
the duration of each such call and each write it issues. It does not determine
when the caller should issue the next call, or reproduce program-load phase.

## Native model

`native-hall/desktop/control_timing224.hpp` contains two small native laws:

- `modulation_cycles224`: conditional instruction costs from the v4.4
  modulation routine. Inputs are the prepared program, modulation state,
  enabled mode and random sequence. The result includes divider/hold work,
  interpolation-direction branches, accepted/rejected address moves and
  coefficient writes. The model neither executes opcodes nor mutates DSP state.
- `WcsTiming224`: grant/wait timing for the four supported fixed network
  shapes. It preserves the T&C protect-pair flip-flop and the displaced
  fetch after a write grant. A complete unaffected pass resets that logic, allowing
  long elapsed intervals to be skipped without iterating over their duration.

The graph's unprotected rows are 46/95 for Hall B and Hall A, 46/69/93 for
Plate, and 94 for Chamber. The model validates those properties, including
the reset at row 99, against the independent ROM-loaded WCS. These sparse graph
properties are not a ROM or WCS image.

A write request reaches the T&C six CPU states after the instruction entry:
four states before its memory-cycle T1 and two before MWTC. Treating that as
five states passed most calls but failed at an access-window boundary. The
first matrix exposed 488 whole-call mismatches; per-write timing checks made
the incorrect boundary visible. Correcting it removed all observed mismatches.
No empirical rate multiplier or fitted tail correction was used.

The laws use fixed storage and bounded native branches. They perform no
allocation, I/O, locks, waits or ROM access. Emulator tracing and CSV output
remain confined to the offline tool. No runtime storage or CPU claim is made
until the model is actually integrated and benchmarked.
The wait-clock model does not simulate the audio effect of displaced
instructions or held operand-register clocks in the DSP network.

## Independent verification

The checked-in `native_224_control_timing_check` recognizes all five ROMs by
SHA-256. Its matrix covers six programs × six Depth values (0/7/21/35/54/71)
× all four controller modes × four constant ADC mantissas (0/128/1024/2047):
**576 cases**. Bass and Mid vary with Depth, exercising additional controller
work between calls. Detector masks vary with input level.

Each case settles mode/controls for 100 ms, then observes 300 ms of actual
execution. At each modulation entry, the native model receives a snapshot of
the actual modulation state and the actual entry clock/row phase. It predicts
the routine's duration and every WCS instruction's entry/duration **before**
those instructions execute. Instruction durations from the trace are used
only to calculate independently observed wait states and compare results.

The bus model is initialized once per case. Its initial protect-pair history
is unknown because setup may have issued other WCS writes. The first two calls
are excluded from bus/write assertions to cross a full unaffected frame; their
instruction-cost assertions are retained. The model is not re-aligned on
subsequent calls. Actual entry times and modulation states are supplied at
every call: this is a local timing-law test, not a free-running scheduler test.

| Measurement | Result |
| --- | ---: |
| Complete modulation calls | 238,967 |
| Observed WCS writes | 328,567 |
| Instruction-cost mismatches, excluding observed wait states | 0 |
| Whole-call mismatches including native predicted waits | 0 |
| Per-write entry/duration/count mismatches | 0 |
| Observed procedure durations | 748–2,570 CPU states |
| Observed WCS write instruction durations | 10–109 CPU states |

At 2.048 MHz, the observed procedure durations span about 0.365–1.255 ms.
The ranges are observations for this matrix, not guarantees for all possible
firmware activity. The tool checks a maximum of six writes per supported
modulation call and a 109-state per-write model bound.

The existing desktop ADC/DAC/controller/Gain tests also passed: seven CTests,
including independent six-program XREG sources, sparse peaks, direct-switch
continuity and allocation tracking. No sound comparison or CPU result is
claimed for a changed processing path: there is no processing-path change.

## Reproduce

```sh
cmake -S . -B build/sound-validation -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON -DCMAKE_BUILD_TYPE=Release -DNATIVE_HALL_ROM_DIR="/private/path/224 v4_4"
cmake --build build/sound-validation --target native_224_bank native_224_control_timing_check native_224_controllers_check native_224_gain_timing_check native_224_adc_check native_224_dac_check
native_224_control_timing_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/control-timing.csv
ctest --test-dir build/sound-validation/native-hall --output-on-failure -R "native_224_(adc|dac|controller|gain)"
```

Private CSVs, exploratory instruction listings and renders are ignored under
`build/validation/scheduler-20261005/`. They are not needed to rebuild the tool;
no ROM, bank, WCS, firmware listing or audio capture belongs in Git.

## Required continuation

1. Derive native durations and event boundaries for the surrounding scan:
   first modulation return, conditional high/low XREG reads and peak update,
   second modulation call, panel-channel comparisons, and the level controller.
   Include level-controller coefficient writes in the shared WCS grant clock.
2. Validate the whole sequence over complete scans without repeatedly supplying
   actual call-entry times or controller states. Separate stable controls from
   compiler/predelay transitions and distinguish new-instance startup from
   direct switches and Spillover's independent slots.
3. Only then replace the uniform desktop scan. Validate normal and diagnostic
   aligned sound matrices, musical inputs, multiple seeds/start phases and
   input levels, followed by paired CPU and processor/Spillover regression.

The overall sound-accuracy work remains open. Exact local routine timing alone
does not establish identical modulation-on tails or eliminate Chamber's
converter/quiet-floor residuals.
