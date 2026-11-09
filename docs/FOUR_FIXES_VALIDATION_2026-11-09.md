# Assembled control fixes and plugin validation

Date: November 9, 2026. Local development build from `e35ea7991e40fadb2bc29b2d51fbf5e9e4718f6d`
plus the working-tree corrections. No release, tag or issue closure is implied.

## Mod Rate and Mod Depth

At the user's request, original-224 Mod Rate and Mod Depth now retain independent
settings. Editing either parameter preserves the other immediately, including
direct host setters, automation, full/compact editors and saves made before the
message-thread mirrors update. The change removes the Rate setter's overwrite
of Depth in `Original224Parameters.hpp`; it adds no DSP processing or callback work.

This deliberately differs from the physical v4.4 panel behavior documented in
[the earlier diagnosis](ISSUE14_RATE_DEPTH_RECHECK_2026-11-09.md). Both controls
still have 16 positions. Native period remains `max(1, Rate - 1)` and fractional
step remains `4 * Depth`; Rate 1 and 2 therefore share the shortest interval at
a fixed Depth. No rate rescaling was requested or introduced. Factory values
2/1/5, program/Mod reset behavior, parameter identities, preset schema and saved
settings remain unchanged. Existing automation that relied on Rate resetting
Depth intentionally behaves differently. The tooltip explains the new policy.

The new independence assertion was run before the production edit and failed
on the old coupling. It passes afterward. The regression covers all 16 by 16
Rate/Depth pairs in both edit orders on all six programs, retained Rate while
Depth moves, immediate host state, session/preset recall and legacy migration.
The audio comparison also checks the native engine against the processor.

## Four focused regression blocks

| Block | Rechecked behavior in the assembled code |
| --- | --- |
| #13 original-224 Plates | Over-limit moving pre-delay automation produces exactly the same audio as the trajectory capped at 107 ms; both Plates, Original/Clean, three host rates and old host values remain covered. |
| #16/#17/#19/#20 XL Attack/Depth 99 | All catalog full/compact endpoint limits and legacy values pass; raw 255 and physical 254 produce identical nonzero output on the reported programs, both precision modes and three rates. |
| #15/#18 Rich Plate/Split Chorus | All eight Mod/Dynamic/Optimization combinations pass in both precision modes, including four-output stopped tails and live automation. The effect remains conditional and the upper-half timing remains constant. This is verification of the existing correction, not another new DSP change. |
| #14 original-224 modulation | Independent host values, UI/session/preset behavior and live Rate changes retaining Depth pass on all six original programs. |

Earlier independent physical-ROM and before/after sound evidence is retained in
the [pre-delay](ISSUE13_PLATE_PREDELAY_2026-11-09.md),
[Attack 99](XL_ATTACK99_ENDPOINT_FIX_2026-11-09.md) and
[Rich Chorus](RICH_CHORUS_RECHECK_2026-11-09.md) reports. This assembled run uses
the current plugin code; it does not repeat those entire ROM capture campaigns.

## Full processor and actual-format tests

The final `native_hall_plugin_check` passes `--hidden-check`, `--predelay-check`,
`--xl-depth-check`, `--rich-chorus-check`, `--bypass-check`, `--help-check`,
`--banks` and `--clean-check` against isolated copies of the existing private
banks. The full checks cover all 28 algorithms, 44.1/48/96 kHz, block-size and
offline/realtime invariance, mono/four-output routing, Spillover, retained Clean
histories, Dirt, fixed latency, presets, sessions and migration. Bypass explicitly
rechecks the previous click/alignment correction at blocks 1/127/512, normal and
low latency, impulse alignment and smooth transitions with uninterrupted DSP.
Callback allocation/release counters remain zero. `cineol_rom_import_check
--empty` also passes empty-cache, invalid-import and dry-pass safety checks.

The optional `cineol_built_plugin_check` host loads the **actual newly built AU
and VST3 executables**, without linking the product's shared-code target. Both
formats pass on arm64 and x86_64 through Rosetta. Each format/architecture runs
68 independent six-second renders: 272 renders total, using deterministic noise
plus a sine, live control changes and explicit input-silent tails at 48 kHz /
128 frames. All four blocks run in Original and Clean; every output remains
finite, with nonzero wet signal. Maximum recorded peak is 0.0499436 for these
low-level fixtures. These are fixture results, not universal stability limits.

For each architecture, the three retained example WAVs from AU and VST3 are
byte-identical. Per-case signal/tail RMS and peak remain in `signals.csv`.
Core Audio access required running the AU test outside the command sandbox;
inside it, even `auval -a` returned no components. The AU host retains the exact
built factory and registers a process-local validation subtype `Ct9A` to avoid
installed-component selection. It does not install or alter the AU bundle;
the shipped subtype remains `Nh24`, manufacturer `Rflx`.

## Build and cost

AU, VST3 and Standalone were built in Release with Apple Clang 21.0.0, JUCE
8.0.14 and the existing pinned dependencies. Every bundle contains arm64 and
x86_64 and passes `codesign --verify --deep --strict`. The identifiers and
numeric version 1.1.0 are unchanged. Signing is ad hoc for local validation;
this is not a notarized public release. Standalone is built and inspected;
the actual-format signal runs above exercise AU and VST3.

After compilation and format tests finished, the current-build offline CPU
matrix passed all 56 program/precision cases at 48 kHz / 128 frames. Maximum
mean processing cost was 5.98% of one core, and maximum measured callback p99
was 0.239 ms against a 2.667 ms block interval. The rapid original-224 hidden
control/Spillover benchmark reached 8.63% mean during overlap, with maximum
observed first overlap block 0.260 ms and zero callback new/delete. These are
current-host diagnostics, not a paired performance improvement claim or proof
of realtime margin on Intel hardware or Daisy. The prior paired Attack fix CPU
evidence remains in its report.

## Artifacts and limits

Ignored evidence root: `build/validation/four-fixes-final-20261109/`.
It retains processor logs/results, actual-format CSV/WAV results, CPU logs,
bundle inspection and `build-provenance.json` with source hashes. The universal
build tree is `build/validation/issue13-predelay-20261109/plugin/`.
`cleanup.json` records removal of byte-identical VST3 WAV copies and their
retained AU replacements. Reports, metrics, active fixtures, private source
banks and earlier before/after evidence are preserved.

The local archive is
`build/validation/four-fixes-final-20261109/Cineol-X-224-development-2026-11-09-macOS-universal.zip`.
It contains AU/VST3/Standalone plus build notes and checksums, without ROMs,
prepared banks or test caches. No installed plugin was replaced during validation.

Automated regression acceptance is complete. Human listening acceptance in a
DAW remains open. Rosetta is not physical Intel hardware; Windows and hardware
Daisy builds are outside this macOS validation block.

## User-authorized installation follow-up

Later on November 9, the user explicitly requested installation. Both existing
AU copies (user and system Components folders), the user VST3 and the user
Standalone were updated to the tested universal bundles. Installed contents
match their build sources exactly and pass strict signature verification.
Verified previous installations are retained under
`build/validation/four-fixes-final-20261109/installation/backups/20261109-223159/`;
`installation/installed.json` records paths, hashes, architectures and backups.
No DAW was stopped or restarted; open hosts must reload the updated plugin.
