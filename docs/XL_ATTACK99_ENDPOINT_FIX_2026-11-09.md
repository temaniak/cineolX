# XL Attack/Depth 99 endpoint correction

Date: November 9, 2026. Local correction for GitHub
[#16](https://github.com/temaniak/cineolX/issues/16),
[#17](https://github.com/temaniak/cineolX/issues/17),
[#19](https://github.com/temaniak/cineolX/issues/19) and
[#20](https://github.com/temaniak/cineolX/issues/20).

## Cause and user-visible correction

The displayed Attack/Depth maximum **99** covers several logical record values.
At 252–254, the native curve compiler produces the valid maximum. At **255**,
the firmware compiler instead zeros all four coefficient groups. The native
compiler correctly retains this internal behavior, but the editor and host
control resolver incorrectly exposed 255 as an ordinary fader maximum.

Physical LARC faders accept values through **254**, not 255. The physical v8.21
oracle confirms that an attempted 255 move after 254 leaves the record at 254.
Before the correction, the endpoint test fails at Plate, page 1, slot 4:
`requested=255 native=255 physical=254`.

The existing filter-endpoint protection now also covers all semantic
Attack/Depth controls. Physical and compact faders stop at raw 254, still
displaying **99**. Audio resolution maps old raw 255 presets/session/automation
to 254 without rewriting their stored values or changing host IDs, indices,
normalized ranges, bank payload/version or preset schema. No re-import is
required. Valid positions 0–254 and the coefficient curves remain unchanged.

All 23 Attack/Depth controls in 19 reverb programs share this boundary. At raw
255, the reproduced factory fixture becomes completely silent in Dark Hall,
Plate, Small Room, Chamber and Small Plate. Other programs lose a branch or
change its level/envelope; in splits, an unaffected side can conceal the defect
in a whole-output energy check. The three non-reverb effects have no controls
of this semantic kind and retain their existing policy.

## Before/after sound

The native endpoint sweep uses 48 kHz, 0 dB gain, wet-only four-output capture,
factory controls with the three dynamic/modulation switches disabled, seed-17
noise (0.03 left peak / 0.021 right), 0.5 seconds initial silence, two seconds
of excitation and 3.5 seconds stopped input. For each control it renders raw
248, 252, 253, 254 and 255 in a fresh engine.

All **92 valid endpoint fixture metrics** are unchanged between independently
compiled before/after binaries. Only the 23 invalid-255 fixtures change.
Plate at the displayed 99 changes from exact silence on all four outputs to
active RMS **0.00664488**, initial stopped-tail RMS **0.00217064**, later RMS
**0.0000205899**, and peak **0.0331477**. RMS values here combine four channels.

Independent reference/before/after renders additionally cover Plate, Small
Plate, Plate/Plate and Plate/Hall, modes 0 and 7, two seeds and input levels
0.03/0.003. All Attack/Depth controls in each selected program are placed at
their physical maximum. The reference receives real 254 LARC moves; the two
native variants receive host requests of 255 through their own control
resolvers. Only logical controls are read from the reference; no DSP state,
controller phase, converter history or clocks are injected into native audio.
Each case has 0.5 seconds independent silent warmup, a six-second capture, two
seconds excitation and explicit stopped input.

In the representative Plate / mode-0 / seed-17 case, output A active RMS is
0.00599078 in the reference, zero before and 0.00599128 after. Its initial
stopped-tail RMS is 0.00211899 in the reference and 0.00212083 after. These
fixture measurements establish restored signal and decay at the correct
boundary, not universal timing, floor or listening equivalence.

## Checks and limits

- The extended physical `--endpoints` oracle passes **520** composed fixtures
  across all **22** programs, including frequency and Attack/Depth endpoints;
  native coefficients/addresses match the firmware, with no control allocations
  or releases. The frozen old resolver fails the same new check.
- `cineol_xl_control_regression_check BANK --depth` checks all 256 raw requests
  per Attack/Depth control, expected UI maxima, finite active/stopped output
  and sample-identical 254/255 audio on all four channels. It fails before and
  passes after for all 23 controls.
- Plugin `--xl-depth-check ORIGINAL_BANK XL_BANK` passes all catalog full and
  compact bounds and retained old values. It also verifies session round-trip
  and identical 254/255 audio for the four reported programs, both precision
  modes and 44.1/48/96 kHz, with zero callback allocations/releases.
- Full dual-bank plugin `--banks` passes all 28 original/XL programs, preset
  and session recall, fader/display limits, mono, Spillover, three host rates,
  block-size and offline/realtime checks; callback new/delete counts are zero.

## Paired CPU and memory

The existing Release XL CPU matrix runs all 22 programs and six workloads,
three repeats, in before/after/after/before order: 1,584 timed fixtures, with
preparation/warmup excluded and the same FTZ policy, compiler and input recipe.
No builds or other test campaigns were run concurrently with these samples.
The 132 program/workload median pairs have median relative change **−0.47%**
and maximum increase **+1.62%**. This is not evidence of a CPU optimization;
no material increase was found in this desktop measurement.

Mean single-engine mode-on cost is 2.291% to 2.258% of one core; same-program
overlap is 4.564% to 4.525%. Decay-control and Mod/Size control-edge workloads
remain approximately 2.27% after the correction. These native-kernel timings
include control resolution and two-engine overlap, not plugin retirement fades
or host-rate resampling overhead.

All paired benchmark checksums are unchanged for these valid-control recipes.
Bank storage remains 5,828,152 bytes and the two prepared runtime allocations
remain 65,332,736 bytes; processing/control allocations and releases are zero.

This changes only the endpoint policy, outside the sample loop. Internal
sentinel compilation, delay networks, modulation/dynamic controllers,
converters and memory layouts are unchanged. Existing original-224 issue #13
work and unrelated user files remain intact. No release, installation, GitHub
comment/closure or listening approval is implied.

## Reproduction and retained evidence

Ignored evidence lives in `build/validation/xl-attack99-20261109/`: a frozen
core/desktop source snapshot including the previous #13 correction, source
hashes and working diff; old/new executables, native sweeps, physical oracle
logs, independent reference/before/after metrics and listening WAVs, plugin
build/test logs and performance evidence. Reference dependency is pinned at
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`; the existing private bank is version
5, SHA-256 `3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
ROMs are hash-validated v8.21 and remain private. User ROM caches are read-only.

```sh
XL_TOOLS/cineol_xl_controls_check ROM_DIRECTORY XL_BANK --endpoints
XL_TOOLS/cineol_xl_control_regression_check XL_BANK --depth
PLUGIN_BUILD/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check \
  --xl-depth-check ORIGINAL_BANK XL_BANK
```

The listening pair `audio/plate-before.wav` / `audio/plate-after.wav` and
`audio/plate-reference.wav` uses the same input/gain, with input stopping at
2.5 seconds. Silence cannot be level-matched to the restored output. No
listening review or physical-unit capture was performed. Checks are local
macOS arm64; they do not establish Windows/Intel runtime or Daisy CPU margin.
