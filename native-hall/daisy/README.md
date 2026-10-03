# Generic Daisy adapter

The audio engine supports Daisy Seed and Daisy Patch SM board initialization.
The controller interface is independent of the carrier board: ten readings,
two logical buttons and RGB output for the selected algorithm. No button LED
outputs are exposed. The default reference adapter has no assigned I/O pins.

## Connect your hardware

1. Copy `src/ControlConfig.example.hpp` to `src/ControlConfig.local.hpp`.
2. Assign the ten targets, input ranges, inversion and optional curves there.
3. Copy `src/HardwareAdapter.example.hpp` to `src/HardwareAdapter.local.hpp`.
4. Assign the ten analog pins, two button pins and three RGB pins, and set
   each analog input's scale/offset conversion. No pin is assigned by default.
5. Build for `DAISY_BOARD=seed` or `DAISY_BOARD=patch-sm` as appropriate.

Both local headers are ignored by Git. Keep ADC/GPIO setup, conditioning,
calibration and pin choices in the hardware adapter. `Read` returns readings
in the numeric units declared by `control_config`; it also returns `button1`
and `button2` as logical pressed states. The mapper handles clamping,
normalization, inversion, musical scales, hysteresis and button debounce.
Optional curve functions take and return 0..1 positions.

The example initializes ADC smoothing, two GPIO inputs and three GPIO outputs.
Analog readings become `smoothed_adc * scale + offset` before reaching the
mapper. For example, scale 3.3/offset 0 represents 0..3.3 V, while scale
10/offset -5 represents -5..+5 V after the appropriate analog conditioning.
Each button has active-high/active-low and pull configuration; each RGB channel
has its own output polarity. The example palette uses on/off GPIO. Replace
`WriteRgb` with bounded PWM if variable brightness is required. You can replace
`Init`, `Read` and `WriteRgb` entirely for MIDI, an external ADC or another source.

`Init` runs before audio starts. `Read` runs at 500 Hz at the start of every
second 48-frame audio block, outside the sample loop. Poll already-running ADC
conversion results there; do not wait for conversion or perform blocking I/O.
`WriteRgb` runs when the active program changes. GPIO writes or bounded PWM
updates are appropriate; formatting text or waiting for a peripheral is not.
Electrical button polarity and common-anode/common-cathode LED handling are
adapter responsibilities.

The two single releases toggle Mod Enhancement and Decay Optimization; the
two-button chord toggles analog filtering after both buttons are released.
Default RGB colors are cyan, magenta, blue, green, red and yellow for programs
01–06. There are no mode/decay indicator outputs or button backlights.

With the unmodified pin-free adapter, the image processes stereo audio using
the fixed Large Hall B preset. Physical controls, buttons and RGB become active
only after the adapter is implemented. When changing hardware/configuration,
remove the corresponding `build/daisy/<board>/` directory before rebuilding so
removed local-header overrides cannot remain in cached objects.

## Memory and boot

The QSPI application begins at 0x90040000. Its reset stub copies normal code and
constants into AXI SRAM, hot DSP into ITCM, and interrupt vectors into D3 RAM
before libDaisy initialization. Engine state resides in DTCM; the bank is copied
to a dedicated AXI SRAM area. DMA buffers have a separate D2 region. SDRAM is
not used by the engine. The linker and layout check enforce these placements.

The source ROM is checked before every build or flash invocation, including
direct `make`; `make clean` does not require ROMs. Generated bank data is embedded
in the personal image. Desktop preparation executes the control firmware;
neither the 8080 emulator nor a file importer runs on Daisy.

Build and flash instructions, requirements and limitations are in the
[main README](../../README.md#build-daisy-firmware). Never infer MCU timing or
physical I/O correctness from a successful compiler/layout check alone.
