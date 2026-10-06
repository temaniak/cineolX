# XL panel text work checkpoint

October 6, 2026. This follows the explicitly requested
[coupled CPU measurement](XL_COUPLED_CPU_VALIDATION.md) and continues the
Hall / Hall startup/panel investigation. Production Native48 and the retained
independent sound recipes remain unchanged.

`panel_text_work_xl.hpp` derives the interrupt-disabled text-publication
request, its initial UART header write, and the two status text workers.
It contains bounded arithmetic/state updates, no ROM execution, character
tables, I/O or allocation. The enclosing worker supplies prepared text length
and owns text contents; the clock supplies publication work and serial state.
The two status workers use fixed 24-character fields and predictable template
lookup/copy work. Status formatting does not substitute a measured duration.

## Local oracle results

The public `cineol_xl_panel_text_check` physically selects all 22 programs,
reads their pages, returns to page 1 and allows pending panel work to finish.
It passes **1,209 text-publication calls** with zero work, publication-state
or character-deadline errors and zero tracked native allocations/releases.
The request predicts 279 CPU states, UART command/output instruction starts
at 100/205 states, and the next complete-character deadline at output start
+10+2,292 states. The final CMake Release check also verifies these two
instruction-entry boundaries explicitly.

Entry text length/header and any interrupt context are local observed inputs;
this is a publication law oracle, not native glyph formatting or a free-running
panel initialization proof. No interrupt occurs inside the retained 1,209
requests, so the test does not establish receive/error interrupt handling.
Direct optimized and CMake Release variants pass; a selected Hall / Hall
check separately passes 57 requests.

The extended `cineol_xl_panel_service_check` now computes both status worker
durations natively instead of receiving them from the local reference. The
all-22 **explicit synthetic status-boundary** campaign seeds two reference
menu states per program and passes **9,827 dispatch calls**, including
**60 native status workers** (38 ordinary, 22 alternate), 38 title dispatches
and four interrupt spans. Outer work/state errors and native allocations/
releases are zero. Those 44 RAM seeds belong only to this labelled local
boundary oracle; they never enter physical sound fixtures or plugin processing.
Title work, inner title timer clears and interrupt spans remain observed inputs.
The earlier physical-only 9,867-call dispatch campaign remains distinct.

An initial template lookup cost charged register exchange as five CPU states.
The pinned CPU instruction model uses four. The independently frozen actual
wrong header/check source reports an alternate-status worker of 1,683 states
versus the reference's 1,682 and fails the same Hall / Hall fixture (exit 1).
The corrected source predicts ordinary/alternate worker costs of **1,300 /
1,682 states** and passes (exit 0), including the resulting outer return
boundary. This is an instruction-law correction, not a fitted time offset.
Character payload equality has not been claimed by these work checks.

## Scope and next step

The subsequent [normal program-title checkpoint](XL_PANEL_TITLE_VALIDATION.md)
derives title boundary work and timer/context changes. All 22 physical title
calls pass; the retained Hall / Hall trace places its pending title job at
1.388 seconds, after the first-transition mismatch. Control-field formatting,
initial control/scan state and legal interrupt/read chronology remain open.

The helpers are still outside the shipping runtime and outside the prototype
measured by the early CPU campaign. That CPU result therefore remains a
measurement of its recorded frozen ordinary/candidate sources. No new CPU
campaign runs merely for these unused helper additions.

Text request and both status worker work are now derived. The remaining title/
control-field formatter, initial parameter-transaction and timer state, and
legal interrupt/read boundaries must be joined before rerendering the retained
independent Hall / Hall fixture. Its current 18.0204% envelope p95 is not
resolved or replaced by these local work checks. The earlier phase-tuning and
rejected parameter-compiler-state diagnostics remain separate.

## Reproduction and evidence

Repository HEAD remains `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`, Reflexion
remains clean at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`, and the firmware
input is the complete hash-validated original XL v8.21 set. The unchanged v5
bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
No ROMs, prepared banks or captures are added to Git; no plugin installation
or normal user cache replacement occurs.

Private logs, source/binary hashes, the frozen failing source tree and exact
commands are under `build/validation/xl-panel-text-20261006/checkpoint.json`.
The previous outer-only test source is retained separately. Earlier checkpoint
hashes describe their historical source versions; use this new checkpoint for
the extended current public helpers/oracles.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_panel_text_check cineol_xl_panel_service_check -j2
build/xl-sound-validation/native-hall/cineol_xl_panel_text_check ROM_DIRECTORY
# Explicit synthetic reference menu seeds; not a physical sound fixture:
build/xl-sound-validation/native-hall/cineol_xl_panel_service_check ROM_DIRECTORY -1 --synthetic-dispatch
```
