# XL normal fader transaction checkpoint

October 6, 2026. This follows the [startup/read investigation](XL_STARTUP_READ_VALIDATION.md)
and derives two boundaries of normal physical fader handling. Production audio,
bank data and the previously CPU-measured pilot remain unchanged.

## Derived native boundaries

`fader_transaction_xl.hpp` implements soft pickup after the enclosing calibration
has supplied a clamped logical value. Live pickup accepts immediately; otherwise
equality or crossing the stored value latches the fader. A fader still on its
original side of the stored value is rejected. The corresponding work costs are
**64 states** for live pickup, **93** for equality, **99** for rejection and
**120** for crossing. The native law predicts the resulting logical value,
pickup state and returned acceptance/carry flag without ROM execution or heap
allocation.

The normal transaction's final suffix takes **40 states** and assigns dynamics
flags **192 / 0xC0**, requesting deferred LF/MID refresh. It preserves the rest
of the dynamics state. This happens after calibration and any nested coefficient
compilation/record copy; it does not reset a scan clock or start a new Slow call.
The tested enclosing handler returns to `0x81A1`, where the current main
reconciliation pass resumes.

Calibration and full enclosing compiler/formatter work remain separate. Pickup
acceptance alone does not imply that the enclosing calibration requests a full
compiler invocation. The helper is not a complete parameter-transaction clock.

## Physical validation

The public `cineol_xl_fader_transaction_check` uses real LARC fader messages
and page operations. Its sweeps retain firmware pickup state: they do not use
RAM seeds or the operator's `takeOver` helper. The check compares pickup return
work/state/carry, suffix return work and the complete exposed dynamics state.
Across the enclosing normal transaction it also checks retention of held level,
running average, current LF/MID, trigger/stop state, both controller dividers,
eleven history entries, software headroom latches/accumulator and monitor peak.
Compiler-owned amount, period and previous-peak fields may change and are not
asserted invariant across compilation.

Both final CMake Release campaigns pass on **all 22 programs**:

| Campaign | Transactions / main-loop returns | Pickup accepted / rejected | Compiler invocations | Stop-bit entries |
| --- | ---: | ---: | ---: | ---: |
| Factory, three toggles off | 502 / 502 | 462 / 40 | 458 | 0 |
| Active-tail recipe | 502 / 502 | 502 / 0 | 458 | 459 |

The active recipe physically enables Dynamic, disables Mod/Decay Opt, sets
stop faders to 18 and delay to 10, then sends deterministic left-channel noise
at amplitude 0.12 for approximately 100 ms followed by silence to 250.667 ms.
All 19 reverbs receive that preparation; the three non-reverb effects retain
their ordinary fixture. The subsequent fader sweeps cover first-page slots
0/1 and applicable stop/delay controls. This is a control-law fixture, not a
new full numerical sound render.

Both campaigns have **zero work/state/retention errors**, zero nested Slow/Fast
entries in a fader handler and zero tracked native allocations/releases. All
1,004 handlers return to the current main reconciliation pass. The pickup work
path totals are 380/44/40/38 in the factory campaign and 409/44/0/49 in the active
campaign, ordered as 64/93/99/120 states. Only pickup states 1/2/4 are physically
covered. No interrupt occurs inside the two locally timed workers. Receive/
parser flow outside those workers is not modeled by this boundary oracle.

A separate private Hall / Hall trace adds an initial below-target message and
passes **24 transactions**, including **two rejected below-target pickups**.
It retains individual before/after contexts. Even a rejected pickup assigns
the final `0xC0` refresh request while leaving the logical parameter unchanged.
For example, a physical message clamped to 64 while LF remains 132 preserves
current LF/MID 16/16, held level 52,992, average 72 and both dividers at 4;
flags change from 1 to 192. The accepted equality case separately shows the
compiler changing amount from 0 to 1 while retained fields stay unchanged.
This separates physical message handling from a host merely resubmitting an
unchanged coherent parameter snapshot.

## Existing prototype counterexample

A native-only negative check compiles the actual retained experimental audio
source, warms it for one second and changes logical LF from 132 to 160. Its
current `set_native_controls` path resets the entire pilot from the older
`dynamics_` snapshot: scan clock **2,047,996 -> 0**, held level **5,888 -> 2,612**,
decay divider **1 -> 2** and peak divider **1 -> 8**. It fails the physical
retention property as expected (exit 1).

This identifies a concrete defect in the experimental update path, beyond an
unexplained phase difference. The passing pickup/suffix law is a foundation
for replacing it, not an already integrated replacement. Accepted physical
updates must act on live pilot state, preserve the ongoing scan and use the
appropriate compiler/refresh boundaries; busy updates need deferred handling.

## Scope and evidence

Program recall, Size/Chorus recompilation, Dynamic/Opt/Mod key transactions,
busy parameter-update scheduling, complete calibration/formatter clocks and
legal instruction/interrupt boundaries remain outside this checkpoint. No
shipping behavior or plugin identifiers change. No new bank or phase offset
is accepted. The retained Hall / Hall envelope p95 remains **18.0204%**.

HEAD is `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion is clean at
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Inputs are the complete hash-validated
original XL v8.21 set and unchanged v5 bank, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
No ROMs, banks, traces or private adapters enter Git. The five production
control/runtime files and both CPU-measured source trees retain their hashes.
CPU is not rerun for the unused helper and offline checks.

Private source snapshots, commands, logs, individual Hall contexts and the
expected failing native counterexample are recorded in
`build/validation/xl-parameter-transaction-20261006/checkpoint.json`.
An initial observer attempted to track receive IRQ returns outside the timed
workers and was rejected as incomplete; final checks restrict IRQ accounting
to those worker windows. That harness repair is not a native-law correction.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_fader_transaction_check -j2
build/xl-sound-validation/native-hall/cineol_xl_fader_transaction_check ROM_DIRECTORY PREPARED_BANK
build/xl-sound-validation/native-hall/cineol_xl_fader_transaction_check ROM_DIRECTORY PREPARED_BANK --active-tail
```

The next bounded step is the native live-state parameter-update path and its
deferred reconciliation/compiler boundaries. The independently retained sound
recipes must be rerun after an assembled correction; state retention alone
does not establish sound acceptance or solve initial program/key chronology.

## Subsequent live-state integration

The [live parameter queue checkpoint](XL_LIVE_PARAMETER_VALIDATION.md) implements
the bounded LF/MID/STOP/DLY replacement in an ignored pilot. All 19 reverbs pass
114 integration scenarios including busy requests, with zero state-retention
errors and processing heap activity; the frozen old Hall fails the same check.
The paired physical LF-event sound case improves band RMS error from 25.3844%
to 1.5975% and envelope p95 from 648.8907% to 18.7391%. Canonical no-event WAVs
remain identical, with p95 18.0204%. This is a coherent transaction core with
the main compiler and deferred-refresh suffix, not the complete physical
calibration/compiler wrapper or startup/key chronology. Production is unchanged;
the new candidate is not covered by the earlier CPU measurement.

The [ordinary numeric calibration follow-up](XL_FADER_CALIBRATION_VALIDATION.md)
now predicts physical page/slot clamping, fixed scaled limits and pickup timing.
Two isolated all-22 campaigns cover 4,070 transactions and 3,811 supported
calibrations with zero work/state/retention errors and heap activity. Variable
pre-delay and effect-specific mappings remain separately excluded; Size pages
are outside the expanded fixture. Full compiler-wrapper/copy chronology and
its runtime integration remain the next step. No new sound render is made.
