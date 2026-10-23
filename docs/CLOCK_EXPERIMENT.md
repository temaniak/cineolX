# Clock Experiment

Experimental desktop branch: `codex/clock-experiment`, 2026-10-23.
The feature was initially validated as a local trial, then selected by the
owner for [1.0Alpha](releases/v1.0Alpha.md). Bundle identities and existing
parameter IDs/indices are retained. Installation is separate from building;
private ROMs and prepared banks are excluded from archives.

## Controls and sound

- **Space:** upward motion reduces the selected algorithm's clock continuously
  from its own nominal rate to exactly half. The original 224 changes from
  20.48 to 10.24 kHz; XL rates are derived from each graph's row count.
  Delays, modulation and controller time advance more slowly together. The
  display shows the target DSP rate in kHz. Other algorithm values retain their
  nominal firmware units.
- **Dirt:** the existing `analog` parameter keeps its clean=1 polarity and host
  index, while the interface shows its inverse. Its sound deliberately changes:
  it now opens converter filter bandwidths instead of crossfading two signals.
  High poles move together over ten octaves, the output DC-block pole moves
  toward zero, and the last 15% smoothly removes input reconstruction and the
  remaining low-frequency contribution. At 100% the modeled converter filters
  are bypassed; quantization, DAC holds and algorithm-internal filters remain.
  Consequently old sessions with nonzero Dirt sound different in this branch.
- **Analog filters follow DSP rate:** checked, converter cutoff frequencies
  scale with the clock; unchecked (default), inverse compensation keeps their
  physical frequencies fixed. Dirt opens them in either setting.
- **Outputs:** the enabled Front stereo bus retains legacy stereo operation.
  Hosts can enable the additional Rear stereo bus. Front L/R and Rear L/R have
  independent A/B/C/D assignments, initially A/C/B/D. Rear carries the wet
  signal at the Mix amount; dry input appears only in Front. An algorithm's
  available outputs need not all contain independent signals.
  Logic Pro does not expose auxiliary outputs for AU audio effects and remains
  stereo with this plugin. No Quad/surround or instrument/Side Chain variant
  is included. Supporting hosts can activate Rear and route it independently.

Space uses a shared four-channel, bandlimited variable clock domain around
the existing 48 kHz engine. Nominal clock SRC transport is an exact delayed
wire. Its input delay changes converter phase relative to the signal and the
cold-start boundary compared with the stable build; complete plugin renders
are therefore not claimed bit-identical to that build.
Coefficient tables are prepared outside processing; sample histories and
queues have fixed storage. Control motion is smoothed. Dynamic filter
coefficients interpolate prepared circuit responses; this is a practical
experimental control, not a complete time-varying circuit simulation.
The SRC bandwidth and host Nyquist limit still apply at full Dirt.

Normal latency is fixed across Space positions: 256 frames at 48 kHz, plus
the existing host-rate bridge at other rates (398 frames at 44.1 kHz, 688 at
96 kHz). Low latency mode retains zero-delay dry monitoring and the delayed
wet signal. This changes the nominal-clock latency from the stable build.

## Interface

Six page-bound faders, separator, Input/Mix, separator, Space/Dirt. The last four
columns share their axes with the routing/status fields above. The text block
is raised slightly and the glass expanded upward to balance its top/bottom
padding. Rails start
below the display bezel with a visible gap. Small text uses plain bold Arial;
caption size increases from 20 to 22.5 logical pixels (+12.5%). Large preset and
algorithm names retain crisp dot lettering. Both display modes use dark glass
without a glow. Matte black worn caps and keys are embedded assets. Help sits
immediately left of Settings.

Presets use schema 5; schemas 1–4 and older sessions load defaults for the
appended controls. Existing stereo sessions retain a disabled Rear bus until
the host enables it. New-format presets are intended for this trial build.

## Validation

Universal macOS Release builds: Standalone, AU and VST3, arm64 + x86_64;
ad-hoc bundle signature verification passes. Runtime checks ran on arm64.
Reflexion remains pinned to `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.

- `cineol_space_clock_check`: clock ratios, frame counts, moving clock,
  fixed impulse onset at frame 256, exact nominal unity and quad coherence.
- `native_hall_plugin_check --clock-experiment-check BANK XL_BANK`: 108
  moving-clock/Dirt/link cases across all 28 algorithms and representative
  44.1/48/96 kHz cases; finite output, active rear bus, no callback new/delete,
  existing host indices, session/preset migration and UI bindings.
- `--preset-check`, `--display-check`, `--help-check`: preset/tail recall,
  editor persistence, screen/fader automation, compact settings bounds and
  all-page captions/help. The full `--banks` regression suite also passes:
  nominal clean engine comparison with input/output transport represented
  explicitly, all-algorithm block invariance, low latency, mono, fractional
  Dirt, cross-engine recalls and spillover tails/rapid transitions. These
  checks do not substitute for listening acceptance of the new behavior.
- Original and XL ADC/DAC boundary checks retain their independent reference
  results at Dirt=0 and nominal clock. Original ADC words and over five million
  XL words/comparators remain exact; DAC errors stay at the existing float scale.
- `--clock-experiment-probe`: eight six-second quad renders of original Large
  Concert Hall B and XL Plate, Space/Dirt endpoints, half-second seeded
  input followed by silence. All tails are finite and decrease between seconds
  1–2 and 5–6. Full filter bypass retains a small predominantly DC residual
  (final-second aggregate energy about 0.015 across four outputs); removing
  the DC blocker is audible/measurement-relevant behavior, not a zero-floor claim.

An isolated arm64 offline probe at 48 kHz/128-frame blocks measured about
2.14–3.05% of one audio second at nominal clock, and 1.30–1.65% at half clock;
99th-percentile callback wall times were 0.054–0.113 ms. These are local offline
measurements without spillover overlap, not guaranteed DAW scheduling margins.
No listening acceptance or Intel runtime acceptance is claimed.

Logs and screenshots are retained under `build/clock-experiment/`. Existing
listening comparisons from earlier clock/Dirt experiments remain available
under `build/validation/`; none were removed. Cleanup review found no obsolete
audio/reference material that could safely be discarded.
