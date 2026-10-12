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
Control-envelope observations still use the original level coding; original
gain/control steps remain. Internal filter feedback reads retain their original
coefficients and scaled delays, including a native one-sample delay of
`1 / 20480` seconds, or `2.34375` host frames. All fractional reads use
fifth-order Lagrange interpolation, reducing repeated high-frequency loss in
feedback. It preserves DC gain and delay; magnitude and phase remain
approximations at higher frequencies. Short recursive reads include a small
current-sample term, solved analytically before writing the delay line.
All bank coefficients, including rounded allpass complements, remain literal.

The first prototype's linear interpolation and pole remapping caused audible
darkening. See [the diagnosis and correction](../../docs/FLOAT_HALL_DARKNESS.md).

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
cmake --build build/float-ab --target cineol_float_fractional_delay_check
build/float-ab/native-hall/float/cineol_float_fractional_delay_check
build/float-ab/native-hall/float/cineol_float_hall_check /absolute/path/to/programs-v44-import-v1.bank224
build/float-ab/native-hall/float/cineol_float_ab_plugin_check_artefacts/Release/cineol_float_ab_plugin_check
```

Use VST3/Standalone targets on non-Apple hosts. `CINEOL224_CACHE_DIR` can point
the plugin/check to a private test cache containing
`programs-v44-import-v1.bank224`; the ordinary cache is read-only for this work.
ROMs, banks and rendered audio stay outside Git.

### Windows x64

Install Visual Studio 2022 Build Tools with the C++ desktop workload and
Windows SDK, CMake 3.22 or later, Python 3, and Git for Windows. The Git for
Windows `patch.exe` utility must be on PATH during configuration. In
PowerShell, from the repository root:

```powershell
git fetch origin
git switch cineol_float
git pull --ff-only
git submodule update --init deps/reflexion

# Locate Git for Windows' patch utility, as in script/build_plugin.ps1.
$floatGit = (Get-Command git -ErrorAction Stop).Source
$floatPatchDir = Join-Path (Split-Path (Split-Path $floatGit -Parent) -Parent) 'usr/bin'
if (Test-Path (Join-Path $floatPatchDir 'patch.exe')) {
    $env:PATH = "$floatPatchDir;$env:PATH"
}
cmake -S . -B build/float-ab -G "Visual Studio 17 2022" -A x64 -DCINEOL_BUILD_FLOAT_AB=ON -DNATIVE_HALL_BUILD_TOOLS=OFF
cmake --build build/float-ab --config Release --parallel 6 --target CineolFloatHallAB_VST3 CineolFloatHallAB_Standalone
```

The VST3 bundle is
`build/float-ab/native-hall/float/CineolFloatHallAB_artefacts/Release/VST3/Cineol Float Hall AB.vst3`.
Copy the entire bundle to your VST3 directory and rescan the host. The ordinary
`script/build_plugin.ps1` builds the main Cineol-X plugin; it does not select
the experimental A/B targets. This float revision has been validated on macOS;
the Windows commands describe the intended build and are not a completed
MSVC/runtime validation. Private ROMs or an existing compatible bank are needed
for audio, but are not needed to compile the plugin.

## Current limits

The offline 20.48 kHz specialization
checks the literal-bank collapsed graph against an independent, real-valued
three-edge ARU with the original shared delay RAM.
It proves algebra and memory connectivity without asserting identical integer
rounding, converter behavior or sound at 48 kHz.

The 48 kHz version intentionally changes bandwidth, arithmetic and quiet-tail
behavior. Fractional delay magnitude and phase differ from an ideal delay at high
frequencies. The wider bandwidth and removed circuit filters can make B
brighter above the original passband. This is not a claim of identical
original arithmetic or sound.
Modulation and Decay Optimization retain nominal control rates rather than
the original desktop engine's full scan/write timing. Live predelay changes
are immediate retaps; no click-free predelay transition is claimed.

Stability, reset, finite guards, allocation checks, native-rate algebra and
plugin routing/state/host-rate checks do not establish listening acceptance.
Desktop CPU results do not establish Daisy hardware margin.

See [the historical first validation report](../../docs/FLOAT_HALL_VALIDATION.md)
and [the current darkness correction report](../../docs/FLOAT_HALL_DARKNESS.md).
