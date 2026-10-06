# XL live parameter queue checkpoint

Date: October 6, 2026. This is an independent experimental desktop pilot,
outside the production runtime. Ordinary LF/MID and applicable STOP/STOP DLY
updates now retain the running controller instead of resetting it from an older
dynamics snapshot. The paired Hall / Hall LF-event fixture improves maximum
band RMS error from **25.3844% to 1.5975%** and active-envelope p95 from
**648.8907% to 18.7391%**. The remaining envelope error is open; this is not a
catalog, listening or host acceptance result.

## Change and control boundaries

The [preceding physical fader investigation](XL_PARAMETER_TRANSACTION_VALIDATION.md)
demonstrated a retention violation in the actual old pilot setter: changing LF
reset its scan clock from 2,047,996 to zero and replaced live held/divider state
with an older snapshot. This checkpoint addresses that specific update path.

The new standalone `native-hall/desktop/parameter_queue_xl.hpp` owns two fixed
48-byte logical snapshots: active and latest requested. A request cannot change
values used by an in-flight Slow/Fast/gradual/compiler operation. Later requests
replace the pending snapshot; requesting the active values cancels pending work.
Identical submissions do not start another transaction. Requests and processing
are serialized on the same audio thread; this is not an inter-thread queue.

In the private `candidate/native-hall/desktop/concert48.hpp`, the retention path
requires the same control profile, coefficient/offset base and optimization
setting. Changed cells are limited to LF/MID (0/1), the applicable STOP pair
(6/7 for shared-stop profiles, otherwise 12/13), and STOP DLY (45). Profiles
without reverb dynamics use the old path. Program/base changes, other cells,
Size/Chorus and mode/key changes still use the legacy reset path.

The private `coupled_pilot.hpp` commits the latest coherent snapshot at the end
of its ten-cell reconciliation pass after the preceding nine monitor passes.
It asserts that prior compiler writes have drained, runs the derived main decay
compiler against **live** dynamics/cache state, schedules individual READY-bound
coefficient writes, accounts for 1,957 record-copy work states, and finishes with
the derived 40-state deferred-refresh suffix before entering Slow. Clock, held
detectors and running state are retained rather than reinitialized.

This assembles a semantic coherent transaction core. It does **not** implement
the full physical per-cell receive/calibration/A791 compiler wrapper, remaining
compiler slots, formatting or legal instruction/interrupt boundaries. The
physical fader pickup law remains separately validated; the host snapshot queue
does not pretend that an unchanged submission is a physical rejected pickup.
Initial bind/recall still takes the existing startup path. Mod-on is excluded.

## Integration checks

`queue_check_template.cpp` compiles separately against the frozen old pilot and
the new source tree. It uses the prepared bank but no ROM execution, injected
reference state or observed controller schedule. At 48 kHz, each reverb runs six
scenarios: identical resubmission, change-and-cancel, coalesced LF/MID changes,
the same coalescing during an actual gradual transition, STOP pair, and STOP DLY.
The normal prelude is one second of silence. The busy prelude uses seed-17 noise
at amplitude 0.12 for 4,800 samples, followed by silence until a gradual stage;
every scenario then processes 36,000 samples after its requests.

All **19 reverbs / 114 scenarios** pass, with **209 setter calls**, **76 committed
snapshots**, and gradual-stage coverage on all 19. Each request preserves the
25-field scan/IRQ observation, 26 dynamics values and coefficient/offset arrays
immediately. Output equals a separately running untouched twin until commit;
canceled/no-op requests remain equal throughout. Pending work drains, the full
active snapshot equals the final request, outputs are finite with magnitude
below 4, and tracked processing/setter allocations and releases are zero.

The same final harness against the frozen old Hall / Hall source fails as
expected (exit 1, **34,188 property errors**); the new all-19 run exits zero.
The error count includes repeated sample-level consequences of the reset and
is not a count of independent defects. The three non-reverb effects are outside
these dynamics integration checks.

## Independent sound evidence

Both renders use the unchanged v5 bank, original XL v8.21 reference and native
clocks running independently. No reference dynamics, event-entry clocks or
coefficient replay enter native processing. Common settings are Hall / Hall
(native program 17, physical bank 5/program 1), mode 4 (Dynamic on, Mod/Opt off),
music seed 17, amplitude 0.08, one-second warmup, 12-second capture, 48 kHz,
256-frame blocks, analog on, 0 dB input, wet A/C outputs, STOP 18/18 and DLY 10.
The reference performs the retained physical boot/program/fader/toggle setup
and final 500 ms settle. Native/reference processing and setters allocate and
release nothing.

The **canonical fixture has no mid-render request**. Input, reference and native
WAVs remain byte-identical to the retained corrected scan fixture. Maximum band
error stays **1.6506%**, envelope median **0.6582%**, p95 **18.0204%**, and maximum
**32.6359%**. This confirms no sound regression in that recipe and no startup fix.

The **paired LF-event fixture is a separate recipe**: the reference physically
selects page 1 before the final 500 ms settle. At frame 28,800 (0.6 s), it receives
the genuine fader-0 value 160 message while native receives LF 160. The render
loop splits its block at that frame. Final reference logical LF is verified as
160. Page setup changes the initial physical phase, so these results must not
be compared directly to the canonical 18.0204% as a before/after improvement.

Within the event pair, input/reference WAV hashes match exactly. Native WAVs
also match for every frame before the event, then differ after the update.

| Metric, same event recipe | Frozen old pilot | Queued live update |
| --- | ---: | ---: |
| Maximum absolute band RMS error | 25.3844% | 1.5975% |
| Bands above provisional 5% criterion | 2 / 8 | 0 / 8 |
| Active 50 ms envelope windows | 39 | 39 |
| Median absolute envelope error | 25.2394% | 0.7712% |
| p95 absolute envelope error | 648.8907% | 18.7391% |
| Maximum absolute envelope error | 1,288.8229% | 32.4426% |
| Final-second native RMS | −98.9310 dBFS | −95.5321 dBFS |
| Final-second reference RMS | −95.6701 dBFS | −95.6701 dBFS |

Envelope percentages are amplitude errors relative to the reference window;
they can exceed 100%. Both variants use the same −82.4491 dBFS activity threshold
and accepted window set. There are **zero usable T20 bands** for this gated
fixture; unavailable decay is not zero error or a pass. The event shows a clear
correction of the update reset, while the remaining approximately 18.7% p95
requires startup/transaction/read chronology work.

## Memory, CPU and scope

Compiled `sizeof(Native48<graph>)` probes show **120 additional bytes per engine**
and **2,640 bytes across all 22 engine objects**. This measures C++ object storage,
not the full Runtime heap or a shipping memory layout. Unchanged private global
trace buffers are separate. The queue uses fixed storage and bounded operations.

The new queued candidate is **not CPU-measured**. Both frozen source trees in
the [earlier CPU report](XL_COUPLED_CPU_VALIDATION.md) retain all 59/76 hashes,
but that result does not cover this new candidate. Following the user's batching
direction, CPU remains deferred until the assembled transaction/startup block
is ready, including control edges and applicable overlap coverage.

Production `dynamics.hpp`, `concert.hpp`, `concert48.hpp`, `runtime.hpp` and
`bank.hpp` remain byte-identical to the frozen actual audio baseline. The public
queue header is not included by production; all integrated source changes are
under ignored `build/validation/`. Original 224 processing, plugin/parameter
identifiers, normal caches and bank format are unchanged by this checkpoint.
No installed plugin, DAW test, listening test or Daisy realtime claim is made.

## Reproduction and next step

HEAD: `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`, branch
`codex/original-224-sound-accuracy`. Reflexion is clean at
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`; libDaisy remains pinned at
`f044cdc312455f174b01bef98d9e97598d94d3bc`. Reference inputs are the complete
previously hash-validated original XL v8.21 set. Bank version 5 SHA-256:
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Compilers are Apple clang 21 / arm64, C++20, O3, `-ffp-contract=off`, assertions
retained. Use only the build-export Reflexion headers, leaving dependencies clean.

Private evidence root: `build/validation/xl-live-parameters-20261006/`.
`commands.json` records compilation/render/analysis commands; `results.json`
records the checked totals, metrics, WAV identities and memory. `checkpoint.json`
hashes candidate and independently frozen old source, adapters, binaries, logs
and results. Preliminary four-scenario logs are historical; acceptance uses
`new-all-final.log` and `old-hall-final.log`. ROMs, banks and captures stay ignored.

```sh
# Run from repository root. R is the private evidence root above.
"$R/queue-check-old" "$BANK" 17 # expected exit 1
"$R/queue-check-new" "$BANK"   # expected exit 0
"$R/live-parameter-renderer" "$ROM" "$BANK" "$R/p17-m4-canonical" \
  17 4 music 17 .08 1000 12 256 1 --gate-controls
"$R/lf-event-renderer-old" "$ROM" "$BANK" "$R/p17-lf-event-old" \
  17 4 music 17 .08 1000 12 256 1 --gate-controls --lf-event
"$R/lf-event-renderer-new" "$ROM" "$BANK" "$R/p17-lf-event-new" \
  17 4 music 17 .08 1000 12 256 1 --gate-controls --lf-event
python3 "$R/analyze_live_parameters.py"
```

Next derive the physical parameter compiler wrapper/calibration and its actual
reconciliation work against live state. Preserve this event pair as the update
regression fixture. Resolve initial program/key/scan history and legal IRQ/read
boundaries using the retained startup trace, then repeat the canonical sound
case and representative event variants. Full parameter/key/recall and Mod-on
behavior, the complete sound matrix and host/listening acceptance remain open.
