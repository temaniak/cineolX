# Cineol-X 224

Cineol-X 224 is a native reverb plugin and a portable Daisy DSP project based on
the six programs of the **original Lexicon 224, firmware v4.4**. The desktop
plugin is version **0.5.1**, with the current metal-panel artwork and 21-mark
fader scales.

Primary repository: [temaniak/cineolX](https://github.com/temaniak/cineolX).

ChatGPT was actively used during the creation of this project, assisting with
code development, debugging, testing and documentation.

![Cineol-X 224 interface](docs/images/cineol-x-224.png)

This repository contains the Cineol DSP, plugin, ROM import tools and a generic
Daisy adapter. [Reflexion](https://github.com/joelanders/reflexion-224) is an
external, pinned Git submodule. Its emulator prepares and validates control
data; the audio engine runs native C++ networks. No ROM files or prepared banks
are shipped in this repository or embedded in the desktop plugin.

## Programs and controls

| Program | Name |
| --- | --- |
| 01 | Small Concert Hall B |
| 02 | Vocal Plate |
| 03 | Large Concert Hall B |
| 04 | Acoustic Chamber |
| 05 | Percussion Plate A |
| 06 | Small Concert Hall A |

The main parameters are Bass Decay, Mid Decay, Crossover, Treble Decay, Depth,
Pre-delay, Diffusion, Input Gain, Dry/Wet and Program. The plugin also exposes
Mod Enhancement, Decay Optimization, the analog-filter mode and independent
left/right choices from outputs A–D. The default output pair is A/C.

Bass/Mid labels use the original discrete time scale; they are not a promise of
measured T60. Diffusion has no effect in Acoustic Chamber. Small and Large Hall
B share a topology; their factory settings differ. Changing programs clears
the previous reverb tail immediately. Pre-delay changes can produce a transient.

The plugin's **Digital Dirt** switch disables the analog-filter path when
enabled. Its existing automation parameter remains `analog` / **Analog Filters**
with the original polarity, so saved sessions keep their meaning. Plugin IDs
are retained: AU `aufx/Nh24/Rflx`, bundle ID `net.joelanders.nativehall224`.

The gear in the top-right corner opens **Settings**, with room for future fine
tuning. **Low latency** sends the dry input directly at the project sample rate
and reports **0 samples** of plugin latency at every supported rate. Only the
wet signal passes through the resamplers and reverb; its filter delay and
pre-delay remain unchanged. This changes the timing between dry and wet (by
70 samples, about 1.46 ms, at 48 kHz). At 100% wet the DSP output is unchanged,
but the DAW no longer compensates its delay. Audio-interface/buffer latency is
independent of this setting.

Low latency is saved per instance and defaults to **off**, including when
loading older sessions. Existing parameter IDs and indices are unchanged.
The setting is not automatable. Host latency updates happen outside the audio
callback, even with the editor closed. Switching may produce a brief transient
or require a transport restart in hosts that defer compensation changes.
Click the gear again or press Escape to close Settings.

## ROM requirements and first use

Only the five **original 224 v4.4 ROM1–ROM5** chips are accepted. Each file must
contain 2,048 bytes and match the expected SHA-256 digest. Names do not matter.
224X, 224XL, other 224 versions, modified files and incomplete sets are rejected.
The `X` in the product name does not mean support for the 224X hardware.

The desktop plugin can be compiled without ROMs. On first use, open its editor,
choose **Choose ROMs...**, and select a folder, a ZIP, or the five files. Import
runs on a background thread, shows progress and can be cancelled. Audio passes
through dry until preparation finishes. The ROM control firmware is executed
locally to extract coefficients, delay addresses, control tables and modulation
data for all six programs. This does not compile new plugin code or extract an
arbitrary new algorithm: the four supported native topologies are already built.

On macOS the resulting cache is stored at:

```text
~/Library/Application Support/Cineol-X 224/programs-v44-import-v1.bank224
```

Later launches use that cache without asking for the original files. A corrupt
or incompatible cache returns to the import screen. The cache is not included
in DAW state. To repeat setup, close all instances and remove this cache file.
`CINEOL224_CACHE_DIR` selects a different cache directory for isolated tests.
Runtime ROM import does not upload files or require a network connection.

Daisy requires the ROMs **at every firmware build**, including incremental
builds and the build-before-flash command. Cached banks and old images cannot
bypass the check. ROM data is prepared on the computer and embedded in the
personal firmware image. After flashing, Daisy runs independently; it does not
need the original files, a computer, an 8080 emulator or runtime ROM loading.

## Dependencies

Clone the primary repository and initialize the desktop dependency:

```sh
git clone https://github.com/temaniak/cineolX.git
cd cineolX
./script/setup_dependencies.sh
```

For Daisy, initialize libDaisy and its nested dependencies as well:

```sh
./script/setup_dependencies.sh --daisy
```

Reflexion and libDaisy are pinned in the submodules and `dependencies.json`.
Build-only exports apply the small compatibility patches from `patches/` while
leaving the dependency checkouts unchanged. JUCE **8.0.14** is fetched by CMake
unless `JUCE_DIR` points to an existing checkout. Initial dependency downloads
require network access; builds with initialized local dependencies can run offline.
DaisySP is not required because the DSP is implemented by this project.

For an upstream update, fetch and check out the desired dependency commit,
update its entry in `dependencies.json`, validate the compatibility patch and
run the relevant checks. Commit the manifest and submodule pointer together.
Upstream changes do not enter the project automatically.

## Build the desktop plugin

The verified desktop platform is macOS with Xcode/Command Line Tools, CMake
3.22 or later, Python 3 and Git. The default architecture is Apple silicon.

```sh
./script/build_plugin.sh --build-only
```

This produces AUv2, VST3 and Standalone bundles in:

```text
build/plugin/NativeHall224_artefacts/Release/AU/Cineol-X 224.component
build/plugin/NativeHall224_artefacts/Release/VST3/Cineol-X 224.vst3
build/plugin/NativeHall224_artefacts/Release/Standalone/Cineol-X 224.app
```

Local macOS builds are ad-hoc signed. The script does not install or launch
them. Copy the AU to `~/Library/Audio/Plug-Ins/Components/` or the VST3 to
`~/Library/Audio/Plug-Ins/VST3/`, then restart or rescan your DAW. Configure
audio input/output in the standalone's audio settings when needed.

`BUILD_JOBS` controls build parallelism; `JUCE_DIR` reuses a local JUCE checkout.
For Intel or a universal macOS build, configure `build/plugin` manually with
`-DCMAKE_OSX_ARCHITECTURES=x86_64` or `"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"`
before running the script. Non-macOS builds select VST3 and Standalone, but
Linux has not been validated here.

For Windows x64, install Visual Studio 2022 Build Tools with the C++ desktop
workload and Windows SDK, CMake 3.22 or later, Python 3, and Git for Windows
(including its `patch` utility). Initialize Reflexion, then build in PowerShell:

```powershell
git submodule update --init deps/reflexion
./script/build_plugin.ps1
```

The Release VST3 bundle is
`build/windows/NativeHall224_artefacts/Release/VST3/Cineol-X 224.vst3`.
Copy the entire bundle to `C:\Program Files\Common Files\VST3\` and rescan your
DAW. The script also runs the first-use check with an isolated empty cache;
it does not require or distribute ROMs. ROM import and full DSP validation
require your own supported ROM set. `-Jobs 6` controls build parallelism, and
`JUCE_DIR` can reuse a local JUCE 8.0.14 checkout.

### Automatic desktop builds and releases

The [Desktop builds workflow](https://github.com/temaniak/cineolX/actions/workflows/desktop-build.yml)
builds Windows x64 VST3/Standalone and macOS universal AU/VST3/Standalone on
GitHub-hosted runners. No local Windows machine or private ROM upload is needed.
Run it from the Actions page or from any authenticated checkout:

```sh
gh workflow run desktop-build.yml --ref main
gh run list --workflow desktop-build.yml
gh run download RUN_ID --dir build/downloaded
```

Manual runs and pull requests upload downloadable archives without publishing
by default. On `main`, selecting **publish** (or passing `-f publish=true` to
`gh workflow run`) creates the version tag and release after both builds pass.
To publish a version, update the version in `CMakeLists.txt`, add English release
notes at `docs/releases/vVERSION.md`, commit and push, then push the matching tag:

```sh
git tag vVERSION
git push origin vVERSION
```

Tag builds publish both ZIPs and `SHA256SUMS.txt` only after both platforms pass.
Actions uses its built-in repository token; no additional release secret is
required. The packaging script selects only the plugin bundles and public
documentation, verifies the binary architectures, and checks macOS signatures
before and after ZIP extraction. CI runs first-use and Settings checks without
ROMs; full imported-bank DSP tests remain local with private fixtures.

## Universal Daisy controls

The public interface has **ten assignable controllers, two buttons and one RGB
LED for the selected algorithm**. It has no button LED outputs, proprietary
panel names, calibration measurements, pin assignments or custom carrier-board
dependency.

`native-hall/daisy/src/ControlConfig.example.hpp` defines the default assignments:

| Controller | Default target |
| --- | --- |
| 1 | Bass Decay |
| 2 | Mid Decay |
| 3 | Crossover |
| 4 | Treble Decay |
| 5 | Depth |
| 6 | Pre-delay |
| 7 | Diffusion |
| 8 | Input Gain |
| 9 | Dry/Wet |
| 10 | Program |

Each entry has a target, an input minimum/maximum, inversion and an optional
curve function. Reorder targets to change assignments; each target is assigned
once. Ranges describe the numeric readings returned by your hardware adapter:

```cpp
{Target::Bass,      0.0f, 3.3f, false}, // adapter returns 0..3.3 V
{Target::Mid,      -5.0f, 5.0f, true }, // adapter returns -5..+5 V, reversed
{Target::Crossover, 0.0f, 5.0f, false}, // adapter returns 0..5 V
{Target::Mix,       0.0f, 1.0f, false}, // adapter already returns normalized ADC
```

The mapper normalizes and clamps each range to 0..1 before applying its musical
mapping. Bass/Mid/frequency parameters use discrete ROM indices; Depth,
Pre-delay and Diffusion use their supported integer ranges. Input Gain spans
−36 to +12 dB with 0 dB at the normalized midpoint; Dry/Wet spans 0–100%.
The six Program sectors and integer parameters have hysteresis.
These numeric ranges do not change a board's electrical ADC limits or replace
the external conditioning needed for a particular voltage interface.

Copy the examples to the ignored files `ControlConfig.local.hpp` and
`HardwareAdapter.local.hpp` to keep board details private. The example hardware
adapter already implements ADC smoothing, two GPIO buttons and GPIO RGB output.
Assign the ten analog pins, two button pins and three RGB pins in the private
copy. Each analog input has a scale/offset conversion to your declared units:
for example, scale 3.3/offset 0, scale 5/offset 0, or scale 10/offset −5. Replace
its `Init`, `Read` and `WriteRgb` methods for another I/O source or bounded PWM.
Button polarity, pull-ups, GPIO/PWM configuration,
RGB polarity and analog calibration belong in that adapter.

The default button behavior, after 16 ms debounce at 500 Hz, is:

- Button 1 release: toggle Mod Enhancement.
- Button 2 release: toggle Decay Optimization.
- Press both, then release both: toggle analog filtering; suppress single actions.

The RGB palette is configurable. Defaults for programs 01–06 are cyan, magenta,
blue, green, red and yellow. RGB indicates the algorithm only. The included
adapter has all pins disabled, returns a fixed Large Hall B preset and performs
no physical input or LED I/O. Assign pins and conversions to use real controllers
and RGB hardware.
See [the Daisy adapter guide](native-hall/daisy/README.md).

## Build Daisy firmware

Requirements: Arm GNU toolchain (`arm-none-eabi-g++` and binutils), make, CMake,
Python 3, Git, `patch`, and a host C++17 compiler (`clang++` for the check script).
`dfu-util` is needed only for flashing. Use the public Daisy bootloader configured
for a QSPI application at **0x90040000**.

Place your five ROM files directly in a private folder. The Daisy command-line
importer expects a folder, rather than the plugin's ZIP/file chooser.

```sh
NATIVE_HALL_ROM_DIR='/path/to/224-v44-roms' \
  DAISY_BOARD=seed ./script/build_daisy.sh --check

NATIVE_HALL_ROM_DIR='/path/to/224-v44-roms' \
  DAISY_BOARD=patch-sm ./script/build_daisy.sh --check
```

`--build-only` skips signal/control checks but still verifies ROMs and the image
layout. `LIBDAISY_DIR` can point to an existing checkout of the pinned libDaisy
revision with its nested dependencies initialized. Private uncommitted changes
are not copied: the build export uses the pinned Git objects.

The result is `build/daisy/<board>/Cineol224_Daisy.bin`, with ELF/MAP/HEX files
alongside it. After implementing and checking your hardware adapter, flash with:

```sh
NATIVE_HALL_ROM_DIR='/path/to/224-v44-roms' \
  DAISY_BOARD=seed ./script/build_daisy.sh --flash
```

The command builds and validates first, then waits for the existing bootloader.
Press normal RESET when prompted. It replaces the QSPI application and does not
install or replace the bootloader. It assumes the standard Daisy USB DFU setup.
MCU compilation and layout checks do not establish CPU margin, physical I/O
correctness or compatibility with another bootloader. Verify those on your board.

## Tests and DSP limits

Run the desktop checks with your own supported ROMs:

```sh
NATIVE_HALL_ROM_DIR='/path/to/224-v44-roms' ./script/build_plugin.sh --check
```

Checks cover import/cancellation/cache restart, dry behavior before import,
processor state and automation, allocation-free processing and all six native
networks against the independent row machine. Daisy `--check` also exercises
voltage normalization, inversion, controller mapping, button gestures, RGB
colors and stereo DSP before checking the ARM image layout.
`build_plugin.sh --compare` writes local reference/native audio diagnostics to
`build/validation/`; these are not distribution assets.

The outer DSP path runs at **48 kHz** and the integer network at **20,480 Hz**.
The desktop rate bridge supports other host rates. The portable core has fixed
storage and no JUCE, emulator, file I/O or locks. Daisy uses 48-frame blocks and
updates logical controls every second block (500 Hz). Hardware adapter reads
and RGB updates must remain bounded, allocation-free and nonblocking.

Native row arithmetic and extracted control tables have independent exactness
checks. The complete original hardware audio/control path is not bit-exact:
analog filters run on the 48 kHz grid, controller clocks use measured nominal
rates, modulation phases can differ, and the original pre-delay adjustment ramp
is not implemented. This is not a claim of complete hardware equivalence.

## Repository layout and private data

```text
native-hall/core/          portable C++17 DSP
native-hall/import/        shared original-224 ROM importer
native-hall/plugin/        JUCE processor, editor and current artwork
native-hall/daisy/         generic I/O contract, configuration and ARM project
native-hall/tools/         local ROM preparation and reference comparison
native-hall/tests/         DSP, processor and import checks
deps/reflexion/            pinned upstream Git submodule
deps/libDaisy/             pinned hardware library Git submodule
patches/                  compatibility changes applied only in build exports
script/                   setup and build commands
build/                    ignored outputs, exports and private generated banks
```

ROMs, banks, captures, firmware images, local hardware adapters and build
products stay outside version control. A Daisy image contains ROM-derived data;
the source repository does not include personal firmware images. The private
review translation is also excluded from this repository.

## Attribution and license status

Reflexion provides the emulator, row-machine reference, rate bridge and source
analog model used by this project. JUCE and libDaisy remain separate dependencies
with their own notices and terms. See [NOTICE.md](NOTICE.md) for provenance.
A project-wide license for Cineol-owned files has not been selected for this
prepared repository; no license has been invented for upstream code.

ROM import is a local compatibility and data-preparation step. It does not prove
ownership or grant permission to redistribute ROMs or extracted data. Lexicon
and Electro-Smith names identify the supported reference and platform; this
project is not affiliated with or endorsed by those companies.
