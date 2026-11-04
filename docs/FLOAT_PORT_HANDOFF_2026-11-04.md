# Float port handoff — November 4, 2026

## Purpose and next-session entry point

Continue the separate 48 kHz float port of original Lexicon 224 v4.4 and
224XL v8.21 algorithms, using the corrected float Hall as the example.
The user requested this handoff and will start the next session personally.
This handoff does not start implementation, another chat or an automation.

Read current `AGENTS.md`, this handoff and
[the complete candidate/implementation plan](FLOAT_PORT_PLAN_2026-11-04.md).
Then inspect the actual branch, working tree and build configuration before
editing. The next small step is to parameterize Hall B and add Small Concert
Hall B. The **next new topology is 224 Percussion Plate A**, followed by its
Vocal Plate profile. Do not begin by retuning the accepted Large Hall B.

## User decisions to preserve

- Audio runs directly in float at 48 kHz, with original topology and numbers
  translated into physical times. No original-rate audio processing or
  input/output circuit filters in this version. Internal diffusion and
  frequency-dependent decay filters remain part of the algorithm.
- Keep this experimental core/plugin separate from the main original engines.
  Preserve main plugin identities, parameter IDs, sessions and current defaults.
- The user listened to corrected float Hall and found it brighter than A,
  but considers leaving it that way acceptable for now. Original output
  filtering is a plausible contributor, not an independently isolated cause.
  Do not add EQ/filtering solely to remove that difference.
- **Do not chase 100% similarity. Error is acceptable; listening decides.**
  Checks should establish a correct, stable and useful candidate, followed
  by bounded A/B evaluation. Do not claim acceptance for unreviewed programs.
- Port shared family graphs once, then supply each member's own data.
  Different visible fader settings alone do not reproduce every member.
- Existing fixed-point **Clean** is a separate product feature: it retains
  original audio rates and converter filters. It is not this float project.

## Repository checkpoint and unrelated work

Repository: `temaniak/cineolX`; the local checkout is named `reflexion-224`.
Checkpoint inspected here:

```text
branch: codex/release-1-1a
HEAD:   30ac7d136899bfe51ee8f11ddc423324bd1e8a9c
local cineol_float ref: 1372c0b3cfb6a502e6eea85318074374bcdc71f0
```

The corrected float implementation and its earlier reports are tracked in
the current checkout. The current session added only the candidate plan,
ignored source-audit evidence and this handoff. No commit/push, DSP change,
build, installation, upload or release was performed in this planning/handoff
block. The plan and this handoff are local/untracked at creation; a pull on
another computer does not fetch uncommitted documents.

Pre-existing tracked modifications at handoff time:
`AGENTS.md`, `CMakeLists.txt`, `NOTICE.md`,
`docs/XL_SOUND_ACCURACY_HANDOFF.md`. Pre-existing untracked work includes
Pitch Lab sources/scripts, LexIR/listening reports and scripts, and `tools/`.
Preserve these; do not stash, reset, stage or commit unrelated work as part
of the float port. Their contents may be changing in other work.

The user originally requested the branch name `cineol_float`. Resume isolated
float development while retaining recent core/XL fixes from the current
checkpoint. Inspect the branch ancestry first; do not blindly switch the
dirty checkout, reset the older float branch or overwrite its work. A clean
worktree can isolate implementation. Do not copy unrelated dirty root CMake
changes into that worktree just to make its source tree resemble this one.

Dependency pins remain in `dependencies.json`:
Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`,
libDaisy `f044cdc312455f174b01bef98d9e97598d94d3bc`.
JUCE is pinned to 8.0.14. Inspect CMake before implementation; keep dependency
checkouts clean and use build-only exports for compatibility patches.

## Confirmed family selection and order

| Representative | Same-graph member | Evidence |
| --- | --- | --- |
| 224 Large Concert Hall B — already ported | Small Concert Hall B | Both original-224 network 0. |
| 224 Percussion Plate A | Vocal Plate | Both original-224 network 1. |
| XL Concert Hall | Room | Both dispatch `concert_network.inc`. |
| XL Plate | Small Plate | Both dispatch `plate_network.inc`. |

Order: Hall B profile generalization/Small Hall B → Percussion Plate A/Vocal
Plate → XL Concert Hall/Room → XL Plate/Small Plate → separate original-224
Small Concert Hall A and Acoustic Chamber. The first four bases cover eight
programs after their members are validated; the two additional bases bring
coverage to ten, including all six implemented original-224 programs.

The audit found four original-224 network IDs and 20 distinct ordered XL row
signatures for 22 XL programs. These are conservative implementation facts,
not a requirement to write 24 separate float kernels. Mathematical collapse
may expose more common blocks. Equal row signatures do not establish equal
delay bindings, controls, modulation or sound.

Bright Hall differs from Concert Hall in 10 of 105 row records; assess it
as a topology variant, not a confirmed parameter preset. Dark Hall, Small
Room, Chamber, CD Plate A/B, Rich/Inverse graphs and the five splits have
distinct row programs. Rich Chamber is a useful common-block anchor for
Dark Chamber/Inverse Room, but their data-only equivalence is unproved.
The complete plan assigns all 22 XL programs, then the three non-reverb
effects and five split programs to later blocks. Do not replace split graphs
with two full-size ordinary algorithms without checking their actual wiring.

## Current float implementation and the previous correction

| File | Role / limitation |
| --- | --- |
| `native-hall/float/hall.hpp` | Collapsed Large Hall B signal graph; separate float delay lines. `Hall48` is `Hall<48000>`; `program=2` is currently fixed. Writer/reader lists and equations are Hall B-specific. |
| `native-hall/float/fractional_delay.hpp` | Six-tap, fifth-order Lagrange weights; allocation-free reads use the graph's reserved history. |
| `native-hall/float/check.cpp` | Independent real-valued shared-RAM ARU oracle at 20.48 kHz, 48 kHz safety/control/allocation/long-tail checks. Oracle currently fixes network 0. |
| `native-hall/float/fractional_delay_check.cpp` | All 32 phases: DC gain/time, integer case, full-band gain bound and passband loss. |
| `native-hall/float/plugin/Processor.cpp` | Separate A/B shell; both A and B currently fix program 2. Shared controls, continuous histories, 10 ms switch crossfade, B trim and 70-frame B/dry pad. |
| `native-hall/float/plugin_check.cpp` | A/current-engine identity, B/core routing, state, tails, dry/mono, 44.1/48/96 kHz and allocation checks. Currently Hall B-specific. |
| `native-hall/float/CMakeLists.txt`, `README.md` | Experimental targets and build/use documentation. |

The first float attempt was too dark because repeated linear fractional reads
attenuated high frequencies, and pole remapping changed the full response of
short recursive filters. Long-delay conversion was already correct. The
selected correction keeps literal bank coefficients, physically scaled short
feedback delays and fifth-order reads. Current-sample terms of short recursive
reads are solved analytically; both captured inputs are available before short
cross-channel reads. Preserve this logic when deriving new equations.

Rejected candidates: lossless first-order allpass reads allowed late growth
with the rounded original section gains in an extreme test; global replacement
of complements with `1 - g*g` shortened some maximum tails dramatically.
Do not revive either as a blanket correction.

Historical validation on October 12: core/interpolator/plugin/ASan/UBSan and AU
checks passed, with no processing allocations and A identical to the then-current
original engine. AU/VST3 were installed at that time with prior copies retained.
Those are historical results, not proof of today's installed bundles or of a
future expanded implementation. Windows was not validated in that block.

Details: [darkness diagnosis/correction](FLOAT_HALL_DARKNESS.md),
[historical initial validation](FLOAT_HALL_VALIDATION.md), and
[float README](../native-hall/float/README.md).

## Concrete first implementation block

1. Preserve a reproducible corrected Hall B baseline and the original A
   reference before changing the common code. Reuse retained fixtures.
2. Parameterize the Hall B profile selection; use the selected profile during
   preparation, reset, control compilation and complete delay-capacity
   reservation. Add Small Concert Hall B without changing Large Hall B's data
   or equations. Do not generalize Plate by merely changing `program=2`.
3. Derive the plate equations from original network 1. Port Percussion Plate A
   first, then Vocal Plate using its actual tables, delays and control laws.
   Extend the independent oracle to the selected network rather than leaving
   it hardcoded to Hall B. Reuse common primitives without a broad refactor.
4. Extend the experimental A/B selector, state and controls for ready programs.
   Preserve existing experimental parameter meanings where possible. Its
   current predelay parameter is Hall-specific (24–152 ms); plate profiles
   have a zero minimum, so inspect ranges and display mappings explicitly.
   Keep both sides on the same selected program and logical controls.
5. Check routing, controls, finite/reset behavior, no audio allocations and
   bounded extreme tails. Perform only meaningful tests for the changes;
   do not rerun a full catalog campaign after every helper edit.
6. Build the separate candidate and prepare a small level-matched listening
   set with percussive and sustained input, followed by explicit input silence.
   Include ordinary/long tails and modulation where applicable. Measure CPU
   and memory once for the assembled block; desktop timing is not Daisy margin.

The next XL block needs a separate XL profile/control bridge, not use of an
original-224 bank in an XL graph. Reuse `desktop/bank.hpp`, `controls.hpp`,
modulation and dynamics metadata. Before marking an XL program complete,
include its Size, modulation, Dynamic/Stop and Optimization behavior or clearly
document any incomplete experimental mode. Do not inject arbitrary modulation
into Rich Plate/Rich Split: their Chorus control has timing semantics without
the usual interpolation destinations. Preserve Dark Hall's documented
Stable/Original design and explicitly check float feedback bounds.

## Units and realtime rules

- Original 224: native rate 20,480 Hz; host/native delay ratio `75 / 32`.
  One native sample is 48.828125 microseconds / 2.34375 frames at 48 kHz.
- XL: native rate `10,240,000 / (3 * rows)` Hz. Convert offset difference `d`
  to `d * 48,000 * 3 * rows / 10,240,000` frames. Do not use 34.13 kHz for
  all XL algorithms. Compile Size/address layouts in native units first.
- Preserve coefficient units and program-specific controls. Audio rate,
  modulation rate and controller/dynamics cadence are different quantities.
- Allocate/reserve outside processing, including all supported predelay,
  Size and modulation extents. No heap allocation, blocking I/O, locks or
  waits in audio; keep DSP, controls and hardware I/O separate.
- No ROMs, banks, audio/captures, firmware or private adapters in Git.
  Documentation stays English. Retain uncertain evidence and user assets.

## Build and checks for the resumed implementation

Do not run a build merely because this handoff lists commands. When
implementation resumes, inspect the configuration and use a compatible fresh
directory if needed. Existing `build/float-ab` uses a local absolute JUCE
source override and old cache settings; do not copy its cache to another
computer. Portable macOS example from the repository root:

```sh
cmake -S . -B build/float-port-next -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=ON -DCINEOL_BUILD_FLOAT_AB=ON -DNATIVE_HALL_BUILD_TOOLS=OFF
cmake --build build/float-port-next --target cineol_float_hall_check cineol_float_fractional_delay_check cineol_float_ab_plugin_check CineolFloatHallAB_AU CineolFloatHallAB_VST3 CineolFloatHallAB_Standalone -j 6
build/float-port-next/native-hall/float/cineol_float_fractional_delay_check
build/float-port-next/native-hall/float/cineol_float_hall_check PRIVATE_BANK224
CINEOL224_CACHE_DIR=PRIVATE_TEST_CACHE build/float-port-next/native-hall/float/cineol_float_ab_plugin_check_artefacts/Release/cineol_float_ab_plugin_check
```

Replace the two uppercase placeholders with existing private paths. The core
check accepts a bank file; the plugin cache directory must contain
`programs-v44-import-v1.bank224`, not just a differently named core bank.
Do not mutate the user's regular cache for a test. On Windows omit AU, use
the appropriate generator and `--config Release`; see the float README.
Fresh Windows build/runtime validation remains separate work.

Experimental identity: product `Cineol Float Hall AB`, bundle
`net.joelanders.cineolfloatab`, AU `aufx / CfAB / Rflx`. Main Cineol-X identity
must remain separate. New main releases, publishing and unrelated commits
are outside this handoff's scope.

## Retained local evidence and listening

- `build/validation/float-family-plan-20261104/audit.py` and `catalog.json`:
  source-only family audit, hashes and native XL clock factors. No ROM import
  or audio tests were needed for that planning result.
- `build/validation/float-darkness-20261012/provenance-final.json` and final
  check logs: correction identity and historical validation. Earlier rejected
  candidate sources/metrics are retained alongside them.
- Same directory: `probe.cpp`, `analyze.py`, mode 0/1/3 metrics,
  `music-probe.cpp`, `prepare-listening.py` and `listening/manifest.json`.
- Listening files: `listening/original-then-corrected.wav` and
  `listening/before-then-corrected.wav`. Each has 12 s of the first version,
  0.5 s silence, then 12 s of corrected B; each excerpt includes 4 s of input
  and 8 s of tail. Matching uses constant active-input RMS gain, not EQ or
  fitted tail changes. Historical manifests say `listened: false`; the user's
  later listening judgment is recorded in this handoff, not retroactively
  attributed to those validation runs.

These ignored local artifacts do not travel via Git. Reuse them if available,
otherwise reproduce narrowly from documented sources and private banks. No
cleanup was performed in this planning/handoff block because retained material
includes unique evidence and comparisons useful for the next session.

If the user later requests streaming publication, read `tools/listen/README.md`
and use the existing Listen JSON/publisher workflow. Upload only requested
audio/presets, keep access private and return the collection link; do not
create another standalone player. No streaming upload is requested now.

Relevant later-XL context:
[Rich Chorus](RICH_CHORUS_FIX_2026-11-02.md),
[Rich dynamic timing](XL_RICH_DYNAMIC_TIMING_FIX_2026-11-02.md),
[Dark Hall feedback](DARK_HALL_FEEDBACK_CONTROL_2026-11-02.md),
[current fixed-point Clean](CLEAN_ALL_ALGORITHMS_2026-11-02.md).

