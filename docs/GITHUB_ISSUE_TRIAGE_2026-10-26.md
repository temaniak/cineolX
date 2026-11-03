# GitHub issue triage — October 26, 2026

## Scope and provenance

Reviewed all nine issues in `temaniak/cineolX`, including their bodies and
attached screenshots/manual tables where inspected. All nine are open and have
zero comments. Issues #3–#9 are from one reporter and refer to 0.9.8; #2 reports
Cubase bypass clicks without host/plugin version details; #1 requests Linux.
GitHub Discussions are disabled. No comments, labels or issue states were changed.

Local baseline: `f545a0b412de6b72de7617b1e4aa755f10fd2343`, also the peeled
`v1.0Alpha` tag. GitHub `main` is at
`65b260c748cf70061e3810c453df56e2a98953b1`, seven commits behind this release
branch. The relevant `desktop/bank.hpp` and `desktop/controls.hpp` logic is
unchanged between `v0.9.8` and local HEAD. Existing uncommitted Editor/plugin
test changes and `LOGIC_AU_INPUT_WORKAROUND.md` were preserved. No production
code, sound behavior, plugin identity or parameter IDs were changed by triage.

Private input: existing XL v8.21 version-5 prepared bank, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The bank was read in place without changing the application cache. No ROM,
bank or audio capture was added to the report or tracked files. Reflexion is
pinned to `f68ea1d069fef4a5663201693bfdfa1c579ffd69`; JUCE is 8.0.14.

## Findings and proposed order

| Issue | Assessment | Next action |
| --- | --- | --- |
| [#5: maximum HF kills reverb](https://github.com/temaniak/cineolX/issues/5) | Confirmed severe endpoint problem in native audio. Raw 255 zeros both filter coefficient groups, whereas 253/254 use the open-filter endpoint. The editor labels all three `INF`. | First: establish the correct physical endpoint/sentinel mapping with the ROM oracle, then correct the control path and test audio at 252/253/254/255. |
| [#4: missing voices 1/2](https://github.com/temaniak/cineolX/issues/4) | Confirmed UI eligibility bug. Chorus & Echo levels, Resonant Chords pitches and Multiband Delay times use cells 6/7. `control_active()` treats those cells as stop-decay controls before examining their actual descriptors; these non-reverb programs have dynamics disabled. | Restrict stop-decay special cases to applicable dynamics programs, then verify all six voice controls and session/automation bindings. |
| [#6: Inverse Room Mid](https://github.com/temaniak/cineolX/issues/6), [#7: CD Plate LF](https://github.com/temaniak/cineolX/issues/7) | Confirmed hidden functional controls. The UI rejects controls with no direct target group, overlooking indirect dependencies. Changing these raw values changes native output and input-silent tail energy. | Recognize dependent controls explicitly, preserving their existing IDs/scales; compare their behavior against physical ROM controls before acceptance. |
| [#2: Cubase bypass clicks](https://github.com/temaniak/cineolX/issues/2) | Strong source-level cause; Cubase reproduction still pending. Processor has no bypass parameter/override. JUCE's VST3 wrapper calls the default bypass path, which passes main input through without matching the reported processing latency or smoothing the transition. The DSP also stops advancing on that path. | Add a focused bypass continuity test, design latency-matched smooth bypass with explicit tail behavior, then test the reported Cubase version/platform. |
| [#8: Rich Split Chorus](https://github.com/temaniak/cineolX/issues/8), [#9: Rich Plate Chorus](https://github.com/temaniak/cineolX/issues/9) | Missing UI access confirmed; DSP completeness unresolved. Both prepared profiles have zero interpolation descriptors. Chorus extremes produce identical measured RMS/tail/sum in this fixture with Mode Enhancement on. Simply showing a slider would not establish functioning Chorus. | Probe physical v8.21 Chorus compilation and the applicable modulation mechanism. Attached manual tables establish expected user controls, but are not an executable firmware oracle. |
| [#3: hidden original-224 controls](https://github.com/temaniak/cineolX/issues/3) | Confirmed feature gap: the public original-224 parameter set has Mod/Decay Opt switches, but no independent modulation rate/depth or optimization threshold controls. | Verify the reported Shift/program-key operations in v4.4, then define compatible parameter additions and defaults. |
| [#1: Linux VST3](https://github.com/temaniak/cineolX/issues/1) | Platform request. Release CI builds Windows and macOS; its Ubuntu job publishes archives rather than building a Linux plugin. | Separate Linux build/package/host validation work from sound fixes. No Linux build or runtime acceptance was performed here. |

## Native audio evidence

Probe source, output, issue snapshot and inspected images are retained under
`build/validation/github-triage-20261026/`. The probe compiles directly against
the current native headers; it does not use the modified Editor or existing
plugin test executable. Commands from the repository root:

```sh
c++ -std=c++20 -O2 -ffp-contract=off -I . \
  build/validation/github-triage-20261026/probe.cpp \
  -o build/validation/github-triage-20261026/probe
build/validation/github-triage-20261026/probe \
  "$HOME/Library/Application Support/Cineol-X 224/programs-v821-native-v5.bankxl" \
  > build/validation/github-triage-20261026/probe.log
```

Endpoint fixtures use an independent fresh engine per value, factory controls,
Dynamic/Optimization/Mod off, 0 dB gain, fully wet output, 48 kHz, deterministic
LCG noise with seed 17 and 0.03 peak: two seconds silent warmup, two seconds
input, two seconds silence. Measurements aggregate all four engine outputs;
signal RMS covers seconds 3–4 and tail RMS seconds 4–5. The probe measures
native runtime samples directly, without the plugin's host bridge or SPACE layer.

There are 65 endpoint cases across Concert Hall, Inverse Room, CD Plates A/B,
Rich Plate and Rich Split. Concert Hall Treble Decay signal RMS falls from
0.006506 at raw 254 to 0.00001136 at 255 (about 55 dB). Its HF Bandwidth falls
from 0.005590 to 0.00001136 (about 54 dB). Inverse Room HF Bandwidth falls from
0.006448 to approximately 1.77e-12. Every endpoint case remains finite.
The effect is program/control dependent: this does not prove every Treble
Decay endpoint fully mutes every algorithm. Raw 253/254 retain audio.

Ten additional hidden-control cases use the same signal with Mod on and
Dynamic/Optimization off. CD Plate A LF raw 0/248 changes tail RMS from
0.003207 to 0.004207; CD Plate B changes from 0.003086 to 0.005475. Inverse
Room Mid changes from 0.002461 to 0.002037. Rich Plate/Rich Split Chorus
extremes give identical reported metrics. These are functional sensitivity
checks, not decay-time fits, samplewise equivalence or hardware acceptance.

The existing fader-limit regression checks display values and bindings at
endpoints. It does not check audible output there. Its static compiler oracle
samples raw 2/66/130/250, missing the 253–255 sentinel boundary. This explains
why prior passing checks did not exclude #5. Eligibility tests also reuse the
same `control_active()` predicate, so they cannot independently detect #4/#6/#7.

`INF kHz` concerns filter bandwidth, not infinite reverberation time. The
reporter's proposed input-cut/infinite-tail action is a separate Freeze-style
feature request and should not be conflated with correcting the HF endpoint.

No production correction, full ROM oracle campaign, Cubase run, physical-unit
comparison, listening acceptance or CPU-margin claim is included. Retained
evidence is small. Cleanup removed only
`build/validation/github-triage-20261026/probe`, the disposable compiled
executable; its source, command and complete output remain in that directory
and this report. No historical caches, user assets or existing validation were
deleted.
