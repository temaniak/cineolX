# Native desktop expansion

The desktop plugin is moving to six page-bound faders, a shared preset library
and explicit firmware identities with one unified algorithm selector. Daisy remains on the original 224 v4.4 native
engine. ROMs, prepared banks and private diagnostic captures remain local.

Implementation order:

1. Six-slot editor pages, visual-only page recall, inactive parking and preset
   firmware identities, preserving the current material and LED design.
2. Desktop-only native 224XL v8.21 Concert Hall, Bright Hall, Dark Hall, Plate
   Room and Rich Chamber graphs, verified against the independent row machine with private
   ROM fixtures.
3. Native control compilation and modulation/decay for all Concert Hall pages,
   the X/XL analog profile, rational resampling and latency alignment. Define
   continuous Dirt beside the permanent Input and Mix faders.
4. Background bank preparation, automatic engine selection and atomic
   preset recall across engines, with a short wet fade and tail reset.
5. Additional validated algorithms and ROM revisions.

The 0.9.0 unified list exposes six original 224 programs and all 22 native XL
graphs. The engine indicator shows 224 or 224 XL. XL remains a preview:
static controls and actual pages are implemented, while dynamic decay/gating,
Decay Optimization and full controller timing remain pending. Arithmetic checks
do not establish complete hardware equivalence.

The XL cycle must be measured per algorithm. Rates use the nominal 30.72 MHz
master clock divided by nine and by the row count, including the flush row.
Each graph has its own exact rational resamplers and rounded integer dry delay.

| Native v8.21 graph | Rows | Internal rate | 48 kHz input ratio | Dry delay at 48 kHz |
| --- | ---: | ---: | --- | ---: |
| Concert Hall, Bright Hall, Room, Small Room, Plate / Chorus | 105 | 32,507.9365 Hz | 128/189 | 56 samples |
| Dark Hall | 107 | 31,900.3115 Hz | 640/963 | 56 samples |
| Plate, Chamber, Small Plate, CD Plate A, Chorus & Echo, Resonant Chords, Multiband Delay | 100 | 34,133.3333 Hz | 32/45 | 55 samples |
| Rich Chamber, Dark Chamber, Inverse Room | 108 | 31,604.9383 Hz | 160/243 | 56 samples |
| CD Plate B, Hall / Hall | 102 | 33,464.0523 Hz | 320/459 | 55 samples |
| Plate / Plate, Plate / Hall, Rich Split | 104 | 32,820.5128 Hz | 80/117 | 55 samples |
| Rich Plate | 109 | 31,314.9847 Hz | 640/981 | 57 samples |

The often-quoted 34.13 kHz applies to a 100-row cycle, not to every program.
Control clocks, converter timing and resampler latency are separate. Switching
integration aligns all XL paths to 57 samples, then adds 13 samples to match
the original 224 path's shared 70-sample delay at 48 kHz.

Firmware identity is part of a preset together with its algorithm and parameters
on every page. Page navigation is editor state and cannot change DSP parameters
or clear a tail. Parking an inactive cap cannot write a parameter minimum.

## Historical 0.8.0 checkpoint

- Main/Detail pages for the original 224: six visible slots, visual-only motion,
  inactive parking, editor-page restoration and no parameter writes on navigation.
- A wider, shorter faceplate with nine uniform, longer faders and no footer
  controls. The red display contains algorithm selection, outputs and modes,
  plus nine aligned label/value cells. Long preset names scroll periodically
  at a fixed text size; all animation stays on the UI thread. The original
  four screws, outer corners and a narrow, empty lower plate are retained;
  the display bezel keeps its border thickness and corner radii.
- Continuous Dirt before Input and Mix, using the existing smoothed clean/dirty
  blend. The 0/100% endpoints are retained, with a squared response for gentle
  lower-half adjustment. The `analog` ID and index keep their clean polarity;
  the parameter is now a float, with fractional preset/state restoration.
- Firmware-aware preset format 3, shared-library grouping and atomic recall
  between original 224 and XL. Presets also store both native XL controls.
- Cancellable background import of complete original 224 v4.4 or XL v8.21
  sets. Shared banks publish once; processing reads immutable data. Missing
  banks disable their list entries, while the engine indicator opens import.
- Six playable XL algorithms, native Chorus and Diffusion on one page, four
  uniform parked faders, permanent Dirt/Input/Mix, Mod Enhancement, X/XL analog
  filtering and independent output selection. Pending Decay Optimization is
  disabled. XL program switches retain prepared kernels, reset fixed storage
  and fade in the new wet path over 128 samples at the 48 kHz processing rate.
- Common 70-sample internal delay: one extra Plate sample aligns XL graphs to
  56; a further 14 samples aligns them to the original 224 path. Host-rate
  compensation and the instance Low latency setting are shared.
- Whole-plugin tests: twelve programs, cross-engine manual/preset/session
  switching, 44.1/48/96 kHz, stereo/mono, block-size/offline invariance and
  allocation-free processing. Original processing still matches Daisy exactly.
- Six fixed native XL graphs and a graph-specific 48 kHz audio path with X/XL
  circuit filters, exact rational conversion and aligned dry output. Room shares
  Concert Hall's topology but uses its own prepared coefficients and addresses.
- Six Concert Hall control snapshots and three snapshots each for Bright Hall,
  Dark Hall, Plate, Room and Rich Chamber: 72,000 full-scale stereo frames per graph, with
  exact A-D output, arithmetic state, delay memory and saturation counts against
  the independent row machine. Signal/tail, dry alignment, rational clocks,
  circuit filters and allocation-free processing also pass.
- Native v8.21 Chorus compilation and interpolation modulation on all six
  graphs. All 32 Chorus positions and thousands of actual firmware controller
  transitions per graph match coefficients, addresses and controller state.
  The descriptors contain two taps for Concert/Bright/Room, four for Dark Hall
  and one for Plate/Rich Chamber. Reset restores the prepared settings and modulation state;
  static re-preparation disables modulation.
- Native allpass coefficient compilation for the independent 64-position
  Diffusion fader and the feedback groups, including normal/stop MID decay
  indices, Definition limits, saturation caps, original signs and staged integer
  rounding. Each graph passes 256 composed Diffusion/Definition fixtures.
  Inputs to the feedback compiler are prepared, range-limited record bytes;
  physical-fader calibration and the remaining control compilers are pending.
- XL bank format 2 stores six graphs and up to 108 rows per graph in
  `programs-v821-native-v2.bankxl`; the previous cache is preserved separately.

Next: complete native parameter pages, dynamic decay/gating and the remaining
XL programs. X/XL revisions still require
their own sonic validation. This playable preview must not be described as
complete XL firmware support.

The private v8.1 X catalog lists 17 program names, all present among the 22 in
XL v8.21. The additional XL names are Rich Chamber, Dark Chamber, Inverse Room,
Rich Plate and Rich Split. Matching names do not prove identical algorithms,
control laws or audio. Keep original 224 v4.4 for its earlier architecture and
XL v8.21 for desktop expansion; defer a separate X engine until sonic comparison
establishes a useful difference.

The modulation law is checked at actual ROM call boundaries. The native audio
clock currently schedules it at a measured nominal rate per graph, with exact
integer accumulation. Firmware CPU scheduling changes with modes, controls and
signal, so these checks do not establish bit-exact modulated audio timing or
complete hardware equivalence. Core-only benchmarks are not whole-plugin CPU
measurements and say nothing about Daisy realtime margin.

## 0.9.0 validation

Version 0.9.0 expands the fixed native graph catalog to all 22 v8.21 programs
(28 programs including the original six 224 algorithms). This is not a completed
firmware control implementation. The earlier checkpoint above documents the
six-XL release and its intermediate validation.

The universal arm64/x86_64 AU, VST3 and standalone bundles were installed and
verified on 2026-10-04. The installed binaries match the signed build outputs,
Audio Unit validation passed, and the installed version 3 XL cache passed restart
and allocation-free rendering checks. Previous bundles are retained in a local
build-directory backup. The screenshots in `docs/images` show the actual editor.

- Every XL topology passed 72,000 full-scale stereo frames across three complete
  page-setting fixtures, with exact A-D output, arithmetic state, delay memory,
  saturation counts and envelope/peak output against the independent row machine.
- The desktop processor passed all 28 program switches, cross-engine preset and
  session recall, 44.1/48/96 kHz, stereo/mono, block/offline invariance and zero
  audio callback allocations/releases. Rich Plate adds a 109-row clock at
  31,314.9847 Hz. The XL paths align to 57 internal host samples, followed by 13
  samples to retain the shared original 224 latency of 70 samples.
- The native static coefficient compiler passed 1,176 composed fader fixtures
  over all 22 programs: LF/MID decay, filters, depth, levels and pan. This compares
  compiler targets after firmware calibration; it does not yet validate the
  physical-fader calibration or full runtime controller scheduling.
- Prepared version 3 banks isolate each program's semantic descriptors, discard
  previous-program descriptors captured during selection, and use coroutine
  failure propagation for cancellation. Private banks/captures stay local.
- Native delay address compilation also passed all 22 programs, bringing the
  composed coefficient/address fixtures to 1,776. Size changes are checked
  separately against the memory-layout compiler and all executed memory rows.
- Expanded interpolation checks passed all 22 programs, including 79,162
  firmware controller transitions for programs with interpolation targets,
  exact tap coefficients/addresses/state and allocation-free rational scheduling.
  CD Plate A required preserving the original signs of each tap pair; nine-tap
  and negative-coefficient modulation is now covered.

- Size passed 52 composed fixtures across the 12 algorithms that expose it,
  including independent left/right sizes in Rich Split.
- Feedback/Definition compilation passed all 22 programs. The separate stop
  group applies only to the final two targets; definition aliases on split
  pages bind to the actual semantic cell.
- The complete compiler now starts from factory settings, rather than seeding
  each target from the reference's current output. This catches uncompiled
  dependencies; zero-type MID targets write the direct MID index. All 22
  programs passed 1,928 composed static fixtures and 1,694 display/bounds checks.
- Six faders bind to every real XL page, with visual-only navigation, inactive
  parking, current parameter units, Size-dependent decay labels and pre-delay
  bounds. Format 4 presets include all 48 logical record positions, the existing
  Chorus/Diffusion parameters and globals. Existing parameter IDs/indices stay
  stable. Factory selection chooses both sides of split algorithms (A/B) and
  preserves Chorus/Echo input orientation (C/A).

Remaining: dynamic decay/gating, Decay Optimization, controller scheduling and
pre-delay transition behavior. Stop controls are parked and these modes remain
unavailable. No completed firmware-support claim is made.


## 0.9.1 editor follow-up

Dirt was already present in both file and bank presets. Its inverted parameter
attachment previously suppressed slider notifications, so the painted cap did
not update or animate on recall. Parameter-to-slider updates now notify the
motor fader synchronously, with a binding guard that prevents parameter feedback.
Manual edits and host automation retain their normal behavior; recall applies
the sound immediately and animates only the cap.

The lower plate is 140 native pixels, approximately half the original artwork's
282-pixel lower area. The 525-pixel faders are unchanged. Full-width bands share
their horizontal grid and lower source edges instead of layering separate screw
patches over differently shaded metal. Internal algorithm/output/preset menus,
settings, ROM setup and preset dialogs share the metal texture, dark bezels,
red header/input surfaces and Arial text. The operating system's file picker
keeps its standard platform appearance.

Regression checks cover fractional Dirt in file and bank presets, immediate
parameter application with visual-only motion, rapid recall, manual interruption
and recall across 224/XL. Real modal-dialog tests wait for the message queue
rather than racing modal callbacks against a fixed timer. They exercise save,
replacement cancellation and editor-close lifetime safety.

Version 0.9.1 was installed as signed universal AU, VST3 and standalone bundles
on 2026-10-04, with a verified backup of 0.9.0. The complete 28-program plugin
check, real modal-dialog checks, installed XL cache check and Audio Unit
validation passed. Audio callback allocation/release counts remain zero.

## 0.9.2 faceplate and preset browser

The faceplate now uses one ImageGen bitmap containing continuous metal texture,
outer frame and four screws. The editor draws it once at a uniform scale, with
no stitched bands or separate corner overlays. The lower plate's mechanical
seam is drawn over the same continuous material. The original red bezel is
clipped to its rounded outline and painted opaque. Its corner pixels cannot
overlay unrelated metal texture.

One vertical divider spans both display sections. Right-side controls use
three columns aligned with Dirt, Input and Mix. Their captions and values
follow the parameter-cell typography. Native popup menu corners are transparent;
platforms without transparent windows use square corners. Red section headers
have equal six-pixel top and side insets, including JUCE's custom-item border.

The preset selector opens a themed browser overlay. Presets appear together
in alphabetical order with an adjacent algorithm/model annotation. Algorithm
checkboxes filter by the union of one or several algorithms; All algorithms
resets them. Search matches preset names, algorithms and models. Selection
alone and filtering do not change parameters; double-click, Enter or Load preset
recalls the complete saved settings. The browser refreshes the shared bank on
reopen and remains accessible when either engine bank is available. Save preset
uses the existing name dialog. Preview presets are generated only in an
isolated temporary cache, never added to the user's installed bank.

Browser checks cover one/two/three-algorithm unions, filter removal, search,
empty-result load guarding, zero parameter events during filtering, 224/XL
recall with fractional Dirt, keyboard loading, shared-bank refresh and close.
The full 28-program plugin check also passes, with zero callback allocations
or releases. DSP and Daisy behavior are unchanged by this follow-up.

Version 0.9.2 was installed as signed universal arm64/x86_64 AU, VST3 and
standalone bundles on 2026-10-04. Installation verified each bundle against
the build output and backed up 0.9.1. Installed Audio Unit validation passed.
The rendered menu snapshot has alpha zero at all four corners; its first
header's opaque interior begins seven pixels from both the top and left edge
(six pixels to the header outline). Final preset/browser/modal checks passed.

## 0.9.3 independent menu material

Menus, settings, ROM setup, preset dialogs and the browser now use a separate
1254-square ImageGen material, `menu-metal.png`. Its uniformly distributed
wear excludes the faceplate's directional grime and fader-related marks.
The UI samples the texture at a fixed grain scale rather than stretching a
faceplate crop into differently shaped windows. Normal surfaces fit within
one texture crop; larger surfaces can wrap it. Transparent popup corners,
header margins, control layout and the main faceplate are unchanged. No DSP
or preset storage changes are involved.

The final material was refined in ImageGen to lower wear contrast underneath
small text. Rendered browser, settings, save-dialog and output-menu snapshots
were inspected; popup corners remain transparent and header insets are preserved.
Existing preset/browser/modal and preset-audio checks passed. Signed universal
arm64/x86_64 version 0.9.3 was installed as AU, VST3 and standalone on 2026-10-04,
with a verified backup of 0.9.2. Installed Audio Unit validation passed.

## 0.9.4 upper-limit display and page navigation

The XL formatter preserves the firmware's `--` marker for unbounded upper
frequency/decay display codes. The editor now translates it to `INF`, retaining
the unit, for active controls. Parked and unsupported slots keep `--`. Numeric
finite endpoints, logical control values, the prepared-bank ABI, coefficients,
preset state and audio behavior are unchanged. Endpoint checks exercise actual
sliders throughout all 28 algorithms and parameter pages, plus the finite/INF
frequency boundary and infinite decay.

The right header now follows the permanent fader grid: Left / Right / Model
above Page / Mod Enh / Decay Opt. Direct numbered page selectors beside the
algorithm name complement the cyclic Page n/n control. Only the current page
number is bright; other page numbers remain clickable at reduced brightness.
The gear uses colour feedback without an enclosing frame or focus background.
Navigation checks cover page counts, direct selection, selected-number state,
non-overlapping algorithm text bounds and unchanged audio tails during page
navigation. No parameter IDs or DSP processing changed.

Final checks passed for 575 active fader endpoints, direct and cyclic navigation,
preset/browser/modal behavior and all 28 algorithms at 44.1/48/96 kHz. Callback
allocation checks remained at zero. Actual editor snapshots were inspected for
the original 224, XL with seven pages, the preset browser and the INF limits.
Signed universal arm64/x86_64 version 0.9.4 was installed as AU, VST3 and standalone
on 2026-10-04. The installer verified all three bundles and saved version 0.9.3 in
`build/install-backups/7ad5917b4121426cb1c638fca159e6d4`. Installed Audio Unit
validation passed.

## Desktop spillover (v0.9.5)

The plugin Settings panel adds an optional Spillover toggle and a 1–10 second
duration selector on the same row (default 5 seconds). Two complete runtimes are prepared before
processing. Algorithm changes retain the old runtime's memory, controls and
output pair, transfer input over 5 ms and fade its wet output with a smooth
amplitude envelope. A shared dry delay avoids interruptions or doubled dry audio.

Only one outgoing tail is retained. Another algorithm selection retires the
oldest tail within 20 ms before reusing its runtime, bounding processing to two
active algorithms. These instance options are saved in DAW state and excluded
from sound presets. Old sessions restore the original hard-switch default.
Disabling spillover smoothly retires any existing tail; subsequent switches use
the original path. No Daisy files, DSP kernels or dependency pins are changed.

The offline plugin check compares 1/5/10 second fades against independent old
and new processor instances. It checks 224/XL/split transitions, rapid changes,
turning the option off, shared dry continuity, low latency, mono/stereo,
44.1/48/96 kHz, different host block sizes and zero callback allocations/releases.
These checks do not establish DAW realtime CPU margin or replace listening tests.

## Desktop quick preset keys (v0.9.5)

Eight numbered keyboard-style keys occupy an extended lower faceplate. The
logical panel is 1640 × 1240, with the upper display and faders unchanged and the
bottom frame and mounting screws moved below the keys. A newly generated
full-size faceplate preserves uniform photographic grain across the entire
panel, with no stretched bands or joins. Keys use a shared transparent photograph
of a rounded ivory plastic keycap with no enclosing frames. Each key has a preset
caption and red current-preset indicator; editing sound parameters clears it.

The preset browser adds Assign to... beside Load preset. Its eight-slot menu
shows the existing assignments. Selecting a slot replaces its name reference
without loading sound or notifying audio parameters. A populated key recalls
the named preset through the existing atomic preset application, including
224/XL engine changes and optional spillover. Empty slots are inactive. A
missing or unavailable preset reports an error and preserves current sound.

Mappings and the last selected key are instance metadata stored in DAW state,
not new host parameters or sound-preset data. Old sessions restore empty slots.
Named bank entries are resolved on each recall, so replacing a bank preset also
updates its assigned keys. The feature performs no file I/O in audio processing
and does not modify Daisy builds or either DSP implementation.

Final UI review removed the lower horizontal seam and decorative rules beside
QUICK PRESETS. The faceplate now stays visually continuous into the key row.

Validation passed for eight-slot assignment/replacement, canonical bank names,
224/XL quick recall, host state-change notifications without parameter events,
DAW state round trips and legacy-session empty slots, unchanged instance
options, current bank contents and failed-load sound preservation. A separate
macOS GUI check exercised the actual eight-item assignment menu, its captured
selection and quick-key click/indicator behavior. Full plugin regression
covered all 28 algorithms, 575 fader endpoints and spillover at 44.1/48/96 kHz;
audio callbacks retained zero allocations and releases. Final editor and
browser snapshots are in docs/images/cineol-x-224-quick-*.png.

Signed universal arm64/x86_64 version 0.9.5 was installed as AU, VST3 and
standalone on 2026-10-04. The installer verified signatures and executable
hashes, retaining the previous bundles in
build/install-backups/20261004-123543-quick-presets-v0.9.5. Installed Audio Unit
validation passed. Dependency checkouts and Daisy/core files remain unchanged.

## Desktop editor focus fix (v0.9.5)

Popup menus now attach to the owning panel instead of creating detached native
windows. Preset dialogs are children of their AudioProcessorEditor and only
block controls within that instance. Hiding, detaching or leaving an editor
cancels its transient UI. Closing an editor dismisses only its own menus.
Dialog results clear editor state synchronously; deferred modal cleanup only
deletes the window, so host message delays cannot leave Save disabled.

On 2026-10-04 the dedicated macOS focus regression passed with two editor
windows: hidden-dialog cancellation without saving, focus-change cancellation,
immediate Save reopening, parented output menus, instance isolation and fader
drag delivery after preset recall. Preset save/replace/cancel, browser and
quick-key tests also passed. Full processor regression covered all 28 programs,
fader endpoints and spillover at 44.1/48/96 kHz with zero callback allocations
or releases. These tests simulate host windows; direct verification in Logic
Pro was unavailable because the Mac was locked.

Signed universal arm64/x86_64 AU, VST3 and standalone bundles were reinstalled
and their executable hashes matched the build. Previous installations are
retained in build/install-backups/20261004-143509-focus-fix-v0.9.5.
Installed Audio Unit validation passed. Plugin identifiers, parameter IDs,
sound processing, Daisy files and dependency checkouts are unchanged.

## Native XL dynamics update after the published 0.9.5 release

The desktop source now implements XL Dynamic Decay/gating, LF/MID STOP DECAY,
REV STOP DLY and Decay Optimization. The existing Decay Opt switch selects the
appropriate controller for original 224 or XL. Dynamic Decay uses the existing
`xl_42` logical control's dynamic bit; both switches and the stop parameters
belong to format-4 sound presets and DAW state. No parameter ID or host index
was added or reordered. Daisy processing is unchanged.

The native controller measures the input headroom and the graph's transfer
output, follows a logarithmic peak, recognizes falls and retriggers, counts the
stop delay, compiles the effective LF/MID decay and adjusts feedback diffusion.
Decay and feedback have distinct commit boundaries. Fixed-size state and
rational clocks use nominal call rates measured during offline ROM import;
audio never interprets a CPU or loads ROMs. The CD Plates retain the firmware's
zero-valued 256-tick optimization divider. Non-reverb Chorus/Echo, Resonant
Chords and Multiband Delay do not expose reverb dynamics.

The version-4 prepared XL cache adds controller metadata and initial state.
Upgrading from the published version requires one new import of the user's
complete original 224XL v8.21 ROM set. The old XL cache is retained; the
original 224 v4.4 cache and existing presets remain compatible.

Dynamic Decay occupies the former Page n/n cell in the red header, beside
Mod Enh and Decay Opt. Numbered page selectors provide all page navigation.
The permanent column boundaries, typography and fader alignment are retained.
Quick preset keys no longer draw hover/focus underlines; selection uses the LED.

The independent `cineol_xl_dynamics_check` compares slow and fast controller
transitions and committed decay/feedback coefficients with the private v8.21
firmware. It exercises all four switch combinations, pulse/drop/retrigger
levels and a nonzero stop delay. Separate 48 kHz native renders verify that
both switches change the tail, keep output finite and allocate/release no
memory. The full plugin checks cover presets/session recall, numbered pages,
all 28 algorithms at 44.1/48/96 kHz and host block-size independence.
These checks establish native control behavior; they do not establish exact
physical hardware timing or an auditory match to a particular hardware unit.
For listening, compare a sustained input and a stopped input with Dynamic
Decay on/off, shorten STOP DECAY, vary REV STOP DLY, compare Decay Opt on/off,
and recall the same settings through a saved preset.

On 2026-10-04 the complete dynamics oracle passed for all 19 reverb programs
and all four switch combinations. The three other XL effects have no reverb
dynamics. Native audio checks passed for all 19 reverbs with finite output and
zero allocations/releases. Full plugin regression passed all 28 algorithms,
618 active fader endpoints, preset/session recall and block independence at
44.1/48/96 kHz. The signed universal AU, VST3 and standalone were installed,
their executable hashes matched the build, and installed AU validation passed.
The user's existing v3 cache was preserved beside the new private v4 cache.
The README screenshots show the actual updated editor. Listening in the host
remains necessary to assess the musical behavior and hardware similarity.
