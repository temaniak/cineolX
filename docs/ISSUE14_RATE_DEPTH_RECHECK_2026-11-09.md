# Issue #14: original-224 Mod Rate / Depth coupling

Historical diagnosis: the user subsequently requested independent desktop
controls. That follow-up removes the Rate-triggered Depth reset while keeping
the native DSP mapping and saved values. See
[the assembled validation](FOUR_FIXES_VALIDATION_2026-11-09.md). The firmware
observations below describe the original behavior, not the new desktop policy.

Date: November 9, 2026. Recheck of
[#14](https://github.com/temaniak/cineolX/issues/14) against the supported
original **Lexicon 224 v4.4** firmware. The observed reset is firmware behavior,
not a newly reproduced native implementation defect.

## Verified control law

| Last physical operation | Firmware result | Effective plugin values |
| --- | --- | --- |
| Shift + program 7, Rate 2–16 | Period = Rate − 1; fractional step = 4 | Depth becomes 1 |
| Shift + program 7, Rate 1 | Period = 1; fractional step = 8 | Depth becomes 2 |
| Shift + program 6, Depth D | Fractional step = 4 × D; period retained | The newly selected Depth replaces the preceding step |

Rate and modulation Depth write shared modulation state; they are not two
independent persistent factors in this firmware. In particular, Depth 8 then
Rate 4 produces period 3 / step 4 (effective Depth 1), whereas Rate 4 then
Depth 8 produces period 3 / step 32 (Depth 8). The useful adjustment order is
therefore **Rate first, Depth second**.

This concerns the hidden modulation Depth control, not the ordinary reverb
Depth/distance fader. The claim is limited to the supported v4.4 ROM; it does
not establish the control law of other Lexicon firmware versions.

## Current recheck

The reference runs the unchanged pinned Reflexion dependency
`f68ea1d069fef4a5663201693bfdfa1c579ffd69` with all five SHA-validated private
v4.4 ROMs. Selection uses CALL/program-button operations. Each edit uses real
Shift + program 6/7 holds and movement of the Pre-delay pot, followed by key
release/settling. No logical control RAM is written to obtain the result.

The narrow probe covers all six original programs, every Rate 1–16 and both
orders, with Depth 8 as the non-default marker: **192 state observations**.
The actual period and step bytes are read after the physical operations. Rate
edits must produce step 4 (step 8 at Rate 1), and the subsequent Depth edit
must restore step 32 while retaining the period. Ordinary Pre-delay and
Optimization Response must remain unchanged.

The current plugin `--hidden-check` passes all six programs and every Rate,
both edit orders, the Rate-1 exception, immediate canonical host values,
deferred full/compact UI mirrors, same/different-program behavior, Mod/Opt
toggles, immediate session save/restore, sound presets and legacy migration.
Moving audio and direct host setters allocate/release nothing. Its desktop
comparison also passes 36 directed program switches, supported pre-delay
ranges and ordered hidden controls at 48 kHz.

The existing November 2 independent ROM controller regression is reused
rather than repeated as a full audio campaign. Its 180 fixtures had zero state
or instruction-cost errors across 148,131 modulation and 8,223 level-controller
calls; see [the implementation report](HIDDEN_CONTROLS_FIX_2026-11-02.md).
Those are diagnostic controller checks, not a listening-identity claim.

`Original224Parameters.hpp` and its existing plugin regression have no changes
from `v1.1a`. The current coupling was implemented intentionally, and the
existing Rate tooltip already states both reset values and the adjustment
order. No production code, parameter identity, preset schema, sound behavior
or installed plugin is changed by this recheck.

## Evidence and reproduction

Ignored current evidence is under
`build/validation/issue14-rate-depth-20261109/`: physical probe source/binary,
192-row panel CSV, command/hash provenance, plugin log and empty v1.1a source
diff. Private ROMs and user caches remain read-only and are not copied into
tracked files. Current reports/metrics and reproduction sources are retained;
no disposable artifact removal was needed.

```sh
PLUGIN_BUILD/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check \
  --hidden-check ORIGINAL_BANK XL_BANK
```

This resolves the technical question behind #14: the reset agrees with the
supported original firmware. No GitHub reply or closure is performed. A policy
that retained Depth independently when Rate changed would be a separate
behavior change requiring its own request and sound evaluation.
