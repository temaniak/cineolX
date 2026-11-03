# Global Clean and Dark Hall variation — November 2, 2026

## User controls

The main **Cineol-X 224** plugin now has **Settings → Clean**, applying the
accepted fixed-point Clean A/B method to all **28 algorithms**: six original
224 programs and all 22 XL programs, including effects and split programs.
The switch also selects clean panel, bezel, fader, quick-key and menu textures.
The existing physical faders and quick keys retain their shapes and positions. The
[ImageGen asset record](CLEAN_SKIN_ASSETS_2026-11-02.md) documents the six edits.

Dark Hall has an additional **Variation** parameter on **page 5, fader 3**,
after Size and Stop Delay. It uses an ordinary one of the six vertical faders,
with no extra physical label or separate slider. Its screen value shows
**Stable** at 0, **Original** at 1, and a percentage between them. Compact mode
uses the same editable screen value. Double-click restores Original.
Variation operates in both precision modes and is independent of Clean.

Clean defaults off and Variation defaults to Original. Existing sessions and
presets therefore retain their original precision and Dark Hall behavior.
Both controls are saved in sessions and sound presets. Existing plugin IDs,
host parameter IDs, indices and version hints remain unchanged; the new
parameters are appended with version hint 9. Preset schema 7 adds the two
values; schemas 1–6 and sessions without them receive their legacy defaults,
including when recalled into an already-modified instance.

## Precision and sound

Clean adds 12 fractional signal bits to the original 16-bit fixed-point
recurrence, held in signed 64-bit containers. Input conversion retains the
original gain-ranging thresholds and scale with finer quantization, and output
conversion omits the final coarse DAC truncation. The original six-bit
coefficients, word wrapping, scaled accumulator limits, delay network, clocks,
modulation, detector/controller scheduling and converter filters remain.
This is the same precision method accepted in the
[seven-program A/B trial](CLEAN_AB_PROTOTYPE_2026-11-02.md).

The build creates namespace-isolated precision copies from the current native
sources using checked substitutions and a source hash manifest. Bank clones
are prepared and published outside audio processing. No ROM executes in the
audio callback. Original and Clean keep separate, warm delay histories behind
one common Space/host-rate path. A 480-internal-frame ramp changes precision
without resetting the ringing tail. At nominal clock this is 10 ms; Space
lengthens that interval. Host latency, dry path and four-output routing remain.
Spillover tails also receive a global Clean change.

Both precision engines run even when one is inaudible. This deliberately adds
CPU and memory cost to preserve tails during a mode change. Dirt continues to
control filter bandwidth independently; Clean does not automatically replace
or brighten the filters. Clean is not an output gate, expander or denoiser and
does not itself correct the original Dark Hall feedback instability.

## Stable → Original implementation

The [firmware investigation](DARK_HALL_RUNAWAY_2026-11-02.md) and
[cause/intervention analysis](DARK_HALL_FEEDBACK_CONTROL_2026-11-02.md) establish
an unstable coupled feedback mode in the tested original v8.21 algorithm.
The additional variation controls that internal return rather than reducing
the external wet level. It retains the v8.21 topology at both ends; Stable
is **not** a claim of exact v8.1 firmware emulation.

For each current feedback-coefficient snapshot, take the maximum absolute
Mid and LF return coefficients in the two channels (rows 41/42 and 94/95).
In original integer coefficient units, let that maximum be `m`:

```
stable_gain = min(1, 19 / m)      # unity when m <= 19
gain_target = stable_gain + variation * (1 - stable_gain)
```

The gain is Q24, applied to the four existing LF/Mid product operands.
Original six-bit coefficients remain intact. At Variation 1 the multiplier
is bypassed exactly. Where existing coefficients already lie below the cap,
both endpoints bypass the correction. Excessive long-decay feedback is
reduced at Stable, rather than zeroing the feedback network. Gain changes
have a bounded slew of 20 gain units per second and retain delay memory.

This is an empirical, conservative stable endpoint supported by bounded
tests, not a proof for every possible automation trajectory or infinite-decay
combination. Lower values necessarily shorten or change the long-lived modes;
the control adjusts decay/growth, not a guaranteed oscillation output level.
Original deliberately preserves the original runaway where it occurs.

## Validation and practical limits

Private evidence is under `build/validation/clean-all-20261102/`.

- A zero-extra-bit export matches the native engines exactly across all 28
  programs with Mod/Opt both off and on: maximum difference 0, with no
  selection or processing allocations/releases.
- The actual Clean export passes 56 stopped-input fixtures, 30.5 seconds each.
  On 48 cases with measurable original late residuals, the median AC reduction
  at 28.5–30.5 seconds is **73.14 dB**, minimum **61.68 dB**. The highest Clean
  residual is **−160.31 dBFS**. Eight other cases already reach numerical zero
  in Original. Residual RMS includes any remaining reverberation, so these are
  fixture results rather than independent hardware noise-floor measurements.
- In the retained 0.25–0.5 second active windows, the largest band-energy change
  is **0.0164 dB**, over five bands spanning 40 Hz–10 kHz. Short-window spectral
  agreement does not by itself establish listening equivalence.
- All 28 programs' four main-plugin outputs exactly match independent Clean
  engine instances, including restoration after a warm-tail precision toggle.
  All 28 also pass 44.1/48/96 kHz, host block/offline invariance and wet-only
  low-latency checks. Clean Spillover crosses all programs at all three rates;
  compensated and direct dry outputs match their Original counterparts.
- Stable passes **144** long-tail cases: both precision paths; six LF/Mid
  pairs; two Treble values; three Size values; Mod/Opt both off and on.
  Each uses a 0.2-second noise burst followed by silence through 45 seconds.
  The highest final AC level is −86.92 dBFS in the quantized path. All cases
  fall to their residual floor or decrease by more than 3 dB after second 10.
- Ordinary short-decay settings produce identical samples at Stable and
  Original. Moving Variation retains delay memory; the largest gain step per
  48 kHz host frame is 10,519 Q24 units, within its bounded slew. Processing and
  setters allocate and release no C++ heap memory in these checks.
- Dirt still changes the Clean output in both original-224 and XL fixtures;
  its filter-bandwidth control remains independent of precision.
- The main processor regression, all-page fader/number/automation checks,
  help coverage, hidden-control regression and separate seven-program A/B
  plugin checks pass. Clean skin, page-5 Variation endpoints, reopened editors,
  v7 preset/session round trips and v6 migration pass in full and compact views.
- The installed AU passes Apple's `auval` validation. AU, VST3 and Standalone
  retain their identifiers and numeric version, contain arm64 code and pass
  strict code-signature verification. The initial sandboxed `auval` could not
  see the component registry; validation outside that sandbox succeeded.

Steady nominal-clock CPU was measured after the build and other DSP checks
completed: all 28 programs × both Clean switch states, 48 kHz, 128-frame
blocks, 3 measured seconds per case after warmup. Offline wall time is
**4.22–6.26% of one core** on this arm64 macOS machine, with per-case callback
p99 between **0.153 and 0.272 ms**. Both precision engines run in either switch
state. These are steady processing measurements, excluding plugin creation
and program activation; a retained Spillover tail adds another active slot.

A five-point Clean sweep at the problem LF/Mid settings with Size 128 gives
the following pre-overload envelope fits over seconds 5–12. They describe
one fixture, not a universal slider-to-decay calibration:

| Variation | Internal gain | Slope, dB/s |
| --- | ---: | ---: |
| Stable / 0 | 0.79167 | −5.58 |
| 25% | 0.84375 | −3.23 |
| 50% | 0.89583 | −0.90 |
| 75% | 0.94792 | +1.54 |
| Original / 1 | 1.00000 | +4.11 |

Original eventually saturates in this fixture. Fits after saturation are not
growth rates. Differences from the earlier reported +2.8 dB/s fixture include
the explicit Size setting here; they are not contradictory measurements of an
identical control state.

The earlier user listening acceptance covers the seven-program prototype's
tested material. This all-program rollout and the Stable endpoint still need
user listening review. Offline CPU numbers do not prove a host's realtime
margin or embedded/Daisy suitability.

## Reproduction

Configure the usual main plugin with `CINEOL_BUILD_CLEAN_AB=ON` to build the
optional comparison tools. The main precision engine itself is always built.

```sh
cmake --build build/clean-main --parallel 3 --target \
  NativeHall224_Standalone NativeHall224_AU NativeHall224_VST3 \
  native_hall_plugin_check cineol_clean_catalog_check \
  cineol_clean_zero_catalog_check cineol_dark_stability_check

build/clean-main/native-hall/clean-ab/cineol_clean_zero_catalog_check ORIGINAL_BANK XL_BANK
build/clean-main/native-hall/clean-ab/cineol_clean_catalog_check ORIGINAL_BANK XL_BANK METRICS_DIR
python3 native-hall/clean-ab/analyze_catalog.py METRICS_DIR
build/clean-main/native-hall/clean-ab/cineol_dark_stability_check XL_BANK STABILITY_CSV
build/clean-main/native-hall/clean-ab/cineol_dark_stability_check XL_BANK --edges
build/clean-main/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check \
  --clean-check ORIGINAL_BANK XL_BANK ABSOLUTE_SCREENSHOT_DIR
```

No ROM, bank or capture is embedded in the plugin or delivery. Existing private
fixtures and installed imported caches are reused. The pinned dependency
checkout remains clean. No public release, tag or version change is made.

## Local delivery

The test AU, VST3 and Standalone are installed in the user's existing plugin
and `~/Applications` locations. Previous bundles are preserved under
`out/local-install-before-clean-20261102/`; imported ROM caches and user presets
are untouched. Hosts already holding the old plugin need to reopen/rescan it.

The portable review archive is
`out/Cineol-X-224-Clean-Stable-macOS-arm64-2026-11-02.zip`, containing the three
bundles, instructions, current screenshots, validation notes and file hashes.
It uses the existing development identity; no public release is published.
Private install/bundle/archive identities are retained with the validation logs.
