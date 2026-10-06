# XL normal program-title work checkpoint

October 6, 2026. This continues the [panel text work checkpoint](XL_PANEL_TEXT_VALIDATION.md)
after the requested [CPU campaign](XL_COUPLED_CPU_VALIDATION.md).

`panel_title_clock_xl.hpp` derives the normal program-title worker's boundary
work and state changes: selection/context snapshot, timer clearing, template
copy, text-publication request and title formatting. It uses bounded native
arithmetic without ROM execution, character tables, I/O or heap allocation.
It predicts work and text length; it does not generate or compare glyph payloads.
The memory changes are validated at worker boundaries, not at every internal
firmware write instruction.

## Physical local validation

The Release `cineol_xl_panel_title_check` physically selects each of the 22
programs, reads its pages, returns to page 1 and waits for panel work. All
**22 normal title calls, 22 publications and 22 format boundaries** pass with
zero work/state errors and zero tracked native allocation/release. The check
also compares final text length and the actual next-character UART deadline.
A separate selected Hall / Hall run passes the same strengthened check.

For the covered mode-1 path, publication begins after 1,395 CPU states;
publication itself takes 279, formatting begins at 1,698, and the caller
returns at 3,562. Timer clears, copied selection, view, cached slider/column
and compiler-request state match the reference at the tested boundaries.
Entry panel/selection context is observed locally. This does not establish
free-running native startup state.

All retained physical calls use selection mode 1 and contain no interrupt
inside the title worker. Mode 2, other selection modes and retained view 8
have derived branches but no empirical coverage here. Special menu and
control-field workers are excluded. Receive/error interrupts and title work
interrupted by UART service remain unvalidated.

## Retained Hall / Hall chronology

A private observation-only renderer preserves the canonical physical recipe:
Hall / Hall, Dynamic on, Mod/Decay Opt off, physical stop controls 18/18 and
stop-delay 10, music seed 17, amplitude 0.08, 1,000 ms warmup, 12 seconds,
48 kHz, block 256, analog mode 1, wet A/C output.

At input start, the reference title timer is 11, view is 5, cached slider is
128 and page type is 3. The panel dispatch at **1.38788037109 seconds** enters
the normal program-title worker at **1.38790380859 seconds**, 48 CPU states
later. It does not take the control-field formatter route. This late title
job cannot account for the first gradual-transition entry discrepancy:
43.580 ms in the reference versus 48.820 ms in the retained prototype.

`input.wav`, `reference.wav` and `native.wav` are each byte-identical to the
retained `representatives-corrected/p17-m4-gate-tx` case. The panel observation
therefore leaves the existing **18.0204% envelope p95** sound discrepancy
unchanged. No phase offset or new sound correction is accepted.

A second private renderer attaches the same native title oracle to this
canonical physical recipe, alongside the observation trace. Its one pending
title call passes with zero work/state errors and no nested interrupt; native
processing allocation/release remains zero. Its three WAVs also match the
retained case byte-for-byte. The title law remains a diagnostic side check,
not a controller clock injected into native audio.

## Scope and reproduction

The new title helper is outside production Native48 and outside the prototype
measured by the CPU campaign. CPU is not rerun for this unused addition.
The five production control/runtime files still match the frozen audio baseline.
The next investigation is the initial control-transaction/scan state and
legal interrupt/read boundaries; the late title job is now separated from
the first-transition timing question. Full control-field formatting remains
necessary for complete controller coverage.
The subsequent [startup/read checkpoint](XL_STARTUP_READ_VALIDATION.md)
identifies different initial scan positions and the first headroom-read
sequence, confirming the lost detector-bit retrigger mechanism locally.
Its canonical WAVs and sound percentages remain unchanged.

Repository HEAD is `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion is
clean at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Inputs are the complete
hash-validated original XL v8.21 set and the unchanged v5 bank, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
No ROMs, banks, captures or private adapters enter Git. No plugin installation
or user cache replacement is performed.

Private sources, commands, logs, binaries, waveform identities and the panel
timeline are recorded in `build/validation/xl-panel-title-20261006/checkpoint.json`.
Historical checkpoints retain their historical source hashes.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_panel_title_check -j2
build/xl-sound-validation/native-hall/cineol_xl_panel_title_check ROM_DIRECTORY
build/xl-sound-validation/native-hall/cineol_xl_panel_title_check ROM_DIRECTORY 17
```
