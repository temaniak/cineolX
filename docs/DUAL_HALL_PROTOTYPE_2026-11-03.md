# Dual Hall B prototype — November 3, 2026

## Delivery and routing

**Cineol Dual Hall B** is a separate local AU/VST3/Standalone prototype. Its
bundle identifier is `net.nkstuff.cineoldualhallb`; AU identity is
`aufx/CdHB/Rflx`. AU and VST3 are installed under the user's ordinary plugin
folders with their own names. No public release or tag is created.

Two complete original v4.4 Large Hall B `DesktopEngine48` instances run at the
existing 48 kHz boundary. Each input channel feeds both ordinary input ports of
its own engine. Left wet is engine 1 output A; right wet is engine 2 output C.
Each complete internal algorithm remains intact. All Hall controls are common
to both instances. The read-only original bank cache is reused.

Both tank inputs use the previous frame's opposite wet output. Reciprocal
Crossfeedback is 0–50%, initially 15%, with a bounded full-range 20 ms slew.
The explicit link delay is one 48 kHz frame, in addition to the engines' existing
converter and algorithm delays. Input Gain affects source excitation only;
Dry/Wet and Output Gain are outside the return loop. Clear tails explicitly
resets both preallocated histories and the reciprocal return while retaining
parameters.

The root CMake file gains two optional-subdirectory entries. Main plugin,
core/desktop algorithm sources and dependency checkouts are unchanged. The
new option defaults OFF. Installed main AU/VST3 binary hashes were recorded
before installation and match afterward. The [prototype README](../native-hall/dual-hall/README.md)
contains build commands and complete routing details.

## Functional evidence

- Six core cases: coupling 0/25/50%, Mod+Optimization off/on, shared live
  control changes, then input silence. The wrapper matches two independently
  instantiated original engines exactly: maximum sample difference 0.
- A left-only source gives zero right output at 0% coupling in those fixtures;
  positive coupling transfers energy to the right tank. Both returns are
  captured before either instance advances.
- Actual processor output exactly matches an independent wrapper through live
  shared-control changes and coupling changes during the tail. Crossfeedback
  changes have the specified bounded slew and do not reset either history.
- All 16 legal parameter values survive a new-instance state roundtrip.
  Clear tails matches fresh histories with the same settings. Mono input
  equals duplicated stereo input despite unrelated data in the unused buffer.
- At 44.1/48/96 kHz, rendering in blocks of 128, single frames, or the repeating
  pattern 1/127/511/4096/17 gives maximum sample difference 0. Preparation uses
  capacity 128, also testing larger host blocks. Host bridge underruns: 0.
- The dry signal is sample-exact with 70-frame compensation at 48 kHz. NaN/Inf
  source samples are sanitized. Missing-bank processing returns silence.
- Core setters/processing and actual callbacks, including Clear tails, count
  zero C++ heap allocations and zero releases. No new callback file/network I/O,
  waits or locks are introduced. ASan/UBSan core checks pass.
- AU, VST3 and Standalone build on macOS arm64 with JUCE 8.0.14 and verify their
  ad-hoc signatures. The installed AU passes targeted Apple AUval. An initial
  sandboxed validation did not discover the new component; the subsequent
  system validation succeeds. Offline editor snapshots were visually reviewed.
  No live DAW listening or Windows/Daisy execution is claimed.

## Stopped-input tails and the intentional limit

The tail screen uses 0.5 seconds of deterministic left-only noise at peak 0.08,
followed by exact input silence. Recipes are default controls; short LF/Mid
decay with maximum diffusion; and maximum LF/Mid/Treble decay, maximum diffusion
and maximum pre-delay. Each runs with modes off/on and coupling 0/25/50%:
18 cases. Ordinary/short recipes run 30 seconds; maximum decay runs 90 seconds.
Early AC power is measured over 0.5–1.5 seconds, final AC power over the last two
seconds, removing each channel's own mean before pooling channel powers.

All 12 ordinary/short cases decay to a low residual. The highest final AC level
among them is -96.34 dBFS. Original integer quantization can recirculate into a
residual floor; numerical zeros and the -300 dBFS reporting floor are not
physical-device noise specifications.

At maximum decay, the isolated engines still decrease, but the tested 25% and
50% positive reciprocal returns **grow into self-oscillation and original ARU
saturation**. Final AC levels reach approximately -5.48 to -3.66 dBFS. These
cases remain finite because of the original arithmetic behavior, which does
not establish decaying or musically acceptable operation. The prototype keeps
the direct coupling and original algorithms, shows this limitation in the UI,
and provides Clear tails. Lowering Output Gain changes listening level only.

## CPU observation

One assembled actual-callback benchmark used 48 kHz and 128 frames, five
repetitions, 50 warmup and 200 timed blocks per repetition. Preparation, input
generation and UI rendering are untimed; other task builds/renders had stopped.
Median callback wall time was 9.30% of the processed audio duration. The largest
observed wall time was 21.107 ms against a 2.667 ms audio budget, exceeding that
budget. This ordinary-priority offline timing includes scheduling pauses and
does not prove realtime scheduling margin or establish a DSP worst case.

## Evidence and listening handoff

Private evidence is in `build/validation/dual-hall-20261103/`: `core.log`,
`tails.csv`, `plugin.log`, `sanitizer.log`, `missing-bank.log`, `auval.log`,
`editor.png`, build logs, source/bank/artifact provenance and the installed-main
binary hashes. Small initial diagnostic logs are retained. No WAV campaign,
ROM or bank copies were generated. Cleanup records any superseded executable
removed and the retained sources/current build replacement.

Open **Cineol Dual Hall B** in the host, start with ordinary decay and 100% wet,
and compare 0% / 15% / 30% coupling at matched listening levels. A left-only
source demonstrates inter-tank transfer. Review stereo motion, transient
definition, decay, residual floors and behavior after stopping the source.
Listening acceptance remains with the user.
