# Float Hall and A/B plugin

Experimental Large Concert Hall B (224 v4.4 program 3), on branch
`cineol_float`. The existing original/XL engines and main plugin sources are
unchanged. This is a first algorithm prototype, not a release or a completed
224/XL catalog port.

`Hall48` runs its signal graph directly at 48 kHz with float delay storage,
sums, feedback and fractional delay reads. It has no input/output circuit
filters, ADC/DAC quantization, low-rate audio processing or original-rate
audio resamplers. Diffusion and frequency-dependent decay inside the graph
remain. The original delay-memory dependencies are resolved into individual
delay lines during preparation. All storage is reserved outside processing.

The coefficient tables and modulation/decay control laws reuse the existing
locally imported bank. Audio never runs through the integer signal core.
Control-envelope observations still use the original level coding; control
tables retain the original coefficient steps. Positive one-sample recursive
poles are converted to preserve their time constants and DC gains at 48 kHz.
Other delays retain their durations with linear fractional reads.

## A/B listening

Build the separate **Cineol Float Hall AB** AU, VST3 or Standalone. It has
distinct plugin/bundle identifiers and can coexist with Cineol-X 224.
It reads the same original-224 bank cache, without importing or changing it.
Import 224 v4.4 ROM1-ROM5 in the main plugin first, then open this plugin.
The experimental plugin does not require XL ROMs to process audio.

- **A / Original 224** uses the current `DesktopEngine48` with its ADC/DAC,
  circuit filters and integer Hall. Dirt is fixed at zero.
- **B / Float 48 kHz** uses the separate float Hall with direct boundaries.
- Both receive identical input and common controls, run continuously, and
  retain their tails. A/B switches use a 10 ms linear crossfade.
- Common Input Gain and Dry/Wet operate outside the two signal cores.
  **B Level Trim** adjusts only B's wet output for manual loudness matching.
  The default is 100% wet, A/C output routing, and both control modes off.
- A 70-frame pad on B and the dry path preserves the main engine's advertised
  48 kHz latency. It is transport alignment, not sample-identical phase
  alignment of the original analog circuitry.
- At a host rate other than 48 kHz, the existing host-rate bridge resamples
  both paths together. The float algorithm itself always runs at 48 kHz.

Start at 48 kHz and 100% wet. Match loudness with B Level Trim, switch while
playing and during input silence, and compare attack, stereo motion, brightness,
frequency-dependent decay and the final tail. Then try other controls and modes.

## Build and checks

From the repository root:

```sh
cmake -S . -B build/float-ab -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_FLOAT_AB=ON
cmake --build build/float-ab --target CineolFloatHallAB_AU CineolFloatHallAB_VST3 CineolFloatHallAB_Standalone cineol_float_hall_check cineol_float_ab_plugin_check -j 6
build/float-ab/native-hall/float/cineol_float_hall_check /absolute/path/to/programs-v44-import-v1.bank224
build/float-ab/native-hall/float/cineol_float_ab_plugin_check_artefacts/Release/cineol_float_ab_plugin_check
```

Use VST3/Standalone targets on non-Apple hosts. `CINEOL224_CACHE_DIR` can point
the plugin/check to a private test cache containing
`programs-v44-import-v1.bank224`; the ordinary cache is read-only for this work.
ROMs, banks and rendered audio stay outside Git.

## Current limits

The offline 20.48 kHz specialization checks the collapsed graph against an
independent, real-valued three-edge ARU with the original shared delay RAM.
It proves algebra and memory connectivity without asserting identical integer
rounding, converter behavior or sound at 48 kHz.

The 48 kHz version intentionally changes bandwidth, arithmetic and quiet-tail
behavior. Linear interpolation introduces its own high-frequency loss in
delay feedback; it is an initial interpolation policy, not a final sonic choice.
Modulation and Decay Optimization retain nominal control rates rather than
the original desktop engine's full scan/write timing. Live predelay changes
are immediate retaps; no click-free predelay transition is claimed.

Stability, reset, finite guards, allocation checks, native-rate algebra and
plugin routing/state/host-rate checks do not establish listening acceptance.
Desktop CPU results do not establish Daisy hardware margin.

See [the first validation report](../../docs/FLOAT_HALL_VALIDATION.md).
