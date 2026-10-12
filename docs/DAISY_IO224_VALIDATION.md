# Compact original-224 Daisy converter boundaries — 2026-10-11

Daisy now uses the current Cineol-X original-224 input/output boundary as its
target. The correction is confined to the converter/filter policies selected
in `daisy/src/main.cpp`. No 224XL, Hall network, ARU arithmetic, controller,
parameter ID, plugin source or dependency pin was changed.

## Implementation and sound change

The previous Daisy chain used a sampled 48 kHz analog approximation, a 64-tap
input SRC and a 32-tap output SRC. It lost additional high frequencies and
approximated the independent original-224 converter holds.

`daisy/src/Io224.hpp` uses the desktop's analytic 33-tap input kernel with its
six-mode tail, separate left/right ADC holds, and event-timed DAC captures/AOUT
reconstruction. All audio arithmetic is float. Real poles omit imaginary
arithmetic; mirrored input history removes circular indexing inside the tap
loop. Output requires no interpolation FIR. Only the negligible -500 kHz output
mode and tiny direct output term are omitted. The desktop's 38-frame output
transport and shared 70-frame dry/bypass delay are retained.

The sound change is a closer input bandwidth, output reconstruction and channel
phase relationship to current Cineol-X. Reverberation is not retuned.
Analog/bypass controls, smoothing, 48 kHz host rate, 20,480 Hz native rate and
48-frame blocks retain their existing laws. The independent desktop controller
scheduler was deliberately not ported in this filter-only block.

`generate_io_tables.py` derives float coefficients from desktop ADC/DAC source
pole/residue data and analytic interpolation equations, without ROMs or
Reflexion. The generated header records the source digest. Direct make and the
build script reject stale tables. Declared kernel data is 26,160 bytes; the ARM
compiler emits 25,920 bytes of coefficient arrays in immutable AXI SRAM and
folds smaller constants into code. State is fixed in DTCM. No allocation,
coefficient preparation, file I/O, logging, locks or waits were added to audio
processing. Outlined converter functions, if any, are covered by ITCM placement
rules and the layout validator. ARM disassembly contains no double arithmetic;
existing control-rate gain changes still use `powf`.

## Boundary comparison against current Cineol-X

`daisy/tests/io224_check.cpp` feeds identical signals/converter streams into
the actual current Cineol-X `Adc48` / `DesktopOutput48`, compact Daisy policies,
and old `LegacyInput48` / `LegacyOutput48`. Reflexion is not the comparison
oracle. Desktop sources remained unchanged; before/after digests are retained.

Fixtures cover impulse pairs, stepped levels, seeded noise followed by silence,
and 40–20,000 Hz tones. Output checks cover four capture networks and four DAC
channels. Above the native output Nyquist, tests exercise folded native tones;
these are not output passband measurements. Release checks retain assertions.
A separate million-frame run checks resets, arbitrary-phase network switching,
histories and capture queues.

| Boundary metric | Compact Daisy vs current Cineol-X |
| --- | ---: |
| Input values checked | 1,105,920 |
| Output values checked | 20,736,000 |
| Input maximum absolute error | 4.28e-7 |
| Output maximum absolute error | 5.96e-8 |
| Input maximum gain error, 40–9600 Hz | 0.0000164 dB |
| Output maximum gain error, 40–9600 Hz | 0.000000020 dB |
| Worst input relative error, 40–9600 Hz | -111.90 dB |
| Worst output relative error, 40–9600 Hz | -143.05 dB |
| Audio allocations / releases | 0 / 0 |

Previous input gain errors were -0.7815 dB at 8 kHz, -2.6535 dB at 9 kHz and
-6.8482 dB at 9.6 kHz. Previous output errors were +1.4992 dB, +0.6480 dB and
-3.5161 dB respectively (network 0, channel A). The correction addresses those
filter/SRC differences without extra EQ.

Float input arithmetic intentionally approximates desktop double precision:
261 of 1,105,920 checked ADC words (0.0236%) and one comparator mask differ.
Exact ADC-word or complete reverb sample equivalence is not claimed. The
input-silent filter checks after 1.5 seconds showed zero additional residual
error with the configured flush-to-zero policy.

## Narrow response/tail and listening fixtures

Two six-second renders use identical portable Hall algorithms and controls,
with current desktop boundaries on one side and Daisy boundaries on the other.
This explicitly excludes desktop controller scheduling differences. Input is
a one-second deterministic noise/tonal burst sequence followed by five seconds
of silence, after 100 ms silent settling. Measured files are unnormalized;
additional level-matched files are supplied only for listening.

| Metric | Program 03: Large Hall B | Program 05: Small Hall A |
| --- | ---: | ---: |
| Complete render gain change | -0.0000153 dB | -0.00000141 dB |
| Complete render relative waveform error | -51.60 dB | -55.68 dB |
| Largest tail band-energy difference, 20–10,000 Hz | 0.001415 dB | 0.000334 dB |
| Final 0.5 s RMS, Cineol-X boundaries | -98.78 dBFS | -90.44 dBFS |
| Final 0.5 s RMS, Daisy boundaries | -98.18 dBFS | -90.28 dBFS |

The 250 ms RMS windows and band energies inspect input-silent intervals.
Quiet quantized network residuals differ even though linear boundaries are
very close. No algorithm correction or attempt to null those tails was made.
These limited diagnostics are not all-program listening acceptance. Listening
review remains pending; no actual listening acceptance is claimed.

## CPU, memory and firmware

Three interleaved host timing rounds cover six programs and three modes:
basic, Mod+Opt, and analog bypass. Each excludes preparation and processes one
second of input plus one second of silence. Both sides use the same Hall, bank,
controls, Clang `-O3 -ffp-contract=off`, and ARM64 flush-to-zero policy.

Across 18 workloads the median new/old time ratio is **0.8651** (13.49% less
host processing time); individual ratios are 0.8524–0.8739. Against same-Hall
current desktop boundaries the median ratio is 0.9063. Host engine state fell
from 57,040 to 40,344 bytes; desktop boundaries with the same Hall use 82,024
bytes. Reduced operations and float-only ARM audio arithmetic support the
intended cheaper implementation. Host timings do not establish Cortex-M7
realtime margin or a worst-case callback deadline; AXI table/cache behavior
also needs on-device timing.

| Final QSPI build | Seed | Patch SM |
| --- | ---: | ---: |
| Binary bytes | 306,748 | 309,116 |
| Hot ITCM bytes | 23,256 | 23,256 |
| DTCM stack margin bytes | 75,552 | 74,680 |
| Free AXI bytes after control bank | 127,700 | 125,332 |

Both images passed layout checks with Arm GNU 15.2.1, `-O3`, hard float and
`-ffp-contract=off`, verified original v4.4 ROM1–ROM5, and the unchanged pinned
library export. Dependency checkouts are clean. Checks enforce ITCM hot code,
AXI bank/constants, DMA isolation and a 16 KiB minimum stack reserve. Images
and banks remain ignored private build outputs. The pin-free reference adapter
is used; no local hardware adapter was present. Neither image was flashed.

Validation passed: ROM gate, all-six-program bank/ARU checks, controls and
hardware-adapter checks, CTest processing/converter regressions, and the
million-frame converter run with AddressSanitizer + UndefinedBehaviorSanitizer.
Existing bare-metal libc syscall and RWX load-segment linker warnings persist.

Physical Daisy codec/front-end response, on-device callback timing and physical
listening remain unmeasured. The software comparison models original-224
converter/filter behavior; it cannot measure the carrier's actual ADC/DAC
circuitry or compensate its response without hardware captures.

## Reproduction and retained evidence

Local evidence directory: `build/validation/daisy-io-20261011/` contains
`boundaries.csv`, `cpu.csv`, `summary.json`, `tail-windows.csv`, frequency/tail
plots, program 03/05 raw and matched WAVs, source digests, compiler disassembly,
build/CTest/sanitizer logs, and previous Seed binary/map under `baseline/`.

Check coefficients with `python3 native-hall/daisy/tools/generate_io_tables.py --check`.
Build CMake target `cineol_daisy_io224_check`; run it with the private bank and
an existing output directory. Omitting `--quick` generates timing/listening
evidence; `--quick` validates boundaries/resets without another WAV/timing
campaign. Analyze complete evidence using
`python3 script/analyze_daisy_io224.py OUTPUT_DIRECTORY` (numpy/matplotlib).
`script/build_daisy.sh --check` includes quick converter regression. Set
`NATIVE_HALL_ROM_DIR` to original v4.4 ROMs and optionally `DAISY_BOARD=patch-sm`.
Regenerate coefficients after desktop ADC/DAC source changes.

Generated-material review: retain the active narrow comparison, listening
pairs, provenance, baseline and regression evidence. No user ROM/bank/capture,
hardware adapter or dependency resource was deleted. No redundant full WAV
campaign was created.
`cleanup.json` records removal of the byte-identical sanitizer metrics copy
(`sanitized/boundaries.csv`, retained as `boundaries.csv`) and its empty
header-only timing CSV; the sanitizer log and executable are retained.
