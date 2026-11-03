# Clean skin assets — November 2, 2026

The user requested the existing panel and controls with clean surface textures,
preserving their shape, arrangement, colors and material. The six raster edits
were made with the built-in ImageGen tool, each using its original project
asset as the image reference. This is a material restoration, not a new layout.
Text, scales, indicators and interactive controls continue to be drawn by JUCE.

## Assets and source references

All project paths below are relative to `native-hall/plugin/assets/`.

| Original reference | Clean output | Dimensions | Transparency |
| --- | --- | --- | --- |
| `panel-quick-presets.png` | `panel-clean.png` | 1443 × 1090 | Opaque |
| `panel.png` | `bezel-clean.png` | 1470 × 1070 | Opaque |
| `panel-compact-integrated.png` | `panel-compact-clean.png` | 2062 × 764 | Retained |
| `fadercap-black.png` | `fadercap-clean.png` | 1113 × 1413 | Retained |
| `quick-key-black.png` | `quick-key-clean.png` | 1254 × 1254 | Retained |
| `menu-metal.png` | `menu-clean.png` | 1254 × 1254 | Opaque |

The compact generation was 2061 × 763; macOS `sips` normalized it by one pixel
to the original dimensions. All semantic surface edits were done by ImageGen.
No Python painting, synthetic replacement controls, or color redesign was used.

## Edit briefs for reproduction

Each reference received a precise-object-edit request: keep the identical
object, geometry, framing, perspective, silhouette, colors, material, lighting,
bevels and existing details. Remove only accumulated wear: scratches, scuffs,
stains, dust, grime and chipped or rubbed coating. Retain realistic fine material
grain and shading. Add no labels, controls, decoration or other objects.

- Full panel: restore the existing ivory faceplate, preserving its four black
  slotted corner screws and the exact blank panel layout.
- Display bezel: restore the same ivory housing and dark recessed display,
  including the original divider, border and screw locations.
- Compact housing: restore the same wide ivory housing, dark burgundy glass,
  screws, transparent exterior padding and crop.
- Fader cap: restore the same black rounded cap with its ivory horizontal
  position marker, retaining highlights, bevels and transparent background.
- Quick key: restore the same black rounded key cap, its edge shading,
  proportions and transparent background.
- Menu surface: restore the same ivory material tile, retaining fine grain,
  neutral lighting and original base color while removing marks and wear.

## Private generation provenance

Generation directory:
`~/.codex/generated_images/01a19cd3-c8cd-7413-9a21-867f6470d1d1/`.

| Output | Original ImageGen file |
| --- | --- |
| Full panel | `exec-7ea30c8a-5942-4aec-ab98-ca39e10ae148.png` |
| Bezel | `exec-9c8265ba-4cfb-4808-a8a8-8564621cf4f3.png` |
| Compact | `exec-3ce653ba-0bd6-4e67-9ed9-8a65fd34bc27.png` |
| Fader | `exec-1a775d8f-cb3c-409f-992a-b0b718232acd.png` |
| Quick key | `exec-b2911e6d-5d94-47b1-a4b6-58369ba8463d.png` |
| Menu | `exec-3a3f16aa-3b25-49ad-9800-fa1d59477406.png` |

The original worn assets remain intact. Clean and Original use the same JUCE
control geometry. Disabled faders derive their dimmed appearance from the
selected cap; the mode does not change control hit areas or placement.
