# Native XL correction work

This continues the independent catalog baseline in
[XL_CATALOG_VALIDATION.md](XL_CATALOG_VALIDATION.md). The full sound-accuracy
objective remains open. A startup correction is the first bounded change;
local state equivalence does not establish converter, controller scheduling,
listening or realtime performance acceptance.
The subsequent bounded [XL event DAC correction](XL_DAC_VALIDATION.md) has an
independent all-22 converter/policy oracle and a retained old-policy failure.
Its full-path validation is tracked separately from the startup-only evidence.
The DAC checkpoint has passed all-22 request/graph checks and 28-program plugin
regression. Its six primary pairs retain mixed metrics. The initial CPU rise
and Resonant Chords outlier are resolved in the final matched CPU campaign:
6.84% below pre-DAC overall, with Chords 13-16% below pre-DAC. The DAC report
preserves the earlier results and the final correction evidence separately.

## Frozen baseline and physical startup evidence

Baseline production revision: `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`.
The existing uncommitted validation tools are retained. Before changing
production headers/import, 132 files under `native-hall/` were frozen with
individual SHA-256 values in ignored
`build/validation/xl-corrections-20261005/baseline/manifest.json`. Baseline
binaries must compile these headers independently, not link the changed core.

Reference identity is unchanged: the eleven hash-validated original v8.21
chips, pinned Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`, and its
build-only compatibility export. ROMs, banks, raw descriptors and WCS remain
private. The current tools-only build is Release macOS arm64, AppleClang
21.0.0.21000334, `-ffp-contract=off`.

An independent process per graph booted for 16 simulated seconds and used
physical program keys, Mod/Dynamic/Opt keys and Chorus/Size faders. No DSP
state or RAM mode flags were injected. Watches at AB4F, AB55 and the first
following AD5C establish the compiler boundary before modulation executes.
Across all 22 graphs, all 134 observed compilations reset the sequence pointer
to 59 and retain the three counters at 3E44/3E45/3E46. Program selection,
physical Mod edges and applicable Size moves compile the graph. Chorus and
Dynamic/Opt edges do not invoke this program compiler. Other controller
effects of Dynamic/Opt are not ruled out by that observation.

`cineol_xl_startup_check` makes these observations reproducible. It compares
native startup with the physical compiler's descriptors, interpolation
coefficients and low addresses. On the frozen v4 baseline, all 22 graphs were
tested; 17 graphs with interpolation failed with 336 startup-field mismatches.
The five graphs without interpolation have no active interpolation-state
comparison and are not counted as additional sound passes.

The extended test also runs native audio independently, changes Mod and Size,
checks retention of the native counters and complete delay memory, and tracks
native allocations/releases. All 22 graphs pass with zero mismatches and
zero native allocation/release after the correction. The frozen old engine
fails the extended startup/Mod/Size test on Concert Hall with 52 field
mismatches. Paired CPU results are being collected. The full first
run found a Plate/Chorus Size fixture reading the parameter record during
firmware restoration. The oracle now waits two simulated seconds after a
fader plus `recordSettled()`. The entire catalog passes with this consistent
settling protocol. Composed controls, graph arithmetic, full Chorus/modulation
and dynamics oracles pass after the correction. The dual-bank plugin regression
also passes at 44.1/48/96 kHz with varied blocks, presets/state, mono/stereo,
Low latency and Spillover; callback allocation/release remains zero. AU, VST3
and Standalone build and verify their ad-hoc signatures; they are not installed
by this work. Cache migration checks the explicit re-import message, retained
original bank, accepted v5 cache and byte-identical preserved v4 cache.

The first paired sound fixture uses the preserved Concert Hall mode-0 noise
recipe (seed 17, level 0.08, 1000 ms warmup, 12 seconds, block 256, Analog on).
Input and reference WAVs are byte-identical to the catalog baseline; only the
bank/native output changes. On the same eight usable paired bands, mean
absolute T20-derived decay error falls from 11.6079% to 1.5291%, maximum from
30.7059% to 4.3423%. Overall energy error changes from -0.4426 to -0.4052 dB.
The ordinary new native output is also byte-identical to the separately
labelled static-reference-WCS diagnostic for this one fixture. No reference
state was injected into the ordinary path. This pilot does not establish
mode-on, remaining-program, converter, listening or CPU acceptance.

Forty-four mode-off recipes (the preserved seed-17 and quiet seed-991 cases
for all 22 programs) completed with byte-identical input/reference WAVs
relative to the v4 baseline. Eight cases change their accepted decay-band
sets; paired statistics therefore use the common accepted bands, with all
rejections retained in CSV. There are residual errors and some unfavorable
quiet cases, so these are measurements rather than overall sound acceptance.
Private `startup-44-comparison.json` and `startup-44-common-bands.json` retain
both versions and the identical-reference checks.

The old renderer hardcoded the CSV bank-version label as 4. The v5 bank hash
was correct. For these 44 cases and the pilot, only that label was corrected
against the retained bank header/hash, preserving every original CSV beside
it. Analysis was regenerated with the same analyzer; WAVs/commands/binary
identities are unchanged. The renderer now emits the actual supported bank
version. The repair and reanalysis provenance are kept under ignored build.

## Startup lookup follow-up

The extended first-transition oracle exposed another startup boundary. AB52
stores the pointer 003B, so AD98 initially reads the SBC chip at 003B. Only
the first random-hold/index advance at AD77..AD81 adds the NVS high bits.
The bytes at 003B and 803B differ in the verified firmware. The previous native
law always used the NVS sequence, including immediately after a compiler key.

The importer now retains the SBC startup byte, and modulation state explicitly
tracks the initial lookup until the first index advance. These two bytes use
previous struct padding, preserving the v5 payload size. Mod/Size resets also
restore that lookup state while retaining counters/tail. The new diagnostic
oracle observes actual AD5C entries from before the physical Mod-on key and
checks each state/WCS transition through the SBC-to-NVS boundary. It remains
separate from the ordinary independent native audio clock.

The final startup-byte bank is prepared. Its SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`;
the file is 5,828,172 bytes including its 20-byte header. The final reader
requires the canonical startup flag/index for active interpolation, rejecting
the unreleased first v5 prototype as well as legacy v4. The first prototype
and its evidence remain preserved under their original hashes/paths.

At the October 6 checkpoint the final implementation passes physical
startup/key/Size checks for all 22 graphs, including 27,609 first enabled
transitions across the 17 active interpolation profiles with zero state/WCS
errors and zero native allocations/releases. The identical final oracle was
compiled against independently frozen old headers: Chorus & Echo fails with
9,465 first-transition errors on the same 739 calls, plus 74 startup/Mod
fields. This is a meaningful old-engine failure, not bank rejection. Exact
commands, pre-launch binary/source hashes and bank hashes are in ignored
`startup-first-steps-active-runs.json`.

An earlier all-22 attempt incorrectly compared normalized inactive bank
profiles against dormant firmware counters in Resonant Chords, Multiband
Delay and Rich Split. That failure is retained in
`startup-first-steps-current-all.log`. The final oracle explicitly excludes
inactive interpolation transitions for all five inactive profiles, while
still observing their physical compiler/counter-retention/key/Size behavior.
Inactive interpolation is not an additional active-law or sound pass.

The final bank also passes 1,928 composed controls, 52 Size fixtures,
79,162 modulation transitions and all 32 Chorus compiler positions for each
active profile, 27,117 dynamics transitions across all four applicable switch
combinations, and all 22 integer graph/arithmetic/memory/saturation checks.
The 28-program dual-bank plugin regression passes at 44.1/48/96 kHz with
varied blocks, presets/state, mono/stereo, Low latency and Spillover, including
the original 224 path. Callback allocation/release is zero. The explicit
v4-to-v5 migration passes with the legacy cache preserved. AU, VST3 and
Standalone were rebuilt and signature-verified with the final implementation;
these builds are not installed or host/listening-validated.

The final 313-case campaign completed separately under
`matrix-startup-final/`, with zero failed/pending cases. All 313 have
byte-identical input/reference WAVs and physical fixture metadata versus the
preserved baseline. There are 303 signal-bearing cases and ten unexcited
cross-pair routing cases; the latter are not sound passes. Native audio is
byte-identical before/after in 65 cases. On common accepted decay fits, 52
cases improve, 11 are unchanged, 17 worsen and 233 are unavailable. Missing
fits include deliberately excluded dynamic/gated/inverse/effect responses and
weak or incomplete tails; none are counted as zero error or passes. The
mode-off subset still gives 19 improved, 5 unchanged, 1 worse and 19 unavailable
cases. These measurements are not full-catalog sound acceptance.

`compare_xl_corrections.py` checks actual reference/recipe identity and compares
only common accepted bands, retaining changed fit sets and unfavorable cases.
It also retains per-band energy errors and exploratory RMS-envelope differences
for effects/gates, with missing/extra windows reported separately. Its envelope
selection is explicitly reference RMS within 60 dB of a reference peak above
-200 dBFS; this is a measurement policy, not an acceptance threshold or a
change to the decay-fit guards. A real altered-reference guard check fails as
required. The paired summary includes source/WAV/bank identities, nominal and
observed aggregate rates, and both versions under ignored build. Final paired
CPU sampling completed after all renders/oracles. The final results and the
historical first compiler-state measurement are distinguished below.

All 22 ordinary mode-0 synthetic music fixtures also have listening copies
under `listening-startup-final/`. A common -24 dBFS RMS target over the first
two seconds is reduced when necessary for full-capture peak headroom. Only
scalar level changes are applied: no EQ, time alignment or fades. Each paired
file plays reference, 500 ms silence, then native. Float-WAV readback verifies
matching RMS, finite/unclipped output and the concatenation order; source WAVs
remain unchanged. `prepare_xl_listening.py` and its manifest reproduce this.
These materials have not been listened to or compared with a physical unit.

Aggregate reference entries also confirm a remaining shared-loop timing
approximation. Concert Hall mode 0 runs the observed slow controller at
70.25 Hz versus the bank's fixed nominal 60.9 Hz; mode 1 gives 60.9167 Hz,
mode 2 gives 68.25 Hz and mode 3 gives 59.3333 Hz. The active mode-2
optimization controller therefore differs by about 12.1% from that nominal
rate. These are aggregate observations, not independently predicted call
times or WCS visibility. A state-dependent XL scan model, retaining global
counter semantics through program changes, remains required.

## Local modulation branch timing

The offline `cineol_xl_modulation_timing_check` derives the AD5C..AE9B branch
cost and WCS byte order from XL behavior, separately from original-224 costs.
The disabled gate uses a 72-iteration delay, totaling 1,127 CPU states before
the caller. The native arithmetic cost model contains no ROM instructions or
opcode dispatch and currently lives only in the offline tests.

All 22 programs pass 104 physical fixtures: Mod off/on and Chorus raw
2/130/250 wherever exposed. The final run observes 20,894 calls and 70,260 WCS
writes with zero cost, instruction-boundary or byte-payload errors. Each
fixture samples at least 200 calls within a bounded two simulated seconds.
Exact source/binary hashes and commands are in
`modulation-timing-all-final-provenance.json`.

This local oracle supplies the observed WCS instruction wait duration and
observed serial interrupt duration. It does not predict either independently.
An interrupt adds the eleven-state RST-7 acknowledge plus its handler; treating
that as modulation branch work initially produced false cost errors. A short
fixed window also undersampled high-Chorus CD Plate cases. Earlier runs and
their failures are retained. The final driver uses `Machine.render` so the
operator's frame/serial clock advances alongside the host, and turns Mod off
before selecting the next graph; direct Host stepping had left those clocks
behind and caused a later selection failure. These are oracle corrections,
not audio-path changes. The full free-running scan, interrupt context and WCS
grant/visibility model remain open.

## Paired CPU and storage (final startup implementation)

Five alternating old/new rounds compare the independently compiled frozen
v4 engine with the final startup-byte implementation and bank. Both use
Release `-O3 -ffp-contract=off` and the same FTZ/FZ policy. No task build,
render, oracle or analysis ran during sampling; external host activity remains
uncontrolled. Preparation and one-second warmups are untimed.

All ten runs and 1,320 rows pass finite/peak guards, per-version checksum/peak
stability, and allocation/release=0. The sum of 132 per-group median costs gives
new/old **0.99616**, effectively unchanged within measurement spread. This
does not establish a speed improvement or host deadline margin.

| Workload | Final median audio-time cost, range over 22 graphs | Sum-of-medians final/old |
| --- | ---: | ---: |
| Single, modes off | 1.740-2.421% | 0.99676 |
| Single, modes enabled | 1.753-2.441% | 0.99682 |
| Same-program retiring tail plus current | 3.495-4.903% | 0.99617 |
| Resonant Chords retiring tail plus current | 3.484-4.205% | 0.99687 |
| LF/MID edges each 128 frames | 1.769-2.447% | 0.99643 |
| Mod and applicable Size edges each 128 frames | 1.772-2.428% | 0.99333 |

The timed segments are the same 96,000 frames at 48 kHz/block 128 as the
historical benchmark. Two Runtime/Engines preparations request 9,416,128 bytes
versus 9,402,512 before (+13,616); the payload remains 5,828,152 bytes. This is
requested storage, not RSS. Overlap excludes plugin retirement fades and host
conversion, and the designated Chords overlap is not a worst-pair proof.
Actual plugin callback regression/allocation is verified separately. All rows,
spread, commands, bank/binary hashes and frozen-source identity are retained
under `cpu-paired-startup/`. Desktop measurements do not establish Daisy CPU
margin. Later scheduler/converter changes require fresh paired measurements.

## Paired CPU and storage (first compiler-state correction)

Frozen old/new binaries were compiled independently in Release with identical
benchmark source and desktop FTZ/FZ policy. Five rounds alternate old/new
matrix order; each version has five samples per program/workload. Preparation
and a one-second warmup are untimed. No task builds, captures or oracles ran
during sampling; unrelated host activity remains uncontrolled.

All 1320 rows pass finite/peak guards, repeat checksum/peak stability within
each version, and allocation/release=0. The sum of the 132 median costs gives
new/old **0.99966**, effectively unchanged within measurement spread. Sonic
checksum differences between versions are expected, not regression failures.

| Workload | New median audio-time cost, range over 22 graphs | Sum-of-medians new/old |
| --- | ---: | ---: |
| Single, modes off | 1.758-2.445% | 0.99925 |
| Single, modes enabled | 1.754-2.444% | 0.99854 |
| Same-program retiring tail plus current | 3.522-5.003% | 0.99952 |
| Resonant Chords retiring tail plus current | 3.515-4.289% | 1.00070 |
| LF/MID control update every 128 frames | 1.779-2.468% | 0.99983 |
| Mod and applicable Size edges every 128 frames | 1.775-2.475% | 0.99946 |

Each timed segment processes 96000 frames at 48 kHz, block 128. Storage
requested while preparing two Runtime/Engines objects changes from 9,402,512
to 9,416,128 bytes (+13,616); the bank payload remains 5,828,152 bytes.
This is not RSS, allocator overhead or a host deadline guarantee. Runtime
overlap excludes plugin retirement fades and host conversion; the designated
Chords overlap is not claimed to be the worst program pair. Actual plugin
callback allocation/regression was checked separately. Paired per-group
medians, min/max spread, exact commands, binary/bank hashes and all rows are
preserved under `build/validation/xl-corrections-20261005/cpu-paired/`.

## First correction and cache migration

The importer still measures the enabled controller's nominal rate, then uses
the physical Mod-off key and verifies the disabled compiler state before
saving interpolation descriptors and WCS. It no longer saves an arbitrary
phase after a second of enabled modulation.

Native Mod edges restore the compiler's taps, descriptors and index while
retaining the current native counters, delay memory and graph registers.
Applicable Size updates perform the same interpolation reset after compiling
the new layout. Ordinary controls preserve the running interpolation state.
The converter path and nominal free-running controller clocks are unchanged
in this bounded correction.

The private XL bank moves from version 4 to version 5, with the same payload
layout. The original descriptor phase bytes cannot be recovered from the v4
running snapshot. A one-time offline re-import of the original v8.21 chips is
required; the plugin reports this explicitly when it finds a v4 cache. It
writes `programs-v821-native-v5.bankxl` alongside the retained v4 file. This
work does not replace the user's cache or installed bundles. Original-224
banks, plugin identifiers, parameter IDs and preset schema are unchanged.

First compiler-state-only test bank SHA-256 (historical, now rejected by the
final reader):
`c6b597e591ac3d7dea4aed269cad79a649fbe5b416580979de158d122bffe15c`.
The v4 baseline remains
`1330245bfafe90df553545d5b7807f631f6b95dfce06d610e45cd49fba7448c3`.
Audio changes are expected for the 17 interpolating programs, especially with
Mod disabled; Mod/Size changes restore the firmware's interpolation state
without clearing the existing tail. No EQ/feedback compensation was added.

## Remaining completion gates

- Extend the independently checked [ADC integration](XL_ADC_VALIDATION.md)
  and [event DAC](XL_DAC_VALIDATION.md) to the firmware's cold first scan,
  CPU-displaced rows and write visibility. Periodic converter/policy checks,
  failing-old oracles, all-22 graphs and 28-program plugin regression pass;
  those boundaries do not establish the complete firmware scheduler.
- Establish state-dependent controller scans and WCS write visibility. The
  current nominal clocks and whole-graph update boundary remain approximations.
  The [all-22 controller chronology](XL_CONTROLLER_SCHEDULING.md) now records
  the coupled nine-pass loop, physical port samples, matched grants/commits and
  invariant protect/reset properties; the corresponding native clock and
  per-row integration remain unimplemented.
- Expand tail/control-edge probes beyond the completed 313 preserved recipes,
  preserve rejected decay fits and evaluate effect/split delay/envelope/spectral
  differences under the future scheduler/converter corrections.
- Repeat the completed frozen-old/final paired Release CPU measurements after
  later corrections, with matching FTZ/FZ, control edges, overlap, storage and
  actual plugin callback behavior.
- Repeat the passed 28-program plugin regression after subsequent changes at 44.1/48/96 kHz, varied blocks,
  presets/state/cache migration and Spillover. Recheck original 224 whenever
  shared code changes, and build the relevant desktop formats.
- Use the prepared level-matched listening fixtures and record which host/listening
  checks actually occur. No physical-unit or listening acceptance is
  claimed here; desktop measurements do not establish Daisy CPU margin.

Reproduction (private paths are examples inside ignored `build/`):

```sh
cmake -S . -B build/xl-sound-validation
cmake --build build/xl-sound-validation --target cineol_xl_extract cineol_xl_startup_check -j2
build/xl-sound-validation/native-hall/cineol_xl_extract build/validation/xl-catalog-20261005/rom-private-local build/validation/xl-corrections-20261005/programs-v821-native-v5-startup.bankxl
build/xl-sound-validation/native-hall/cineol_xl_startup_check build/validation/xl-catalog-20261005/rom-private-local build/validation/xl-corrections-20261005/programs-v821-native-v5-startup.bankxl
python3 script/compare_xl_corrections.py build/validation/xl-catalog-20261005/matrix build/validation/xl-corrections-20261005/matrix-startup-final --require-complete
cmake --build build/xl-sound-validation --target cineol_xl_modulation_timing_check -j2
build/xl-sound-validation/native-hall/cineol_xl_modulation_timing_check build/validation/xl-catalog-20261005/rom-private-local
python3 script/prepare_xl_listening.py build/validation/xl-corrections-20261005/matrix-startup-final build/validation/xl-corrections-20261005/listening-startup-final
```
