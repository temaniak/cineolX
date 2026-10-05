# Cineol-X 224 development roadmap

Updated: October 5, 2026.

For the active desktop sound-accuracy work, read the
[cross-computer continuation record](SOUND_ACCURACY_HANDOFF.md). It records
the four remaining working stages, checkpoint status and reproduction setup.

This roadmap records the four development stages agreed in the discussion.
Their order is agreed; detailed designs, implementation choices, dates and
release versions remain to be defined. Checked items record completed work;
unchecked items remain open.

## Direction

Keep the efficient native engine and bring its sound closer to the original
Lexicon 224. Then improve desktop usability, explore creative modifications
of the existing algorithms, and develop original algorithms in the same
processing environment. Use Reflexion as an offline reference; full firmware
and CPU emulation is not the intended runtime architecture.

## Stage 1 — Native sound accuracy

Improve the fidelity of the native original-224 algorithms while measuring
performance cost. Start with the audio boundary, where the investigation
identified the strongest systematic difference.

- [ ] Establish repeatable comparisons against Reflexion with matching firmware,
  algorithm, control values, gain, Analog/Dirt, output routing and mix settings.
- [ ] Expand wet-only comparisons to all six original-224 programs. Measure
  frequency response, phase/delay, output levels and decay envelopes, and
  include representative musical input in listening comparisons.
- [ ] Bring the combined input/output filtering, sample-rate conversion and
  DAC reconstruction closer to the reference. Evaluate aliasing and spectral
  images as well as bandwidth and phase.
- [x] Replace the original-224 desktop output boundary with event-timed DAC
  holds and continuous-time AOUT reconstruction. Verify converter phases,
  spectrum, decay estimates, allocations and paired desktop CPU cost.
  See [DAC validation](EVENT_DAC_VALIDATION.md). Daisy work is deferred for
  the current phase.
- [x] Replace the original-224 desktop input approximation with the reference
  interpolator/circuit response sampled at separate ADC hold times. Verify
  aliasing, converter words, six-program tails and desktop CPU cost.
  See [input/ADC validation](INPUT_ADC_VALIDATION.md). Full-firmware
  controller differences and dry/wet timing remain open.
- [ ] Improve modulation and decay-controller timing and initial state where
  comparisons establish a meaningful difference.
- [x] Correct the desktop decay startup period, sparse transfer-peak sampling
  and shared panel-scan cadence. Retain decay state on direct native program
  switches and measure CPU. See [controller validation](CONTROLLER_VALIDATION.md).
  Full modulation phase and signal-dependent firmware timing remain open.
- [x] Preserve the three free-running modulation counters on direct desktop
  program switches. Validate the complete step state at actual ROM clocks;
  record frozen-phase and scheduling limitations. See
  [modulation validation](MODULATION_VALIDATION.md).
- [x] Derive native modulation procedure costs and WCS grant/write timing.
  Validate 576 ROM-private cases and every observed write boundary; see
  [control-timing foundation](CONTROL_TIMING_VALIDATION.md). The model is not
  yet connected to audio processing; complete scan scheduling remains open.
- [x] Investigate desktop dry/wet timing and Input Gain behavior. Current
  behavior passes the defined fixtures; see [gain/timing validation](GAIN_TIMING_VALIDATION.md).
- [x] Add portable ROM-private all-six comparisons, fit analysis and paired
  CPU tooling, with Git continuation reports. See
  [residual measurements](RESIDUAL_SOUND_VALIDATION.md) and
  [regression checkpoint](FINAL_SOUND_VALIDATION.md). Full sonic acceptance,
  including phase ensembles and musical input, remains open.
- [ ] Measure CPU and memory cost on desktop and Daisy targets. Verify actual
  realtime margin on hardware rather than relying on build success.

**Outcome:** closer native sound, supported by measurements and listening,
with documented remaining approximations and measured performance cost.
Specific numerical acceptance targets still need to be defined.

The [sound comparison report](REFLEXION_SOUND_COMPARISON.md) records the
starting evidence. Reduced upper-band bandwidth and tail-controller
variations were measured; a persistent low-mid resonance was not established.
The verified CPU optimizations are not a demonstrated cause of the sonic
mismatch. XL needs its own comparison before applying original-224 findings.

## Stage 2 — Controls designed for desktop use

Make the plugin easier to operate on a computer. The interface currently
imitates the hardware too literally; organize controls around the user's
work with each algorithm.

- [ ] Review the available controls and current page layout for each algorithm.
- [ ] Design logical, algorithm-specific parameter groups and page distribution.
- [ ] Improve navigation, labels and presentation so users can find related
  controls and understand their purpose.
- [ ] Evaluate proposed layouts in normal desktop use before selecting a design.
- [ ] Implement the selected control organization while preserving existing
  plugin identifiers and parameter IDs for DAW sessions and automation.

**Outcome:** coherent control pages for each algorithm and more convenient
computer interaction. The exact groups, page counts and visual design remain
open; no particular replacement interface is specified by this roadmap.

## Stage 3 — Creative tweaking of existing algorithms

Explore changes inside the existing native algorithms, including possible
circuit-bending-style manipulation. This follows the work on sound accuracy
and desktop controls.

- [ ] Investigate which internal parameters, coefficients, connections or
  processing operations offer useful creative variations.
- [ ] Prototype candidate modifications and evaluate their musical behavior.
- [ ] Decide which modifications to expose and how to control them: a hidden
  or additional experimental mode is a possibility, not a selected design.
- [ ] Keep original-algorithm behavior identifiable and verify stability,
  realtime cost and behavior when switching or recalling settings.
- [ ] Document the sonic effect and practical limits of the selected tweaks.

**Outcome:** a defined set of useful creative modifications with an agreed
control approach. The specific bending operations and their presentation
remain to be chosen.

## Stage 4 — Original algorithms and a possible “224 Plus”

Use the native algorithm code and development experience as a foundation for
new algorithms running in the same processing environment.

- [ ] Define and prototype original algorithms, potentially including delays
  and other effects alongside reverbs.
- [ ] Integrate selected algorithms with the plugin's control and preset
  environment.
- [ ] Investigate internal extensions such as larger delay memory and other
  engine improvements that enable useful new behavior.
- [ ] Evaluate precision, memory and CPU tradeoffs for desktop and embedded
  targets before selecting extensions.
- [ ] Define the scope and presentation of a possible “224 Plus” direction
  from the successful prototypes.

**Outcome:** original algorithms and justified engine extensions in the same
native environment. “224 Plus” is a working direction, not a confirmed product
name or release. Exact effects, memory sizes and internal changes remain open.

## Implementation constraints

- Keep DSP, logical controls and hardware I/O separate.
- Preserve realtime safety: no allocation, file/network I/O, locks or waits
  inside audio processing. Prepare storage and coefficients outside it.
- Prefer targeted changes and retain independent checks of integer arithmetic.
- Preserve plugin identifiers and existing parameter IDs. Consider session
  compatibility whenever a sonic default or parameter behavior changes.
- Inspect build paths and dependency pins before substantial changes. Keep
  dependency checkouts clean; apply compatibility patches to build exports.
- Keep ROMs, banks, captures and private hardware adapters out of Git.
- Keep repository documentation in English and explain sonic/control changes.

## Updating the plan

Refine each stage's implementation and completion criteria as work begins.
Record selected designs separately from options still under investigation.
Mark tasks complete only when their implementation and verification exist.
Link the relevant changes and reports when available.

This document authorizes no claim that the planned features already exist.
