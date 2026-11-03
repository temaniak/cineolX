# Original 224 hidden controls compared with Reflexion

Date: October 26, 2026. This investigation changes no production code.

November 2 follow-up: the [manual audit](MANUAL_CONTROL_AUDIT_2026-11-02.md)
extends this evidence with physical Mod/Optimization toggle and register-A
tests. The storage/recall limitations below describe the October 26 block only.

## Findings

All three reported physical key combinations work in the supported original
224 v4.4 ROM running on the pinned Reflexion machine. They are not separate
controls in Reflexion's generic JUCE editor either. Cineol captures their factory
effects during bank preparation, but has no runtime/host parameters for editing
them. The existing reverb `depth` parameter is a different, ordinary control.

| Physical operation, moving Pre-delay while held | Verified settings | ROM result |
| --- | --- | --- |
| Shift + program 6 | 1–16 | Fractional modulation step becomes 4 times the setting. |
| Shift + program 7 | 1–16 | Settings 2–16 set the update divider to setting minus 1 and reset the step to 4. Setting 1 uses divider 1 and step 8. Higher settings slow the modulation updates. |
| Shift + program 8 | 1–16 | Decay Optimization response/amount byte becomes the setting. It is used as a diffusion-controller period, rather than an input level threshold. |

These are unitless settings. The reference's hidden display retains a SEC unit
LED from its normal display; that does not establish seconds or hertz.

Two corrections to earlier assumptions are important:

1. Shift+6 is supported even though it is absent from the attached manual page.
2. Rate and modulation Depth are not independent persistent controls in this
   firmware. A Rate adjustment overwrites the step established by a preceding
   Depth adjustment. A later Depth adjustment directly overwrites that step.

For example, Depth 8 followed by Rate 2 ends at divider 1 / step 4. Rate 2
followed by Depth 8 ends at divider 1 / step 32. Rate 1 followed by Depth 8 also
ends at divider 1 / step 32; it does not retain a separate times-two multiplier.
Any native implementation claiming the original control behavior must account
for this last-write rule. Treating the two settings as independent multiplicative
knobs would be an intentional extension of the measured firmware behavior.

## Factory state and recall

All six supported programs were selected through CALL and actual program-button
operations. The physical probe and the existing prepared original-224 bank agree:

| Program | Mod divider | Mod step | Random hold | Decay Optimization amount |
| --- | --- | --- | --- | --- |
| Small Concert Hall B | 1 | 4 | 32 | 5 |
| Vocal Plate | 1 | 4 | 32 | 5 |
| Large Concert Hall B | 1 | 4 | 32 | 5 |
| Acoustic Chamber | 1 | 4 | 128 | 5 |
| Percussion Plate A | 1 | 4 | 32 | 5 |
| Small Concert Hall A | 1 | 4 | 32 | 5 |

Divider 1 / step 4 corresponds to the factory Rate setting 2 and modulation
Depth setting 1 under the measured adjustment law. The attached manual page
describes a usual Optimization setting of 5 for halls and 7 for plates. The
supported v4.4 fixture actually loads 5 for all six programs; the native bank
also contains 5 for all six. The manual's plate recommendation is not evidence
for silently changing the current factory sound to 7.

After changing any hidden setting to 9, releasing the keys and pressing the
same program preserves it. Selecting a different program and returning restores
the table above. The normal Pre-delay value stays unchanged throughout the
hidden adjustments. Program and Mod/Optimization enable bits also stay intact.
Register storage/recall was not tested in this block.

## What Threshold means here

Physical Shift+8 changes the `decay_amount` value captured from RAM 3F71. It
does not change the fixed encoded level-drop comparison used to detect a
falling signal. The native implementation's comparison is 22 in its internal
level units, not 22 dB. The ROM has the corresponding immediate comparison at
07B9. The physical controls and runtime comparison are recorded separately in
the evidence directory.

The control changes how the optimization diffusion process evolves. Depending
on controller state and decay settings, other period values can temporarily
override it; therefore it is not a simple maximum diffusion depth or a direct
amplitude threshold. A faithful user-facing name would be Decay Optimization
Response or Amount, with the original 1–16 scale and an explanation of its
effect. A Threshold control that changes the encoded level-drop constant would
be an additional feature, not this verified Shift+8 control.

## Reflexion's interface boundary

The inspected dependency is exactly
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, without dependency modifications.

- `source/operator/panel224_operator.hpp` exposes the six normal pots and one
  Shift-page Diffusion pot through its ordinary page/slider interface.
- `plugin/ram_reader.hpp` reads those six normal values and Shift Diffusion,
  plus the Mod Enhancement/Decay Optimization toggles. It does not read these
  three hidden values as generic sliders.
- The v4.4 four-chip catalog key is `90b0c38075310544`; its named pages likewise
  contain the normal controls and Shift Diffusion, without hidden Rate/Depth/
  Optimization sliders.
- `editor/ui_interface.hpp` offers program/variation selection, catalog slider
  moves and named toggles. The generic JUCE editor has no raw simultaneous
  button-hold command to expose these hidden chords directly.
- The underlying `Engine::button` / `Engine::pot` and `Panel224Operator::hold`
  APIs can deliver physical inputs to the complete firmware. That is the path
  used by this probe; the firmware recognizes the combinations itself.

Thus Reflexion's emulation can perform these operations, while its generic
plugin UI has a similar discoverability/access limitation. Cineol's limitation
is deeper: it runs a native algorithm using immutable captured defaults, so an
editor-only addition would not yet modify its effective controller parameters.

## Current Cineol path and implementation implications

`native-hall/import/bank_import.cpp` captures `modulation_period`,
`modulation_hold`, `modulation_step`, `modulation_mask` and `decay_amount` into
each `ProgramProfile`. `Hall::update_modulation()` reads them from that immutable
profile. The desktop scan/level controller reads `decay_amount` the same way.
The public `Controls` structure and APVTS layout provide only the enable
switches; no independent runtime overrides exist.

The native arithmetic already handles the measured non-default modulation
settings. For diagnosis only, a copied in-memory bank profile was given the
period/step observed after physical adjustments. The native modulation state
then matched every checked Reflexion transition. The shipped/default profile
does not follow changed physical settings because it retains factory values.

This was a state-aligned diagnostic: each native comparator starts from the
reference state at a procedure boundary. It establishes the parameterized
transition law, not free-running timing, audio equality or listening acceptance.
It does not mutate the application bank/cache or introduce firmware execution
into native audio.

A future correction should add runtime-owned effective controller values and
append stable host parameter IDs, preserving factory defaults for old sessions.
Both the modulation arithmetic and its write/cycle timing helpers must read
the same effective settings. Program reload/retention and the Rate/Depth
last-write behavior need explicit handling. A bank-version change is not
required merely to add runtime overrides, but preset/session storage and
defaults need regression coverage. No implementation or sound correction is
included in this investigation.

## Evidence and reproduction

Private evidence is retained under
`build/validation/original-hidden-20261026/`:

- `panel-full.csv`: 396 snapshots including 324 actual pot moves, all six
  programs, three key combinations and every displayed setting 1–16.
- `interactions.csv` and `interactions-depth-last.csv`: 108 order-dependent
  fixtures, 52,232 observed procedure entries and 52,124 compared transitions.
  Adapted-profile mismatches: zero. The stock factory-profile comparator
  disagrees after non-default physical edits, as expected.
- `bank-defaults.csv`: the original prepared bank's actual captured defaults.
- `threshold.csv`: physical Optimization endpoint settings compared against
  execution of the fixed level-drop comparison.
- Probe sources, compiler logs, exact commands, chip hashes and portable
  summaries. Full private filesystem locations occur only in ignored command/
  provenance files.

The complete original-224 v4.4 ROM1–ROM5 set was read in place and recognized
by SHA-256 using the existing loader. The application bank SHA-256 remains
`15d14dcebbbafad35269e8159ddfedfb24ca9fdd203531b3d85cf094ce6e72ed`.
Inputs to the primary panel probes are physical buttons/pots only; they do not
seed hidden firmware variables through RAM writes.

From the repository root, with private paths supplied locally:

```sh
c++ -std=c++20 -O2 -ffp-contract=off -I . -I build/deps/reflexion \
  build/validation/original-hidden-20261026/probe.cpp \
  build/plugin/native-hall/libnative_224_import.a \
  build/plugin/native-hall/libnative_hall_core.a \
  -o build/validation/original-hidden-20261026/probe_full
build/validation/original-hidden-20261026/probe_full ROM_DIRECTORY

c++ -std=c++20 -O2 -ffp-contract=off -I . -I build/deps/reflexion \
  build/validation/original-hidden-20261026/interactions.cpp \
  build/plugin/native-hall/libnative_224_import.a \
  build/plugin/native-hall/libnative_hall_core.a \
  -o build/validation/original-hidden-20261026/interactions
build/validation/original-hidden-20261026/interactions ROM_DIRECTORY ORIGINAL_BANK
build/validation/original-hidden-20261026/interactions ROM_DIRECTORY ORIGINAL_BANK --depth-last
build/validation/original-hidden-20261026/interactions ROM_DIRECTORY ORIGINAL_BANK --threshold
```

No independent native/reference musical rendering, human listening, physical
Lexicon comparison, preset-register storage or realtime CPU measurement was
performed. No production sources, dependencies, ROMs or caches were changed.
The small sources/logs and unique diagnostic evidence are retained; no existing
assets or older validation were deleted.
