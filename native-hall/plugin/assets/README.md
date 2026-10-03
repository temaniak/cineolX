# Cineol-X 224 artwork

`panel.png` is the metal faceplate texture; `fadercap.png` is the transparent
ivory fader cap. Both were generated with ImageGen for the approved interface.
JUCE draws labels, faders, display segments and controls. The assets are embedded
in the plugin and decoded on the UI thread. No external file or network lookup
is needed. The editor trims transparent fader padding in memory and preserves
the source pixels.

The current 0.5.1 design uses 21 evenly spaced ticks on either side of each
fader, with the upper, middle and lower ticks thicker. Numeric scale legends
are omitted; the actual parameter values appear below the faders and on the
display. See the [interface image](../../../docs/images/cineol-x-224.png).
