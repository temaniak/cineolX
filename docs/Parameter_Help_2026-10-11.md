# Parameter help audit

The question-mark button opposite Settings enables hover explanations. It is
off when an editor opens; clicking it again hides the current popup immediately.
The button is present in both full and compact layouts. Help is a local editor
mode and does not change processor state, sound presets or audio parameters.

Help covers faders, screen values and their captions, output routing, mode
switches and Settings parameters. Explanations have a heading and two or three
short sentences. They use the existing English interface language. The tooltip
uses 28-unit type, about 17 screen pixels at the minimum editor width, with
wrapping and a bold heading. The existing tooltip used JUCE's 13-unit type.
Popup bounds stay within the housing, including the compact layout.
The popup is anchored to the hovered control in panel coordinates, so resized
host windows and multiple displays do not displace it from its target.

Text lives in `native-hall/plugin/ParameterHelp.hpp`. For XL controls, the
selected program's prepared page caption, control availability, dynamic profile,
modulation descriptor and feedback targets determine the explanation. The
same text is assigned to each fader, screen value and caption. Changing program
or page updates the explanations and dismisses stale popups.

## Evidence boundary

The audit uses the current original-224 v4.4 and XL v8.21 implementation and
existing version-5 bank fixtures. ROMs and banks were not changed or added to
tracked files. The isolated UI inventory reads existing prepared metadata; it
does not execute firmware, change controls in audio or introduce a new DSP path.

Implementation sources:

- `core/hall.cpp`, `core/control_scales.hpp`, `core/decay.hpp`: original control
  compilation, displayed scales, moving taps and level-driven loop diffusion.
- `desktop/controls.hpp`, `desktop/diffusion.hpp`: XL control kinds and actual
  coefficient/address changes, including signed feedback, inverse Definition
  scaling and feedback-diffusion limits.
- `desktop/bank.hpp`: real page names, active controls, variable pre-delay,
  Size-dependent displayed times and program-dependent control availability.
- `desktop/modulation.hpp`: Chorus changes modulation period/step; zero is
  not described as a bypass. The Mode Enhancement switch enables modulation.
- `desktop/dynamics.hpp`, `desktop/concert48.hpp`: level fall/retrigger logic,
  running/stopped decay, hold time and feedback-diffusion updates. In split
  reverbs the stopped-decay controls affect the left effect; their names and
  explanations retain that scope.
- `desktop/size.hpp`: network scaling and independent Rich Split side sizes.
- `plugin/Processor.cpp`, `desktop/engine22448.hpp`, `desktop/concert48.hpp`:
  input gain, wet/dry blend, nonlinear Dirt blend and selected output routing.

The original [Lexicon owner manual scan](https://therackofages.com/Lexicon/224x%20digital%20reverborator/Lexicon-224X-owner-manual.pdf)
was visually inspected, particularly sections 3.3.1–3.3.2, 4.2.3, 4.3.2,
4.4.1–4.4.3 and 4.5. The scan contains 224X material and a 224XL errata sheet.
It is supporting terminology, not the authority for present-day control
availability: current native metadata takes precedence. Manual-only settings,
unimplemented hardware operations, precise measured RT60 promises and inferred
musical-note labels are not presented as plugin capabilities.

The main distinctions retained in the wording are attack versus distance,
input bandwidth versus high-frequency decay, early-reflection gain/timing versus
overall mix, ordinary delay time versus resonant-loop pitch, and running versus
stopped decay. Signed resonance/feedback and split routing are explained using
their actual control roles.

## Program coverage

Every page of every program was enumerated. The full prepared inventory is in
`build/validation/help-20261011/control-inventory.txt`; the generated help texts
for every screen binding are retained in `help-check.log` in that directory.

| Program | Audited parameter distinctions |
| --- | --- |
| 224 Small Concert Hall B | Bass/Mid regions, treble decay, distance, program minimum pre-delay, Diffusion |
| 224 Vocal Plate | Same visible controls; program-dependent diffusion response |
| 224 Large Concert Hall B | Same visible controls; program minimum pre-delay |
| 224 Acoustic Chamber | Diffusion unavailable; six running controls retained |
| 224 Percussion Plate A | Running controls and program-dependent diffusion response |
| 224 Small Concert Hall A | Running controls and program minimum pre-delay |
| XL Concert Hall | Depth, four preechoes, fine pre-delays, Size, stopped decay |
| XL Bright Hall | Same control family; its own prepared ranges and routes |
| XL Dark Hall | Depth, AD/CB preecho routes, fine pre-delays, Size |
| XL Plate | Attack, six preechoes, Size, stopped decay |
| XL Room | Depth, four preechoes and fine pre-delays, Size |
| XL Rich Chamber | Attack, six preechoes, feedback-derived Definition, Size |
| XL Small Room | Depth, four preechoes, fine pre-delays, no Size page |
| XL Chamber | Attack, four preechoes, no Definition or Size page |
| XL Dark Chamber | Attack, four active preechoes, unused extra taps, Size |
| XL Inverse Room | Decay Slope, Size, indirect stopped Mid reference; unavailable running Mid and Chorus |
| XL Small Plate | Attack, six preechoes, Size, stopped decay |
| XL CD Plate A | Unavailable running LF, constant-density Diffusion, no optimization diffusion targets |
| XL CD Plate B | Same exceptions, its own four-page layout and modulation descriptors |
| XL Rich Plate | Attack, six preechoes, Size; Chorus unavailable |
| XL Chorus & Echo | Voice levels/delays, signed feedback, output-group panning; no reverb dynamics |
| XL Resonant Chords | Loop delay determines pitch, signed resonance, onset delay, panning, own/opposite crossfeed, feedback HF cutoff |
| XL Multiband Delay | Band levels/times, low/high band edges, panning, common-input feedback, transient diffusion |
| XL Hall / Hall | Independent left/right running decay and Depth, shared LR options, left stopped decay, A/C versus B/D |
| XL Plate / Plate | Independent left/right Attack and running controls; shared LR options, split preecho routes |
| XL Plate / Hall | Left Attack versus right Depth, independent running controls, split preecho routes |
| XL Plate / Chorus | Left reverb and stopped decay versus right chorus voices, feedback and panning; Size |
| XL Rich Split | Independent left/right running controls and Size; shared LR options; Chorus unavailable |

Existing `control_active()` exclusions are described accurately instead of
quietly enabling controls. In particular, cells 6/7 are reserved by that helper
for dynamic-decay availability, so some controls occupying them in the three
effects remain disabled in the current UI. This pre-existing behavior is outside
the help change. CD Plate running LF controls are also disabled even though LF
values participate indirectly in the prepared Mid compiler; their stopped LF
controls remain available. Inverse Room, CD Plate A and CD Plate B have no
optimization feedback-diffusion targets; the help states that limitation without
changing their existing switches or dynamic-controller behavior.

## Validation and reproduction

- Universal arm64/x86_64 Release AU, VST3 and Standalone builds succeed and the
  bundles are signed and verified. Build logs are `build.log` and
  `final-build.log`.
- `--help-check` passes for all **28 programs, 121 pages and 957 active screen
  bindings**, plus inactive slots, global and mode controls. It checks concise
  text, current program/page binding, fader/value/caption agreement, CD Plate and
  split exceptions, compact popup bounds, default-off help and immediate hide
  on disable. The help toggle leaves processor state unchanged.
- The existing expanded `--display-check` passes, preserving all-program drag
  gestures and parameter binding, appearance isolation, saved geometry,
  editor/session/application persistence and restoration into an open editor.
- Real component snapshots `help-full.png` and `help-compact.png` were visually
  inspected for text legibility, wrapping, button placement and clipping.
- The native preview was operated with a real pointer in both layouts: hover
  is silent while help is off, enabling the question mark shows the caption and
  fader explanations, and disabling it hides the popup. Bass and Crossover
  values did not change during the checks. The final placement correction was
  rechecked in the native preview. No audio device or DAW session was used.
- Separate `--appearance-write`/`--appearance-read` processes still restore the
  saved editor defaults. Logs are `restart-write.log` and `restart-read.log`.
- The preview uses symlinks to existing bank fixtures; no repeated bank or WAV
  campaign was generated. Active evidence, the public manual and the inventory
  source are retained in ignored validation storage.

Cleanup review: irrelevant generated manual thumbnails `manual-005.png` through
`manual-007.png` and `manual-110.png` through `manual-115.png` were removed from
`build/validation/help-20261011/`. The unchanged public source PDF,
`224xl-owner-manual.pdf`, relevant inspected pages and render logs remain there;
these thumbnails can be reproduced with `pdftoppm` from that retained PDF.

```sh
cmake --build build/plugin --parallel 6 --target NativeHall224_VST3 NativeHall224_AU NativeHall224_Standalone native_hall_plugin_check
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --help-check BANK XL_BANK
build/plugin/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check --display-check BANK XL_BANK
```

The correction changes presentation and explanations only. It does not establish
a new sound-accuracy result, and no DSP/audio callback or plugin parameter ID
was changed.
