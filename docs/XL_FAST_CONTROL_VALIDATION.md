# Native XL fast-controller work and stages

October 6, 2026. This continues the
[outer scan-pass model](XL_SCAN_PASS_VALIDATION.md). It adds native fast
control work and a resumable event clock; Native48 still uses nominal rates.
The full XL sound correction remains open.

`desktop/fast_control_timing_xl.hpp` predicts the fast routine's early returns,
two detector reads, software headroom accumulation, logarithmic level work,
optimization release, retrigger tests and normal-decay restoration. It retains
each intermediate feedback compilation while the optimization amount returns
to zero in one/two-unit steps. Compilers receive the intermediate state rather
than only the final amount. The model uses typed state/control fields and
bounded arithmetic, without ROM/opcode interpretation or processing heap/I/O.

`desktop/fast_control_clock_xl.hpp` yields individual detector reads, compiler
entries and compiler returns. The enclosing scan can advance rows to each
event instead of reading future input. A compiler's return state remains
observable before the following branch or next compilation executes. The
clock is 24 bytes and borrows a separate `FastControlMemoryXL` object; 24 bytes
is not the complete controller/engine storage. Period and compiler work are
supplied by the separately implemented native compiler/profile. Serial IRQ
wall time remains the enclosing scan's responsibility.

## Physical local oracle

`cineol_xl_fast_timing_check` loads the unchanged private v5 bank and
hash-validated v8.21 ROMs. It boots normally, physically selects all 22 programs,
and sets Dynamic Decay/Decay Opt through actual operator keys with retries and
stored-switch verification. Each of the 19 reverbs covers four combinations;
the three effects cover their disabled dynamics branch. STOP faders use raw
18 and stop delay raw 10, through physical slider operations and settling.
There are 79 windows, each two seconds at 48 kHz in 64-frame blocks.

The windows deliberately stimulate reference comparator pins with a repeating
15/3/0/31/0 pattern to exercise fall/retrigger branches. Audio pins stay zero.
This is a labelled detector/controller diagnostic, not an analog-input or
primary sound comparison. State, raw words, cached reset period and special
flags are observed at each local fast entry. Actual detector bytes and nested
compiler durations/IRQ spans are diagnostic inputs. Native branch work,
read starts, intermediate states, compiler entry/return boundaries and final
controller/software-headroom state are predicted and compared separately.
The native model calculates compiler period/marker changes from the bank's
control profiles; compiler return state is not copied into it. Actual sample
times match IN start plus nine CPU states and phi2 rise. The detector hardware's
independent clear/hold schedule is a separate remaining requirement.

The eager all-22 model passes 54,154 calls, including 16,812 active calls,
492 feedback compilations and 158 normal-decay restorations, with zero work,
state and boundary errors. Up to six feedback compilations occur in one fast
call. It subtracts 911 observed local IRQ spans. The actual eager source and
executable are preserved under `eager/`, with hashes, commands and exit zero.

The first resumable candidate incorrectly advanced past the compiler return
before exposing its state. Final fast state and total work still matched,
but the physical intermediate-state check rejected it with 411 stage errors.
That actual candidate source/executable/results are preserved under
`rejected-return-boundary/`. The corrected API yields an explicit compiler
return and resumes following work separately. This is a rejected new component
prototype, not a claim that old production Native48 failed this new timing API.

The corrected clock and eager law also agree over 1,048,576 synthetic cases,
24,547,000 compiler events and 1,049,022 reads, including varied byte states,
amounts through 255 and origins beyond 32 bits. Compiler return boundaries
are explicitly checked before resumption. Synthetic equivalence supports
the stage implementation; physical reference checks establish the local law.

The final corrected resumable clock passes the same 79 physical windows and
54,154 calls: all work, state, event and return-boundary errors are zero,
including the 492 feedback and 158 restore invocations. Native computation
has zero tracked new/delete calls. The exact command, final source/binary
hashes and exit zero are retained in `all-run.json`; `final-component/` freezes
the actual sources/executable. `final-fast-checkpoint.json` retains per-window
counts and all limits. The preceding 78-file production manifest and all 185
frozen baseline files are reverified; production files are unchanged.

## Reproduction and remaining work

Private evidence is under `build/validation/xl-fast-controller-20261006/`.
The preceding source is frozen in `baseline/`. The unchanged bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Reference remains Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69` through
its build-only export. Release macOS arm64, C++20 and `-ffp-contract=off` were
used. Extended private compiler/serial listings preserve every previously
recorded Concert Hall reference metadata/fingerprint field.

```sh
cmake -S . -B build/xl-sound-validation -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON
cmake --build build/xl-sound-validation --target cineol_xl_fast_timing_check -j2
build/xl-sound-validation/native-hall/cineol_xl_fast_timing_check /private/path/to/v8.21-roms /private/path/to/v5.bankxl
```

Complete native coefficient-compiler work/individual WCS commits, the slow
controller, active auxiliary ramps, serial IRQs and display/record work still
need independent timing. Detector read/clear boundaries, startup/program/key
phase and displaced-row arithmetic also remain open. After connecting the
full scan to Native48, repeat all-22 and plugin oracles, paired sound/CPU,
Spillover/control edges and host/listening acceptance. This local component
changes no production sound and proves no whole-plugin or Daisy CPU margin.
The following [native feedback compiler](XL_FEEDBACK_TIMING_VALIDATION.md)
now removes observed feedback duration in an optional coupled fast check:
54,153 local calls and 492 predicted feedback invocations pass. Restoration
duration, IRQ/origin and coarse-index context remain observed inputs.
