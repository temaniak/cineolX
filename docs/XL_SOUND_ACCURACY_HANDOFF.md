# Native 224XL sound baseline

Date: October 5, 2026. Engine checkpoint: `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`
on `codex/original-224-sound-accuracy`. This investigation adds offline tools
and documentation; production DSP, import, plugin parameters and installed
bundles are unchanged at that baseline. The harness changes are initially uncommitted.

## Active correction work

Later October 6 UI follow-up: the user's requested uniform controller typography
and compact default editor are implemented, built and installed as an updated AU.
See [plugin UI validation](PLUGIN_UI_VALIDATION.md). All user banks are unchanged;
full plugin regression and strict AU validation pass. No sound change or CPU rerun
is made. Expanded isolated macOS focus checks pass, but the user's intermittent
Logic return-from-another-window issue remains unconfirmed in the updated host;
Logic needs a restart to load the replacement AU. Preserve the installed bundle
and preceding rollback copy under `build/validation/ui-consistency-20261006/`.

Installed-AU follow-up, October 6: the user requested local AU installation.
The current universal bundle, including updated original 224 and XL, is installed
in the user's Components directory and passes strict system AU validation
(`aufx / Nh24 / Rflx`, exit 0). Its binary matches the final built AU SHA-256.
The previous AU is backed up in `build/validation/au-install-20261006/previous/`.
A freshly extracted, verified XL v5 cache is installed; original-224 and XL v4
user caches are unchanged. Exact identities, installation and `auval` logs are
retained in that private root. See the installed follow-up in
[final desktop validation](XL_FINAL_REPRESENTATIVE_VALIDATION.md). No DAW listening
acceptance is inferred from AU validation; the fourteen pending musical cases
remain awaiting user review.

Current checkpoint, October 6: eight distinct musical algorithms have positive
user listening feedback; the [remaining-fourteen/final desktop block](XL_FINAL_REPRESENTATIVE_VALIDATION.md)
now supplies musical comparisons for the rest of the 22-program catalog. Ordinary
reverbs use A/C, while all five splits use the plugin's A/B factory route. Five
grouped listening files await review. 111/112 new primary level bands are within
5%; CD Plate B's exception is a very quiet 8–12 kHz band around −103 dBFS. All
27 valid decay fits are within 5%; 85 unavailable/inapplicable fits remain unscored.
Current full dual-bank plugin regression passes all 28 programs at 44.1/48/96 kHz,
presets/session, controls and Spillover, with zero callback heap activity. Universal
AU/VST3/Standalone builds pass without installation. One final batched native CPU
run gives five repeats of 132 workloads: single-engine program medians about
1.50–2.75%, two-engine medians about 3.00–5.03%; raw outliers and limits are retained.
This is native processing cost, not a full DAW meter or hard realtime/Daisy proof.
No DSP correction or private pilot integration occurs. Next obtain the user's
listening feedback on these fourteen cases, preserving accepted sound and avoiding
further fitting unless a material repeatable difference is heard.

Latest user direction, October 6: prioritize practical sound similarity and
avoid extended controller investigation without a bounded sound correction.
The immediate target is the first Hall / Hall tail/retrigger discrepancy.
After a bounded cause/correction attempt, compare independent sound and prepare
level-matched listening evaluation if exact matching remains impractical.
Internal component checks remain supporting evidence, not the main deliverable.
This priority is now explicit in repository `AGENTS.md`.

The latest [bounded Hall tail/listening review](XL_HALL_TAIL_LISTENING_VALIDATION.md)
adds only four independent sound cases, changing warmup, seed or level one at
a time. Maximum primary-band RMS error stays below 2.030%, while first-tail
maximum varies from 5.9294% to 43.6075%; smaller error with a longer warmup is
not a code correction or whole-envelope pass. Restore chronology also changes
with level, so no universal startup offset or detector-retention heuristic is
accepted. Component work is bounded here. Two short scalar-level-matched clips
now compare reference/current/pilot Dynamic tails and reference/current static
sound. They are under `build/validation/xl-hall-tail-review-20261006/listening/`.
Next evaluate those clips for audible/material differences before extending
controller work. Listening remains unperformed; production and CPU are unchanged.
The user then questioned whether tail differences reflect nonlinear behavior
rather than a defect. Treat the reported short-window percentages as deviations
for a particular reference history, not proven algorithm bugs. Dynamic remains
level-dependent with Mod off; Mod-on phase adds separate variation. Assess the
original's own variation and perceptual decay/stereo character before accepting
a correction. This distinction is recorded in `AGENTS.md` and the tail report.
The saved-recording variability comparison now completes that bounded step:
three sample-identical input prefixes at 257/1,000/2,000 ms warmup show a
3.4977 dB original first-tail RMS spread. Current-source native is inside that
range in 5/6 first-tail windows and exceeds it by at most 0.0307 dB; all eight
first-tail band levels are inside the original range. This supports startup
variation for that example, not wholesale acceptance. A later current-source
window remains 1.1717 dB outside the range at 700 ms. Results/plot are in the
private tail review's `variability/`; no new WAV or CPU runs were added.
An asynchronous question requests actual listening feedback on the existing
reference/current/pilot clip. Until a listener responds, auditory acceptance
remains unperformed and no new controller correction is accepted.
The user's next listening observation reports a digital-sounding trail.
Inspection finds a late tonal residual already in the reference and both
native versions at about −95 dBFS, with prominent 280/560 Hz components;
whole-file listening normalization adds about 26.3 dB and exposes it near
−69 dBFS. Saved clip samples are exactly source × scalar gain, ruling out
extra preparation processing. Quantized feedback/DAC arithmetic is a plausible
cause, not an isolated proof. `residual-inspection.json` records the analysis.
Preliminary user listening feedback has now occurred; equivalence acceptance
and precise identification of the described sound remain open.
The latest listener outcome reports no huge difference in the presented
material. Treat current Hall / Hall as provisionally retained for this short
example and stop first-tail percentage fitting/controller expansion here.
This is not an all-mode/program acceptance or a claim that all differences
are inaudible. Next prioritize short representative-algorithm and musical-source
listening comparisons, reusing existing pairs/evidence and pursuing material,
repeatable differences only. No production integration or CPU run is performed
at this listening checkpoint.
The following [representative musical checkpoint](XL_MUSICAL_LISTENING_VALIDATION.md)
adds four current-source comparisons on a four-second local demo SH-101 riff:
Concert Hall, Plate, Room and CD Plate A, factory controls and all toggles off.
Maximum band RMS differences are 2.4771%, 0.6481%, 0.7388% and 0.4494%; active
envelope p95 is at most 1.5307%. An offline `file` fixture accepts bounded
canonical float WAVs, records actual input stop time, and passes four invalid
input guards plus exact legacy waveform-prefix regression. Production DSP is
unchanged and CPU is not repeated. Four 12.5-second level-matched musical pairs
are in `build/validation/xl-musical-listening-20261006/listening/`. The user has
now explicitly found all four pairs close. Provisionally retain these mode-0
musical cases and stop baseline fitting here. Keep Dynamic/Mod-on and untested
program/material coverage open; the next bounded block is representative
Dynamic and Mod sound behavior on musical input, rather than expanding
first-Hall tail fitting or repeating the accepted baseline without new evidence.
The next [enabled-mode musical checkpoint](XL_ENABLED_MUSICAL_VALIDATION.md)
renders only CD Plate A Dynamic (Mod/Opt off, physical gate controls) and
Concert Hall Mod (Dynamic/Opt off, factory controls), reusing the same input
and unchanged renderer/current source. Dynamic maximum band deviation reaches
69.5149% in very quiet 8–12 kHz energy (reference −102.83 dBFS); below 4 kHz
it is about 1.71%. Mod's largest band deviation is 7.3150% at 100–250 Hz.
Active-envelope p95 is 22.0847%/25.8908%; these are diagnostics for particular
independent histories, not proven audible defects. Two 12.5-second reference/
current pairs are in `build/validation/xl-enabled-musical-20261006/listening/`.
The listener now reports digital crunch during playing in both Dynamic variants;
At that stage Mod feedback was unspecified. A bounded
inspection rules out file clipping and added pair-preparation distortion. A
private native probe reports zero core saturations and reproduces 575,998 saved
frames exactly, excluding the first two wrapper latency frames. Reference internal
saturation is not measured. Input coloration versus Dynamic coefficient-transition
artifacts remains unisolated; a short dry-source clip awaits listener feedback.
Do not attribute this during-playing concern to the earlier late tonal residual.
No DSP change or CPU run is made; pursue a
material heard difference with a bounded mode-specific check, or provisionally
retain these examples and advance representative coverage if they sound close.

The listener deferred the dry-source question, then clarified a rapidly stair-like
Dynamic ending on monitors. The [bounded CD Plate A tail follow-up](XL_DYNAMIC_TAIL_VALIDATION.md)
corrects the prior STOP interpretation: this profile selects shared cells 6/7
(18/18, native falling positions 16/16), not unused 12/13 (5/5). Dynamic off
with identical controls extends floor arrival from 0.76 s after input stop to
1.68/1.69 s reference/native. Raising effective shared STOP to 64/64 gives
1.59/1.60 s; both independently rendered versions respond similarly. Native
state tracing records 119 low/mid changes during playing, none after input stop,
and a feedback-mid update 0.30842 s after stop. Core saturation remains zero
with exact saved-output reproduction excluding two latency frames. This isolates
the dependence of rapid decay on Dynamic/STOP, not each audible artifact's cause.
Quantization exposure is plausible; aliasing and physical-hardware normality
remain unproved. A requested delay override remained logically zero and is not
accepted as a changed-delay experiment. Two short listening clips compare short
STOP, Dynamic off and longer STOP at one common gain; listener feedback is pending.
No production changes or CPU run occur. Keep any future smoothing separate from
default fidelity until independently validated; do not expand controller work here.
The user then gave positive listening feedback and provisionally accepted this
example's behavior. A bounded hardware-literature search found Sean Costello's
direct 224XL quantization observations and related 224X manual warnings about
Dynamic response to short pauses. It found no reliable direct account of the
exact CD Plate A Dynamic quantized short-tail complaint on a physical XL. Links
and the distinction are retained in the tail report. Preserve current default
behavior; exact hardware symptom/mechanism remains unproved.
The user has now explicitly heard no difference in the presented Concert Hall
Mod reference/current musical pair. Provisionally retain that exact example and
close Mod-on fitting for it. Along with the four reviewed mode-0 examples and
retained CD Plate A Dynamic behavior, this completes the current representative
listening block. Other programs/settings/combined modes and installed-host
validation remain open. Next prioritize unreviewed representative algorithms
using existing evidence; do not repeat accepted cases or CPU without new changes.
The following [Inverse Room/effects musical checkpoint](XL_EFFECTS_MUSICAL_VALIDATION.md)
adds only four independent current-source riff cases: Inverse Room with audible
levels, Chorus & Echo Mod on, Resonant Chords and Multiband Delay. Maximum primary
band RMS deviations are 0.4178%, 8.5589%, 0.3401% and 0.7056%. Chorus & Echo's
largest difference is 500–1,000 Hz (+0.7133 dB), with envelope p95 52.8377% under
independent chorus histories; no implementation fault or listening acceptance is
asserted. All 32 exponential fits are inapplicable, not passes. Four 16.5-second
reference/current pairs include four seconds of riff and four seconds after input
stop per variant. The user has reviewed all four and reports satisfactory sound
that is identical to their ear. Provisionally accept those exact musical examples
and settings; close further fitting for this block, including Chorus & Echo's
independent-history envelope deviations. DSP, renderer, bank, dependencies and
CPU are unchanged. This is auditory acceptance of four examples, not sample
identity or all-settings/hardware acceptance. Retain these fixtures for regression
and advance remaining representative coverage and final plugin checks.

The October 6 priority is the audible comparison: excess bass/boominess and
attenuated highs. Internal tests must report percentage differences on identical
signals/settings. A provisional 5% RMS-level/valid-decay criterion is used for
triage; small differences may be temporarily retained. Controller component
work below is preserved, but further controller implementation requires evidence
that it addresses a measured sound discrepancy. The current sound campaign is
under ignored `build/validation/xl-sound-focus-20261006/`; unavailable decay fits
are not passes, and numerical triage does not establish listening acceptance.
The [internal percentage report](XL_INTERNAL_SOUND_VALIDATION.md) records the
completed 313-case current all-22 matrix plus eleven targeted diagnostics.
Mode-0 noise/music band levels differ by at most 0.3651%/0.4316%; extra 20–100 Hz
noise/music measurements differ by at most 1.6938%. Prepared WCS values match
for all 22 mode-0 programs. The matrix still has 91 level bands and 31 usable
decay bands over the provisional 5% criterion: it is not accepted wholesale.
Dynamic stop and Mod-tail differences remain, including a 24.1570% Hall / Hall
decay estimate. Injecting observed mean controller rates does not resolve the
stop envelopes and is rejected as a shipping correction. The next bounded
experiment compares trigger/ramp/coefficient visibility in the retained
CD Plate A mode-4 stop fixture, instead of expanding controller implementation
without evidence. Listening pairs are prepared; listening/host validation
remain unperformed.
The subsequent [dynamic-envelope isolation](XL_DYNAMIC_ENVELOPE_VALIDATION.md)
confirms a concrete coefficient-chronology cause for that fixture. Independent
primary observation renders stay byte-identical. The physical reference makes
eight gradual decay compilations and retriggers, whereas ordinary native collapses
the fall into one tick. Observed coefficient replay reduces maximum band-level
error from 8.5058% to 0.0575% and active-envelope p95 from 26.8945% to 1.2953%.
This remains a labelled diagnostic; a native gradual/busy control law is the next
bounded correction, with no need to change the converter/DSP core for this case.
The native gradual-transition law is now implemented and locally verified:
all 19 reverbs, 57 physical-control variants, 6,829 transitions and 5,113
compiler entry/return pairs match ramp state/work boundaries with zero errors
and zero native heap activity. The independently frozen old controller fails
the intermediate-step check (16/16 versus the first 72/72 reference step).
Compiler duration remains observed in this local oracle; runtime integration
and the resulting ordinary sound correction remain open. See the implementation
checkpoint in the dynamic-envelope report and the private
`build/validation/xl-decay-transition-20261006/boundary-checkpoint.json`.
The main native decay compiler now predicts all 19 reverb programs' 2,207
calls and 14,018 writes with zero work/state/payload/commit/grant differences;
entry/cache/IRQ context is observed in this local oracle. The independent
private native busy/ramp sound prototype preserves identical input/reference
WAVs and improves the CD Plate A maximum band error from 8.5058% to 4.9177%,
but active-envelope p95 remains 22.8037% (previously 26.8945%). Keep it as a
partial prototype, not a production or catalog pass. Native scan/read/retrigger
coupling is the next unresolved integration step. Evidence is under
`build/validation/xl-decay-compiler-20261006/`; no CPU campaign is run here.
The following [slow-controller stage checkpoint](XL_SLOW_CONTROL_VALIDATION.md)
passes all-22 input/display/post-ramp local checks: 5,995 entry calls, 3,330
display calls and 6,005 tail calls, with zero state/work/read differences and
zero native heap activity. A newly exercised division path removes 32 excess
work states; frozen old/current Hall checks fail/pass on the same recipe.
The extended all-19 unequal-stop compiler check passes 2,359 calls and 12,466
writes. These helpers are still outside production Native48. Remaining work is
their coupled nine-pass integration, actual slow return/panel service and
serial chronology, followed by the retained independent sound comparison.
Evidence is under `build/validation/xl-headroom-display-20261006/`; CPU and
the full sound/plugin campaign stay deferred under the cadence below.
The subsequent [coupled scan sound checkpoint](XL_COUPLED_SCAN_VALIDATION.md)
assembles a private Mod-off Dynamic pilot using native compiler/monitor/Fast/
Slow work and TX deadlines. The auxiliary-corrected canonical CD Plate A case
improves maximum band RMS error from 8.5058% to 0.1554% and envelope p95 from
26.8945% to 1.2800%, with identical input/reference WAVs. Four additional
seed/level/warmup variants remain below 0.5679% band and 3.8954% envelope p95.
Plate improves to 5.4751% envelope p95, but Hall / Hall and Plate / Plate remain
at 18.0204% and 6.8889%; the pilot is not accepted or integrated in production.
The Hall / Hall trace identifies a later first Slow/compile phase and a missing
first retrigger. Next, derive startup/control-transaction and panel-service
state, plus legal IRQ entry timing, before expanding the stable correction.
The standalone native TX law passes all 22 programs' 5,102 local interrupts;
the corrected secondary-MID binding passes four split programs' 233 calls and
2,586 writes. Evidence is under `build/validation/xl-coupled-scan-20261006/`.
The startup/panel follow-up passes the slow-return dispatch on all 22 programs'
9,867 local calls, with zero outer work/state errors and heap activity; nested
text/status workers still supply observed work and timer-clear context. A
separate synthetic boundary check covers both status branches. A nine-case
Hall / Hall native-only phase sweep preserves input/reference WAVs and
confirms that phase affects the missed first retrigger. Its 6 ms diagnostic
restores at 83.572 ms but still has 13.3036% envelope p95; no offset is shipped.
An additional native parameter-compiler-state candidate regresses that p95
to 22.9273% and is not accepted. Production remains unchanged. Next derive
the inner text/status worker, initial control-transaction state and legal IRQ/
read boundaries; see `startup-panel/checkpoint.json` under the same evidence
root and the updated coupled report. CPU checks remain batched/deferred.
The user then explicitly requested CPU before continuing. The
[early coupled CPU checkpoint](XL_COUPLED_CPU_VALIDATION.md) completes five
alternating paired rounds, 22 algorithms and six Mod-off workloads (1,320 rows).
Candidate single-network medians range from 1.51% to 2.38% of the audio budget;
two identical networks reach 4.73%. Matched aggregate thread CPU rises 1.12%
relative, and two Runtime objects request 171,472 additional bytes. All processing
and control updates allocate/release nothing. Large retained wall-time outliers
occur in both versions on the active host; this is not DAW/Spillover realtime
acceptance. Complete panel startup, Mod-on and physical control transactions
remain excluded. Panel/startup work resumes after this requested measurement.
The subsequent [panel text work checkpoint](XL_PANEL_TEXT_VALIDATION.md)
derives text publication and both status workers. All 22 programs pass 1,209
physical publication calls; the separate synthetic all-22 dispatch oracle passes
9,827 calls, including 60 natively predicted status workers. Work, state and
publication deadlines match with zero native heap activity. A frozen one-state
template-lookup error fails the same fixture and the correction passes. Title/
control-field formatting and startup/IRQ chronology remain open. These new
helpers are outside the measured prototype and production; CPU is not rerun
per helper, and Hall / Hall sound acceptance remains unchanged.
The next [normal program-title checkpoint](XL_PANEL_TITLE_VALIDATION.md)
passes 22 physical normal title calls, including native timer/context changes,
publication/format work, text length and UART deadline, with zero work/state
errors and heap activity. Coverage is selection mode 1 without a nested IRQ.
The canonical Hall / Hall trace preserves all three WAVs byte-for-byte and
locates the pending title job at 1.388 seconds; it does not explain the initial
43.580/48.820 ms gradual-transition discrepancy. The title helper remains
outside production and the CPU-measured prototype. Next isolate initial
control-transaction/scan state and legal IRQ/read timing; full control-field
formatting remains open.
The [startup/headroom-read checkpoint](XL_STARTUP_READ_VALIDATION.md) preserves
the canonical Hall / Hall input/reference/native WAVs byte-for-byte. At input
start the reference is finishing Slow, while the prototype is in monitor pass
ordinal 4. Reference Fast reads detector bit 16 at 83.583 ms; the prototype's
later Slow consumes its remaining bit at 47.947 ms and its first Fast gets zero
at 87.647 ms. A labelled observed-context local counterfactual restores with
16/0 but not 0/0, confirming this missed-retrigger mechanism without injecting
state into audio or accepting a phase offset. Envelope p95 remains 18.0204%.
Next derive physical parameter-transaction/reset and scan-retention semantics;
no production or CPU-measured source changes are made by this observation.
The [normal fader transaction checkpoint](XL_PARAMETER_TRANSACTION_VALIDATION.md)
passes two all-22 physical campaigns, 1,004 transactions/main-loop returns,
with zero pickup/suffix work, state or retention errors and native heap activity.
The active-tail campaign includes 459 stop-bit entries across 19 reverbs.
The native pickup and 40-state deferred-refresh suffix are derived; full
calibration/formatter/compiler transaction work remains separate. A private
24-call Hall trace covers rejected below-target pickup as well. The actual
experimental setter fails a retention counterexample: LF update resets scan
clock 2,047,996 to zero and held level 5,888 to stale 2,612. Next replace that
path with live-state updates and deferred reconciliation/compiler handling.
The new boundary helper is not yet integrated; Hall / Hall p95 remains 18.0204%.

The [live parameter queue checkpoint](XL_LIVE_PARAMETER_VALIDATION.md) now
replaces that reset path for LF/MID, applicable STOP pairs and STOP DLY in a
private Mod-off pilot. It retains the live scan/compiler state and applies the
latest coherent snapshot after reconciliation. All 19 reverbs pass 114 integration
scenarios, including actual gradual-stage requests, cancellation and coalescing;
209 setter calls produce 76 committed snapshots with zero processing heap
activity. The independently frozen old Hall fails the same retention check.
In a paired physical LF event at 0.6 seconds, maximum band RMS error improves
from 25.3844% to 1.5975% and envelope p95 from 648.8907% to 18.7391%, with identical
input/reference WAVs and identical native output before the event. This event
recipe selects physical page 1 before settling and is separate from the
canonical startup recipe. The canonical three WAVs remain byte-identical and
p95 stays 18.0204%. Full physical calibration/compiler-wrapper/formatting clocks,
initial program/key history and legal IRQ/read timing remain open. The queue
adds 120 bytes per engine object; this new candidate has not been CPU measured
and is not integrated in production. Next derive the physical transaction
wrapper against live state, retaining both canonical and LF-event regressions;
CPU remains batched until the assembled transaction/startup block is ready.

The [ordinary numeric fader calibration checkpoint](XL_FADER_CALIBRATION_VALIDATION.md)
derives page/slot bounds, fixed scaled limits, pickup and scratch/work state.
Two isolated all-22 physical campaigns pass 4,070 transactions and 3,811 supported
calibrations with zero work/state/retention errors and native heap activity;
259 variable/effect calibrations are explicitly excluded. The active campaign
includes 927 stop-bit entries across 19 reverbs. A frozen one-state timing guard
fails Hall as expected. Expanded page sweeps initially contaminated subsequent
program selection; independent boots repair the oracle, and partial failing
campaigns are retained. The helper remains outside audio and the measured pilot;
no WAV or CPU campaign is added. Next derive the enclosing compiler wrapper and
join complete transaction timing to the live pilot before rerendering sound.
The [outer parameter-frame checkpoint](XL_PARAMETER_COMPILER_FRAME_VALIDATION.md)
then passes two all-22 physical campaigns: 1,004 fader transactions, 916 frames/
copies and 4,012 additional groups, with zero outer work/state/copy errors and
heap activity. Compact logical/cache cursor bookkeeping is derived; masked pages
consume no logical cells. Nested preparation/main/group durations remain observed
local inputs, so complete sound integration is still open. No new WAV/CPU campaign
is added. The user's tail reminder is retained as the next priority: saved Hall
data isolates first-burst tail p95/max at 30.4842%/32.6359%, versus second-burst
tail maximum 4.9972%. The final tail has only two active windows; its small error
is not general acceptance. Address initial scan/read/retrigger history and rerun
the first-tail sound regression before extending unrelated controller helpers.

## Execution and validation cadence

User direction, October 6: periodically remove confirmed obsolete generated
material that will not be needed again. The repository `AGENTS.md` now requires
a review after completed validation blocks and under disk pressure, preserving
active regressions, unique evidence, pending listening pairs and provenance.
The first cleanup removed 2,726 byte-identical archived WAV copies (11.699 GiB),
with surviving identical replacements recorded in
`build/validation/wav-cleanup-20261006/deleted-duplicates.json`. Historical
manifests may name these removed archive paths; use that mapping to locate the
retained content. Current Hall event/canonical fixtures and listening pairs
were preserved. Do not regenerate full WAV matrices merely to restore duplicates.

A subsequent storage audit consolidated 753 byte-identical generated WAV paths
using hard links, saving 3,366,428,672 allocated bytes (3.135 GiB). Every pathname
and WAV payload remains available; listening directories were excluded. The
SHA-256 checks and retained-path mapping are recorded in
`build/validation/wav-cleanup-20261006/consolidated-duplicates.json`; the offline
reproduction script is `consolidate_remaining.py` in the same directory. Unique
WAVs, reports, metrics, ROMs, banks, source assets and backups were preserved.
Use a fresh output directory when rendering; do not overwrite shared WAV storage
in place. The current XL comparison tool already rejects existing fixtures.

User direction, October 6: batch related changes and avoid CPU benchmarking
after each edit. This cadence applies to the remaining XL sound work.
The later explicit request to measure CPU now is fulfilled by the early coupled
checkpoint above; it does not restore per-helper CPU benchmarking.

1. **Dynamic-transition block:** derive the necessary native compiler law,
   gradual decay steps, compiler-busy boundaries and retrigger/read behavior as
   one related implementation unit. During development, build affected targets
   and run short boundary checks plus the retained CD Plate A fixture. Add
   representative Plate/split cases when the implementation is ready to check
   shared behavior. Component, instrumentation and documentation edits do not
   trigger CPU benchmarks, full sound campaigns or all-format plugin rebuilds.
2. **Sound regression and remaining Mod block:** once the dynamic unit is
   stable, compare the affected algorithms/modes on identical retained recipes
   and report percentage errors. Address the remaining Mod-tail discrepancies
   in a separate related unit, using short reproductions while editing and
   catalog coverage when that unit stabilizes. Keep the full objective and
   retain unfavorable cases; small errors use the provisional 5% triage policy.
3. **Final candidate validation:** run the full required sound/graph/plugin
   checks for the assembled correction, then perform one paired Release CPU
   campaign against the frozen actual baseline, including applicable Spillover
   overlap/control edges. Repeated alternating rounds are one campaign. Build
   and validate the requested plugin formats after the correction is stable.

Keep callback heap/finite-output assertions in the short functional checks;
they are safety/correctness checks, not CPU profiling. An earlier narrow CPU
measurement is justified only to diagnose an observed processing regression;
record that concrete trigger. Do not benchmark unused offline controller
components merely because a helper changed.

Freeze the actual baseline once for the correction unit, and reuse immutable
input/reference fixtures and validated evidence when their recipe/source
identities still apply. Repeat a check only after a relevant change, failure or
unresolved result; use narrower checks while localizing the cause. Do not repeat
the CPU campaign before the final build if the measured processing source and
build configuration remain unchanged. Implementation details, tests and reports
must distinguish completed production changes from diagnostic-only results.

The full catalog baseline is preserved. Subsequent physical startup/key
evidence and the version-5 compiler-state correction are tracked in
[XL_SOUND_CORRECTIONS.md](XL_SOUND_CORRECTIONS.md). Its completion gates remain
open; the historical version-4 observations below are baseline evidence.
The October 6 correction checkpoint has a final v5 startup-byte bank, all-22
startup/key and explicit XL oracle passes, dual-bank plugin regression and
rebuilt desktop formats. Its 313-case paired sound campaign has completed with
identical input/reference evidence, mixed sound metrics and prepared listening
pairs. A local Mod branch/write-cost oracle also passes all 22 programs;
paired CPU cost is effectively unchanged with zero callback heap activity.
State-dependent controller scheduling, ADC events and host/listening
acceptance remain open. The correction report distinguishes the historical
first v5 prototype and CPU results from the final startup implementation.
The subsequent [XL event DAC correction](XL_DAC_VALIDATION.md) processes every
WR_DA request and passes independent all-22 FPC/circuit checks. Its full-path
graph/plugin regression, six identical-reference render pairs and paired CPU
measurements are complete. The initial CPU rise and Resonant Chords outlier
are resolved by the subsequent row-proxy/capture-state optimization: the final
matched median cost is 6.84% below pre-DAC, and Chords is 13-16% below pre-DAC.
Separate ADC/scheduler work remains open. The saved startup-only 313-case
results do not validate this newer output path across the full sound matrix.
The subsequent [XL input correction](XL_ADC_VALIDATION.md) is connected to
Native48, including held bypass pins, Dirt and precise comparator masks.
The actual old-input extraction is bit-exact over 2,112,000 stereo frames;
its independent startup-impulse oracle fails meaningfully. The current
component matches 5,010,412 words/comparators, and the actual policy passes
15,947,624 channel words across all 22 graphs. Full graph and 28-program
plugin regressions pass. Six identical-reference primary sound pairs retain
mixed results, including worse envelope fits in some cases. Controller scan,
IRQ/write visibility, displaced-row and host/listening work remain open;
the historical 313-case matrix does not validate this newer input/output path.
Five alternating paired CPU rounds complete with zero processing allocations;
the matched-median ratio to the actual pre-ADC checkpoint is 0.991926.
Requested storage for two prepared Runtime objects is 6,457,216 bytes;
shared ADC/DAC tables are separate. This is native desktop evidence, not
whole-plugin worst-case or hardware realtime acceptance.
The subsequent [controller chronology](XL_CONTROLLER_SCHEDULING.md) captures
all 22 physical programs, 50 final windows, 164,288 matched WCS writes and
302,206 software-state snapshots. Trace/non-trace fingerprints and CPU endpoints
match. It establishes a coupled nine-pass scan, input/mode-dependent rates,
actual port sample times and invariant protect/reset graph properties over
80 Mod/Size fixtures. These are reference measurements; Native48 still uses
nominal clocks, and the native scan/grant/visibility correction is not yet done.
The following [native T&C clock](XL_WCS_CLOCK_VALIDATION.md) passes exact
read/write grant/sample/commit/displacement/hold checks over 90,112 accesses
with all 22 physically selected layouts. Its 64-byte state has no processing
heap calls; paired component optimization is field-identical. This clock is
not yet connected to Native48 and does not validate the full firmware scan.
The following [timed Mod component](XL_TIMED_MOD_VALIDATION.md) predicts READY
waits and individual commits for 20,898 local Mod calls and 70,377 writes over
all 22 programs. Its observed local entry state/origin/IRQ spans are explicitly
labelled; independent scan generation and production integration remain open.
The following [outer scan-pass work](XL_SCAN_PASS_VALIDATION.md) adds native
descriptor delays, signed monitor/peak branches and a resumable event clock.
The eager all-22 local oracle passes 27,328 passes and 43,509 monitor reads,
including caller gaps. Complete slow/fast, IRQ and active auxiliary laws and
the production scheduler are still open.
The final resumable variant passes 27,315 local passes, 43,500 monitor reads,
42,366 caller gaps and 3,080 encoding calls with zero errors and zero native
heap calls. Its event state is 16 bytes; nested work and IRQ remain labelled
observations, so this checkpoint still does not replace nominal Native48 rates.
The following [fast control law](XL_FAST_CONTROL_VALIDATION.md) passes 54,154
eager local calls, 492 intermediate feedback compilations and 158 normal-decay
restorations over all 22 programs. A resumable prototype's premature compiler
return state was rejected and replaced by an explicit return event. Complete
nested compiler/slow/IRQ work and production integration remain open.
The corrected 24-byte resumable fast clock passes those same 54,154 physical
calls, including each compiler return, with zero stage/state/work errors and
zero native new/delete calls. Its separate borrowed memory is not included
in the 24 bytes. All 78 prior production source hashes remain unchanged.
The following [feedback compiler and fast coupling](XL_FEEDBACK_TIMING_VALIDATION.md)
passes 1,219 B000 calls/13,392 writes and 54,153 coupled fast calls with 492
predicted feedback invocations/4,851 writes. Native graph-derived template
bits pass all-22 structure checks; measured feedback duration is no longer
supplied in coupled mode. Coarse indices/cache state, restoration duration,
detector/IRQ/origin context and full production integration remain open.

## Scope and identity

The first full-path comparison covered **Concert Hall**, XL index 0, physical
bank 1/program 1, 105 rows. At that stage, sound fixtures had only been validated
for this program. This document preserves those initial recipes and findings.
The subsequent [all-22 catalog validation](XL_CATALOG_VALIDATION.md) records
the expanded coverage and its remaining sound-acceptance limits.

- Reference: complete original **224XL v8.21**, eleven SHA-256-validated chips
  (SBC1-SBC3 and NVS1-NVS8), `lexplug::Engine(0)` and normal AnalogIO rendering.
- Reflexion: pinned `f68ea1d069fef4a5663201693bfdfa1c579ffd69`, through the
  build-only export and existing scheduler compatibility patch. The dependency
  checkout remains clean. Dependency and firmware support are unchanged.
- Bank: version 4, private `programs-v821-native-v4-final.bankxl`, SHA-256
  `1330245bfafe90df553545d5b7807f631f6b95dfce06d610e45cd49fba7448c3`.
- Host/build: macOS arm64, AppleClang 21.0.0.21000334, Release,
  `-ffp-contract=off`, default host FP environment; no explicit FTZ/DAZ setup.
- Audio: 48 kHz, stereo wet A/C (DAC channels 0/2), input gain 0 dB, mix 1,
  Analog enabled/Dirt 0. Native Runtime includes its 57-sample alignment.
  The plugin's additional 13-sample original/XL alignment and host sample-rate
  conversion are outside this harness.

Private evidence is under `build/validation/xl-sound-20261005/`, including
`provenance.json`, `extended-commands.json`, `analysis-provenance.json`,
`fixtures-manifest.json`, tooling snapshots and per-case CSV/WAV files.
The provenance records actual chip hashes, tool hashes, compiler, dependency
export stamp and commands. None of these private outputs belongs in Git.

## Reproduction tools and startup recipe

`cineol_xl_sound_compare` is a separate XL renderer. The original-224 renderer,
startup probes, fixtures and analysis are preserved. The XL analyzer reuses
only the existing float-WAV reader and FIR band filtering helper.

The primary reference boots with normal analog/DSP processing for 16 simulated
seconds, selects the program through LarcOperator, disables all three switches,
settles, resolves factory controls through physical fader commands if needed,
and sets the requested physical switches. It verifies the resulting toggle
display, active control record and graph shape, then settles for 500 ms.
No reference flags are cleared by direct RAM writes.

Native Runtime independently selects the private bank and receives the same
resolved factory controls. It starts after reference setup; the physical setup
interval is not replayed through the native engine. Both render equal silence
for the requested warmup, then consume identical deterministic inputs on
independent converter/graph/controller clocks. Metadata records reference setup
frames, render start/end cycles, nominal native clocks and actual reference
procedure-entry counts. The prepared bank modulation state is labelled
`bank_seed`; it is not a live native state observation after warmup.

Mode bits are 1 = Mod Enhancement, 2 = Decay Optimization and 4 = Dynamic Decay.
All eight combinations were rendered for Concert Hall. Non-reverb graphs reject
requested reverb dynamics; their explicit fixtures are covered in the subsequent
catalog validation.

Inputs:

- `noise`: independent stereo LCG noise for 200 ms, followed by silence.
  Main seed 17/level 0.08/warmup 1000 ms; an additional seed 991/level 0.02/
  warmup 250 ms tests a less favorable case.
- `impulse`: left impulse at sample 0 and right impulse at sample 480, level 0.08.
- `music`: deterministic percussion bursts and a stopped three-note chord,
  level 0.08. This is synthetic listening material, not a recorded performance.
- Default captures are 12 seconds. Quiet noise, impulse and music also have
  30-second captures. There are 16 retained fixtures in total.

`--static-wcs` adds **only a labelled mode-0 diagnostic**. A separate native
graph uses the reference's prepared WCS coefficients/addresses, with frozen
controls, empty native delay memory and independent clocks. It does not copy
reference delay memory, converter phase, controller counters or graph registers.
It is not plugin behavior or a shipped correction. Primary native/reference
audio is rendered beside it.

## Confirmed observations

The seed-17 noise render is **byte-identical** at block sizes 256 and 127 for
input, native and reference WAVs. Adding the diagnostic also preserves all
three primary WAVs byte-for-byte:

| WAV | SHA-256 |
| --- | --- |
| Input | `a8cace422e22be7426b59780a08f55ab4e0a1f769c38d6b7df03a3d35b2a7b72` |
| Native | `bdeaffa4686ec61ab30e4cfc4f58e28c80ae7146b117717db6f0386c44492f7b` |
| Reference | `e85304e46a56378854f2ad5958b95b95266306ffa627fc48b8e96dd35ecade86` |

With all modes off, the bank snapshot has modulation index **126**; the prepared
physical reference has index **59** and remains there through warmup/render.
Only four initial WCS rows differ between the bank and prepared reference:

| Row | Bank coefficient | Reference coefficient | Bank offset | Reference offset |
| --- | ---: | ---: | ---: | ---: |
| 19 | 12 | 32 | 180 | 254 |
| 20 | 20 | 0 | 181 | 254 |
| 70 | 26 | 32 | 73 | 1 |
| 71 | 6 | 0 | 72 | 1 |

All four are interpolation rows. The native path freezes its prepared
interpolation coefficients/addresses when Mod is off, whereas this physical
reference recipe freezes the program-compiled state. This establishes a
specific startup-state difference; it does not yet establish every XL key's
reset/retention policy or a bank migration design.

For the seed-17, level-0.08, mode-0 noise fixture, eight 100 Hz-15 kHz paired
decay fits are usable. Mean absolute decay-estimate difference is **11.61%**,
maximum **30.71%**; total native/reference energy difference is **-0.443 dB**.
The frozen-reference-WCS diagnostic gives **1.53%** mean/**4.34%** maximum
and **-0.405 dB** total energy difference on the same eight bands.
These are diagnostic differences, **not a before/after plugin improvement**.
Residual boundary/clock/quantization effects remain even with identical static
WCS settings. Do not use EQ or feedback changes to conceal the phase difference.

The mode matrix uses the same noise input and factory controls. Rates below
count firmware procedure entries, including disabled-mode early returns; they
are not counts of committed WCS updates:

| Mode | Energy error dB | Usable paired decay bands | Mean absolute decay difference | Reference Mod entries/s | Slow entries/s | Fast entries/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | -0.443 | 8 | 11.61% | 1264.083 | 70.250 | 351.167 |
| 1 | -0.407 | 8 | 2.95% | 1096.417 | 60.917 | 304.583 |
| 2 | -0.445 | 8 | 7.02% | 1229.583 | 68.250 | 341.500 |
| 3 | -0.402 | 8 | 2.63% | 1068.000 | 59.333 | 296.667 |
| 4 | -0.469 | 0 | Not fitted: Dynamic Decay | 1215.750 | 67.583 | 337.750 |
| 5 | -0.394 | 0 | Not fitted: Dynamic Decay | 1058.917 | 58.833 | 294.167 |
| 6 | -0.476 | 0 | Not fitted: Dynamic Decay | 1191.667 | 66.167 | 331.000 |
| 7 | -0.396 | 0 | Not fitted: Dynamic Decay | 1039.417 | 57.750 | 288.750 |

The bank's fixed nominal native rates are 1094.3/60.9/304.0 Hz respectively.
The measured reference entry rates depend on enabled routines and input.
Matching a local controller step law does not make these independent clocks
equivalent. This is a second confirmed approximation, not a newly corrected
scheduler.

## Fit validity and low-level limits

The analyzer retains raw band energy and 50 ms RMS/peak envelopes for every
case. Bands are 100-250, 250-500, 500-1000, 1-2k, 2-4k, 4-8k, 8-12k, 12-15k
and 15-20 kHz. The last is a spectral-image diagnostic and is excluded from
paired decay summaries. These are exploratory bands, not XL acceptance targets.

Decay fitting integrates the post-input energy and fits its -5..-25 dB slope,
extrapolated to T60. Usable fits require negative slope, R-squared >= 0.95,
at least 100 fit samples, 1.2 s capture margin and final-second energy below
-40 dB relative to post-input energy. Both native and reference must qualify.
Rejected fits retain reasons; unavailable results are null, never zero error.
Dynamic Decay, Inverse Room and the three non-reverbs do not receive blanket
exponential-decay fits.

Quiet noise, impulse and music produce unusable primary paired fits in both
12- and 30-second captures. Extending capture did not solve late-energy floors
and non-exponential behavior. For example, the final-second reference/native
RMS levels in the 30-second captures are approximately -99.4/-108.7 dBFS for
quiet noise and -97.3/-94.8 dBFS for impulse. These data remain envelope/floor
observations; fit thresholds were not relaxed and no floor subtraction was used.
The music diagnostic has one usable band, insufficient for a broad claim.

## Validation performed

- Release tools build passed. `cineol_xl_graphs_check ROM_DIRECTORY 0` passed:
  72,000 stereo frames across three control settings, exact A-D outputs,
  arithmetic state, delay memory and saturation counts; signal/tail/dry/rational
  clock checks passed.
- All 16 comparison fixtures passed graph/control/toggle checks, finite-output
  and peak < 4 guards. Native processing and diagnostics reported zero tracked
  C++ new/delete calls, including warmup. Control preparation is outside tracking;
  these fixtures do not replace plugin callback/control-edge allocation checks.
- Analyzer checks passed against a known 3-second exponential T60, silence,
  non-reverb exclusion, persistent-floor/truncation rejection and silent-energy
  handling.
- Invalid graph/mode, mode-on static diagnostic and NaN input level were rejected
  before ROM access or output creation.
- Original-224 source/fixtures and both dependency checkouts remain unchanged.
  No plugin installation, user cache replacement or ROM reimport was performed.

During this initial Concert Hall investigation, no new full-path CPU benchmark,
Spillover margin check, 44.1/96 kHz host check, AU validation, human listening or
physical-unit comparison occurred. The graph check's incidental core timing is
not a paired desktop/plugin CPU measurement. See the catalog validation for
subsequent measurements.

## Portable commands

Use private v8.21 ROMs and the recorded version-4 bank. Choose a fresh output
directory; the tool refuses to overwrite a completed fixture.

```sh
cmake -S . -B build/xl-sound-validation -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON
cmake --build build/xl-sound-validation --parallel 2 --target cineol_xl_sound_compare cineol_xl_graphs_check
XL_TOOLS=build/xl-sound-validation/native-hall
XL_ROM_DIRECTORY=/private/path/to/224XL-v8.21
XL_BANK=/private/path/to/programs-v821-native-v4.bankxl
"$XL_TOOLS/cineol_xl_graphs_check" "$XL_ROM_DIRECTORY" 0
"$XL_TOOLS/cineol_xl_sound_compare" "$XL_ROM_DIRECTORY" "$XL_BANK" build/validation/xl-new/p0-m0 0 0 noise 17 .08 1000 --static-wcs
python3 script/analyze_xl_sound.py build/validation/xl-new/p0-m0
"$XL_TOOLS/cineol_xl_sound_compare" "$XL_ROM_DIRECTORY" "$XL_BANK" build/validation/xl-new/p0-m0-block127 0 0 noise 17 .08 1000 12 127
"$XL_TOOLS/cineol_xl_sound_compare" "$XL_ROM_DIRECTORY" "$XL_BANK" build/validation/xl-new/p0-m7 0 7 noise 17 .08 1000
"$XL_TOOLS/cineol_xl_sound_compare" "$XL_ROM_DIRECTORY" "$XL_BANK" build/validation/xl-new/p0-impulse-long 0 0 impulse 17 .08 1000 30 256 1 --static-wcs
```

Some generators add `Release` and Windows adds `.exe`. For another machine,
record compiler/architecture, bank and firmware identities again. Do not use
the original-224 comparison/CPU scripts as an XL oracle.

## Next bounded investigation

1. Build an independent XL startup/key oracle for Concert Hall. Observe the
   first AD5C procedure entry after an actual program/WCS recompile; capture
   descriptor seed, interpolation rows and the three global counters. Exercise
   Mod on/off, Decay Opt and Dynamic Decay separately through physical commands,
   including repeats and non-default Chorus/Size. Keep direct-RAM experiments
   explicitly separate from physical operator behavior.
2. Establish the XL seed/reset/retention rule and its representation in current
   v4 banks before changing Runtime, Network or import. Cache compatibility and
   existing DAW-session behavior need an explicit design; original-224 rules
   and seed indices are not evidence for XL.
3. Verify a bounded native correction against the failing startup oracle, retain
   these identical baseline recipes, measure paired Release CPU/memory and
   control-edge/Spillover costs as applicable, then expand the shared cause to
   all 22 graphs. Address event-timed converters and firmware scheduler costs
   as independently measured causes.

For listening, compare the unnormalized native/reference percussion and stopped
chord fixtures at matched playback levels; assess onset/routing, brightness,
tail envelope and the low-level end of the decay. Keep the injected-WCS render
labelled as a diagnostic. Host listening and numerical/listening acceptance
targets remain open.
