# Controller typography and compact editor

October 6, 2026. User-requested changes to the native desktop plugin editor.
Private evidence: `build/validation/ui-consistency-20261006/`.

## Behavior

The bottom controller captions and values, and the right-hand Left/Right,
Model, Dynamic Decay, Mod Enhancement and Decay Optimization captions/values,
share the same LED glyph pitch of 2.1 at the reference panel size. These fields
no longer shrink individual strings or enlarge values. Editor scaling applies
uniformly to them. Preset and algorithm headings retain their existing hierarchy.

Display-only abbreviations retain channel/route suffixes and numbers: examples
include `HF BANDWIDTH` → `HF BW`, `CROSSOVER` → `XOVER`, `REV STOP DLY` →
`STOP DLY`, and long routed Level/Delay labels → `LVL`/`DLY`. Firmware padding
is collapsed. Full fader names/tooltips, imported bank metadata, parameter names
and IDs remain intact; values keep their original units and precision.

New editors open at 984 × 744, the existing minimum allowed size, rather than
1148 × 868. Resizing and the original aspect ratio remain available. A host may
restore a previously saved window size separately.

No DSP, filters, runtime control logic, cache format, plugin identifier or
parameter identifier changes are made in this UI block.

## Verification

- All 618 active endpoint cases across every original-224/XL page pass. Every
  displayed caption and value fits its 128-unit cell at the fixed pitch, with
  glyph glow margin. Infinite decay and frequency boundary behavior pass.
- The full isolated dual-bank plugin regression passes: all 28 programs,
  44.1/48/96 kHz, mono/stereo, block invariance, presets/session restoration,
  Dirt, page navigation, quick presets and Spillover. Callback allocations and
  releases remain zero. The default editor size assertion and resized snapshots
  pass. Four compact-editor PNG snapshots were inspected.
- The existing focus regression passes before the change. The expanded macOS
  two-window regression also passes: scoped preset dialogs, cross-window focus,
  menu ownership/instance isolation, eight rapid hide/reopen cycles without a
  timer gap, and slider mouse handlers reaching the processor after preset recall.
  It checks modal input blocking but does not inject a native OS mouse event.
- Universal arm64/x86_64 Release AU, VST3 and Standalone builds pass. Strict
  signature checks and system AU validation accompany local AU installation.

The user clarified that the intermittent Logic issue occurs on returning from
another window. It is not reproduced by the isolated focus checks above; no
speculative focus-grabbing workaround is added. Logic remained running with its
previously loaded bundle during this block, so the updated AU still requires a
host restart and actual first-click verification in that scenario. Do not claim
that the Logic issue is fixed, or that these tests constitute a Logic session.

## Installation and provenance

The updated AU replaces only
`~/Library/Audio/Plug-Ins/Components/Cineol-X 224.component`. The preceding sound
checkpoint AU is retained in this block's `previous/` directory; the earlier
pre-installation backup remains in `au-install-20261006/previous/`. Exact binary
hashes, installation outcomes and strict `auval` results are in private JSON/logs.
Original-224, XL v4 and XL v5 user banks are verified unchanged.

Retain commands, results, screenshots and rollback evidence. Preliminary overflow
failures led to caption shortening and padding removal. An expanded test's initial
crash used an older compiled callback with a dangling local reference after its
source was corrected during compilation; forcing a fresh test compilation resolves
that test-only failure. The stack and failed runs are retained. No test WAVs,
bank reimports, repeated CPU campaign or dependency modifications are needed for
these editor-only changes.
