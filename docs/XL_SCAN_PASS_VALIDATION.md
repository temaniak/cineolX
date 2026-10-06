# Native XL Mod/monitor scan stages

October 6, 2026. This continues the
[coupled controller investigation](XL_CONTROLLER_SCHEDULING.md) and
[timed Mod component](XL_TIMED_MOD_VALIDATION.md). Native48's nominal clocks
are still present; production sound is unchanged at this checkpoint.

`desktop/scan_timing_xl.hpp` expresses the work of one outer Mod/monitor pass
as a typed control law. It accounts for the descriptor-count-dependent delay,
auxiliary invocation, high/optional-low transfer-monitor reads, signed byte
magnitude, retained peak and conditional second Mod call. It also derives
headroom logarithmic encoding and its data-dependent duration. The fixed
caller work between slow/pass/fast entries is separate from the invoked
routines and serial interrupts.

`desktop/monitor_pass_clock_xl.hpp` provides the same outer pass as a resumable
16-byte clock. It requests one Mod, auxiliary or monitor event at a time.
A future native engine can advance DSP rows to each event and provide its
value then; the clock does not pre-read future input. Nested control work
includes the separately predicted READY waits. It leaves serial IRQ time to
the surrounding scan. Both implementations use bounded arithmetic and typed
stages without ROM/opcode execution, heap activity, I/O, locks or waits.

The offline `cineol_xl_scan_pass_check` independently selects all 22 physical
v8.21 programs. Concert Hall covers all eight mode combinations; the other
reverbs cover modes 0, 1 and 7, and the three effects cover modes 0 and 1.
Each mode has silence and seed-17 stereo noise at level 0.5, with at least
200 completed outer passes per window. Physical switches settle before a
window. Noise is sustained to exercise positive/negative monitor bytes and
peak increases; the silence window exercises the optional low-byte read.
This is a local controller oracle, not a primary sound-render fixture.

The reference supplies each nested Mod duration and observed serial IRQ spans.
The outer model predicts where those nested calls occur, where each monitor
read starts, whether the low read is required, final return work and retained
peak. Actual sample times are checked separately against IN start plus nine
CPU states and phi2 rise. Idle auxiliary durations are predicted from its
gate fields. An active auxiliary ramp remains a labelled observed duration;
its complete state/work/write law still needs implementation. The headroom
encoding helper is checked at its actual reference entries/returns, including
interrupt subtraction. Its word is observed locally, not an independently
generated native detector reading.

The eager outer-pass check passed 136 windows, 27,328 completed passes and
43,509 monitor reads with zero pass/caller-gap errors and zero tracked native
new/delete calls. No active auxiliary ramps occurred in those windows.
The resumable interface additionally matches the eager law over 1,048,576
synthetic cases: all 16 descriptor counts, all high/low bytes, varied nested
durations/peaks and origins beyond 32 bits. This synthetic equivalence check
does not by itself prove a physical control law; the separately recorded
reference check supplies that evidence.

The final physical check of the resumable clock passes all 136 windows:
27,315 completed passes, 43,500 monitor reads, 42,366 caller gaps and 3,080
headroom encoding calls. Every eager/stage/level/gap comparison has zero
errors, including 3,301 retained-peak increases and 3,877 observed IRQ spans.
All 27,315 auxiliary calls take independently predicted idle gates; active
ramps remain untested. Native computations report zero new/delete calls.
The additional level-helper drain can change the next window's initial phase;
the first eager and final stage runs are separate component fixtures, not a
paired sound comparison. Final exact commands, source/binary hashes, exit zero
and per-window results are in `pass-all-run.json` and
`final-stage-checkpoint.json`; `final-component/` freezes those source files
and the actual executable. The 75 frozen desktop/core/import/plugin code
files selected by this checkpoint remain unchanged. The preceding WCS
checkpoint's complete 78-file production manifest is also reverified unchanged.

The frozen preceding source and private runs are under
`build/validation/xl-controller-stages-20261006/`. `functional-pass/` preserves
the first all-22 eager check, its source/binary hashes and exact command.
Listings remain private. The tracer now includes bounded listings for the
remaining slow/fast, auxiliary and helper derivation. Reflexion remains pinned
at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`; no dependency checkout or bank is
changed. Release macOS arm64, C++20 and `-ffp-contract=off` were used.

```sh
cmake -S . -B build/xl-sound-validation -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON
cmake --build build/xl-sound-validation --target cineol_xl_scan_pass_check -j2
build/xl-sound-validation/native-hall/cineol_xl_scan_pass_check /private/path/to/v8.21-roms
```

The following [fast-controller model](XL_FAST_CONTROL_VALIDATION.md) now
derives its outer work, software-headroom state, retrigger branches and ordered
intermediate compilations. Nested compiler duration and IRQ remain local
observations. The remaining implementation includes complete slow and nested
coefficient-work laws, fast-stage integration,
individual dynamics coefficient commits, active auxiliary ramps, independently
generated serial IRQs, record reconciliation, detector read/clear boundaries,
program/key phase and displaced-row arithmetic. After connecting the complete
scan to Native48, repeat independent all-22 and plugin regressions, paired
sound/CPU checks and host/listening acceptance. A local timing pass does not
close these requirements or establish Daisy realtime margin.
