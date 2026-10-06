# XL dynamic-envelope causal comparison

Date: October 6, 2026. This is a bounded sound investigation following the
[313-case internal comparison](XL_INTERNAL_SOUND_VALIDATION.md). It establishes
a cause for the retained CD Plate A stop-envelope discrepancy; it is not a
shipped correction or full-catalog acceptance.

## Fixture and unchanged primary output

CD Plate A, XL index 11, physical bank 3/program 3, 100 rows; mode 4 (Dynamic
Decay on, Mod/Decay Opt off); seed-17 synthetic music/percussion at level 0.08;
1000 ms silent warmup; 12 seconds at 48 kHz/block 256; Analog on/Dirt 0, wet A/C,
0 dB input gain. The physical stop controls are 18 and stop delay 10; the actual
operator record resolves stop delay to 0 and both stop values to 18.

The current v5 bank remains SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Reflexion remains `f68ea1d069fef4a5663201693bfdfa1c579ffd69`; production is
revision `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86` plus the existing corrections.
At the causal-isolation checkpoint, all 78 production source hashes were frozen
and verified against the preceding sound checkpoint. That isolation changed no
production processing, parameter ID, bank payload, dependency checkout,
installed bundle or normal cache. The later implementation checkpoints below
add native helper laws and offline checks; production Native48 integration
remains open.

A private single-graph observation adapter reproduces Runtime's preparation,
modulation/control setup, converter path and 57-sample alignment. Both the
state/write observation and the additional port/BC observation produce
**byte-identical input, native and reference WAVs** to the retained primary
case. Thus instrumentation does not explain the original discrepancy.

## Observed transition difference

The ordinary native path moves both decay values from 85 directly to 16 at
about **55.208 ms**. Its local slow-state oracle previously checked the completed
procedure's final state/coefficients, not every intermediate state or visibility.

The physical reference's first decay compiler starts at about **41.661 ms**.
Its intermediate values are **72, 64, 56, 48, 40, 32, 24, 16**. Those compiler
entries span **56.387 ms**; the first compiler entry to the slow procedure's
return spans **65.264 ms**. These are entry/return measurements; actual per-row
coefficient commits are retained separately. The reference then restores normal
decay at about **108.377 ms** and performs another gradual fall. The ordinary
native path does not reproduce that restoration in the first percussion tail.

Reference headroom reads establish that the slow procedure really reads/clears
both headroom registers at PC 0x0FA2/0x0FAC; fast reads occur at 0x81D4/0x81D9.
Removing the native slow clear is therefore rejected as a proposed explanation.
The relevant difference is the gradual compiler/busy chronology and its relation
to subsequent reads and retriggers. No headroom change is shipped.

## Coefficient-trajectory isolation

A separate diagnostic disables the native controller and applies only the
reference's observed time-varying coefficient commits at host-frame boundaries
(at most 20.833 microseconds granularity). Initial prepared coefficients are
checked equal before rendering. The traced reference makes **zero offset changes**
in this fixture. Native delay memory, graph registers, converter clocks/state and
audio are never copied from reference; both paths retain independent startup and
clocks. Input/reference WAVs remain byte-identical to the primary case.

| Metric | Ordinary native | Observed-coefficient replay diagnostic |
| --- | ---: | ---: |
| Maximum absolute 100 Hz–15 kHz band RMS difference | 8.5058% | 0.0575% |
| Active 50 ms envelope p95 absolute difference | 26.8945% | 1.2953% |
| Active 50 ms envelope maximum absolute difference | 27.8443% | 1.5667% |
| Overall energy difference | −0.147858 dB | −0.000022 dB |

The unchanged final-second native/reference RMS levels are approximately
−102.033/−103.429 dBFS. These quiet residual differences remain measured;
they are not suppressed or declared listening-equivalent. Exponential T20 is
unavailable for this dynamic fixture. The active-envelope threshold is the same
`max(reference_peak - 40 dB, -90 dBFS)` policy as the internal percentage report.

This isolates the principal audible-response discrepancy to coefficient
transition chronology. The earlier observed-mean-rate injection alone did not
improve CD Plate A, whereas reproducing the coefficient sequence reduces the
response/envelope differences below the provisional 5% criterion. The diagnostic
does not prove a free-running native control schedule, and no reference trajectory
may be used as production implementation data.

## Evidence and next implementation

Private evidence: `build/validation/xl-dynamic-envelope-20261006/`.
`baseline/manifest.json` freezes actual old sources/renderer. `trace-run.json`
and `trace-v2-run.json` record commands, source/binary hashes and primary-WAV
identity. Their source versions are preserved under `trace-v1/` and
`trace-v2-source/`. `first-second-events.json` retains the significant transitions
and headroom reads; `reference_states.csv`, `reference_ports.csv` and
`reference_writes.csv` retain the underlying observations.

`replay-run.json` labels the observed-coefficient injection and exact reference
recipe. `replay-comparison.json` and `replay-percentages/` contain the paired
metrics. A further bounded trace captures the first A7A9 compiler's instructions
only to derive native semantic work/write laws offline; firmware bytes, traces
and private adapters remain outside Git.
That bounded instruction observation also preserves all three primary WAVs
byte-for-byte. Its first A7A9 invocation spans 15,678 CPU states (including READY
waits), contains 1,813 observed instructions and no serial IRQ entry. The six
first-page semantic handlers are indirectly dispatched; nested interpolation,
arithmetic and WCS writers are retained in the private call/dispatch summaries.
One invocation does not establish all branch costs or a catalog timing law.

Next: independently derive the decay compiler's step order and durations from
the existing semantic control profile, then schedule gradual transitions and
compiler-busy/read boundaries in fixed native state. Preserve the serialized
DynamicsState/bank layout; any extra event state belongs to the prepared runtime.
The DSP/converter implementation can stay intact: the isolated replay already
reaches small differences without copying DSP state or implementing extra bus
displacement/hold behavior. Validate the native law across the applicable catalog,
retain meaningful old-fail/new-pass boundary checks, and repeat paired sound,
heap/CPU/Spillover and plugin checks after a production correction. Listening and
host acceptance remain open.

Execution follows the user's [batched validation cadence](XL_SOUND_ACCURACY_HANDOFF.md#execution-and-validation-cadence):
implement related transition/compiler changes together, use short functional
checks during development, and defer the paired CPU campaign and full-format
builds until the assembled sound correction is stable. Existing valid performance
evidence is reused when the measured processing code/configuration is unchanged.

## Native gradual-transition implementation checkpoint

The first native part of the correction is now implemented in
`native-hall/desktop/decay_transition_xl.hpp`. `DecayTransitionXL` yields each
LF/MID step's compiler entry and return, retains busy state until the caller
resumes, and handles shared/separate stop controls and stop-delay counters.
It derives branch work from the control law rather than a measured millisecond
constant. Downward values are quantized and decrease by eight; an unaligned
stop value of 18 finishes at 16. Upward restoration installs the requested
normal value in a single compilation. Neither `DynamicsState` nor the bank
serialization layout changes.

The local oracle `cineol_xl_decay_transition_check` covers all 19 reverb
programs in three physical-control variants: equal stop values, unequal LF/MID
stop values, and unequal stop values with stop delay. Physical faders retain
their normal calibration; reference memory is observed rather than overwritten.
Pulses, drops and retriggers use detector masks 15/3/0/31/0 over two seconds
per variant. The three non-reverb programs have no such controller.

All **57 variants, 6,829 transitions, 5,113 compiler entry/return pairs and
32 excluded serial-IRQ spans** pass with zero state/work-boundary differences
and zero native allocations/releases. The largest transition contains 22
separate compilations. A separately compiled actual frozen `Dynamics` source
fails the new intermediate-boundary check on CD Plate A: its LF/MID values
are 16/16 when the physical reference's first compilation is 72/72. All 59
frozen core/desktop source hashes used by that baseline remain verified.

This is a native **ramp** law, not a completed sound correction. The oracle
supplies each compiler's observed duration and excludes observed IRQ time;
it checks the ramp-owned LF/MID/flags/stop-counter fields, not the compiler's
other state/cache effects. `Native48` still uses the preceding collapsed
controller, so the 313-case sound figures remain unchanged. No CPU campaign,
plugin format rebuild, installation or listening was performed for this helper.

Exact commands, source/binary/log hashes and all variant results are retained
in `build/validation/xl-decay-transition-20261006/boundary-checkpoint.json`.
The next part of the same correction block derives native decay-compiler
durations/write visibility and connects gradual/busy/retrigger behavior to
the runtime. Only then does an independent current sound render establish
whether the ordinary CD Plate A envelope error improves.

## Native decay-compiler work and write checkpoint

`native-hall/desktop/decay_compiler_xl.hpp` now derives the main LF/MID compiler's
work and writes from the existing semantic profile. It handles the first-page
cache invalidation, LF/MID arithmetic, signed curve interpolation, byte/wide
products, LF ratio division, decay-time calculation and the individual sign/
magnitude writes. A zero-target LF group invalidates the MID cache, not the LF
cache. Coefficient-lane structural bits come from the native graph, including
the source-less RESET row; no ROM instruction stream is stored in the helper.
The enclosing runtime must provide a coherent non-decay control snapshot.

The local `cineol_xl_decay_compiler_check` passes all **19 reverb programs,
2,207 compiler calls, 14,018 writes and 47 excluded serial-IRQ spans**. Work,
cache/control state, payloads, write-instruction starts, commit times and grant
clock all have zero differences. The native kernel allocates/releases nothing.
Its entry/cache snapshot, initial quiet row-zero clock phase and IRQ spans are
observed oracle context; compiler arithmetic, durations, writes and READY waits
are independently predicted. It is not a free-running controller check. Direct
and CMake target builds pass; no CPU benchmark or plugin format rebuild is run.
Commands and hashes are in
`build/validation/xl-decay-compiler-20261006/compiler-checkpoint.json`.

## Independent native busy/ramp sound prototype

A private runtime prototype combines the native gradual law and compiler with
a fixed 128-entry pending-write array. It retains detector input while busy,
freezes the preceding nominal Fast/Slow poll timers during compilation and
applies native-predicted coefficient writes after DSP passes. It uses its own
graph/write clock and prepared controls, with no observed state, coefficient
trajectory, mean-rate injection or recorded millisecond duration in processing.
The production Native48 source remains unchanged; the prototype is labelled
`diagnostic_native_gradual_busy_nominal_poll` and is not a shipping correction.

The retained CD Plate A mode-4 recipe has **byte-identical input and reference
WAVs** to the ordinary primary case. Native audio changes, stays finite and
records zero processing allocations/releases. Results are:

| Metric | Ordinary native | Native busy/ramp prototype |
| --- | ---: | ---: |
| Maximum absolute 100 Hz–15 kHz band RMS difference | 8.5058% | 4.9177% |
| Active 50 ms envelope p95 absolute difference | 26.8945% | 22.8037% |
| Active 50 ms envelope maximum absolute difference | 27.8443% | 24.5122% |
| Overall energy difference | −0.147858 dB | −0.087789 dB |

The final-second native/reference RMS remains approximately −102.033/−103.429
dBFS; T20 remains inapplicable to this dynamic envelope. Smaller band errors
do not justify accepting the still-large active-envelope discrepancy. Keep
this result as partial improvement, not a completed correction or catalog pass.

Private source/binary/recipe/WAV/analysis hashes and the exact compile/render
commands are in
`build/validation/xl-decay-compiler-20261006/busy-sound-checkpoint.json`.
The untouched original primary and coefficient-replay diagnostic are retained.
The prototype still uses bank nominal idle clocks, lacks native serial-IRQ
chronology and has not addressed Mod-on scan freezing or parameter transactions
during busy work. Next, couple poll/read/retrigger timing to the actual native
scan work; repeat only this representative sound case while localizing that
remaining envelope difference. Full sound/plugin/CPU checks follow the assembled
stable correction, under the agreed batched cadence.

The subsequent [slow-controller stage checkpoint](XL_SLOW_CONTROL_VALIDATION.md)
derives the input/trigger stage, headroom-display work and post-ramp counters
without nominal poll-rate injection. Local all-22 checks pass 5,995 entry,
3,330 display and 6,005 tail calls with zero differences/heap activity. The
tail check exposed and corrected 32 excess states in the shared division
work helper. An additional all-19 unequal Low/Mid stop compiler campaign passes
2,359 calls and 12,466 writes. Runtime/free-running serial integration remains
open, and this checkpoint makes no new audio or CPU acceptance claim.

The following [coupled scan checkpoint](XL_COUPLED_SCAN_VALIDATION.md) connects
those stages in an independent private Mod-off Dynamic pilot. After correcting
its initial auxiliary flag, the canonical CD Plate A band / active-envelope
p95 errors improve to 0.1554% / 1.2800%, with byte-identical input/reference
WAVs. Input variants remain favorable, but Plate and two split cases are not
yet accepted. Hall / Hall still begins its first fall about 5.24 ms late and
misses the reference's first restoration; startup/read phase remains the next
bounded cause to resolve. Native TX and secondary-index component checks pass.
The report retains superseded variants and all larger residuals. Production
Native48, the ordinary matrix and CPU acceptance remain unchanged.
