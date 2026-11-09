# Rich Plate / Rich Split Chorus recheck

Date: November 9, 2026. Investigation of GitHub
[#15](https://github.com/temaniak/cineolX/issues/15) and
[#18](https://github.com/temaniak/cineolX/issues/18). No new production DSP
correction is supported by these checks; the issues remain open pending any
more specific reproduction conditions from the reports.

## Finding

For the supported **224XL v8.21** firmware, Rich Plate and Rich Split have
**zero interpolation destinations** and no Chorus-driven delay-address writes.
Their Chorus control changes the time spent in the modulation worker, which
changes the cadence of the level/decay controllers. It does not provide the
usual moving-delay/pitch-chorus sound in these two programs.

The measured native behavior, including the complete host parameter path, is:

| Conditions | Chorus response |
| --- | --- |
| Mode Enhancement off | No change to rendered audio |
| Mode Enhancement on, Dynamic Decay and Decay Optimization off | No change to rendered audio |
| Mode Enhancement on, Dynamic Decay and/or Decay Optimization on | Lower-half changes can alter decay/optimization timing and the tail |
| Chorus 50–97, regardless of modes | No additional native timing or audio change |

There are 32 positions. The lower half, displayed 00–47, changes the worker's
period. The upper half, displayed 50–97, changes a modulation step that has no
interpolation destination here. A test sweeping only the upper half, or using
Mod off, or expecting ordinary chorus with both dynamics controls off can
therefore accurately report no audible change without identifying a defect.

The Rich controller, Chorus timing helper, Native48 integration and Runtime
files have no differences from the `v1.1a` tag. The preceding November 9
pre-delay and Attack/Depth fixes do not add this behavior; it was already in
the release source. The existing tooltip describes the same restrictions.

## Current validation

- Rebuilt `cineol_xl_rich_chorus_check`: all 32 worker-budget positions, both
  programs, finite stopped tails, low/high response with active controllers,
  exact upper-half invariance and Mod-off invariance pass.
- Rebuilt `cineol_xl_rich_timing_check`: all 24 cadence cases pass the existing
  5% / two-poll tolerance against the retained physical-ROM observations;
  gradual fall/retrigger, retained live state, request cancellation/coalescing
  and mode/Size checks pass.
- Added plugin `--rich-chorus-check ORIGINAL_BANK XL_BANK`: both programs,
  Original and Clean precision, all eight switch combinations, positions
  00/50/97, four output routes, 48 kHz and 128-frame blocks. The 32 combinations
  require exact equality where Chorus has no applicable action and a changed
  stopped tail where it does. There are 96 complete six-second renders.
- Four extra live-automation renders switch Chorus from 00 to 97 at frame
  128,000 during excitation. Pre-edit audio matches the untouched run exactly;
  the subsequent stopped tail changes. This verifies changes after engine
  activation, rather than only initial preset loading.
- All plugin callbacks allocate/release nothing. The new source changes are
  tests/documentation only; no new performance claim or redundant all-catalog
  CPU campaign is made. The immediately preceding full 28-program regression
  passed before these test-only additions.

## Independent firmware check

The retained November 2 40-case native/reference matrix is reused for all
eight mode combinations and the two established seeds/levels. A fresh narrow
check adds **12** physical-reference/current-native cases: two programs,
Mod-only and all-three modes, and physical faders 2/128/250 (displayed
00/50/97). Physical program selection and toggle operations are verified in
the actual logical record.

Each fresh case boots for 16 seconds, selects the program and controls with
the LARC operator, settles for 0.5 seconds, then captures six seconds at 48 kHz
in 256-frame blocks. Input has one second silence, two seconds of seed-17 noise
plus tones (left noise peak .03 with tone .015; right .021 with tone .013), then
three seconds input silence. The native engine receives only logical controls
and input. It runs its own empty audio history, control clock and converters;
no reference state or phase is copied. All four outputs are measured.

All 12 reference cases have zero interpolation destinations and zero WCS
writes from the modulation routine. With Mod-only, changing Chorus 00 to 97
changes modulation-call counts from about 5,630 to 9,955 per capture, but does
not materially change sound. When all controllers are active, the tail changes:

| First stopped-second RMS, 97 minus 00 | Firmware reference | Native |
| --- | ---: | ---: |
| Rich Plate, Mod only | −0.00031 dB | 0 dB |
| Rich Split, Mod only | +0.00029 dB | 0 dB |
| Rich Plate, all three modes | −0.614 dB | −0.510 dB |
| Rich Split, all three modes | −0.247 dB | −0.245 dB |

These are window diagnostics, not decay-time fits or listening acceptance.
Independent physical setup/controller phases leave small residual differences:
for example Rich Plate's fresh upper-half pair differs by 0.114 dB in this
short tail window, despite the same zero-destination work law. No change to
the native graph is justified by that short-window difference. Existing native
floor and peripheral-timing limits remain as documented in the
[November 2 timing correction](XL_RICH_DYNAMIC_TIMING_FIX_2026-11-02.md).

The observed v8.21 behavior is narrower than the generic chorus description in
the retained manufacturer manual. This check does not prove the behavior of
every physical unit or other firmware revision. No extra LFO or delay-tap
modulation is added to make the knob produce a conventional chorus effect.

## Evidence, listening and next diagnostic

Ignored current evidence: `build/validation/rich-chorus-check-20261109/`.
It includes current check/build logs, 24 cadence rows, the fresh physical
reference harness/source and CSV, the retained-metric extraction, tag diff,
bank/source hashes and commands. Reference is pinned Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`; the SHA-validated v8.21 bank remains
v5, hash `3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.

Only the Rich Plate / all-three pair is saved as fresh WAVs, with both native
and reference 00/97 variants. Matched files target active-input RMS 0.1 using
one common gain per native/reference pair; that gain is shared between low
and high Chorus to preserve the tail difference. Input stops at three seconds.
Raw files and gain metadata are retained. No listening review is claimed.

To expose the expected effect in a host, enable Mode Enhancement and Dynamic
Decay, select short LF/Mid Stop Decay and Stop Delay, and compare Chorus 00
with 50 after the input stops. Decay Optimization can also be enabled. This
tests the tail response rather than continuous pitch motion.

If the reported failure persists with those conditions in v1.1a, the next
useful evidence is the exact preset/settings, host/build identity and input
capture. The current issue bodies do not contain those conditions; these tests
are insufficient grounds to close the reports or claim physical listening
acceptance. No release, installation or GitHub mutation occurred.

```sh
XL_TOOLS/cineol_xl_rich_chorus_check XL_BANK
XL_TOOLS/cineol_xl_rich_timing_check XL_BANK
PLUGIN_BUILD/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check \
  --rich-chorus-check ORIGINAL_BANK XL_BANK
```
