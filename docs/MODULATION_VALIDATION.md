# Original-224 desktop modulation checkpoint

October 5, 2026. Baseline: `0ebb668`. Reference: Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, original 224 v4.4 ROM1–ROM5.
See [continuation record](SOUND_ACCURACY_HANDOFF.md).

## Status and resulting behavior

Phase/counter diagnosis and the direct-program-switch correction are complete.
Precise signal/state-dependent scan scheduling remains open. Do not treat this
checkpoint as complete modulation-on tail equivalence.

`DesktopHall::select_program()` now retains the modulation divider, random
divider and random hold across direct program switches. It still loads the new
program's tap descriptors/coefficients/offsets and random-table index (four in
all six supported profiles). Previously it replaced the three counters with
the destination bank's import-time snapshot, creating a different first set
of modulation steps after switching.

The firmware compiler was tested independently: all six program loads retained
counter sentinels 11/5/7, while resetting the random index to four. Modulation
was disabled at its entry during this offline probe, so subsequent routine
calls could not obscure the compiler's writes. The sentinels are private
oracle RAM, not shipped data or a fitted audio fixture.

This changes direct switching in the desktop application. With **Spillover**,
the old network continues and the new network starts as a separate instance;
its independent startup remains intentional. The correction does not transfer
the old tail's counters into that new instance or change the overlap mixer.
The default portable `Hall` switching/processing law is unchanged. No Daisy
configuration, build or hardware work was performed.

`ModulationState` provides a fixed-size in-memory control/diagnostic snapshot.
It is not serialized, does not change the bank/session ABI and rejects tap
addresses belonging to another program before restoring state. Snapshot work
occurs at direct switches or offline diagnostics, not each processing sample.
There are no new processing allocations, I/O, locks or waits.

## Independent step-law measurements

The checked-in `native_224_modulation_check` uses SHA-256 recognition for all
five private ROMs. For each of six programs it tests Depth 0/7/21/35/54/71,
two enhancement-enabled modes (with/without decay optimization), and four
constant ADC mantissas (0/128/1024/2047 with corresponding detector masks).
Bass/Mid also vary with Depth to exercise the level-controller workload.

At each actual ROM routine entry (PC `0x0c7c`), the native law advances once.
At the next entry, the test compares descriptors, random index, all three
counters, all four interpolation coefficients and their offsets. Only the
first state is aligned per case; it is not copied again during the comparison.

- First 288-case run: **203,250 exact transitions**, zero state mismatches.
- Repeat including the compiler counter probe: **203,286 exact transitions**,
  zero state mismatches.
- Depth-table/modulation-row intersections: **zero** in every program.
  Reapplying Depth does not overwrite the modulated interpolation coefficients.

Therefore neither the native step law nor a Depth-table collision explains
the remaining default modulation-on differences. These checks prove the law
at matched call boundaries, not an identical runtime call schedule.

## Timing and initial phase

The current desktop scheduler distributes 18 calls uniformly per panel scan.
The ROM has unequal intervals, conditional XREG reads, level-controller work
and WCS bus waits. Constant-input cases differed from the bank's silent nominal
rate by up to **3.17%** in the first run and **3.00%** in the repeat. These
600 ms measurements depend on counter/history phase and control values; they
are observed bounds for these fixtures, not global timing-error guarantees.

The same initial phase is also needed when enhancement is disabled: the taps
freeze at whichever interpolation phase was reached before disabling it.
Two valid frozen phases can give very different high-band decay estimates.
An offline state-aligned result must never be presented as the plugin result.
See [residual sound measurements](RESIDUAL_SOUND_VALIDATION.md) when available.

No empirical rate multiplier or compensating EQ was introduced. A rate chosen
to improve one tail would not reproduce the firmware's changing branch paths.
The next correction should derive bounded native event timing from independent
ROM traces, then validate signal levels, controls, phases and CPU together.

Useful trace boundaries: modulation entry `0x0c7c`; first/second returns
`0x00bb`/`0x01b5`; XREG read `0x018f`; level word `0x0779`; completed level
update `0x0228`. Counter RAM is `0x3e66..0x3e6a`; descriptor pointer is
`0x3e27..0x3e28`. Keep the emulator confined to offline tools.

The subsequent [native control-timing foundation](CONTROL_TIMING_VALIDATION.md)
derives the modulation procedure's instruction costs and individual WCS
access waits. All 576 cases matched, including 238,967 complete call durations
and 328,567 write boundaries. This local model receives actual entry clocks
and states in its offline oracle; it does not replace the uniform scheduler
or establish a new modulation-on sound result.

## Reproduction

```sh
cmake --build build/sound-validation --config Release --target native_224_modulation_check native_224_controllers_check
native_224_modulation_check "/private/path/224 v4_4" build/sound-validation/native-hall/programs-v44.bank224 build/modulation-rates.csv
native_224_controllers_check build/sound-validation/native-hall/programs-v44.bank224
```

The controller test checks counter retention, index reset, decay continuity,
sparse transfer reads, comparator holds and allocation-free bounded processing.
For desktop CPU, compare against the frozen baseline using the tracked driver:

```sh
python script/benchmark_sound.py 0ebb668 build/sound-validation/native-hall/programs-v44.bank224 build/modulation-cpu
```

The driver exports only core/desktop source into a private build directory,
compiles baseline/current independently, alternates five run pairs and reports
per-case medians. Each run contains three repetitions for six programs in
controllers-off, both-on and analog-bypass modes. Preparation/warmup are untimed;
x86 FTZ/DAZ matches the desktop callback. Run it without other measurement jobs.

The paired CPU run against `0ebb668` measured a summed median ratio of
**1.01676 (+1.68%)**, with individual cases **0.99732–1.02866**. Normal
non-switching output checksums agreed in all 18 workloads. Desktop storage
remains **82,088 bytes**, default storage 57,040 bytes. The correction introduces
no extra work in steady processing; this small measured difference includes
compiler layout/timing variation and is not a whole-DAW CPU percentage.
Switch/Spillover costs and plugin regression are recorded in the subsequent
acceptance checkpoint. Local private evidence is under
`build/validation/modulation-20261005/`; no ROM, bank, WCS or audio file belongs
in Git. Plugin/parameter IDs and format versions remain unchanged.
