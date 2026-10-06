# XL coupled scan sound checkpoint

October 6, 2026. This continues the Dynamic Decay investigation after the
[slow-stage checks](XL_SLOW_CONTROL_VALIDATION.md). A private native pilot now
connects Slow input, gradual decay compilation, post-ramp work, display,
nine monitor passes and five Fast calls. It derives the next poll from the
preceding work and individual READY waits, instead of bank nominal rates.
Production `Native48` remains unchanged; these are experimental renders.

## Independent native processing

The pilot owns per-channel headroom accumulation and clearing, fresh monitor
reads, native LF/MID and feedback compilers, fixed pending-write storage and
its own graph grant clock. It uses the validated ADC/DAC and DSP implementation
without changing filters, frequency response or delay memory. No reference
state, coefficient trajectory, event schedule, average rate or measured
millisecond duration enters native processing.

Its TX variant schedules complete-character UART deadlines and a native
transmit interrupt law. Pending writes account for inserted interrupt work
before calculating their READY grants. Interrupt entry is still approximated
at semantic computation/write boundaries; legal instruction-boundary timing
has not been fully derived. Initial panel timers/text and scan phase are also
incomplete. Mod-on, active auxiliary ramps, receive/error interrupts and
parameter transactions during compilation remain outside this pilot.

The display's periodic port-3 read is the DSP arithmetic monitor, not the DIP
switch. The earlier display check called that event `DIP`; its observed byte
and work checks remain valid. The actual slow DIP check reads address FFFF.

## Native transmit and secondary-index checks

`serial_tx_work_xl.hpp` derives settled TX-ready interrupt state, work and
output timing. The fixed 2,292-state character boundary is inherited from the
pinned reference peripheral model. It is not a measured controller rate.
The public `cineol_xl_serial_tx_check` physically selects all 22 programs and
passes **5,102 interrupts** with zero work, state or character-deadline errors
and zero tracked native allocations/releases. Covered branches are 3,957
indicator bytes, 6 queued indicator headers, 150 text bytes and 989 transmitter
disable operations. Echo and receive/error branches are not covered. Entry
panel/UART context remains a local observed oracle input. Direct optimized and
CMake Release builds pass; this does not prove free-running IRQ entry timing.

Broader sound checks caught an incorrect pilot initialization: reverb programs
start with auxiliary-enable bit 0 clear. Charging its set-bit path added 41
states per pass and changed the first transition's phase. The pilot now derives
that bit from the already prepared dynamics-enabled property. Earlier
`special=1` prototype figures, including a 0.029% CD Plate A band figure and
a Plate envelope regression, are retained but superseded.

A second binding correction uses control cell 7 and its Definition cap for
the second MID feedback index, rather than Stop MID cell 13. A private local
oracle passes all four relevant split programs, 16 physical mode cases,
233 compiler calls and 2,586 writes with zero errors. The independently frozen
old binding fails the same index check. Other entry/index/IRQ context remains
observed. This correction does not remove the remaining split sound mismatch.

## Paired sound results

All primary cases use independently clocked native/reference processing:
48 kHz, 12 s, 256-frame blocks, analog path on, wet A/C, physical stop values
18/18 and zero effective stop delay, Dynamic on and Mod/Optimization off.
Canonical cases use music seed 17, amplitude 0.08 and 1,000 ms warmup.
Input and reference WAVs are byte-identical to the retained ordinary native
cases for all four compared programs; only native audio changes.

| Canonical program | Ordinary maximum band RMS error | Pilot band RMS error | Ordinary active-envelope p95 | Pilot active-envelope p95 |
| --- | ---: | ---: | ---: | ---: |
| CD Plate A | 8.5058% | 0.1554% | 26.8945% | 1.2800% |
| Plate | 1.4356% | 0.4715% | 27.0711% | 5.4751% |
| Hall / Hall | 1.3448% | 1.6506% | 24.2062% | 18.0204% |
| Plate / Plate | 2.8853% | 1.2789% | 12.7787% | 6.8889% |

The pilot is not accepted across these programs. Hall / Hall's band error
increases slightly, and its envelope remains materially different. Envelope
errors are diagnostics, not an automatic extension of the provisional 5%
band-level criterion. Dynamic T20 fits remain inapplicable, not zero error.

CD Plate A also passes independent input variations: seed 991, amplitudes
0.04/0.16 and 257 ms warmup. Across those four variations and the canonical
case, maximum band RMS error is at most **0.5679%**; active-envelope p95 ranges
from **1.2800% to 3.8954%**. The 0.04-amplitude case has a 5.3546% maximum
active-window error. Retained final-second floors and all unfavorable cases
remain in the percentage reports. These measurements do not establish listening
or host acceptance. Every pilot render stays finite, below the existing peak
guard and records zero processing allocations/releases.

The same auxiliary-corrected source supplies the first six table/variation
cases. The later secondary-index correction is rerendered for Hall / Hall and
Plate / Plate. It leaves the former WAV unchanged and only slightly changes
the latter's band error; both envelope p95 figures remain unchanged.

## Remaining Hall / Hall chronology

Additional reference state/write/port instrumentation preserves all three
primary WAVs byte-for-byte against its untraced render. The first reference
gradual compiler starts at **43.580 ms**, while the pilot starts at
**48.820 ms**. Intermediate step intervals match the derived compiler law.
The reference restores normal decay at **84.039 ms** and begins another fall
at 99.316 ms. The pilot misses that first restoration and stays stopped until
the next input pulse at about 502 ms. Its first Slow read occurs late enough
to consume the remaining hardware headroom before the busy interval.

This identifies a remaining initial scan/read phase and retrigger discrepancy.
It does not justify changing EQ, smoothing the envelope, fitting a poll rate
or copying observed event times into production. The worst retained Hall /
Hall active-window error is 32.64% at 200 ms: reference/native RMS are about
−54.71/−58.14 dBFS. This is not merely a late numerical-floor fit.

Next, derive the startup/control-transaction and panel-service state needed
to reproduce that phase, check actual read/return boundaries, then rerender
these retained recipes. After the complete Dynamic block is stable, expand
its affected catalog/modes and handle Mod-on coupling before final CPU,
Spillover, plugin-format and host/listening validation.

## Startup/panel follow-up

Physical setup probes retain the same program/fader/toggle operations for
Plate, CD Plate A, Hall / Hall and Plate / Plate. After setup and the 1,000 ms
silent warmup, the reference still has a pending title timer; the pilot starts
its panel timers at zero. Hall / Hall's timer is 20 at setup completion and
11 at input start. It does not expire during that warmup, so its pending title
worker cannot by itself explain the first 5.24 ms difference. The probe's
`frame` column is the operator frame counter: direct Engine warmup does not
advance that counter. It is not an event timestamp or phase oracle.

`panel_service_clock_xl.hpp` now derives the slow-return dispatch, including
idle/busy returns, title dispatch with timer clearing, and the two status
branches. Its physical all-22 local oracle passes **9,867 calls**, including
38 title dispatches and four IRQ spans, with zero outer work/state errors and
zero native allocations/releases. A separate, explicitly synthetic Hall /
Hall boundary check seeds two reference menu states to cover status dispatch:
419 calls, two title, two status and one alternate-status dispatch, also with
zero errors. Those RAM seeds are confined to the diagnostic oracle and never
enter the physical sound recipes. Title/status worker duration, inner timer
clears and IRQ spans remain observed local inputs. This is not a complete
native text worker, UART timeline or production integration.

A labelled native-only phase sweep retains the canonical Hall / Hall input
and reference WAVs unchanged. Nine independent runs add 0–16 ms of native
silent warmup in 2 ms steps. The zero-offset run reproduces the retained pilot
WAV byte-for-byte, validating the smaller native-only harness and alignment.
All outputs remain finite and allocate/release nothing in processing.

| Extra native warmup | Maximum band RMS error | Active-envelope p95 | First native restoration |
| --- | ---: | ---: | ---: |
| 0 ms | 1.6506% | 18.0204% | 502.278 ms |
| 2 ms | 1.6475% | 17.7504% | 503.907 ms |
| 4 ms | 1.6081% | 17.7647% | 501.846 ms |
| 6 ms | 0.7897% | 13.3036% | 83.572 ms |
| 8 ms | 0.8556% | 18.4079% | 81.506 ms |
| 10 ms | 1.0129% | 19.2075% | 79.510 ms |
| 12 ms | 1.6336% | 16.7258% | 502.072 ms |
| 14 ms | 1.6324% | 16.5835% | 505.563 ms |
| 16 ms | 1.3828% | 19.9536% | 87.657 ms |

The 6 ms diagnostic starts its first fall at 44.276 ms and restores at
83.572 ms, versus reference 43.580/84.039 ms. This confirms phase sensitivity
of the missing retrigger; it does not resolve the remaining envelope errors.
No selected offset, measured phase or reference schedule is added to production.

A separate private candidate also executes the already derived main compiler
law during coherent parameter preparation, with unchanged coefficient caches
and zero writes. Its controller amount/index/period side effects differ from
the older nominal parameter path. For Hall / Hall, maximum band error becomes
1.6378%, but envelope p95 **regresses to 22.9273%**. Retain this unfavorable
result; refreshing compiler state alone is not an accepted sound correction.
The importer's pre-Mod-off dynamics snapshot remains a startup-coherence
question, not a demonstrated cause or a bank migration made here.

The next bounded step is the title/status worker and control-transaction
initialization, followed by legal interrupt/read boundaries and the same
independent retained sound recipes. Phase tuning is excluded. The follow-up
sources, binaries, logs, commands and identities are recorded separately in
`build/validation/xl-coupled-scan-20261006/startup-panel/checkpoint.json`.
The four-program sound table above remains the current independent checkpoint;
none of these diagnostics replaces it or establishes catalog acceptance.
The subsequent [panel text work checkpoint](XL_PANEL_TEXT_VALIDATION.md)
derives publication and both status worker costs, removing their observed
duration inputs from the extended local oracle. Title/control-field work and
startup/free-running IRQ state remain open; no sound correction is claimed.
The subsequent [normal title checkpoint](XL_PANEL_TITLE_VALIDATION.md) passes
22 physical title workers with native boundary work, timer/context changes,
text length and UART deadline. Its observation-only canonical Hall / Hall
trace preserves all three WAVs and places the pending normal title job at
1.388 seconds, after the first-transition discrepancy. Initial control/scan
state, control-field formatting and legal IRQ/read timing remain open.
The [startup/read follow-up](XL_STARTUP_READ_VALIDATION.md) observes the
reference finishing Slow at input start, versus prototype monitor-pass ordinal
4. Its later Slow consumes the detector bit that reference Fast uses for the
first restore. A separate local 16/0 versus 0/0 counterfactual confirms the
native branch consequence. The canonical WAVs remain byte-identical; this
does not reduce the retained 18.0204% Hall / Hall envelope p95. Physical
parameter-transaction/reset and scan-retention semantics are the next step.
The [fader transaction follow-up](XL_PARAMETER_TRANSACTION_VALIDATION.md) passes
1,004 all-22 physical transactions and derives native pickup/deferred-refresh
boundaries. Its actual experimental-setter counterexample resets clock and
held/divider state, violating observed retention. Live-state update integration,
busy reconciliation/compiler handling and complete transaction clocks remain
open; no new independent sound improvement is claimed.
The subsequent [live parameter queue checkpoint](XL_LIVE_PARAMETER_VALIDATION.md)
fixes that setter reset for bounded ordinary parameters in a separate ignored
pilot. All 19 reverb retention/busy/coalescing checks pass. In its matched
LF-event pair, band RMS error improves from 25.3844% to 1.5975% and envelope p95
from 648.8907% to 18.7391%. This is separate from the unchanged canonical startup
fixture (18.0204% p95). Complete physical transaction clocks and startup/IRQ
chronology remain open; this newly changed pilot has not been CPU measured.

## Reproduction

Repository HEAD remains `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion
remains clean at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Firmware is the
complete hash-validated original XL v8.21 set. The unchanged v5 bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Five production control/runtime files remain byte-identical to the actual
frozen audio baseline. The public change adds the standalone TX work law/check;
all coupled runtime variants, ROM listings, banks and renders stay ignored.

Private commands, source/binary/log hashes, per-case metrics, frozen failed
bindings and WAV identities are recorded in
`build/validation/xl-coupled-scan-20261006/checkpoint.json`. This includes the
superseded no-serial and incorrect-auxiliary variants. At this sound checkpoint,
CPU benchmarking, the full 313-case campaign, plugin installation and listening
had not been run for the assembled experimental block. The user's subsequent
explicit request is fulfilled by the [early CPU checkpoint](XL_COUPLED_CPU_VALIDATION.md):
five alternating paired Mod-off rounds, approximately 1.12% relative extra
thread CPU and zero processing heap activity. Full sound/plugin, complete panel/
Mod-on and host/listening acceptance remain open; wall outliers are retained.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_serial_tx_check -j2
build/xl-sound-validation/native-hall/cineol_xl_serial_tx_check ROM_DIRECTORY
cmake --build build/xl-sound-validation --target cineol_xl_panel_service_check -j2
build/xl-sound-validation/native-hall/cineol_xl_panel_service_check ROM_DIRECTORY
# Explicit synthetic branch oracle, separate from physical sound fixtures:
build/xl-sound-validation/native-hall/cineol_xl_panel_service_check ROM_DIRECTORY 17 --synthetic-dispatch
```
