# Windows control texture filtering — November 3, 2026

The reported aliasing affects fader caps and quick-preset keys at 100% Windows
display scale and above. These PNGs are reduced substantially when drawn: for
example, the fader cap is drawn into a 52 by 66 logical-unit rectangle, then
scaled with the editor and display DPI.

JUCE 8.0.14 initializes Graphics with medium image resampling quality. Its
Direct2D backend maps medium to `D2D1_INTERPOLATION_MODE_LINEAR`, while high maps
to `D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC`. The Windows drawing paths now
request high quality immediately before drawing fader caps and quick keys.
This includes both skins and disabled faders. It does not change their source
images, geometry, hit areas, DPI scaling, host parameters or audio processing.
The setting is conditional on `JUCE_WINDOWS`; other platforms retain their
previous rendering path. No renderer switch or permanent repaint loop is added.

## Reproduction and visual review

The existing ROM/import checker also accepts:

```powershell
& './build/windows/cineol_rom_import_check_artefacts/Release/cineol_rom_import_check.exe' --graphics "$PWD/build/windows/graphics-check"
```

It prepares a temporary synthetic 224 cache, opens the actual editor, and saves
16 native-renderer snapshots: Original/Clean, logical widths 1104/1840, and
100/125/150/200% output pixel scales. It reports the concrete image backend in
the log. The temporary cache is deleted after editor/processor destruction;
no user ROM, imported bank, preset or preference directory is used.

These are scaled offscreen renders, not captures of a DAW on each physical
monitor. Backend availability on a CI runner must be read from the log; image
dimensions alone do not establish that Direct2D or a particular GPU was used.
Review curves and highlights on the caps at native image size, including at
fractional scales, and confirm normal fader drag and key behavior in the host.

The Windows build script runs this mode after its existing empty-cache checks.
The desktop workflow uploads the images as a separate `Windows-graphics-check`
artifact, outside the plugin package. The existing VST3/Standalone build,
source audit and packaging checks remain in place. A workflow dispatch with
`publish=false` creates test artifacts without publishing a release or tag.

## Validated build

Source commit `55fbbc16cdbbdccc97eca0250037fe41d5faf84b` passed the
[desktop workflow](https://github.com/temaniak/cineolX/actions/runs/41622729628).
Windows Server 2022 built the x64 VST3 and Standalone, passed the existing
empty-cache/import/UI and Space-clock checks, and produced all 16 images using
`juce::Direct2DPixelData`. The 100% and fractional-scale control edges were
reviewed visually. macOS universal build/checks also passed; local arm64 checks
produced 16 CoreGraphics images. These results do not replace testing the
user's DAW, display driver and monitor configuration.

The downloaded Windows ZIP passed CRC, published SHA-256 and AMD64 PE checks.
It is retained under `out/windows-graphics-fix-20261103/`; its SHA-256 is
`939749f74a6dbd0e5e6a8ecdbb0ed67408d84e6e6016150d83f05cb1a4551691`.
Source identities, logs and both platforms' images are retained in
`build/validation/windows-graphics-20261103/`. No ROM or extracted bank is
included in the images or delivery. No release or tag was published.

The selected Direct2D interpolation mode's pre-downscale behavior is described
in [Microsoft's interpolation documentation](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/ne-d2d1_1-d2d1_interpolation_mode).
