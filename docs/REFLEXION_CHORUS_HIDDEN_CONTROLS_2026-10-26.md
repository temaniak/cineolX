# Chorus and original-224 hidden controls: fresh Reflexion comparison

Date: October 26, 2026. This report supersedes the unresolved Chorus/DSP
hypothesis and manual-only hidden-control assumptions in the earlier GitHub
triage. This investigation changes no production code or installed plugins.

November 2 follow-up: the [manual audit](MANUAL_CONTROL_AUDIT_2026-11-02.md)
adds reset/register tests and preserves the unresolved conflict between the
manual-defined rich Chorus controls and observed v8.21 interpolation behavior.
Zero targets do not establish hardware unavailability.

## Reference and boundaries

The reference is the project's pinned Reflexion commit
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, with original-224 v4.4 and 224XL
v8.21 private chips recognized by the existing SHA-256 loaders. Dependency
checkouts remain clean. Tests use physical button/pot/LARC operations to reach
settings; logical record values are observed afterward.

The usual build-only Reflexion scheduler compatibility/performance export was
used. The 8080 host, DSP ISA, analog I/O and LARC operator headers are identical
to the pinned dependency. The exported Engine adds a silent-import mode; the
XL paired audio probes do not call that mode. No emulator or ROM execution is
introduced into Cineol's realtime audio path.

## XL Chorus: what the two products actually do

| Program | Reflexion generic editor | v8.21 ROM with Mod Enhancement on | Current Cineol |
| --- | --- | --- | --- |
| Concert Hall, positive control | Shows Chorus | Two interpolation targets; Chorus changes controller period/step and writes modulated DSP tap pairs | Shows Chorus; uses the native interpolation controller |
| Rich Plate | Shows Chorus | Parameter record and period/step change, but interpolation target count is zero in tested modes | Hides Chorus because the prepared profile has zero interpolation targets |
| Rich Split | Shows shared LR Chorus | Same zero-target result in tested modes | Hides shared LR Chorus for the same reason |

Reflexion's catalog includes all firmware-named controls. Its generic editor
builds rows for `Slider::named()` entries and forwards physical fader moves;
it does not require proof that a named parameter has active DSP destinations.
The v8.21 catalog has a 32-position Chorus table, displayed 00–97, for all
three programs above. Rich Plate uses page 2/slot 3; Rich Split page 3/slot 3.

Cineol instead checks `(modulation.flags & 15) != 0` before exposing Chorus.
The discrepancy is therefore established at the UI eligibility boundary.
Seeing a Chorus slider in Reflexion is not proof that the rich algorithms
have a missing interpolation mechanism in Cineol.

### Fresh physical and audio evidence

The primary sweep contains 27 independent reference/native paired fixtures:
three programs × factory/minimum/maximum Size requests × physical Chorus
2/128/250. Mode Enhancement is verified in the actual record after physical
key operations; Dynamic Decay and Decay Optimization are off in this sweep.
Both Rich Split Size controls are moved for the size endpoint cases.

Each fixture renders six seconds at 48 kHz: one second silent warmup, two
seconds deterministic noise plus tones, then three seconds stopped input.
All four engine outputs are inspected. Native and reference clocks run
independently; no reference DSP state, delay memory or modulation phase is
injected into native audio. Signal RMS covers seconds 2–3 and silent-tail RMS
seconds 3–4. No short-window percentage difference is used as a sound defect.

| Observation over six seconds | Concert Hall | Rich Plate | Rich Split |
| --- | --- | --- | --- |
| ROM interpolation targets | 2 | 0 | 0 |
| Prepared native interpolation targets | 2 | 0 | 0 |
| ROM modulation DSP writes per fixture | 1,722–27,642 | 0 | 0 |
| DSP rows changed between observation boundaries | 4 | 0 | 0 |
| Largest reference RMS spread across Chorus, fixed Size | Signal 0.81 dB; tail 1.26 dB | Signal 0.00161 dB; tail 0.00199 dB | Signal 0.00010 dB; tail 0.00135 dB |

For the two rich programs, all DSP writes, not merely writes attributed to the
interpolation routine, are zero in these fixtures. Native signal/tail metrics
and checksums are identical across the three Chorus positions at each Size.
The tiny independent-reference RMS variations are not evidence of a working
delay-interpolation Chorus. Preparation and controller/converter phase remain
possible sources of small variation; this is not a listening claim.

A separate physical sweep checks all eight Rich Plate variations at the same
three Chorus positions: 24 more fixtures, with one-second modulation-write
observation windows. All retain zero interpolation targets and zero modulation
DSP writes. Rich Split has one catalog variation, already covered above.

The physical Chorus compiler itself is active in the rich programs: requested
raw values 2/128/250 are retained and produce period/step 17/4, 1/4 and 1/64.
The controller still executes, and its call cadence changes. Thus this report
does not claim every secondary controller-timing effect is identical. The
measured result is specifically the absence of interpolation destinations and
writes in the documented modes.

### Secondary timing with Dynamic/Optimization enabled

A further 12 paired audio fixtures use Chorus 2/250 at factory Size, with
Mod Enhancement on and Optimization, Dynamic, or both enabled. Dynamic cases
set physical LF/Mid Stop controls to 18 and Stop Delay to 10 to make the
input-silent transition measurable. There are still zero interpolation targets
and zero modulation-routine DSP writes. Dynamics/optimization perform their own
DSP coefficient writes, however, and the reference tails now show sensitivity
to Chorus that is absent from the current native renders.

The zero-target modulation routine still has period-dependent CPU work. Four
additional physical replay fixtures directly count the level-controller entries
over three seconds of noise followed by silence, with both Dynamic and
Optimization enabled:

| Program | Physical Chorus | Slow controller entries | Fast controller entries |
| --- | --- | --- | --- |
| Rich Plate | 2 | 140 | 700 |
| Rich Plate | 250 | 225 | 1,126 |
| Rich Split | 2 | 137 | 685 |
| Rich Split | 250 | 225 | 1,122 |

Thus the reference's controller cadence demonstrably changes even though
there is no delay-interpolation Chorus. Cineol's `concert48.hpp` advances the
fast/slow controllers using fixed `DynamicsProfile` nominal rates captured
during bank preparation. Changing the unused interpolation Chorus value does
not update those rates. This is a remaining controller-timing/configuration
gap; it should not be described as merely a missing label.

For the six-second fixtures, the selected stopped-input RMS window changes
by approximately -0.73 dB in Rich Plate and -0.24 dB in Rich Split with Dynamic
on / Optimization off when Chorus moves from 2 to 250. The corresponding
native window is identical across Chorus. These are narrow diagnostic windows,
not decay-time fits or an acceptance threshold. Multiple levels, seeds,
fall/retrigger phases and listening are needed before accepting an envelope
correction. The measured controller-entry change is independent evidence of
the timing dependency, not a conclusion drawn only from a tail percentage.

### Consequence for the Chorus TODO

The earlier suggestion that these two missing sliders necessarily require a
new native interpolation Chorus algorithm was premature. The concrete follow-up
is UI access plus the demonstrated coupling between Chorus and level-controller
cadence, with an honest availability explanation. Showing a currently ineffective
slider alone is insufficient when Dynamic/Optimization are in use.
The change should not invent extra delay modulation and present it as v8.21 behavior.
Any intended creative extension should be named and validated separately.
The existing native Chorus in programs with interpolation targets remains a
different case; Concert Hall supplies a positive control in this new sweep.

## Original 224: Rate, modulation Depth and Optimization response

Full evidence and implementation discussion are in
[ORIGINAL_HIDDEN_CONTROLS_2026-10-26.md](ORIGINAL_HIDDEN_CONTROLS_2026-10-26.md).

| User operation, moving Pre-delay while held | Verified ROM behavior, settings 1–16 | Current Cineol |
| --- | --- | --- |
| Shift + program 6: modulation Depth | Step = 4 × setting | Factory step captured in the bank; no runtime editing control |
| Shift + program 7: Rate | Settings 2–16 set divider = setting − 1 and step = 4; setting 1 uses divider 1 / step 8 | Factory divider/step captured; no runtime editing control |
| Shift + program 8: Optimization response/amount | Sets the controller's response-period parameter to the selected value | Factory value captured; only Optimization enable is exposed |

These controls exist in the actual v4.4 firmware. Reflexion's ordinary generic
plugin editor also omits them: its catalog/operator path exposes the six normal
pots, Shift Diffusion and the Mod/Optimization switches. Its underlying machine
nevertheless executes the hidden button combinations. Cineol's ordinary reverb
Depth fader is a different control, describing distance/early-late balance.

Rate and modulation Depth have an observed last-write relationship. Depth 8
then Rate 2 ends at divider 1/step 4. Rate 2 then Depth 8 ends at divider 1/step
32. A later Depth write also replaces the doubled step from Rate 1. Independent
persistent multiplicative Rate/Depth knobs would extend the firmware behavior.

"Threshold" is an inaccurate name for the verified Shift+8 adjustment. Across
12 endpoint fixtures and 307 executed ROM comparisons, its setting changes
while the encoded level-drop constant stays 22. This constant is not 22 dB.
Shift+8 changes the evolution of optimization diffusion, rather than that
input-level trigger comparison. Other controller states can temporarily
override its response period, so it is not a simple maximum modulation depth.

### Factory values and the actual implementation gap

The physical v4.4 reference and native bank agree for all six supported
programs: divider 1, step 4, Optimization value 5. Acoustic Chamber has random
hold 128; the other programs use 32. The manual's usual plate setting of 7
does not match this supported ROM's observed factory value and is not a reason
to alter existing factory sound. Same-program selection retains hidden edits;
selecting a different program and returning reloads defaults.

For diagnosis, giving a copied native profile the period/step observed after
physical edits yields 52,124 matching modulation transitions across 108
order-dependent cases. This is a state-aligned controller diagnostic, not
independent free-running audio or listening acceptance. The normal native
runtime retains its immutable factory profile and cannot follow those edits.

The missing part is runtime-owned effective settings and host/UI/preset access,
with matching timing-helper inputs, recall rules and explicit Rate/Depth
interaction. The arithmetic does not need to be replaced with firmware
emulation. Old sessions should retain current factory defaults, and any host
parameters should be appended with stable IDs. No such feature was implemented
in this reporting block.

## Retained evidence and reproduction

XL evidence lives in `build/validation/xl-chorus-20261026/`: probe sources,
`results.csv` (27 cases), `variants.csv` (24 cases), `modes.csv` (12 cases),
`clocks.csv` (four controller-entry replays), summaries, build/run logs and
source hashes.
Original-224 evidence lives in `build/validation/original-hidden-20261026/`.
Private ROMs/banks were read in place, never added to repository sources.

The initial XL process capture produced 20 complete fixtures before its
180-second observation budget expired. Seven remaining fixtures were resumed
from the CSV; the final result has all 27 unique cases and successful process
exits. REA capture ID:
`ev_6826407df2e7b21ccaeba5282313ea43a2b14a9bbdfde3e5472a7f9d191dbbda`.
The timeout record is not treated as a complete campaign pass. The complete
CSV and local continuation log provide the remaining coverage.

Portable reproduction of the primary XL sweep, from the repository root:

```sh
c++ -std=c++20 -O2 -ffp-contract=off -I . -I build/deps/reflexion \
  build/validation/xl-chorus-20261026/probe-size-sweep.cpp \
  -o build/validation/xl-chorus-20261026/probe-size-sweep
build/validation/xl-chorus-20261026/probe-size-sweep ROM_DIRECTORY XL_BANK OUTPUT_CSV
build/validation/xl-chorus-20261026/probe-size-sweep ROM_DIRECTORY XL_BANK OUTPUT_CSV --resume
python3 build/validation/xl-chorus-20261026/summarize.py

# Additional Dynamic/Optimization audio and controller-cadence probes:
c++ -std=c++20 -O2 -ffp-contract=off -I . -I build/deps/reflexion \
  build/validation/xl-chorus-20261026/probe.cpp \
  -o build/validation/xl-chorus-20261026/probe
build/validation/xl-chorus-20261026/probe ROM_DIRECTORY XL_BANK MODES_CSV --modes
c++ -std=c++20 -O2 -ffp-contract=off -I . -I build/deps/reflexion \
  build/validation/xl-chorus-20261026/clocks.cpp \
  -o build/validation/xl-chorus-20261026/clocks
build/validation/xl-chorus-20261026/clocks ROM_DIRECTORY XL_BANK
```

No physical Lexicon listening review, other firmware version, full musical
acceptance, register storage/recall or realtime CPU-margin measurement is
claimed. Existing evidence and all current worktree fixes are preserved.
