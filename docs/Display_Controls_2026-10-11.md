# Cineol-X display controls and compact editor

The editor now exposes **Display brightness** (15–100%), **Option illumination**
(0–100%), and **Compact display mode** through the gear's Settings panel.
Brightness affects the virtual LED components, preserving the glass material.
Algorithm and output selectors have illuminated boundaries; enabled switches
and the selected numbered page have stronger illumination. Setting illumination
to zero removes the added boundaries/core brightening.

Compact mode keeps the screen, the title, the gear and a narrow housing border,
hiding the physical faders and quick-preset keys. At the default width the editor
changes from 984×744 to 984×252. Resizing retains the panel's aspect ratio, and
switching modes retains its current width. Settings and the preset browser fit
inside the shorter editor. The gear remains outside the Settings overlay.

Every available numeric value can be dragged vertically in either layout.
Up increases the value; down decreases it; Shift slows adjustment, and double
click restores the existing fader's default. Keyboard operation uses JUCE's
slider handling. Screen values reuse the existing fader's full normalisable
range, snapping and variable XL pre-delay bounds. Parameter attachments preserve
DAW gesture boundaries, original 224 bindings, XL chorus/diffusion scaling and
Dirt's inverse relationship to the legacy `analog` parameter. Unavailable
controls are disabled.

Appearance is stored as session properties (`display_brightness`,
`display_emphasis`, `editor_compact`), outside the audio parameter list and sound
presets. Existing parameter IDs, bank data, DSP processing and firmware are
unchanged. Opening a restored session/editor restores the appearance.

## Artwork

Final asset: `native-hall/plugin/assets/panel-compact-integrated.png`.
It was produced using the built-in imagegen tool with existing panel artwork as
references. Housing, recessed burgundy glass and its molded rim are one image.
The editor clips only the transparent canvas padding (source rectangle
0,128,2062,488), then fits the complete faceplate uniformly with an eight-unit
safety margin. It does not draw the old separate bezel in compact mode, squash
screws, or stitch together material strips. The title is rendered inside the
header independently of the photograph.

The initial material-only asset was rejected after visual review because the
separately overlaid glass looked pasted on. Its repo copy was removed; the
original generated candidates remain in Codex's generated-images directory.

Final image-edit prompt:

> Precise-object-edit of this one complete compact ivory metal audio unit asset.
> KEEP entire outside physical panel boundary, all four screws fully intact at
> same centers, original horizontal proportions, transparent padding, grain scale,
> corner shapes and colors unchanged. Change ONLY height of the integrated red
> glass window: raise its upper lip from approx y225 to y207 and lower its bottom
> lip from approx y485 to y553 in the image's current ~2062x763 canvas, increasing
> blank dark burgundy glass window to about 340px tall. Keep same x positions x80
> to1980 and same rounded integrated beveled rim, inward shadows; expand the glass
> itself, remove old lip positions completely so there is only ONE window. The
> housing top edge is y128, bottomedge616: leave around80px of ivory header ABOVE
> window and64px BELOW window. Screws stay at extreme lateral corners, completely
> separate from window; never crop or paste any part. No text/logo/LEDs/gear/controls.
> Produce full panel with transparent area outside it, do not enlarge the metal
> margins or shrink window.

## Validation

- Universal macOS arm64/x86_64 Release AU, VST3 and Standalone builds succeeded;
  the build signs and verifies the bundles.
- `native_hall_plugin_check --display-check BANK XL_BANK` passed with the current
  original 224 and XL v5 caches. It covers isolated appearance settings, session
  restoration, compact/full size and visibility, mouse drag direction, balanced
  automation events, host-to-screen updates, Dirt inversion, and every available
  numeric control on every page of all 28 programs.
- Existing `native_hall_plugin_check --banks BANK XL_BANK` passed, including the
  existing editor/preset/binding checks, 224 and XL audio regression at
  44.1/48/96 kHz, mono and block-size invariance. Its callback check reported
  `new=0, delete=0`. This ran before the final artwork/layout-only correction;
  screen interaction checks were repeated after the correction.
- Real component snapshots were visually reviewed for compact/full modes,
  Settings, reduced brightness, the compact preset browser and save dialog.
  AlertWindow snapshots require desktop access: without a monitor the JUCE
  preview process cannot construct the save dialog. The desktop-enabled preview
  succeeded. Actual DAW-host installation and user listening were not performed.

Build:

```sh
cmake --build build/plugin --parallel 6 --target NativeHall224_VST3 NativeHall224_AU NativeHall224_Standalone native_hall_plugin_check
```

Built bundles are in `build/plugin/NativeHall224_artefacts/Release/`.
Current screenshots and logs are in `build/validation/display-20261011/`.
