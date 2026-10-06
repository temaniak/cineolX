# XL outer parameter compilation and tail focus

October 6, 2026. This follows [numeric fader calibration](XL_FADER_CALIBRATION_VALIDATION.md)
and derives the enclosing parameter frame and normal fader copy/refresh sequence.
It is a standalone native law and a private local oracle. Production audio and
the queued sound pilot are unchanged; no new sound improvement is claimed.

## Derived boundaries

`parameter_compiler_clock_xl.hpp` contains two bounded clocks. The outer compiler
clock predicts the preparation/main/group call boundaries, mask shifts and page
skips, secondary cache invalidation and feedback-index suffix, compact six-cell
cache commits, final five auxiliary snapshots and limiter/size mirrors.
The fader clock predicts address preparation, calibration/compile dispatch,
48-byte record-copy entry and deferred-refresh return. The copy worker takes
**1,957 work states**; the refresh suffix remains 40 states. The copied record
contains the program selector plus 47 control bytes, rather than the separate
48-cell raw parameter array.

A masked descriptor page consumes no logical cells. Enabled groups consume six
contiguous cells; their cursor is checked against the physical group entry.
This matters when page 2 is masked: a later group can still update logical/cache
cells 6/7. The first provisional model associated those cache cells exclusively
with page 2 and failed CD Plate A and Plate / Chorus. Compact cursor/cache
bookkeeping fixes those failures without injecting observed cache return values.

The clock uses fixed arrays/scalars and bounded loops, with no allocation, I/O,
locks, waits, ROM execution or instruction stream. Program masks, parameter/cache
entry context and auxiliary metadata are inputs supplied by the enclosing owner.
The program preparation, main compiler and six-slot group workers still supply
their durations. The local oracle observes those durations, the main dynamics
return context and the secondary group's limit field. Their complete payload/
duration laws and startup integration are not established by this checkpoint.

## Physical validation

The private `frame-check.cpp` reuses the retained physical fader recipes: page
operations and real messages for LF/MID, applicable STOP pair and DLY, without
RAM seeds or take-over. Factory and Dynamic-active campaigns each cover all
22 programs. The active prelude applies STOP 18/18, DLY 10, Dynamic on, Mod/Opt
off to the 19 reverbs, renders seed-17 noise at amplitude 0.12 for 4,800 frames
and silence to frame 12,032 at 48 kHz / 64-frame blocks. The three effects use
the ordinary prelude. No WAV is written.

| Boundary | Factory | Active | Total |
| --- | ---: | ---: | ---: |
| Normal fader transactions / calibration entries | 502 | 502 | **1,004** |
| Outer compiler frames / preparation / main calls | 458 | 458 | **916** |
| Additional six-slot groups | 2,006 | 2,006 | **4,012** |
| Secondary groups / index suffixes | 88 | 88 | **176** |
| Record copies | 458 | 458 | **916** |

All final outer timing, exposed frame/cache/cursor state and copy work/payload
checks have **zero errors**, with zero tracked native allocations/releases.
Not every transaction requests compilation; calibration's returned carry
controls that boundary. The copy check predicts identity from the entry record
and verifies its physical destination. No IRQ occurs inside these timed frames;
instruction-level interrupted dispatch remains open. The early control-flag
return is implemented but not physically covered by these ordinary sweeps.

The first observer failed to rearm one-shot watches and is excluded. The next
provisional clock omitted a 10-state jump after the main compiler; its frozen
Hall / Hall negative check fails with 132 timing errors and zero state/copy
errors (expected exit 1). The corrected Hall check passes. Subsequent failed
all-program cache-model logs are retained beside the accepted final campaigns.
The negative guard is the provisional standalone law, not shipping code.

## Tail remains the sound criterion

The user's reminder to preserve tail validation is recorded here explicitly.
Passing controller boundaries is preparation for tail validation; it does not
accept a sound correction. Before expanding more standalone controller work,
the next sound experiment must address the measured first-tail/retrigger cause
and reuse independent native/reference processing with the retained recipes.

The current canonical Hall / Hall response still has active-envelope p95
**18.0204%** across 39 selected 50 ms windows, and maximum **32.6359%**. That
response metric includes input-driven windows. The retained fixture has silent
intervals after its two noise bursts and after its final chord. A separate
tail-only read of the saved envelope selects complete windows wholly inside
those silent intervals, keeping the existing −82.44995 dBFS activity threshold:

| Silent input interval | Complete / active 50 ms windows | Median error | p95 error | Maximum error |
| --- | ---: | ---: | ---: | ---: |
| First burst ends at 0.166667 s; next begins at 0.5 s | 6 / 6 | 13.8534% | 30.4842% | **32.6359%** |
| Second burst ends at 0.666667 s; chord begins at 1 s | 6 / 3 | 1.8214% | 4.6796% | 4.9972% |
| Final chord ends at 2 s; silence through 12 s | 200 / 2 | 0.1222% | 0.1959% | 0.2041% |

These short cohorts are diagnostics, not catalog acceptance. In particular,
two active final-tail windows cannot establish general decay accuracy. Quiet
windows remain visible in the full-capture plot and residual-floor metrics;
they are not silently counted as matches. T20 remains unavailable for this gated
fixture. No new render or reference phase/state injection is used for this view.

`retained-hall-tail.png` and `tail-focus.json` show the unchanged discrepancy.
The largest selected first-tail error is at 200 ms, about −54.71 dBFS reference
versus −58.14 dBFS native. Retained state traces place the first fall at
43.580 ms reference / 48.820 ms native. Reference restores at 84.039 ms;
native misses that restore and waits for the next pulse near 502 ms.
The next sound work therefore prioritizes **initial scan/read/retrigger history
and the first tail**, not EQ compensation or an arbitrary phase offset.

## Reproduction and limits

HEAD remains `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86` on
`codex/original-224-sound-accuracy`. Reflexion is clean at
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`; inputs are the complete hash-validated
original XL v8.21 set and unchanged v5 bank, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Direct builds use Apple clang 21 / arm64, O3 C++20, `-ffp-contract=off` and
retained assertions. Private source/log/binary snapshots, exact commands,
results, plot inputs and hashes are under
`build/validation/xl-parameter-compiler-frame-20261006/checkpoint.json`.

```sh
/usr/bin/c++ -O3 -std=c++20 -ffp-contract=off -Ibuild/deps/reflexion \
  build/validation/xl-parameter-compiler-frame-20261006/frame-check.cpp \
  -o build/validation/xl-parameter-compiler-frame-20261006/frame-check
build/validation/xl-parameter-compiler-frame-20261006/frame-check ROM_DIRECTORY PREPARED_BANK
build/validation/xl-parameter-compiler-frame-20261006/frame-check ROM_DIRECTORY PREPARED_BANK --active-tail
python3 build/validation/xl-parameter-compiler-frame-20261006/analyze_frame.py
```

The five production control/runtime files, queued pilot source and both older
CPU-measured trees retain their hashes. There are no new WAVs, cache/bank changes,
plugin identifier changes, installation, host/listening acceptance or CPU campaign.
The standalone frame is outside primary sound processing; the earlier CPU result
does not measure a future integration. At this block's end, cleanup retains the
accepted and meaningful failing evidence; intermediate executables were replaced
in place and no further confirmed-disposable artifacts remain.
