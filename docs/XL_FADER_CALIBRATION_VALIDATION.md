# XL ordinary numeric fader calibration

October 6, 2026. This follows the [live parameter queue](XL_LIVE_PARAMETER_VALIDATION.md)
and derives a missing physical-input boundary before pickup/compilation.
`fader_calibration_xl.hpp` is a standalone native helper; neither production
audio nor the private queued sound pilot includes it yet. No new sound correction
or CPU result is claimed.

## Native law and scope

The helper predicts ordinary numeric page/slot dispatch, minimum and maximum
clamping, fixed scaled limits, soft pickup, returned acceptance and work states.
Minimum-limited slots raise physical values below 8 to 8 before the upper-limit
and pickup check. A fixed scaled limit uses the descriptor's low five bits times
eight. Other numeric slots clamp directly to their descriptor maximum. Pickup
still accepts live/equal/crossing values and rejects a value on its original side.

The law also predicts the limit scratch and physical scratch values: a direct
maximum clamp can leave the physical scratch unchanged, whereas scaled-limit
and minimum branches update it. These fields and the logical/pickup state are
compared at the physical worker's return. The enclosing transaction still
requests deferred refresh and retains the running controller state.

Inputs are local entry context: page type, slot, encoded descriptor maximum,
physical value, logical value and pickup state. Descriptor metadata and initial
context are observed by the offline oracle. Returned values/work are native
predictions, with no observed duration input or firmware execution in the helper.
The maximum argument is the **encoded descriptor**, not the already resolved
logical maximum in `ProgramData::Page`. Preparing this metadata for a future
physical-event runtime remains separate; host logical parameter ranges are not
changed here.

Size pairs (type 5), effect-specific mapping (type 13), and variable pre-delay
(scaled-limit descriptor 255) have separate workers and are explicitly excluded
by `ordinary_fader_calibration`. The test never invokes the helper for them.
The implementation uses fixed scalar state, bounded arithmetic, and no heap,
I/O, locks or waits.

## Physical validation

The existing `cineol_xl_fader_transaction_check` now observes the calibration
entry/return as well as pickup, deferred refresh and enclosing state retention.
Its new `--numeric-pages` fixture sweeps all active numeric-page slots with the
stored physical value and 2/8/64/254. Size pages are excluded from this fixture;
unsupported variable/effect branches can still execute in the reference but
are counted separately from native calibration coverage. Messages use the
physical LARC interface, without RAM seeds or the operator take-over helper.

Each program starts from an independent reference boot in this expanded fixture.
The initial shared-machine attempt failed to select the next program after the
Inverse Room sweep. Its partial logs are retained and are not accepted as a
complete campaign. Isolation prevents the preceding sweep's operator/selection
state from contaminating the next program; it is an oracle setup repair.

Both final CMake Release campaigns cover **all 22 programs**:

| Campaign | Physical transactions / main returns | Predicted calibrations | Excluded calibrations | Accepted / rejected pickups |
| --- | ---: | ---: | ---: | ---: |
| Factory, isolated numeric pages | 2,029 | 1,907 | 122 | 1,847 / 182 |
| Active-tail, isolated numeric pages | 2,041 | 1,904 | 137 | 1,873 / 168 |
| Total | **4,070** | **3,811** | **259** | **3,720 / 350** |

The active campaign prepares Dynamic on, Mod/Opt off, applicable STOP 18/18 and
DLY 10 for all 19 reverbs, physically selects page 1, then renders 12,032 frames
at 48 kHz in 64-frame blocks. Its seed-17 noise amplitude is 0.12 for the first
4,800 frames, followed by silence. The three non-reverb effects use the ordinary
fixture. This observes 927 transaction entries with the stop bit set across the
19 reverbs; it is not a new audio-file comparison.

All 3,811 supported calibrations have **zero work/state errors**. All 4,070
transactions have zero pickup/suffix/retention errors and return to the current
main reconciliation pass; there are no nested Slow/Fast calls. Tracked native
helper allocations/releases are zero. No IRQ falls inside these timed workers,
so legal interrupted instruction boundaries remain unvalidated.

Physically covered page types are 1/2/3/4/7/8/9/10/11/12. Per-program coverage,
including excluded branches, is retained in `results.json`. No unsupported branch
is counted as a passing calibration. A frozen negative timing guard deliberately
charges a register comparison one excess state: the same Hall / Hall recipe
fails with 22 work errors and zero state errors (expected exit 1). The corrected
Hall check passes. This guard is a labelled counterfactual, not an old production
implementation. The initial derivation had the same one-state mistake; its
failing log is preserved.

## Reproduction, retained sound and next step

HEAD remains `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86` on
`codex/original-224-sound-accuracy`; Reflexion remains clean at
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Inputs are the complete hash-validated
original XL v8.21 firmware set and unchanged v5 bank, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The final target uses Apple clang 21 / arm64, C++20, CMake Release O3,
`-ffp-contract=off` and `NDEBUG`; explicit oracle `require` checks remain active.
The independent negative guard uses direct O3 compilation with assertions.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_fader_transaction_check -j2
build/xl-sound-validation/native-hall/cineol_xl_fader_transaction_check ROM_DIRECTORY PREPARED_BANK --numeric-pages
build/xl-sound-validation/native-hall/cineol_xl_fader_transaction_check ROM_DIRECTORY PREPARED_BANK --active-tail --numeric-pages
python3 build/validation/xl-fader-calibration-20261006/analyze_calibration.py
```

Private commands, accepted source/binary snapshots, negative variant, logs,
results and hashes are in
`build/validation/xl-fader-calibration-20261006/checkpoint.json`. ROMs, banks and
firmware-derived listings stay ignored. The five production control/runtime
files, the queued pilot sources and both previously CPU-measured source trees
retain their hashes. Plugin IDs, host controls, bank format and original 224
processing are unchanged.

There are **zero new WAV files** and no CPU campaign. Retained Hall canonical
envelope p95 remains 18.0204%; the separate LF-event pair remains 18.7391%.
The new cleanup rule in `AGENTS.md` is applied at this block's end: the obsolete
intermediate direct executable is removed after preserving final accepted and
negative evidence, with the removal recorded in the private `cleanup.json`.

Next derive the complete enclosing parameter compiler wrapper and copy/refresh
chronology, then join the physical boundary to the retained live-state pilot.
Variable pre-delay, Size/effect mapping, initial recall/key/scan history and legal
IRQ/read boundaries remain open. Subsequent independent sound comparisons must
reuse the canonical and LF-event recipes; this local law alone does not establish
sound, listening, host or realtime acceptance. CPU remains batched until the
assembled transaction/startup unit is ready.
