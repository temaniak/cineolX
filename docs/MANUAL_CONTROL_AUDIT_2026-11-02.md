# 224 / 224XL manual audit: Chorus and hidden controls

Date: November 2, 2026. No production DSP, plugin parameters, banks, installed
bundles or dependency checkouts were changed in this audit.

Subsequent implementation: [original hidden-control fix](HIDDEN_CONTROLS_FIX_2026-11-02.md).
This audit retains its pre-implementation findings and evidence boundaries.

## Conclusion and evidence boundaries

The previous investigation established real ROM behavior, but it did not finish
the control contract. In particular, zero interpolation destinations are not a
sufficient reason to hide every effect of a named control. The Rich-program
Chorus discrepancy remains open, and original-224 hidden settings have reset
and recall rules beyond their numeric adjustment laws.

Four evidence types must remain distinct: manufacturer documentation, physical
operations in an emulated ROM, current native behavior, and sound acceptance.
An emulator agrees with itself when its editor and its instrumented engine are
compared; that agreement alone cannot resolve a manual-versus-machine mismatch.

Baseline: Cineol `f545a0b412de6b72de7617b1e4aa755f10fd2343`, with the existing
uncommitted GitHub fixes preserved. Reflexion remains pinned to
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Supported test fixtures remain
original 224 v4.4 and XL v8.21, not interchangeable firmware versions.

## Manual sources and scope

- Original 224: manufacturer Owner's Manual, section 2.3, *Advanced Features
  of Version 3 (and higher)*. The complete supplied page marked 2.12 was
  visually reviewed again. The [public transcription](https://manualzz.com/doc/23952139/lexicon-224-reverberation-system-owner-s-manual)
  provides surrounding context. This edition is not a complete v4.4-specific
  specification; Shift+6 is absent from the examined section.
- X/XL: [manufacturer Owner's Manual scan](https://therackofages.com/Lexicon/224x%20digital%20reverborator/Lexicon-224X-owner-manual.pdf),
  retained from the earlier help audit. It has a **224X** cover and a **224XL
  errata** sheet, part 070-04902. Do not describe it as an exact v8.21 firmware
  specification. The manufacturer download page was inaccessible during this
  audit; no second independently inspected official PDF is claimed.

Relevant X/XL printed pages and one-based PDF pages:

| Subject | Printed page | PDF page |
| --- | --- | --- |
| Chorus description | 3.10-3.11 | 25-26 |
| Mode Enhancement / Optimization toggles | 3.12 | 27 |
| XL errata | Unnumbered | 28 |
| Rich Plate, table 4.15 | 4.22 | 50 |
| Plate/Chorus exception | 4.30 | 58 |
| Rich Split, table 4.23 and description | 4.34-4.35 | 62-63 |

PDF SHA-256:
`6190f1dd54cb0ccfb197beae9a9170242a0557f854752280d9dc93a4931a1ff8`.
The PDF is scanned; empty text extraction was not used as evidence of absence.
Relevant full pages were rendered and visually inspected.

## XL: documented controls versus the observed machine

The manual links Chorus to randomized delays and Mode Enhancement. Table 4.15
lists Rich Plate Chorus at page 2/slider 3, range 00-97; table 4.23 lists shared
Rich Split L/R Chorus at page 3/slider 3, range 00-99. Its explicit
Plate/Chorus exception applies to the **Plate section of that different
program**, while its Chorus section remains active. The errata correct Size
limits, not Rich-program Chorus availability. See the scan pages above.

The retained v8.21 investigation instead observes zero interpolation targets
and modulation DSP writes for Rich Plate and Rich Split. It covers factory and
endpoint Size requests, eight Rich Plate variations and mode combinations;
Concert Hall supplies a working positive control. See
[the ROM/audio report](REFLEXION_CHORUS_HIDDEN_CONTROLS_2026-10-26.md).

That is a **remaining discrepancy**, not proof that the manual declares Chorus
inactive in the rich algorithms. Possible explanations still include document/
firmware differences or a reference/preparation issue. This audit does not
establish which explanation is correct, and does not establish a hardware bug.
Do not add arbitrary pitch modulation to make the UI agree with a table.

There is a separate, established native gap: changing Chorus changes the
reference's level-controller cadence even with zero interpolation targets.
Current native dynamics use captured fixed fast/slow rates. Reference Dynamic/
Optimization tails can consequently respond to Chorus while native tails do
not. The previous report's selected-window measurements are diagnostics, not
listening acceptance or proof of a particular RT60 error.

The access rule is too narrow in three current source locations:

- `desktop/bank.hpp::ProgramData::control_active`: hides Chorus when the
  modulation descriptor count is zero.
- `plugin/Editor.cpp::refresh`: uses the same descriptor-count test for the
  Mode Enhancement label and button availability.
- `plugin/ParameterHelp.hpp::mode`: reports Mode Enhancement unavailable on
  the same basis.

Thus restoring just the fader would leave its mode access and help inconsistent.
The native interpolation law in `desktop/modulation.hpp::set_chorus` also
combines two behaviors: its lower half changes the divider, and its upper
half changes the fractional step. Describing every position as only a speed
change is incomplete. A minimum Chorus position is not a modulation-off switch.

The observed generic Reflexion catalog has 32 Chorus positions, ending at 97
for both rich programs. Preserve the measured scale rather than manufacturing
an extra position solely to reach the Rich Split table's printed 99.

## Original 224: adjustment, reset and recall

The examined manual gives Rate a 1-16 scale and a usual value of 2. Lower
Optimization settings cause stronger alteration. It describes same-program
retention, register recall and restoration through Mode Enhancement switching.
Its usual plate Optimization value is 7. These statements come from section
2.3 of the original manual linked above.

Existing physical v4.4 evidence adds details that the examined text does not
fully specify:

- Shift+6 edits modulation step: `step = 4 * setting`.
- Shift+7 settings 2-16 select `divider = setting - 1`, resetting step to 4;
  setting 1 selects divider 1 / step 8.
- Rate and modulation Depth have last-write behavior. Rate 2 after Depth 8
  produces step 4; Depth 8 after Rate 2 produces step 32. They are not two
  persistent multiplicative factors.
- Shift+8 edits the Optimization response-period byte. The observed encoded
  level-drop comparison stays 22; it is not a threshold measured in dB.
- Normal reverb Depth, Shift-Depth Diffusion and hidden modulation Depth are
  different controls. Original 224 does not have XL's Dynamic Decay switch.

### New physical-operation verification

The manual prompted a narrow new test of **all six programs**, using the same
hash-validated v4.4 ROMs and pinned offline reference. No RAM patch sets the
controls. The probe physically performs Shift/Pre-delay edits, program and
toggle operations, and SET/CALL with register A. It records 78 snapshots.

| Operation | Observed result in every tested program |
| --- | --- |
| Set Rate 8, then modulation Depth 6, then Optimization 12 | Divider 7, step 24, Optimization 12 |
| Select the same program | All edited hidden values retained |
| Turn Mode Enhancement off | **All three hidden settings already reset to factory**, including Optimization |
| Edit hidden settings while Mod is off, then turn Mode Enhancement on | All hidden settings reset again |
| Re-edit, then turn Optimization off and on | Edited hidden settings retained |
| Store register A, select a different program, CALL A | Edited divider, step, Optimization and program/mode flags restored |

The distinction between the two toggles matters. A generic rule that resets
hidden values on either toggle would be wrong for this fixture. Equally, a
rule that merely gates modulation without resetting the hidden settings would
miss the Mod-key behavior. Re-enabling Mod is not the first reset boundary:
the values have reset at the preceding off transition. Editing them while Mod
is off and then enabling it proves a fresh reset in the other direction too.

Factory state in these fresh fixtures remains divider 1, step 4, Optimization
5 for all six programs. Chamber retains random hold 128; the others use 32.
The plate value 7 from the older text is not what these v4.4 fixtures load.
Do not silently change existing sessions' default sound to 7.

These tests cover register A in a running emulation, not nonvolatile survival
across power loss, every extended register, audio equivalence or listening.

## Consequences for implementation and validation

1. **Original hidden controls:** add runtime-owned effective divider, step
   and Optimization response values, plus host/UI/preset access. Preserve old
   defaults and append stable host IDs. Arithmetic, cycle-cost predictions and
   write scheduling must consume the same effective values.
2. **Original control events:** implement the measured Rate/Depth interaction,
   reset all hidden settings on Mod transitions, retain them on Optimization
   transitions and same-program selection, and serialize effective values for
   preset/session recall. Hardware register behavior is a reference for recall,
   not a requirement to reproduce its button choreography in a DAW.
3. **XL access and timing:** address Chorus, Mode Enhancement availability and
   their help together; account for observed cadence coupling before claiming
   functional parity. Preserve the unresolved rich-algorithm interpolation
   discrepancy explicitly.
4. **Audio acceptance:** use independent reference/native clocks and several
   levels, seeds and controller phases. Check running sound, stopped-input
   tails, fall/retrigger behavior and stereo balance; then prepare matched-level
   listening pairs. State-aligned transitions alone cannot accept the sound.

The November 2 investigation is a reporting/verification block. No DSP fix,
listening approval, new realtime CPU result, release or installation is claimed.

## Reproduction and retained evidence

New ignored evidence: `build/validation/manual-review-20261102/` contains
`retention.cpp`, executable, build log, `retention.csv`, stderr, command/ROM
provenance, `summary.json`, the assertion script `verify.py`, and inspected page renders. The harness reuses
helpers from `build/validation/original-hidden-20261026/probe.cpp` and links
the existing build-only offline import/core libraries:

```sh
c++ -std=c++20 -O2 -ffp-contract=off -I . -I build/deps/reflexion \
  build/validation/manual-review-20261102/retention.cpp \
  build/plugin/native-hall/libnative_224_import.a \
  build/plugin/native-hall/libnative_hall_core.a \
  -o build/validation/manual-review-20261102/retention
python3 build/validation/manual-review-20261102/run.py ORIGINAL_ROM_DIRECTORY
python3 build/validation/manual-review-20261102/verify.py
```

The final run exits 0. Postprocessing verifies all 78 rows and every table
assertion. The first compile lacked the offline ROM-recognition library and
did not produce an executable; the corrected link uses the existing libraries.
The inherited unused renamed probe entry point emits a return-value warning;
it is not called. Actual test entry points return explicit status.

Only superseded exploratory page renders and the empty extraction were removed
from this new evidence directory. Existing manuals, original screenshots,
ROMs, banks, audio comparisons and reports were preserved.
