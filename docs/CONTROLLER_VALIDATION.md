# Original-224 desktop controller validation

October 5, 2026. This follows the [input/ADC step](INPUT_ADC_VALIDATION.md).
The scope is the desktop application's original Lexicon 224 v4.4 engine.
Reflexion remains pinned at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.
No ROM, cache-format, parameter-ID or plugin-identifier changes are required.
Daisy files were not changed or built; the default portable signal path was
checked separately. XL sound was outside this work.

## Findings and implementation

The level-controller arithmetic was already independently checked against
the firmware's 17-byte RAM state and loop allpass coefficients. The strongest
new defect was its starting context, rather than its arithmetic:

- Five of six prepared programs retained diffusion period 15 after import
  sweeps. Ordinary silent firmware startup has period 0. A zero eight-bit
  divider/period wraps through 256 calls; replacing that behavior with period
  15 changes when the first input cessation affects diffusion. Small Hall B
  already had period 0.
- The old native path took the largest transfer-word magnitude from every
  20.48 kHz network pass. Firmware reads the latest XREG word during its panel
  scans. It samples nine transfer words per level-controller call, while its
  modulation routine is called 18 times. Headroom comparator registers retain
  peaks between reads independently of those sparse transfer-word samples.
- Nominal controller rates were measured during silent preparation. Direct
  constant-level tests showed roughly 1–3% changes with signal and controller
  state. The rates therefore cannot reproduce the exact instruction schedule.
- Initial modulation descriptors, random index and divider phase depend on
  the time spent loading a program and sitting silently before audio. Aligning
  decay state alone did not remove the modulation-dependent tail difference.

`DesktopHall` in `native-hall/desktop/controllers224.hpp` now clears the stale
startup diffusion period, couples level/modulation updates through the shared
18-call panel scan, samples the nominal current XREG channel on alternate
calls, and accumulates comparator peaks separately. The XREG channel changes
at input rows 0 and 50; all six independent WCS fixtures confirmed those rows
and their ADC source. Direct native program selection retains the decay bytes
and scan phase. Fresh engine activation/reset still starts a new instance;
plugin spillover continues using its existing independent slots.

The shared integer network gained an entry without controller updates.
The desktop wrapper uses it; ordinary `Hall::process` retains the previous
controller path. No runtime emulator, ROM access, extra audio-rate filter,
dynamic storage, locks or waits were introduced. Old single-Hall profiles
derive their scan frequency from their prepared level-rate table.

The scan uses the existing per-program/mode nominal rate. Individual scan
intervals, signal-dependent CPU cost and the program-load modulation phase
remain approximations. Captured peak/divider phases are still used at fresh
activation. The new startup-period correction does not imply full boot-state
or full-firmware timing equivalence.

## Tail measurements

The six-program test used the same 0.2-second seeded stereo noise burst at
48 kHz, followed by silence to 12 seconds, and a one-second silent warmup.
Both native versions used the completed ADC/DAC boundaries. The reference
executed ROM1–ROM5; matching controls were rebuilt through the firmware:
Bass 17, Mid 14, Crossover 5, Treble 23, Depth 21, minimum program pre-delay,
Diffusion 1, Analog on, wet outputs A/C. These are common table settings,
rather than a claim that every program shares the same factory defaults.
Before input, every static coefficient and delay offset matched the firmware
for all six programs.

Stereo band energy was reverse-integrated from 0.3 seconds. The fit used
−5 to −25 dB and extrapolated its slope to 60 dB (T20 decay estimate).
Acceptance required R² ≥ 0.95 for both signals and the last fitted sample
before 10.5 seconds. No gain normalization or sample null was used. Seven
broad bands cover 100–250, 250–500, 500–1000, 1000–2000, 2000–4000,
4000–8000 and 8000–10240 Hz.

With Decay Optimization enabled and modulation disabled, maximum absolute
T20 errors across those seven accepted bands were:

| Program | Previous desktop | Corrected desktop |
| --- | ---: | ---: |
| Small Hall B | 0.62% | 0.57% |
| Vocal Plate | 4.18% | 0.49% |
| Large Hall B | 4.83% | 0.44% |
| Acoustic Chamber | 4.97% | 2.92% |
| Percussion Plate A | 7.12% | 0.60% |
| Small Hall A | 7.99% | 1.04% |

Mean absolute error over the 42 broad-band comparisons fell from 2.01% to
0.36%. A separate repeat of the earlier Large Hall B startup fixture reduced
the maximum from 5.25% to 0.85%. Different firmware preparation schedules
have different converter/controller phases; these two fixtures are reported
separately.

With both controllers disabled, new and previous native WAVs were byte-exact
for all six programs: 3,456,000 stereo frames. The boundary response therefore
was unchanged by this step. This does not imply a full-reference null: the
static reference comparison still has phase/quiet-tail differences, including
an 11.41% broad-band estimate in Chamber in this fixture.

A separate narrow 9000–10240 Hz sub-band was also measured, rather than
silently included in the broad-band maximum. Most fits were rejected because
of quiet-tail floors or poor linearity. Three corrected decay-only fits were
accepted, with maximum error 6.48%; this edge-band result remains a limitation.

### Modulation and musical input

Both-controller broad-band maxima in the six-program fixture were
6.43%, 5.46%, 9.61%, 11.51%, 10.19% and 10.39%, respectively. Some bands
improved and some worsened relative to the previous desktop phase. This work
does **not** establish an across-the-board improvement with modulation on.

The earlier Hall B noise fixture illustrates the cause: its unaligned
both-controller maximum was 13.80% after the scan change (previously 11.21%).
An offline-only probe copied the reference's decay bytes, modulation
descriptors, random index/dividers and current coefficients/offsets at the
start of input. With nominal scheduling still in place, the maximum fell to
1.94%, and late RMS was `1.55863e-5` against reference `1.55894e-5`.
This diagnostic alignment is **not** present in the plugin.

The ten-second strike/chord fixture also ran in all five comparison modes.
For decay-only, late RMS (3–10 seconds) changed from `2.34502e-5` to
`3.34658e-5`, toward reference `3.47280e-5`. With both controllers it changed
from `2.34666e-5` to `2.96760e-5`, against reference `2.60328e-5`;
that late-level error worsened. These results support the startup correction
but retain modulation phase/timing as the next sound-accuracy investigation.

## CPU, compatibility and verification

The paired CPU test compared the completed ADC/DAC engine plus an independently
compiled frozen pre-controller `Hall` with the new desktop engine. Only
namespaces and include paths were adapted in the frozen source. It covered
six programs, three cases (controllers off, both on, analog bypass), five
alternating pairs per case, two seconds of audio and untimed preparation/
warmup. FTZ/DAZ matched the desktop processor's denormal handling.

Summed per-case median processing time rose **0.91%**. Individual case ratios
were 0.98141–1.02472 (−1.86% to +2.47%). This is relative DSP processing time,
not Windows CPU percentage points or a hardware realtime-margin claim.
Engine storage changed from 82,024 to 82,088 bytes (+64); default `Engine48`
remained 57,040 bytes. Processing and switching allocated no memory.

The default portable engine matched the frozen pre-change engine/core exactly
over 1,152,000 frames with rapid controls and program switches. Existing
independent integer-network, composed-control, converter, SRC and processing
checks passed. The new controller check verifies sparse transfer peaks,
independently held comparator peaks, startup period, direct program continuity,
legacy-profile cadence and 1,080,000 bounded control passes.

Windows x64 VST3 and Standalone builds passed. Processor checks passed for all
six programs at 44.1/48/96 kHz, mono/stereo, 128/511/20,000-frame blocks,
state/automation, directed switches and low-latency wet/dry behavior, with zero
audio allocations/releases. Cached-bank restart and all three ADC/DAC/
controller CTests passed. The mixed 224/XL quick-preset fixture was outside the
original-224-only run.

```sh
cmake --build build/plugin --config Release --target native_224_controllers_check
ctest --test-dir build/plugin/native-hall -C Release -R native_224_controllers --output-on-failure
```

The optional bank argument checks XREG sources in its six accompanying `.wcs`
fixtures. `build_plugin.sh --check` includes it. No ROM is needed for the
basic controller test. Private evidence remains ignored in
`build/validation/controllers-20261005/`: frozen sources, `probe.log`,
`control-rows.log`, `cpu.csv`, `benchmark-final.log`, three initial Hall B
experiments, six `program-*` render/fit directories, `music/`, and build/test
logs. ROMs, banks, WCS images and audio captures must not be committed.
