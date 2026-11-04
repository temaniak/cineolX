# 224 / 224XL float port: candidate families and implementation plan

Date: November 4, 2026. Planning only; no DSP, plugin, bank, dependency or
installed-bundle changes in this block.

## Direction and listening decision

The user compared the corrected float Hall by ear and found it brighter than
the original, but considers that difference acceptable for now. The original
output/converter filtering is a plausible contributor, not a newly isolated
measurement. Keep direct float input/output boundaries and do not add EQ or
circuit filters to force a match. Listening decides acceptance; sample identity
and 100% sonic matching are not targets.

Use the corrected original-224 Large Concert Hall B as the architectural
example: direct 48 kHz float audio, original network coefficients, physical
delay times, fifth-order fractional reads, and separate logical controls.
This is separate from the shipped fixed-point **Clean** mode, which preserves
original audio clocks and converter filtering. Do not replace either main
engine or rename Clean as float.

## What can actually share a base

The source audit finds six original-224 programs mapped to four network IDs,
and 22 XL programs mapped to 20 distinct ordered row-operation signatures.
These counts describe the current implementation, not a proof that 24
independent handwritten float kernels are necessary. Algebraic collapse may
reveal more reusable sections. Conversely, an identical row signature does
not imply identical delay-memory connections, control profiles or sound.

The following four pairs are confirmed candidates for one parameterized
float family. Use each program's own prepared data, rather than copying the
base's fader positions.

| Base candidate | Data-driven family member | Source evidence | Work still required |
| --- | --- | --- | --- |
| 224 Large Concert Hall B | 224 Small Concert Hall B | Both use network 0 | Existing float class fixes program index 2; parameterize preparation, delay reservation and control compilation, then validate Small Hall B. |
| 224 Percussion Plate A | 224 Vocal Plate | Both use network 1 | Port the plate topology once; select separate coefficient, delay, modulation and control tables. |
| XL Concert Hall | XL Room | Both dispatch `concert_network.inc` | One float graph, separate XL profile/layout/control metadata. |
| XL Plate | XL Small Plate | Both dispatch `plate_network.inc` | One float graph, separate XL profile/layout/control metadata. |

Here, **data-driven** includes coefficient tables, delay lengths/connections,
control curves/ranges, Size layouts, modulation descriptors and rates,
dynamics data and routing. It does not mean the other program can be obtained
by selecting different visible faders on the base program. All four pairs need
member-specific validation before claiming family coverage.

Original-224 Small Concert Hall A and Acoustic Chamber have their own networks
(3 and 2 respectively). They complete the original-224 catalog but are not
currently proven parents of other catalog programs. XL's Concert Hall is not
assumed to be a parameter preset of the original-224 float Hall.

## First representatives and order

The first six representative bases are Large Concert Hall B (already ported),
Percussion Plate A, XL Concert Hall, XL Plate, Small Concert Hall A and
Acoustic Chamber. They cover ten catalog programs after the four proven
family members are added: all six original-224 programs and four XL programs.
This is the first milestone, not a claim that all XL reverbs derive from them.

| Step | Deliverable | Listening boundary |
| --- | --- | --- |
| 0 | Freeze the corrected Hall and current original references; keep the accepted brighter float direction. Prepare isolated float development from the current source checkpoint. | Reuse the existing Hall listening pair; no new tuning campaign. |
| 1 | Generalize Hall B profile selection and add Small Concert Hall B. | Small/Large timing, stereo behavior and stopped-input tails. |
| 2 | Port **224 Percussion Plate A**; add Vocal Plate using its own profile. | Plate density/attack and vocal-tail character. This is the next new topology. |
| 3 | Port **XL Concert Hall**; add Room with its own XL data. | Establish the XL control/Size/modulation bridge before adding further XL graphs. |
| 4 | Port **XL Plate**; add Small Plate. | Check contrasting Size, density, stereo and long-tail behavior. |
| 5 | Port **224 Small Concert Hall A** and **Acoustic Chamber** separately. | Complete the six-program original-224 float catalog. |
| 6 | Add XL Bright Hall as a topology variant, then Dark Hall, Small Room and Chamber as individually checked graphs. | Reuse blocks where justified; do not assume names establish equivalence. |
| 7 | Add the remaining Rich/Inverse/CD graphs, retaining their own signal and control definitions. | One accepted representative at a time, including dynamics-dependent tails. |
| 8 | Port Chorus & Echo, Resonant Chords and Multiband Delay. | Echo timing, voice/pan routing and resonances need effect-specific checks. |
| 9 | Add all five split programs after the component blocks and independent control groups are available. | Check both paths, cross-coupling, routing and independent controls. |

Every step ends in a concrete A/B candidate. Do not start an open-ended
controller-equivalence investigation before listening to a usable candidate.
If a later program only needs a different data profile, add it directly;
if its collapsed graph differs, retain that explicit variant.

## Complete XL disposition

All 22 current XL identities are assigned below. An own graph may reuse
float delays, allpass sections, filters and mixers from earlier ports; it is
not necessarily a wholly independent implementation.

| XL base / variant | Related program | Disposition |
| --- | --- | --- |
| Concert Hall | Room | Confirmed common ordered row graph; first XL base. |
| Plate | Small Plate | Confirmed common ordered row graph; second XL base. |
| Bright Hall | Concert Hall | Closely related source, but 10 of 105 row records differ; evaluate the altered sections during float collapse. Not accepted as data-only. |
| Dark Hall | Other halls | Own 107-row graph and distinct feedback behavior. Carry the existing Stable/Original variation policy into the float design explicitly. |
| Small Room | Room | Own graph despite identical 105-row cycle length. |
| Chamber | Acoustic/Rich chambers | Own 100-row XL graph; no cross-model data-only equivalence established. |
| Rich Chamber | Dark Chamber, Inverse Room | Useful shared-block investigation anchor; each has a distinct 108-row signature. Preserve separate topology/control variants until proved otherwise. |
| CD Plate A | CD Plate B | Distinct 100/102-row graphs; not just different decay or size values. |
| Rich Plate | Other Rich programs | Own 109-row graph and Rich-specific control behavior. |
| Chorus & Echo | Later split chorus sections | Own effect; do not equate full effects with their reduced split sections. |
| Resonant Chords | Other effects | Own voice/resonance graph. |
| Multiband Delay | Other effects | Own band/delay/routing graph. |
| Hall / Hall | Hall family | Own reduced split graph; not established as two full Concert Hall instances. |
| Plate / Plate | Plate family | Own reduced split graph and two logical control groups. |
| Plate / Hall | Hall and plate blocks | Own split graph; verify actual tap and return wiring. |
| Plate / Chorus | Plate and chorus blocks | Own split graph, modulation and output wiring. |
| Rich Split | Rich family | Own split graph and Rich-specific control timing. |

The XL audit compares complete ordered row operations, including register,
transfer, zero, output, shift and control-write flags. Different row programs
can still collapse to related float equations, so this is a conservative
reuse classification. No historical lineage is inferred from source identity.

## Implementation rules carried forward

- One shared collection of allocation-free DSP primitives; family graphs
  connect them explicitly. A profile supplies numbers and control metadata.
  Keep ROM execution and arithmetic opcode interpretation out of audio.
- Preserve the source's coefficient units (`integer / 32` where applicable)
  and physical delay times, including short recursive reads. Reuse the
  corrected Hall interpolation approach; do not revive linear reads in every
  feedback pass or apply blanket allpass-complement normalization.
- Original-224 native delay units use 20,480 Hz. XL uses a program-specific
  rational native rate: `10,240,000 / (3 * row_count)` Hz. For an XL offset
  difference `d`, the 48 kHz delay is `d * 48,000 * 3 * row_count / 10,240,000`
  frames. A common 34.13 kHz conversion would be wrong for most XL graphs.
- Reuse the existing prepared banks and logical control laws. For XL, resolve
  Size/address layouts and predelay in native units before translating delay
  differences into seconds/48 kHz frames. Nominal audio rates and controller
  rates are separate quantities; avoid scaling controller cadence twice.
- Establish static graph/control operation first, then include the graph's
  modulation, Decay Optimization, Dynamic/Stop behavior and Size interaction
  before declaring that program complete. Any deliberately deferred behavior
  must be visible in the experimental candidate's notes.
- Keep Rich Plate/Rich Split Chorus semantics: current evidence shows no
  interpolation destinations for those controls, while their controller work
  affects dynamic/optimization timing. Do not invent moving taps because of
  the Chorus label. See the existing Rich corrections.
- Do not silently transfer Dark Hall's known original feedback instability
  into an unbounded float loop. Assess Original and Stable separately using
  the existing variation design and bounded extreme-tail checks; document any
  additional correction instead of shortening every algorithm globally.
- Allocate delay storage and prepare profiles outside processing. Reserve
  all supported Size/predelay/modulation extents. Parameter updates must retain
  tails where the experimental interface promises it. No audio allocation,
  I/O, waits or locks; retain four-output information for split/routing work.

## Acceptance and experimental A/B delivery

Continue through a separate experimental float core/plugin, leaving main
Cineol-X DSP, IDs, sessions, Clean and current defaults intact. Reuse the A/B
shell, common input/controls and constant B trim. Extend the selector and
XL page binding only as each candidate becomes usable. A means the existing
original path with its converter/circuit filtering; B means direct float.
If the A reference is fixed-point Clean rather than original precision, label
that as a separate diagnostic, not silently as Original.

For each new topology, check finite output, bounded extreme feedback, reset,
no callback allocations, routing and controls. Use original-rate real-valued
algebra checks where they are cheap and informative; do not require integer
rounding equivalence from float. Measure Release CPU/memory once per assembled
candidate block. Desktop results do not establish Daisy realtime margin.

Prepare a small level-matched listening set from existing fixtures: percussive
input, sustained/tonal input, then explicit input silence with enough tail.
Include a retrigger when dynamics apply, both ordinary and long settings,
and modulation on/off as relevant. Compare attack, density, stereo motion,
frequency-dependent tail, final residual and response to controls. A single
constant gain per comparison is allowed; no fitted EQ, output fade or tail
gate to manufacture agreement. Stop numerical tuning when the implementation
is sound and the candidate is useful to judge by ear. No new listening
acceptance is claimed by this plan.

Family members receive a narrow validation of their own data and controls;
one base passing does not automatically accept its sibling. Reuse retained
inputs/metrics and avoid rendering a full catalog after every helper edit.
Streaming publication is a later explicit user action through the existing
Listen service; no audio was uploaded during planning.

## Evidence and scope

Source checkpoint: `30ac7d136899bfe51ee8f11ddc423324bd1e8a9c` plus the existing
working tree. The current checkout is `codex/release-1-1a`; planning did not
switch branches or modify unrelated release/pitch/listening work. Before DSP
implementation, resume isolated `cineol_float` work with current sources and
preserve the corrected Hall baseline and recent XL control fixes.

Inspected sources: `native-hall/core/program_bank.hpp`,
`program_networks.inc`, `native-hall/desktop/graph_list.inc`,
`graph_dispatch.inc`, all dispatched network includes, `concert.hpp`,
`bank.hpp`, `controls.hpp`, `runtime.hpp`, Rich control notes and the existing
float Hall. Dependency pins and CMake paths were inspected; none changed.
The source-only audit, exact row-signature groups and source hashes are retained
under `build/validation/float-family-plan-20261104/{audit.py,catalog.json}`.
No new bank import, firmware extraction, audio render, CPU benchmark, DSP test,
installation or release was needed for this planning block. Existing evidence
and listening pairs were retained; no generated assets were deleted.

Related documents:
[float Hall correction](FLOAT_HALL_DARKNESS.md),
[fixed-point Clean distinction](CLEAN_ALL_ALGORITHMS_2026-11-02.md),
[Rich Chorus semantics](RICH_CHORUS_FIX_2026-11-02.md),
[Rich dynamic timing](XL_RICH_DYNAMIC_TIMING_FIX_2026-11-02.md),
[Dark Hall feedback](DARK_HALL_FEEDBACK_CONTROL_2026-11-02.md).
