# Native XL feedback compiler and fast coupling

October 6, 2026. This follows the
[fast-controller work](XL_FAST_CONTROL_VALIDATION.md). It removes observed
feedback-compiler duration from the optional coupled fast oracle. Native48
still uses nominal controller clocks; full XL sound correction remains open.

## Native implementation

`desktop/feedback_timing_xl.hpp` expresses B000's allpass compiler as a
typed arithmetic/work law. It accounts for scale/cap reuse, normal/other MID
indices, reduction, three ordered coefficient-byte writes per target and
the final-two-target index change in applicable layouts. Byte multiplication
uses arithmetic values and a bounded bit-count work formula. Each write asks
the independently checked `WcsTimingXL` for its READY completion and commit;
measured waits and instruction durations are not returned to this model.

`desktop/feedback_clock_xl.hpp` yields each write and its return separately.
Its 40-byte clock borrows profile, structural metadata and three cache bytes;
40 bytes is not the total controller/engine storage. The enclosing native
scan must advance DSP rows and apply the byte at its predicted commit, not
wait until the instruction has returned. IRQ wall time remains separate.
`desktop/fast_feedback_timing_xl.hpp` adds the ten-state AFE4 dispatch and maps
the compiler's work/write offsets into the enclosing fast routine.

`desktop/coefficient_structure_xl.hpp` derives the coefficient lane's two
fixed ZERO/XFER bits, in CPU bus polarity, from the existing native graph.
The shared constexpr table is 2,816 bytes. The final model does not obtain
those bits from reference ROM/shadow words. Reference reads only verify the
native table. No new bank payload/version or ROM words are required for this
structure. All new kernels use bounded native arithmetic/stages without
CPU/opcode interpretation, processing allocation, I/O, locks or waits.

## Physical local checks

The offline compiler oracle physically selects all 22 v8.21 programs. The
19 reverbs cover four Dynamic Decay/Decay Opt combinations; the three effects
cover their disabled dynamics branch. STOP faders request raw 18 and stop
delay raw 10 through operator commands. Available normal MID faders then
request 64 and 232 while tracing. Each of 79 windows also has two seconds of
zero audio with comparator-pin stimulation 15/3/0/31/0 in 64-frame blocks.
These are explicit controller diagnostics, not primary analog/sound renders.

The final compiler check passes:

- 1,219 B000 entries and 13,392 individual writes, including cache reuse for
  1,913 targets and 238 calls in layouts with separate indices.
- Work, payload/start/return, commit, cache-state and resumable-stage checks:
  zero errors. The native computations have zero tracked new/delete calls.
- 36,513 independently predicted reference bus grants, including other
  controllers' accesses; zero grant/XACK errors and zero pre-seed exclusions.
- 21,616 structural-bit comparisons, covering all rows in every selected
  graph plus the active target rows. All agree with the physical template.

The three effects have no B000 calls in these windows. Inverse Room and
CD Plate A also have no calls in mode 0; their other modes include calls.
The full 22-program selection/structure coverage is not a claim of active
feedback writes in every effect. Half-scale B000 calls do not occur in this
physical dataset. A separate stage/eager equivalence check covers 262,144
synthetic cases and 4,718,622 writes/returns, including zero/full target counts,
both layout flags, varied byte states and origins beyond 32 bits. It verifies
stage consistency, not additional physical half-scale sound acceptance.

Profiles, coarse indices, reduction/cache state, a quiet row-zero origin and
observed IRQ spans are labelled local inputs. Profile targets are checked
against the independently prepared v5 bank. READY waits, commits, work and
native structural bits are not copied from the reference. Other actors'
observed bus-request times maintain the local grant-clock fixture. These
checks do not establish an independently generated full scan/IRQ chronology.

The first catalog attempt stopped at a physical program-selection failure
after MID/active-dynamics fixtures. Its partial results/source/executable are
preserved in `initial-selection-failure/`. The fixture now disables dynamics
and optimization through physical keys and settles before the next program.
The native compiler law did not change to accommodate that setup failure.

## Coupled fast result

`cineol_xl_fast_timing_check --predicted-feedback` now supplies native feedback
work to both eager and resumable fast controllers, instead of measured
feedback duration. It passes 54,153 local fast calls, 492 predicted feedback
invocations and 4,851 writes with zero work/state/stage/payload/commit errors.
All 25,430 local bus grants agree, and 8,224 graph-row structural comparisons
pass. One fast call precedes quiet-origin initialization and is excluded
explicitly. The original observed-compiler mode still passes 54,154 calls.
Both modes have zero native new/delete calls. These runs are not sound/CPU
before/after comparisons.

Normal-decay restoration through A7A9 remains an observed duration (158 calls),
along with entry context, detector bytes and 911 IRQ spans. Coarse feedback
indices/cache initialization are still observed context; the complete native
Low/Mid compiler must own those fields. Template structure is independently
native. Consequently this coupling removes the feedback-duration input but
does not make fast/Native48 fully independent of reference timing.

## Provenance and reproduction

Private evidence is under `build/validation/xl-feedback-timing-20261006/`.
The actual preceding 188-file source is frozen in `baseline/`. Final manifests,
commands, binary hashes, per-window summaries and exit codes are recorded in
`all-run.json`, `fast-coupled-all-run.json`, `fast-historical-all-run.json` and
the final checkpoint. The earlier observed-structural-bit version is retained
in `observed-structural-bits/`. Comment-only clarification was followed by
a rebuild: both final executables remain byte-identical to the all-22 tested
binaries. Validated and current source manifests are preserved separately.

Reference remains pinned Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, with hash-validated original v8.21
chips and unchanged v5 bank SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Release macOS arm64, C++20 and `-ffp-contract=off` were used. Both dependency
checkouts and the preceding 78-file production manifest remain unchanged.

```sh
cmake -S . -B build/xl-sound-validation -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON
cmake --build build/xl-sound-validation --target cineol_xl_feedback_timing_check cineol_xl_fast_timing_check -j2
build/xl-sound-validation/native-hall/cineol_xl_feedback_timing_check /private/path/to/v8.21-roms /private/path/to/v5.bankxl
build/xl-sound-validation/native-hall/cineol_xl_fast_timing_check /private/path/to/v8.21-roms /private/path/to/v5.bankxl --predicted-feedback
build/xl-sound-validation/native-hall/cineol_xl_fast_timing_check /private/path/to/v8.21-roms /private/path/to/v5.bankxl
```

Next: derive the Low/Mid compiler's indices, coefficient writes and work,
then complete slow, auxiliary, serial/display/reconciliation and detector
read/clear timing. Initial/program/key phase and displaced-row arithmetic are
still required before production integration. Repeat independent all-22 and
plugin checks, paired sound/CPU, overlap/control edges and host/listening
acceptance after the full scan is connected. No production sound, installation,
whole-plugin CPU or Daisy realtime-margin change is claimed at this checkpoint.
