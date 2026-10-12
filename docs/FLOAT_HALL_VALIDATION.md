# First float Hall / A-B validation

October 12, 2026. Branch: `cineol_float`.

## Scope and behavior

The user requested a separate float Hall at 48 kHz, direct input/output without
the original converter/circuit filters, and a plugin comparing it against the
current Hall. The prototype selects Large Concert Hall B, original 224 v4.4
program 3. The existing DSP, main plugin sources, dependency checkouts, plugin
identifiers and parameter IDs are unchanged by this work. Pre-existing local
UI/Daisy changes were carried into the requested branch and left intact.

New implementation: `native-hall/float/`. Root CMake adds the separate directory;
`CINEOL_BUILD_FLOAT_AB=ON` enables the experimental plugin. AU/VST3 identifiers
are separate (`net.joelanders.cineolfloatab`, `CfAB`), so this work can coexist
with Cineol-X 224. No release/tag/publication was made.

The signal graph uses float sums and individual fractional delay lines.
Connections are collapsed from the original fixed Hall B graph. The most
recent original-RAM writer for each read is resolved at control preparation,
including same-pass write ordering. Preparation reserves all possible predelay
positions and modulation low-address bytes; parameter changes do not resize
storage. Original delay durations are scaled by `48000 / 20480`.

Internal diffusion and damping remain. Positive one-sample recursive poles
are mapped with `p48 = p224^(20480/48000)` and feed gains adjusted to preserve
DC gain. Audio omits ARU rounding/saturation, ADC/DAC quantization, analog
boundaries and original-rate audio conversion. Existing bank coefficient
tables and modulation/decay control laws are retained as control data.

The A/B processor continuously runs the unmodified `DesktopEngine48` with
Analog on / Dirt zero beside `Hall48`. Input Gain and Dry/Wet are shared;
B Level Trim is a manual wet gain for B only. Both engines retain history when
switching, with a 480-frame linear crossfade. B and dry have a 70-frame
transport delay to match advertised A latency. Defaults: A, 100% wet, A/C
outputs, zero gain/trim, Mode Enhancement and Decay Optimization off.

## Sources and reproduction

Source baseline and private bank identity are recorded in
`build/validation/float-hall-20261012/provenance.json`.
Reflexion pin: `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.
JUCE: 8.0.14, reused from the existing local source checkout.
Build: macOS arm64, Release, floating-point contraction disabled.
The original v4.4 bank was reused; no ROMs or banks were reimported or changed.

Exact local commands:

```sh
cmake -S . -B build/float-ab -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_FLOAT_AB=ON -DFETCHCONTENT_SOURCE_DIR_JUCE=/Users/evgeniystrukov/reflexion-224/out/reflexion-private-backup-20261003/juce-plugin/build/_deps/juce-src -DNATIVE_HALL_ROM_DIR= -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/float-ab --target CineolFloatHallAB_AU CineolFloatHallAB_VST3 CineolFloatHallAB_Standalone cineol_float_ab_plugin_check cineol_float_hall_check -j 6
build/float-ab/native-hall/float/cineol_float_hall_check build/rom/native-hall/programs-v44.bank224
build/float-ab/native-hall/float/cineol_float_ab_plugin_check_artefacts/Release/cineol_float_ab_plugin_check
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -ffp-contract=off -Ibuild/deps/reflexion native-hall/float/check.cpp native-hall/core/hall.cpp -o build/validation/float-hall-20261012/check-sanitized
build/validation/float-hall-20261012/check-sanitized build/rom/native-hall/programs-v44.bank224
auval -v aufx CfAB Rflx
```

AU/VST3 were installed in the user's `Library/Audio/Plug-Ins` directories
under the new name. Standalone remains in
`build/float-ab/native-hall/float/CineolFloatHallAB_artefacts/Release/Standalone/`.
Signatures of the installed AU/VST3 bundles were verified.

## Results

- **Independent graph check:** 240,000 native-rate frames across three
  predelay/diffusion configurations. The oracle uses a separate shared 16K RAM
  and all three real-valued ARU edges, rather than the production collapsed
  graph. Maximum absolute error over all four outputs: `2.6077e-08`.
- **48 kHz impulse:** exact initialized silence, peak `0.0565061`; 8–10 s
  energy / first-2-s energy `1.26262e-09`. Finite outputs, excitation and tail
  decay confirmed. Allocations during processing: zero.
- **Live controls:** ten seconds of moving control settings, predelay,
  modulation and optimization followed by input-silent tails passed bounded
  amplitude and finite checks, without allocation. Reset and nonfinite input
  guards passed.
- **Long tails:** 90 s for each Diffusion endpoint, maximum Bass/Mid/Treble/
  Crossover and Depth, Mode Enhancement on and optimization off. Peaks
  `0.0572387` / `0.0420741`; energy at 87–90 s / 1–3 s `0.0182602` /
  `2.04747e-05`. No runaway or nonfinite output detected.
- **Plugin A:** output is bit-identical to the current `DesktopEngine48` for
  the same 51,200-frame stereo fixture at 48 kHz.
- **Plugin B:** output agrees with the separate float core plus its transport
  delay. Switching in silent-input tails returns to the continuously advanced
  independent A/B histories after the crossfade, without reset.
- **Plugin integration:** choice/trim/mix state restoration, exact delayed dry,
  mono versus dual-mono input, 44.1/48/96 kHz hosts, blocks 1/64/257/1024,
  finite output and zero processing allocations passed. Editor was rendered
  and visually inspected, including A/B buttons, output names and units.
- **Sanitizers:** AddressSanitizer and UndefinedBehaviorSanitizer passed the
  offline core check.
- **AU:** installed `aufx/CfAB/Rflx` passed `auval`. VST3 built, produced its
  factory manifest, was installed and signature-verified; a separate VST3
  DAW-host listening session has not been performed. Standalone built and
  signature verification succeeded during the build.
- **Short desktop CPU diagnostic:** 96,000 frames took approximately 0.0288 s
  for A and 0.0098 s for the float core on this host. Float core storage,
  including its control helper and reserved delay samples: 188,952 bytes
  (184.5 KiB), excluding shared bank and allocator overhead. This is a short
  single-engine diagnostic, not an A/B-plugin or Daisy margin guarantee.

Logs and the editor image are under `build/validation/float-hall-20261012/`.
Build warnings concern the existing JUCE/macOS Metal API and the console
check's inherited standalone macro definitions; no project compile error
remains.

## Sound limits and next comparison

This is a topology/algebra and integration checkpoint. No listening acceptance
is claimed. Removing converter/circuit limits intentionally changes bandwidth
and quiet tails. Fractional delay reads currently use linear interpolation,
which introduces high-frequency loss of its own in feedback. Original stepped
control tables remain; modulation/optimization use nominal rates rather than
the current original engine's complete controller scan/write schedule. Live
predelay is an immediate retap. The transport pad does not match analog phase
or every original converter sampling offset.

Next: listen to this single Hall at 48 kHz, 100% wet, modes initially off,
matching B loudness by ear with the trim. Compare attack, brightness, stereo,
decay by frequency, silent-input tails and retriggering. Then compare mode-on
behavior. Investigate measured or heard differences in short blocks before
expanding to other algorithms. No XL float port or Daisy deployment is included.

## Generated material

Only the narrow core/plugin checks and one editor snapshot were created; no
full WAV campaign or new ROM/bank cache was accumulated. Current build products,
logs and provenance remain active. Disposable `/tmp/cineol-float-map.*` and
`/tmp/cineol-float-extreme*` exploration files were removed; exact paths are
recorded in `provenance.json`. Their useful graph/long-tail checks are retained
in the source harness.
No user assets, recordings, banks, ROMs or unrelated caches were removed.
