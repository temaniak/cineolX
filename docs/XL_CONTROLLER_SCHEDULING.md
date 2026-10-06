# XL controller scheduling investigation

October 6, 2026. This follows the validated
[ADC/DAC integration](XL_ADC_VALIDATION.md). The full XL correction remains
open. This checkpoint adds offline chronology tools and establishes the
coupled firmware loop; it does not yet replace Native48's nominal clocks.
The 78 frozen DSP, import and plugin source files remain unchanged.

## Independent physical reference

`cineol_xl_controller_trace` boots the hash-validated v8.21 reference for
16 simulated seconds, selects a physical bank/program, disables the three
switches, resolves factory controls through physical faders when required,
sets and verifies the requested switches, settles for 500 ms and renders
1 s of silence. It then records a 2 s independently clocked reference window
at 48 kHz in 64-frame blocks. It never supplies reference state or event times
to a native audio engine. No reference control flags are cleared by RAM writes.

Mode bits remain Mod Enh = 1, Decay Opt = 2 and Dynamic Decay = 4.
The final dataset contains 50 windows: all 22 programs with Mod off/on and
seed-17 stereo noise at level 0.08 for 200 ms followed by silence, plus the
six remaining switch combinations for Concert Hall. One additional 1 s
instruction trace is a labelled diagnostic. An earlier 34-window investigation
covers silence, noise and steps for all eight Concert Hall modes and noise
for five other graphs; its results are retained separately.

The tool records controller entries/returns, serial IRQ acknowledgement/
return, WCS requests/grants/commits, displaced fetches, operand holds and
DSP-port reads. Every controller entry/return also captures 83 columns of
software state, including modulation/dynamics counters, optimization history,
the monitor peak and the active 48-byte control record. The software headroom
memory fields are labelled as such; they are not hardware register snapshots.

Physical port samples have their own `port_read_sample` events. The existing
`port_read` API reports the IN instruction's start, not its sample time.
All 144,589 final observed port samples occur 2,593,750 ticks later: nine CPU
states plus phi2 rise. Using instruction-start timestamps for the sampled
headroom/monitor values would move their observation about 4.5 microseconds
early. OUT timestamps remain instruction starts; DMEM write visibility needs
its own boundary check.

Traced/untraced Concert Hall windows have identical CPU endpoints and same-host
FNV64 fingerprints for input, four-channel audio, CPU memory, WCS, delay memory,
CPU registers and DSP pipeline/register state. The additional physical-sample
hooks preserve those fingerprints for every previously captured state-rich
fixture. These are deterministic instrumentation consistency checks, not
physical-unit sound identity or a cryptographic byte comparison.

## Coupled scan and measured contradiction

The main firmware loop calls the slow controller, then performs nine passes.
Each pass calls Mod, runs a descriptor-count-dependent delay, an auxiliary
controller and a high/optional-low transfer-monitor read. A second Mod call
occurs when there are fewer than three interpolation pairs. The fast controller
runs on odd passes (five calls per complete nine-pass group). Ten control-record
cells are reconciled before the next slow call. Window endpoints can contain
an unfinished call; those endpoints are retained rather than counted as
complete controller durations.

Mod call duration, WCS waits, monitor branches, dynamics compilation and serial
interrupts therefore change subsequent slow/fast/Mod entry times. The three
rates cannot be independently fixed at their measured Mod-on factory averages.
Non-reverb graphs still execute the firmware scan, even though native reverb
dynamics is disabled for them; its costs contribute to their Mod cadence.

Concert Hall's current bank rates are Mod 1,094.3 Hz, slow 60.9 Hz and fast
304.0 Hz. Its final 2 s noise windows have the following observed entry counts
divided by window length; these are finite-window rates, not universal constants.

| Mode | Mod entries/s | Slow entries/s | Fast entries/s | WCS writes |
| --- | ---: | ---: | ---: | ---: |
| 0 | 1266.0 | 70.5 | 351.5 | 0 |
| 1 | 1099.5 | 61.0 | 305.5 | 8928 |
| 2 | 1188.0 | 66.0 | 330.0 | 144 |
| 3 | 1034.0 | 57.5 | 287.5 | 8546 |
| 4 | 1218.0 | 68.0 | 338.5 | 18 |
| 5 | 1062.5 | 59.0 | 295.5 | 8650 |
| 6 | 1186.5 | 65.5 | 329.5 | 138 |
| 7 | 1033.0 | 57.5 | 287.0 | 8529 |

Silence gives different rates again: mode 0 is about 70.0 slow entries/s,
and mode 2 about 68.5. The current fixed 60.9 Hz clock cannot match both this
and the noise/optimization cases within one-call window quantization.
After subtracting complete observed IRQ spans, all 54,857 completed Mod-off
calls in the final dataset last exactly 1,127 CPU states. The previously
validated local Mod branch-cost oracle remains applicable; it still uses
observed write waits and does not prove an independent free-running scheduler.

## WCS and graph-property evidence

The final 50 windows contain:

- 151,121 controller entries, 151,085 completed returns and 302,206 software
  state snapshots; unfinished window endpoints remain explicit.
- 164,288 WCS grants, commits and displaced fetches, with all CPU data-bus T1
  records matched to the actual commit's time, target lane and value.
- 492,864 operand-hold events (three per write) and 10,036 serial IRQ entries.

Each observed write commits 226,440 ticks after its grant. The next displaced
fetch is at grant + 168,750 + 58,842 ticks: the commit precedes it by only
1,152 ticks (2 ns). Reference ticks are 1/576 ns. Request waits vary with the
protected rows and pair flip-flop; the commit's fixed propagation is only one
part of the write boundary. Captured accesses are writes; read grants and
their longer hold/displacement windows need separate component checks.

`cineol_xl_wcs_properties_check` independently selects all 22 physical programs
and checks Mod off/on plus available Size endpoints and midpoint. All 80
fixtures, covering 8,354 row-property observations, retain identical protect/
reset properties. Every graph has two unprotected rows, and its RESET row
(`rows-2`) is protected. The private CSV contains only these graph properties,
not WCS words, coefficients or delay addresses. These properties can support
a native grant clock; the stable bitmap alone does not establish its phase,
pair state, write visibility or arithmetic effects.
The subsequent [native T&C component](XL_WCS_CLOCK_VALIDATION.md) now predicts
90,112 read/write accesses exactly against independently clocked reference
boards with all 22 physically selected layouts. Stable-span skipping is
field-identical and reduces component cost. Native48 integration, the coupled
firmware scan, IRQ/program phase and arithmetic visibility remain open.
The subsequent [native timed Mod check](XL_TIMED_MOD_VALIDATION.md) now couples
the Mod branch law to this clock: all 22 programs pass 20,898 local calls and
70,377 writes without supplying measured READY waits or commit times to the
prediction. Entry state, settled row origin and IRQ spans remain observed
diagnostic inputs; this does not yet validate free-running scan entries.
The following [native outer-pass model](XL_SCAN_PASS_VALIDATION.md) now
predicts the descriptor delay, monitor reads/peak and conditional second Mod
call. Its eager all-22 oracle passes 27,328 local passes and 43,509 reads;
nested Mod duration and IRQ chronology remain observed diagnostic inputs.
Its final resumable clock also passes 27,315 physical local passes, 43,500
monitor reads, 42,366 caller gaps and 3,080 encoding calls with zero errors.
The following [fast-controller component](XL_FAST_CONTROL_VALIDATION.md)
derives the detector/read work, release/retrigger branches and intermediate
compiler state. Its eager all-22 check passes 54,154 calls and 650 compiler
invocations. Nested compiler work/IRQ remain labelled local inputs.
The corrected resumable variant passes the same 54,154 calls, explicitly
checking compiler returns before following work, with zero errors/heap calls.
The following [native feedback compiler](XL_FEEDBACK_TIMING_VALIDATION.md)
predicts 1,219 local compiler calls and 13,392 writes, and is coupled to the
fast check without measured feedback durations. Its template lane bits now
come from the native graph, checked across every selected layout. Low/Mid
restoration, slow/IRQ and independent full-scan integration remain open.

## Identity, reproduction and remaining correction

The actual integrated ADC/DAC source was frozen before this work as 134 files
under `build/validation/xl-controller-scan-20261006/baseline/`. Frozen manifest
and production-file hashes are verified. Reference remains Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69` through the build-only export, with
the unchanged v5 bank SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Tools build with AppleClang 21, Release macOS arm64, C++20 and
`-ffp-contract=off`. Final tracer source/binary and reference identities are
frozen separately for reproduction. ROMs, listings, event/state CSVs and
captured coefficient values remain under ignored `build/`.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_controller_trace cineol_xl_wcs_properties_check -j2
build/xl-sound-validation/native-hall/cineol_xl_controller_trace build/validation/xl-catalog-20261005/rom-private-local build/validation/xl-corrections-20261005/programs-v821-native-v5-startup.bankxl build/validation/xl-scan-example 0 7 2 1000 64 noise
python3 script/analyze_xl_controller_trace.py build/validation/xl-scan-example
build/xl-sound-validation/native-hall/cineol_xl_wcs_properties_check build/validation/xl-catalog-20261005/rom-private-local build/validation/xl-corrections-20261005/programs-v821-native-v5-startup.bankxl build/validation/xl-properties-example.csv
```

The next implementation is a bounded native controller-stage clock coupled
to a separately checked T&C grant clock. It must predict branch costs, input
sample/clear boundaries, serial IRQ time and individual commit visibility,
including displaced fetches and operand holds. Actual first-scan/program/key
phase and control changes during tails remain explicit requirements. Native
processing must retain no ROM, opcode/CPU interpreter, allocation, I/O or waits.
Reference event injection is a diagnostic, not an acceptable shipped scheduler.

After that correction, repeat independent all-22 oracles, the paired sound
matrix with effect/split routing and control tails, matched Release CPU/storage
and 28-program plugin regression. The existing ADC/DAC checks remain a bounded
checkpoint; final host/listening acceptance is still open. This investigation
does not change sound, install bundles, or establish Daisy realtime margin.
