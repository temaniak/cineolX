# Logic AU idle input trial (2026-10-23)

The user reports a fully drawn 1.0Alpha editor that ignores mouse input on first
opening until closed and reopened. The same intermittent Logic issue had not
been accepted as fixed in earlier releases.

## Evidence and hypothesis

Local process samples identify Logic 12.3.1 on arm64 macOS 27.2, hosting the AU
in AUHostingServiceXPC_arrow. Cineol's UI timers continue running, and the short
samples show no message-thread deadlock. Accessibility actions can open Settings;
native first-click reproduction was inconclusive because of clipped/offscreen
captures, concurrent user interaction, and later unavailable windows.

A matching primary upstream report describes loss of mouse delivery after an
idle paint interval in Logic's remote AU views. A JUCE maintainer reproduced it
with Apple's AUDistortion. Periodic minimal invalidation is a reported workaround:
https://forum.juce.com/t/macos-tahoe-logic-x-mouse-not-respond-after-moving-window/67639
See posts 15, 17 and 19. This supports a workaround trial; it does not prove the
root cause of the user's particular first-open failure.

## Change

Only macOS AUv2 editors request a 1 x 1 logical-pixel repaint at (8, 8), every
200 ms while showing and attached to a native peer. The region lies inside the
panel, away from potentially clipped native corners. No pixel value or control
appearance changes. Hidden editors request no repaint, and destruction stops the
timer. VST3, standalone and Windows editors do not start this timer.

The guard uses wrapper type rather than Logic's executable name: the editor can
run in an AU hosting service. It deliberately avoids an OS-version enumeration
check: pinned JUCE 8.0.14 returns generic MacOSX for future major versions such
as local macOS 27.2. The minimal repaint is harmless on older macOS AUs.

There is no focus grabbing, native-event synthesis, process-wide modal dismissal,
DSP modification, plugin/parameter identifier change or preset migration.

## Validation scope

The `native_hall_plugin_check` modes `--au-idle-check` and `--vst3-idle-check`
use isolated prepared fixtures and real window paint callbacks. They check idle
paint delivery, hidden suspension, reopen recovery, and AU-only activation.
The existing `--focus-check` exercises editor-owned menus/dialogs and multiple
instances. Public empty-cache and installed AU validation complement these tests.

Private logs, local installation hashes and backups are recorded under
`build/validation/logic-first-open-20261023/`. Earlier failed prototype checks are
retained as diagnostic history, not acceptance evidence. In particular, probing
only the extreme corner missed even the first trial's requested corner repaint.

Actual first-open/return-from-another-window acceptance in the user's Logic
session remains pending a complete host restart and hands-on verification.
A generic JUCE host-window test is not evidence that Logic is fixed.
This is a local trial; the published v1.0Alpha tag and release assets are unchanged.
