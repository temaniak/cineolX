# Cineol Dual Hall B

Independent AU, VST3 and Standalone experiment: two complete original Lexicon
224 v4.4 **Large Concert Hall B** engines, one per output channel, with a shared
set of controls and reciprocal wet crossfeedback.

The existing main plugin, original/XL processing sources, delay networks,
coefficients, converters and modulation/controller implementations are not
modified. The root build only adds an optional subdirectory, disabled by default.
This prototype uses the original integer engines, including their quantization
and circuit filters. It does not select the separate Float or Clean engines.

## Routing

The left tank receives the left input on both of its ordinary input ports
(the existing mono-input convention); its original output A is the left wet
signal. The right tank likewise receives the right input on both input ports;
its original output C is the right wet signal. Each tank retains the complete
internal two-input/four-output algorithm. No tank is reduced to one internal
branch, and its unused outputs are not summed into mono.

At the shared 48 kHz wrapper rate, with source gain `s` and coupling `k`:

```text
uL[n] = s * inputL[n] + k[n] * wetR[n-1]
uR[n] = s * inputR[n] + k[n] * wetL[n-1]
wetL[n] = original Hall B #1(uL[n], uL[n]).A
wetR[n] = original Hall B #2(uR[n], uR[n]).C
```

Both previous returns are captured before either tank advances. The explicit
link delay is one 48 kHz frame, independent of the host block size; the full
original converter/filter/transport and algorithm delays remain in each engine.
No extra diffusion, long link delay, filtering or phase inversion is inserted.

Crossfeedback ranges from **0 to 50%** per direction, initially **15%**. A 50%
return is -6.02 dB, not a promise of stability at every Hall setting. Changes
slew by at most `0.5 / 960` per internal frame: a full-range change takes 20 ms.
At 0%, the tanks have independent histories. Mono input is duplicated before
this routing, matching the plugin's mono-to-stereo bus convention.

All Hall controls are shared: Bass/Mid Decay, Crossover, Treble Decay, Depth,
Pre-delay, Diffusion, Mode Enhancement, Decay Optimization and their modulation
rate/depth/optimization-response settings. Hidden-setting knobs directly select
the existing effective control values; the UI is not a physical panel emulator.

Input Gain applies only to new source excitation. The return is taken before
the final common Dry/Wet and Output Gain, so those controls do not alter the
loop. The dry signal has the existing 70-frame compensation at 48 kHz. The
existing pinned RateBridge handles other host rates around the entire coupled
system. No feedback crosses the host-rate bridge.

**Clear tails** clears both preallocated engine histories, the reciprocal return
and the dry pad on the next audio callback, retaining parameters. At other host
rates a short already-buffered bridge output may remain. Changing coupling or
ordinary controls preserves histories; changing pre-delay retains the original
retap behavior and is not claimed to be click-free.

## Long decay and listening

External positive coupling can increase the total decay time and excite
resonances. Maximum LF/Mid decay with strong coupling **does self-oscillate and
saturate in the measured fixtures**, despite each isolated engine decaying.
The UI states this and provides Clear tails. Use Output Gain for listening
level; reducing it does not stop the internal loop. Reducing Crossfeedback or
using Clear tails addresses the return/history directly.

The return is intentionally direct: there is no automatic gain controller or
limiter that changes its behavior. The original fixed-point residual floor can
also rise when its output is recirculated. Listening acceptance has not been
claimed. Start with ordinary decay settings, compare 0% / 15% / 30% at matched
output levels, try a left-only source, and compare stereo motion and stopped-input
tails before moving toward extreme decay/coupling combinations.

## Build and use

On macOS, from the repository root:

```sh
./script/build_dual_hall.sh
```

The script uses an existing pinned JUCE source when available. It builds only
the prototype and its checks, in `build/dual-hall`; it does not build or install
the main plugin. Artifacts are in:

```text
build/dual-hall/native-hall/dual-hall/CineolDualHallB_artefacts/Release/
  AU/Cineol Dual Hall B.component
  VST3/Cineol Dual Hall B.vst3
  Standalone/Cineol Dual Hall B.app
```

The prototype reads the existing `programs-v44-import-v1.bank224` cache, without
writing it. Import original 224 v4.4 ROM1-ROM5 in the main Cineol-X 224 first,
then open this plugin. `CINEOL224_CACHE_DIR` can select an existing private test
cache. Missing or invalid banks give silence and a visible setup message. No
ROM, bank or audio capture is included.

The bundle ID is `net.nkstuff.cineoldualhallb`, AU identity `aufx/CdHB/Rflx`,
and state root `CineolDualHallB`. It can coexist with Cineol-X 224 and the other
test plugins. This is a local prototype, with no new public release or tag.

## Validation

```sh
build/dual-hall/native-hall/dual-hall/cineol_dual_hall_check ORIGINAL_BANK tails.csv
CINEOL224_CACHE_DIR=/absolute/path/to/existing/cache \
  build/dual-hall/native-hall/dual-hall/cineol_dual_hall_plugin_check_artefacts/Release/cineol_dual_hall_plugin_check \
  /absolute/path/to/editor.png
```

Add `--cpu` after the screenshot path for an assembled callback benchmark;
run it after other builds and renders have stopped. A core-only build is also
available with `CINEOL_BUILD_PLUGIN=OFF`, `NATIVE_HALL_BUILD_TOOLS=OFF` and
`CINEOL_BUILD_DUAL_HALL=ON`.

The checks compare the wrapper against two independent unmodified engine
instances, test shared live controls, reciprocal timing, coupling slew, reset,
preserved tails, complete parameter recall, mono input, compensated dry audio,
nonfinite input guards, and 44.1/48/96 kHz block-partition invariance. C++ heap
allocations and releases are counted during setters/processing and the explicit
Clear tails action. The tail screen records per-channel-mean-removed pooled AC
power after stopping the input, including the self-exciting extreme cases.
These checks do not certify all parameter trajectories or listening quality.

Private validation evidence is under `build/validation/dual-hall-20261103/`.
The [validation report](../../docs/DUAL_HALL_PROTOTYPE_2026-11-03.md) records the
completed checks and practical limits. Windows and Daisy execution are not
validated by the macOS build.
