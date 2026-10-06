# XL startup and first headroom-read checkpoint

October 6, 2026. This follows the [normal title checkpoint](XL_PANEL_TITLE_VALIDATION.md)
and isolates the retained Hall / Hall prototype's missed first retrigger.
Production Native48, bank data and the CPU-measured source trees remain unchanged.

## Physical recipe and observation

The private renderer uses the same canonical Hall / Hall recipe: Dynamic on,
Mod/Decay Opt off, physical stop faders 18/18 and stop-delay 10, music seed 17,
amplitude 0.08, 1,000 ms warmup, 12 seconds, 48 kHz, block 256, analog mode 1
and wet A/C output. The native renderer uses the retained secondary-MID-corrected
prototype. No reference controller state, phase or event schedule enters native
audio.

Reference tracing starts after physical preparation and continues through
warmup. It records CPU/stack snapshots, routine entries and headroom port reads.
A private copy of the prototype adds read-only state accessors and fixed-size
headroom-read logging. Native processing allocates/releases nothing; reference
instrumentation and output-file writing remain offline.

All three accepted observation WAVs (`input.wav`, `reference.wav`, `native.wav`)
are byte-identical to the retained
`secondary-corrected/p17-m4-gate-secondary-corrected` case. Its band RMS error
remains **1.6506%**, envelope p95 **18.0204%**, and maximum envelope error
**32.6359%**. This investigation does not claim a sound improvement.

## Initial state and first reads

At input start, the reference CPU snapshot reports `0x0FBD`, the logarithmic
level-encoding loop. Stack return addresses `0x82FB` and `0x816C` identify the
active Slow call. That call entered **0.353516 ms before** input start and
had already read quiet left/right headroom before the signal arrived.
The prototype is instead in its monitor-pass stage, with ordinal 4 remaining
in its nine-pass loop. Both UART transmit deadlines are idle at this boundary.

The native parameter update resets the entire private pilot and starts Slow
at native state zero. It does not reproduce the reference's continuing scan
through the physical program/fader/toggle sequence. The observation establishes
different initial scan positions; it does not yet derive a replacement startup
or parameter-transaction law.

After warmup, both versions have held level 5,888, average 16, flags/amount
zero, LF/MID 132/95 and period 5. Other controller fields differ:

| Field | Reference | Native prototype |
| --- | ---: | ---: |
| Decay divider | 3 | 1 |
| Peak/display divider | 4 | 1 |
| Previous peak input | 0 | 11 |
| Pending title timer | 11 | 0, not initialized |

The native-only state probe obtains its data from the prepared bank and native
control law, with the canonical logical controls. It receives no reference
controller snapshot. All 48 resolved control cells match the retained render's
native control audit, including logical stop values 18/18 and delay 0.
These differences are observations, not individually
proven causes or accepted bank changes. The previously rejected compiler-state
candidate remains rejected.

The first left-channel reads explain how the initial chronology changes the
retrigger decision:

| Variant | Worker | Read time, ms | Detector bits | Average before read |
| --- | --- | ---: | ---: | ---: |
| Reference | Slow | 14.796 | 28 | 16 |
| Reference | Slow | 29.349 | 24 | 41 |
| Reference | Slow | 42.707 | 16 | 61 |
| Reference | Fast | 83.583 | 16 | 56 |
| Native | Slow | 5.081 | 28 | 16 |
| Native | Slow | 21.314 | 24 | 41 |
| Native | Slow | 34.629 | 24 | 61 |
| Native | Slow | 47.947 | 16 | 79 |
| Native | Fast | 87.647 | 0 | 72 |

Reference times use the substate-precision port trace. Native times are scheduled
semantic IN starts plus the read-sampling offset, relative to nominal warmup;
the actual native read is consumed in an input-pass callback. This does not
prove exact legal instruction/ADC-edge timing. The millisecond chronology is
larger than that unresolved sampling granularity.

The reference reacquires detector bit 16 after its earlier Slow read and still
has it available to Fast. The prototype's later Slow read consumes its remaining
bit before Fast. It also performs an additional Slow update during the initial
burst, producing a different running average. The reference starts restoring
at **84.039 ms**; the retained native prototype misses this first restore and
does not restore until **502.278 ms**. This is the observed mechanism of the
missed retrigger, not a complete explanation of every envelope discrepancy.

## Local branch counterfactual

A separate, explicitly labelled local test initializes `FastControlClockXL`
from the reference's first Fast-entry memory at 83.510742 ms. With the actual
read pair **16/0**, native logic requests one restore, returns LF/MID to
**132/95** and clears flags. With the synthetic pair **0/0**, it requests no
restore, retains LF/MID **16/16** and leaves flags at 1. Both terminate.

This checks the branch consequence of losing the detector bit. The compiler
completion uses native period calculation but a zero-duration placeholder;
compiler work/write payloads are deliberately outside this semantic check.
The existing separate compiler and Fast work oracles remain the work evidence.
No local observed state or counterfactual sample is supplied to primary audio.

## Scope, evidence and next step

Only the canonical Hall / Hall case receives this detailed startup/read trace.
There is no new all-program sound acceptance, listening or host validation.
The observation buffers are private test instrumentation, not shipping memory
layout. The prototype does not model the third panel timer; its observation
column explicitly uses a zero placeholder.

Private sources, commands, logs, accepted waveform identities and CSV/JSON
analysis are under `build/validation/xl-startup-read-20261006/checkpoint.json`.
The accepted render is `p17-m4-canonical-v2`. The first provisional observation
accessed a nonexistent third timer in its getter and is excluded; the getter
was corrected before the accepted rebuild/render. No DSP processing change
was made by that correction.

HEAD is `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion remains clean at
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Firmware is the complete hash-validated
original XL v8.21 set; the unchanged v5 bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
No private assets enter Git. The five production control/runtime files still
match the frozen audio baseline, and both CPU-measured source trees retain
their hashes; CPU is not rerun for observation work.

The next bounded step is to derive the physical parameter-transaction/reset
semantics and which scan state they retain, then represent that state natively
before rerendering the same independent fixtures. Arbitrary phase offsets and
copying the observed startup snapshot into primary audio remain excluded.
The subsequent [fader transaction checkpoint](XL_PARAMETER_TRANSACTION_VALIDATION.md)
derives pickup and deferred refresh boundaries, passes 1,004 all-program
physical transactions, and confirms that the experimental setter loses live
clock/held/divider state on an LF update. Its full native replacement remains
the next implementation step; the retained sound percentages are unchanged.
The following [live parameter queue checkpoint](XL_LIVE_PARAMETER_VALIDATION.md)
implements the ordinary-update retention path in an ignored pilot and passes
all 19 reverb integration checks. The paired LF-event recipe improves band RMS
and envelope errors, but the canonical WAVs remain unchanged: its 18.0204% p95
startup discrepancy still requires initial program/key/IRQ/read chronology work.
