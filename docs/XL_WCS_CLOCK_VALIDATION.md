# Native XL T&C access clock

October 6, 2026. This component follows the
[all-22 controller chronology](XL_CONTROLLER_SCHEDULING.md). It supplies a
native grant/visibility calculation needed by the forthcoming coupled scan.
It is not connected to Native48 yet: nominal controller rates, whole-pass
coefficient updates and the shipped ADC/DAC checkpoint remain unchanged.
The full sound-accuracy goal remains open.

## Resulting component

`desktop/wcs_timing_xl.hpp` contains a 64-byte `WcsTimingXL` instance and a
44-byte shared constexpr table of the two unprotected rows for each graph.
Those are intrinsic graph properties, independently checked in 80 physical
Mod/Size fixtures; no firmware words, coefficients or delay addresses are
embedded in this clock.

The clock tracks marker/fetch phase, the protection-pair flip-flop, RESETD
and the previous fetched protect/reset flags. A granted access can replace
subsequent fetches with zero flags; these changes affect the next grant.
For a supplied data-bus T1 state, it predicts grant, XACK, CPU data sampling,
bus-cycle completion, write commit, displaced-fetch and operand-hold windows.
Reads also predict byte-lane drive/release and their longer displacement/hold
windows. Bus-cycle completion is not a general CPU instruction decoder.

Time uses integer ticks of 1/576 ns: a CPU state is 281,250 ticks and a DSP
row 168,750. Marker and fetch remain separate events so a request between
them sees the correct previous microinstruction. Write commits occur at
grant + 226,440 ticks, before the next displaced fetch by 1,152 ticks (2 ns).
Read windows follow the CPU's READY/data-sample boundary rather than assuming
the write's fixed window.

Long quiet intervals jump to a complete-pass boundary. Stable protected spans
jump to the next protection/reset/displacement edge. The component therefore
uses logical control stages rather than evaluating DSP opcodes or firmware.
It has no allocation, I/O, locks, sleeps or waits. The measured maximum work
in the oracle drops from 546 to 61 primitive phase operations per access;
no mathematical time boundary changes.

## Independent oracle and scope

`cineol_xl_wcs_clock_check` independently clocks the pinned reference T&C
`WcsAccess` board and its scheduler. Its ready-controlled bus actor supplies
legal CPU-clock-aligned data-bus T1 times, read/write operations and varied
gaps. Both models receive those component inputs; reference grant/wait times
are never supplied to the native calculation. The next actor request follows
the reference's completed bus cycle, as a real CPU must.

The private-ROM run first selects each of the 22 programs through the physical
operator and validates graph shape/protect/reset. It then copies that selected
WCS into a fresh reference board. Native uses its independently established
fixed graph properties. This is a labelled component boundary experiment,
not native/reference whole-engine phase alignment. The no-ROM CTest variant
uses synthetic layouts and checks the same access-clock logic; it does not
independently establish the property table's physical mapping.

For each layout, 4,096 accesses include 1,171 reads and 2,925 writes, different
byte lanes, dense accesses and long quiet gaps. Across all 22 layouts this is
90,112 accesses: 25,762 reads and 64,350 writes. Grant, XACK, sample, completion,
commit, displaced interval, held interval and read-drive/release boundaries
match exactly, with no tolerance. Native heap new/delete counts are zero.
The physical and synthetic runs also retain identical timing checksums.

The actual unoptimized component source/binary is frozen independently.
Before/after comparison checks all eleven returned time fields for 90,112
inputs; every field is identical. The optimization changes only computation
work. It is not a failing-old Native48 sound oracle: production has no T&C
grant clock to replace yet. A meaningful frozen-production failure/pass check
is still required when the scan and individual commits enter audio processing.

The oracle holds DSP RUN high and begins with a freshly loaded graph at its
canonical row-zero phase. It does not validate firmware power-up loading,
halt/single-step, physical program/key phase, serial IRQ generation, arithmetic
effects of operand holds/displaced fetches, or control changes during tails.
Those are retained integration requirements, not completed gates.

## Matched component cost

After all owned builds/reference checks finished, five alternating before/new
rounds timed 25 repetitions of 22 x 4,096 accesses: 2,252,800 accesses per round.
Preparation and field-by-field correctness checks are outside timing.
Both versions are O3 C++20 with `-ffp-contract=off`; external host activity
remains uncontrolled. The measured median is 0.555423 s before and
0.122078 s after, ratio `0.219792` (about 4.55x faster). Spreads are
0.554320-0.564866 s and 0.121463-0.122531 s respectively. Checksums match in
every round; instance size remains 64 bytes.

This is an integer clock-component benchmark. It does not measure whole-plugin
audio CPU, Spillover worst case or Daisy margin. The last validated ADC/DAC
audio CPU/storage checkpoint remains applicable while production code is
unchanged; fresh matched audio/plugin measurements are required on integration.

## Identity and reproduction

The actual preceding ADC/DAC production and chronology source is frozen as
136 files under `build/validation/xl-wcs-clock-20261006/baseline/`.
Reference remains Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69` through
the build-only export. Physical checks require all eleven hash-validated
v8.21 chips; bank/preset format and supported firmware do not change.
No private ROM/WCS payload or hardware adapter belongs in Git.

```sh
cmake --build build/xl-sound-validation --target cineol_xl_wcs_clock_check -j2
ctest --test-dir build/xl-sound-validation/native-hall -R '^cineol_xl_wcs_clock_component$' --output-on-failure
build/xl-sound-validation/native-hall/cineol_xl_wcs_clock_check build/validation/xl-catalog-20261005/rom-private-local
```

Private evidence contains source/binary manifests, immutable before-component
and production snapshots, exact-boundary logs, paired benchmark input/field
checks and per-round costs under `build/validation/xl-wcs-clock-20261006/`.
The next step couples this grant clock to the native Mod/slow/fast/auxiliary
controller branch/stage costs, then establishes per-row commit and arithmetic
visibility with independent full-path failing-old/passing-new evidence.
