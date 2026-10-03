# Daisy performance review — 2026-10-03

The reported symptom is intermittent clicks/dropouts. Missing an audio deadline
is a plausible explanation, but no on-device timing was captured in this review.
The reference callback has a 1 ms deadline at 48 kHz with 48-frame blocks.
The initial review used isolated source copies in
`/private/tmp/cineol-perf-review`. Following approval, bounded resampler phase
counters and skipping unchanged Hall coefficient updates were implemented in
the shared Engine48 core. The findings below describe the original baseline.

## Implemented changes

`rate48.hpp` now uses bounded unsigned phase and history-position counters.
Weights, tap accumulation order, output scheduling and latency are unchanged;
history remains circular without duplication. `Engine48::set_parameters` compares
all Hall control fields explicitly and reapplies them after initialization,
program changes or changed Hall parameters. Control polling, quantization,
debounce and audio-rate gain/mix/analog smoothing retain their previous timing.
No new parameter smoothing, row inlining or compiler flags were introduced.

`native_hall_processing_check` compares each SRC direction with a frozen
absolute-clock reference for 1,000,000 inputs, including resets and silence. It
also compares cached controls with unconditional Hall writes across all six
programs, single-field changes, tails, hard switches, wet-only output and repeated
preparation. This test is registered with the private-bank CMake checks.

The implemented combination also matched the original, unmodified Engine48
bit for bit over 144,000 stereo frames per program in a separate host comparison.
The independent ARU/bank, analog/core and processing-equivalence CMake checks
passed, as did the Daisy controls and hardware-adapter checks. Seed and Patch SM
images built and passed the existing layout validator with 21,664 bytes of ITCM
each. Neither image was flashed; hardware CPU improvement remains unmeasured.

## Existing behavior

- `native-hall/daisy/src/main.cpp` runs control acquisition and parameter updates
  inside the callback every second block (500 Hz).
- The reference adapter reads running ADC results and GPIO without waiting.
  Allocation, blocking I/O, locks and sleeps were not found in its processing
  path. Placement construction, filter preparation and ADC warm-up happen before
  audio starts. No active local hardware adapter was present for inspection.
- `Engine48::set_parameters` reapplies Hall coefficients and pre-delay offsets
  even when all Hall parameters remain unchanged.
- Selecting another program synchronously clears the 32 KiB Hall delay buffer,
  resets state and applies controls. This is an additional deadline risk during
  switching. Clearing or resetting incrementally would require a separate proof
  of identical switching behavior.
- The FIR resamplers use 64-bit absolute sample/phase counters. ARM GCC already
  replaces constant division with multiplication/shift sequences; there are no
  software division calls in the inspected audio callback. Avoiding the counters
  still removes those sequences.
- The FIRs perform approximately 8.77 million coefficient multiplies per second
  across both conversion directions, before analog filters and the Hall network.
- Hall uses shared `noinline` row helpers. This saves ITCM but costs repeated calls
  in each native network. The four compiled graph branches contain 370 row call
  sites in the baseline object; this is a static count, not calls per sample.
- `-O3`, hardware floating point and `-ffp-contract=off` are already configured.
  The startup also sets FPU flush-to-zero/default-NaN bits. libDaisy's included
  makefile explicitly disables RTL loop-invariant motion for C++.

## Recommended candidates

1. **Bounded phase and ring counters in `RationalFilter`.** Replace absolute
   64-bit scheduling with an unsigned relative phase and ring position. At each
   input, emit while `phase < Up`, use `weights[phase]`, add `Down` after emission,
   then subtract `Up` at the end. Keep coefficients, output timing, tap order and
   floating-point expression order unchanged. The ARM processing wrapper drops
   from 3,592 to 3,266 code bytes and from five static `umull` instructions to zero.
   This is a code-generation result, not a measured speedup.

2. **Contiguous FIR history.** Duplicate the short histories so the complete tap
   window is contiguous, avoiding a circular-slot mask at every tap. Preserve the
   accumulation order and add no allocation. Combined with bounded phase counters,
   the wrapper is 3,254 bytes. The two duplicated histories cost 1,024 additional
   bytes; smaller counter storage makes the tested host Engine48 object grow by
   1,008 bytes overall. Compare this candidate with phase-only on the MCU: extra
   stores may offset some indexing savings.

3. **Skip unchanged Hall updates.** Keep reading, debouncing and quantizing at
   500 Hz, and preserve audio-rate smoothing. Call `Hall::set_controls` only when
   Hall parameters change, after initialization, or following a program change.
   Use explicit field comparisons, not struct `memcmp`. The supported bank's
   control coefficient tables do not overlap modulation coefficient rows; decay
   still updates loop diffusion independently. Do not move all controls to the
   main loop or reduce their update rate as part of this optimization.

4. **Selective row inlining with forced arithmetic inlining.** Keep the existing
   ARU rounding, saturation and edge order. Any inlined row variant must also
   force `edge` and `add` inline: otherwise GCC outlines arithmetic and the
   existing layout validator correctly rejects it. Small boundary-operation
   inlining is the conservative starting point. Memory-read-row inlining offers
   a more aggressive experiment but leaves little ITCM headroom.

5. **Compiler experiments.** Test `-fmove-loop-invariants` and `-funroll-loops`
   separately, then together, while retaining `-ffp-contract=off`. Append the
   positive loop-invariant flag after the included makefile's negative flag;
   setting it only in `OPT` would be overridden. Do not patch the dependency
   checkout. The wrapper grew to 4,080 bytes with unrolling and 4,124 with both
   flags. Larger code alone does not establish faster execution. GCC's
   [optimization documentation](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gcc/Optimize-Options.html)
   explicitly notes that loop unrolling may or may not improve speed.

## ARM linking experiments

Toolchain: Arm GNU GCC 15.2.1, Cortex-M7, Thumb, fpv5-d16, hard float. The original
v4.4 ROM1–ROM5 passed verification before local firmware linking. Reference Seed
main code and the existing libDaisy/startup objects were used. These sizes do not
include a carrier-specific adapter or combinations of all candidates.

| Variant | ITCM bytes | Free ITCM bytes | Existing layout validator |
| --- | ---: | ---: | --- |
| Baseline | 21,808 | 43,728 | Pass |
| Inline boundary operations; force edge/add inline | 24,408 | 41,128 | Pass |
| Inline memory-read rows; force edge/add inline | 63,288 | 2,248 | Pass |
| Inline memory-read rows without forcing arithmetic | 46,280 | 19,256 | Reject: outlined edge helpers |
| Inline all rows without forcing arithmetic | 64,400 | 1,136 | Ineligible: outlined edge helpers |

Inlining all rows **with** forced arithmetic produces 82,104 bytes of Hall code
alone, exceeding the entire 64 KiB ITCM region. QSPI firmware capacity does not
remove this fast-code-memory constraint. Keep the existing layout checks.

## Sound-preservation checks

- Each FIR direction was compared with the original for 1,000,000 input frames,
  including deterministic random input, silence and a reset. Emission counts and
  every output float were bit identical for bounded phase plus mirrored history.
- Each of the six programs was compared for 144,000 stereo host frames (864,000
  total per comparison run), including loud input, tails, control changes,
  unchanged repeated updates, analog/mode switches, gain/mix/output selection
  changes and hard program switches. Mirrored FIR history, cached controls and
  forced memory-read-row inlining each matched the original output bit for bit.
  Final delay memory, cached coefficients/offsets and checked saturation counts
  also matched.
- Comparisons used host Clang with `-O3 -ffp-contract=off`. They are evidence for
  these tested cases, not a proof for all inputs or an ARM runtime capture.
- There was no flashing, physical playtest or MCU CPU timing measurement.

## Implementation and measurement order

First add optional callback timing around both controls and processing; retain
peak duration and deadline misses, not just average load. Measure separately for
unchanged controls, changing controls and program switches, across all programs
and modes. Keep reporting outside the audio callback and preserve normal boot.

Then compare bounded phase counters, cached Hall updates and selective inlining
one at a time. Evaluate mirrored FIR history and compiler flags against those
measurements. Repeat sound comparisons and the existing independent ARU/control
tests for the final combination, then capture ARM output and worst-case callback
timing on hardware. Do not claim a CPU percentage improvement from host timing,
assembly size or a successful linker check.

`-Ofast`, `-ffast-math`, enabling FP contraction, changing FIR taps/sample rates,
approximating analog filters or changing block size are outside the requested
sound/behavior-preserving scope. Automatic skipping of inactive analog paths
also needs care: their evolving filter states affect subsequent transitions.
