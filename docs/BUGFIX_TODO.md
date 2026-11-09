# GitHub bugfix queue

Requested October 26, 2026. Keep existing plugin/parameter identities and private
ROM/cache compatibility. See `GITHUB_ISSUE_TRIAGE_2026-10-26.md` for initial evidence.

1. [x] #5: correct maximum XL HF Bandwidth/Treble Decay; verify the physical
   firmware endpoint and add audible-output/silent-tail regression coverage.
2. [x] #4: stop classifying non-reverb voice cells as Dynamic Decay parameters;
   restore V1/V2 in Chorus & Echo, Resonant Chords and Multiband Delay.
3. [x] #6/#7: restore functional Inverse Room Mid Decay and both CD Plate LF
   Decay controls, including display, gestures, automation and state recall.
4. [x] #2: correct bypass latency and transition discontinuities, preserve
   evolving DSP state, and validate both engines, sample rates and block sizes.
   Local processor validation passes; Cubase-specific acceptance remains open
   because Cubase is not installed on this host.
5. [x] #8/#9 physical Chorus investigation against pinned Reflexion v8.21:
   Rich Plate/Rich Split accept Chorus changes but have zero interpolation
   targets/writes in the tested modes and Size ranges. All eight Rich Plate
   variations agree. Reflexion shows every firmware-named slider; Cineol hides
   zero-target Chorus. The presumed missing interpolation mechanism is not
   demonstrated. See `REFLEXION_CHORUS_HIDDEN_CONTROLS_2026-10-26.md`.
6. [x] #8/#9 restore the two firmware-named rich Chorus controls and Mode
   Enhancement, and add the missing ROM-derived Chorus-dependent monitor work
   budget. Preserve the upper half's lack of timing effect with zero targets,
   the fixed Mod-off branch, host identities and session recall. Forty-eight
   independent sound fixtures, native/ROM component checks, all-catalog plugin
   regression and paired CPU evidence are retained in
   `RICH_CHORUS_FIX_2026-11-02.md`. This completes the bounded Chorus correction;
   it does not establish full native/reference clock or listening identity.
7. [x] #3 physical Shift+6/+7/+8 verification: all settings 1–16 and all six
   programs checked. Rate resets the fractional step; a later Depth overwrites
   it. Shift+8 changes Optimization response, while the encoded level-drop
   comparison remains 22. ROM and prepared-bank factory Optimization are 5
   for all six programs. The generic Reflexion editor also lacks these controls.
8. [x] #3 runtime/UI follow-up: add effective settings and stable appended host
   IDs with explicit Rate/Depth last-write semantics, unchanged legacy defaults,
   program recall/retention, presets, controller timing and sound validation.
   Include the newly verified Mod-key reset of all hidden values, Optimization
   toggle retention, same-program retention and effective-value preset recall.
   Keep Optimization's inverse response scale and distinguish modulation Depth
   from normal reverb Depth and Diffusion.
   Implemented and verified November 2; see
   `HIDDEN_CONTROLS_FIX_2026-11-02.md`. All 180 ROM fixtures pass state/cost
   checks, legacy factory sample hashes match, and independent audio/CPU
   evidence is retained. Level-matched listening acceptance remains open.

9. [x] Correct the large Rich Plate/Rich Split Dynamic/Optimization timing
   discrepancy with coupled native controller/compiler work, gradual writes,
   fresh detector reads and coherent live parameter updates. All 24 cadence
   fixtures pass the permitted small-error tolerance, maximum 4.34% instead of
   31.49%; 48 sound fixtures, all-program plugin/CPU checks and retained floors
   are documented in `XL_RICH_DYNAMIC_TIMING_FIX_2026-11-02.md`. Small peripheral
   timing differences are explicitly acceptable to the user; do not pursue
   mathematical identity. Other XL programs retain their current clocks;
   investigate them further only against a material measured sound mismatch.

10. [x] #13: cap both original-224 Plate pre-delays to the physical v4.4
    limit of 107 ms. Moving over-limit settings previously crossed into other
    delay lines and produced a large burst / overexcited tail. Preserve host
    automation identities and stored values, protect existing banks, and align
    editor/text/import limits. Physical-ROM evidence, old-fails/new-passes and
    independent before/after audio are in `ISSUE13_PLATE_PREDELAY_2026-11-09.md`.
    DAW listening acceptance remains open; no release or issue closure implied.

11. [x] #16/#17/#19/#20: resolve XL Attack/Depth raw 255 to the physical
    254 endpoint, still displayed as 99. Protect all 23 controls across the
    19 reverbs, full/compact UI and old presets/automation; preserve internal
    sentinel compilation, host IDs and bank ABI. The 520-case physical oracle,
    independent sound comparisons and plugin regressions are recorded in
    `XL_ATTACK99_ENDPOINT_FIX_2026-11-09.md`. Listening acceptance remains open.

12. [x] #15/#18 verification: Rich Plate/Rich Split Chorus reaches the native
    controller through live plugin automation in both precision modes. All
    eight switch combinations confirm conditional tail effects and upper-half
    invariance; fresh v8.21 references retain zero moving taps. No new DSP
    defect reproduced. See `RICH_CHORUS_RECHECK_2026-11-09.md`; exact reporter
    conditions and listening remain open, so issue closure is not implied.

13. [x] #14 verification: original v4.4 physical Shift+7 edits reset the
    modulation step, giving Depth 1 at Rate 2–16 and Depth 2 at Rate 1. A later
    Shift+6 Depth edit replaces that step. The plugin initially agreed;
    see `ISSUE14_RATE_DEPTH_RECHECK_2026-11-09.md`. No DSP change or GitHub
    issue closure is implied.

14. [x] #14 requested desktop behavior change: retain Mod Depth on every Rate
    edit and retain Rate on Depth edits. Keep the native period/step mapping,
    stable parameter IDs and stored values. This deliberately supersedes the
    panel coupling described above; the physical-ROM observations remain valid.
    See `FOUR_FIXES_VALIDATION_2026-11-09.md` for the assembled build and signal checks.

Linux VST3 (#1) is outside this work at the user's request. No release or
GitHub issue closure is implied by this queue.
