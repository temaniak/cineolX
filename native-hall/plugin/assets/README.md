# Cineol-X 224 artwork

`panel-monolithic.png` is the continuous metal faceplate texture, generated
with the built-in ImageGen tool using `panel.png` as its visual reference.
The generation brief is saved in [panel-monolithic-prompt.txt](panel-monolithic-prompt.txt).
`panel.png` supplies the red display bezel; `fadercap.png`
is the transparent ivory fader cap. These were also generated with ImageGen.
`menu-metal.png` is an independent uniformly worn metal texture for menus,
settings, ROM setup, preset dialogs and the browser, generated with the built-in
ImageGen tool. Its [generation brief](menu-metal-prompt.txt) excludes directional
bands, fader-related grime and repeated markings; a
[refinement prompt](menu-metal-refine-prompt.txt) reduces wear contrast for small text.
The UI samples this material
at a fixed grain scale rather than stretching it to each window's aspect ratio.
The 1254-pixel square covers normal menus and dialogs with a single crop;
larger surfaces can wrap the material.
JUCE draws labels, faders, LED text and controls. The assets are embedded
in the plugin and decoded on the UI thread. No external file or network lookup
is needed. The editor trims transparent fader padding in memory. Inactive caps
use a darker, desaturated copy with the same alpha and geometry, so the rail
does not show through the cap.

The current design uses 21 evenly spaced ticks on either side of each
fader, with the upper, middle and lower ticks thicker. Numeric scale legends
are omitted; the actual parameter values appear only on the display. All faders
share one layout without text boxes beneath them, leaving room for longer rails
and an intact parked cap. See the [interface image](../../../docs/images/cineol-x-224.png).

The red display is extended vertically and horizontally using the existing artwork.
Preset names use the left header area without a large numeric program indicator.
The algorithm row shares that area with numbered page selectors: the selected
number is bright and the other numbers are dim, while remaining clickable.
Six dynamic label/value cells align with the page-bound faders. Three permanent
cells show Dirt, Input and Mix labels and values above their faders on every page,
using the same text size as the dynamic cells. No labels are repeated on the
metal panel. Algorithm selection, outputs, modes and page navigation all live
on the display; long preset names scroll periodically without shrinking.
The wider, shorter faceplate has no footer controls and retains the Cineol-X 224 name.
The lower plate is half the original artwork height, with four corner screws.
The faceplate is rendered from one image in one pass, without texture bands,
screw patches or separately stretched regions. Its texture stays continuous
across the drawn mechanical seam above the lower plate. The display bezel is
clipped to its rounded outline to avoid overlaying unrelated metal at its corners.
The upper and lower display share one vertical divider. Right-side controls use
three columns aligned with Dirt, Input and Mix; header captions and values use
the same spacing and LED sizes as the parameter cells. The top row contains
Left, Right and Model; the lower row contains Page, Mod Enh and Decay Opt.
The gear has only colour feedback, without a frame or focus backdrop.
Internal menus, settings and preset dialogs use the same
metal style using the separate menu material, dark frames and red header/input surfaces. The faceplate scales
uniformly with the editor; the display bezel preserves its corner geometry.
Popup windows use transparent corners around the rounded metal outline, with
square corners as a fallback on platforms without transparent windows. The
first red section header has equal six-pixel top and side insets.
The preset selector opens a metal-framed browser overlay rather than nested
algorithm menus. Its red header has equal top and side margins. Preset names
and algorithm/model annotations occupy separate columns; a scrollable checkbox
list supports the union of several algorithm filters. The search input, buttons
and scrollbar follow the editor's colour palette.
No image regeneration is required for pages.
