# Issue #13: original-224 Plate pre-delay overexcitation

Date: November 9, 2026. Local correction for
[#13](https://github.com/temaniak/cineolX/issues/13), not a published release
or a claim of listening acceptance.

## Cause and reproduction

The original v4.4 firmware limits both **Vocal Plate** and **Percussion Plate A**
to **107 ms** of pre-delay. Physical panel operations in the pinned Reflexion
reference (`f68ea1d069fef4a5663201693bfdfa1c579ffd69`), using the SHA-validated
private ROM set, show 107 ms for pot requests 216, 220 and 253. The other three
Hall programs reach 152 ms. Chamber's wider firmware range is outside this fix;
its existing supported native range is retained.

The native importer wrote the pre-delay compiler record directly and swept
129 positions for every program, bypassing that panel limit. Runtime and host
controls likewise accepted all 129 Plate positions. With the existing bank,
the relevant read offsets are:

| Requested Plate pre-delay | Left, row 1 | Right, row 51 |
| --- | ---: | ---: |
| 0 ms | 14228 | 12071 |
| 107 ms | 16368 | 14211 |
| 108 ms, unreachable through the original panel | 4 | 14231 |

The 14-bit left address wraps at 108 ms, and both reads enter memory used by
other delay lines. Moving across that boundary while the network contains
audio produces a large burst / overexcited tail. Static startup at an excessive
pre-delay did not reproduce the report at factory controls, so checking only
fixed endpoints would have missed it. A separate factory XL Plate endpoint
probe did not reproduce it either; this correction concerns the original 224.

The reproduced fixture is stereo deterministic noise, peak input 0.1 on the
left and 0.07 on the right, four seconds of excitation followed by six seconds
of silence at 48 kHz. Starting at one second, the pre-delay increases from 0
to 128 ms, one millisecond per 25 ms, with control updates every 128 frames.
Mode Enhancement and Decay Optimization are enabled.

| Default fixture | Peak before | Peak after | RMS at 5–6 s before | RMS after |
| --- | ---: | ---: | ---: | ---: |
| Vocal Plate, original precision | 0.984858 | 0.116719 | 0.0093071 | 0.000617936 |
| Percussion Plate A, original precision | 0.977534 | 0.106635 | 0.0166973 | 0.000515774 |
| Vocal Plate, Clean | 0.984684 | 0.116651 | 0.00929634 | 0.000617767 |
| Percussion Plate A, Clean | 0.978607 | 0.106648 | 0.0167001 | 0.000516025 |

These measurements describe these fixtures, not a universal tail acceptance
threshold. The original artifact remains finite because the integer network
saturates; a NaN/Inf check alone does not detect it.

## Correction and compatibility

- The shared native core resolves Plate pre-delay to 0–107 ms before reading
  its table. The generated Clean engine inherits the same correction.
- Host parameter ID, index and raw/normalized range remain unchanged. Old
  presets and automation above 107 ms resolve to 107 ms without rewriting the
  stored value. Their former over-limit sound intentionally changes.
- Physical and screen faders stop at 107 ms, and host value formatting/text
  entry use the same limit. Switching back to Hall restores its full range
  from the unchanged host value.
- New imports saturate the unreachable upper table entries to 107 ms while
  retaining the bank ABI. Existing caches are protected by the runtime check;
  no re-import or cache migration is required.
- No audio-rate processing loop, allocation, lock, I/O, plugin identifier,
  coefficient curve, delay-memory size or dependency checkout changes.

## Validation

`native_224_predelay_check` checks all 129 native positions on all six programs,
then compares two independent Plate histories: old over-limit automation and
the same trajectory capped to 107 ms. Both Plates and all four Mod/Opt modes
match sample-for-sample on all four outputs through excitation, stopped input
and retrigger. The test fails against the frozen old `hall.cpp` and passes with
both the existing imported bank and a newly prepared bank.

The importer passes 96 composed-control checks per program plus its independent
ROM-clocked modulation/decay checks for all six programs. The new bank passes
the existing row-machine arithmetic, switching, signal/tail and allocation
regression. Native before/after sweeps retain 32 fixtures per precision mode;
all 16 trajectories restricted to 100 ms have unchanged saved metrics in each
precision mode.

The plugin's `--predelay-check` passes old-session round-trip, unchanged raw
host values, physical/screen range switching, text entry, both precision modes,
44.1/48/96 kHz and 128/511-frame blocks. Moving over-limit automation matches
the capped trajectory exactly; callback allocations/releases are zero.

The existing full dual-bank plugin regression (`--banks`) also passes: all 28
programs, fader/display limits, preset/session recall, original/XL host-rate
processing, mono, Spillover and block-size/offline-realtime checks, with zero
callback allocations/releases. Before/after listening WAVs are byte-identical
for their first 177,714 frames (3.702375 seconds), before the invalid request
reaches the output.

## Evidence and reproduction

Ignored local evidence is in `build/validation/issue13-predelay-20261109/`:
physical panel logs, probes and sources, frozen pre-fix core, metrics/summary,
import/build/test logs and old-fails/new-passes status. Private ROMs and banks
remain excluded from version control and publication. The prepared test bank
is local only; user caches were read without modification.

`plate-before.wav` and `plate-after.wav` retain the default Vocal Plate moving
fixture. Both use the same gain of 0.25 (about −12 dB), preserving the burst
and decay differences. Input stops at four seconds. No listening review has
been performed; compare the transition around 3.7 seconds and the stopped tail.

```sh
cmake --build BUILD --target native_224_predelay_check native_224_extract
BUILD/native-hall/native_224_predelay_check ORIGINAL_BANK
BUILD/native-hall/native_224_extract ROM_DIRECTORY OUTPUT_BANK
PLUGIN_BUILD/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check \
  --predelay-check ORIGINAL_BANK
```

The native test targets require `NATIVE_HALL_BUILD_TOOLS=ON`. The plugin was
built and checked locally for macOS arm64. This does not establish Windows,
Intel, physical-Daisy CPU margin, a physical Lexicon listening match or direct
DAW acceptance. Current reproduction and listening evidence is retained;
cleanup removed no artifacts from this block.
