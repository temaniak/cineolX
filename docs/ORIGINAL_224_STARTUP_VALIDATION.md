# Original-224 startup and Mod-key phase

October 5, 2026. Baseline: `f561f54`. Branch:
`codex/original-224-sound-accuracy`. Reference: original 224 v4.4 ROM1-ROM5,
Reflexion `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. Host: macOS arm64,
AppleClang Release, strict FP. See the [continuation record](SOUND_ACCURACY_HANDOFF.md).

## Result and sonic behavior

The physical **Mod key recompiles the program in both directions**. It resets
the random-table index and interpolation descriptors, coefficients and delay
addresses to the program seed. Three global modulation counters survive.
Previously, the desktop plugin froze/resumed the current interpolation phase.
`DesktopHall::set_controls()` now restores the bank's program seed whenever
Mode Enhancement changes. Decay Opt alone retains the modulation phase.

The correction keeps delay memory, converter state, level/history state and
the three modulation counters. It cancels the previous procedure's predicted
writes and restarts the canonical desktop scan at the parameter boundary.
This changes the sound following Mod automation/key changes: modulation
restarts from the original program's seed, and disabling Mod selects that
seed's fixed interpolation taps. The tail's memory is retained.

The existing banks already contain the correct program seeds: descriptors,
tap coefficients/addresses and random index four. No bank/cache migration,
plugin/parameter-ID change, filter, EQ or feedback retuning is required.
The legacy single-Hall path and ordinary portable `Hall` are unchanged.
No DSP interpreter, ROM execution, allocation, lock, I/O or wait was added
to processing. No Daisy build or dependency checkout change was made.

This completes the compiler-seed and Mod-key phase correction. It does **not**
simulate the compiler's instruction-by-instruction transition, its duration,
analog switching transient or pre-delay ramp. Cold startup retains a canonical
native scan/converter context and prepared counter snapshots. Full sonic
acceptance with modulation active remains open.

## Independent seed and transition checks

`native_224_startup_phase_check` runs the real firmware offline. Program-load
probes suppress modulation at its first instruction, allowing the compiler to
run normally. Its complete tap state is compared with the prepared bank.
Injected counter sentinels verify retention, rather than assuming fresh-zero
global state. First-call step checks copy only those retained counters into
the native engine; the tap seed itself comes from the bank.

Mode-key probes use real panel keys. A write to a graph instruction lane
identifies recompilation independently of the modulation routines' data
writes. The first subsequent modulation entry exposes the completed seed,
before an enabled routine can move it. Decay Opt keys provide the negative
control. Graph comparisons ignore only coefficient/sign/address data.

| Check | Coverage | Result |
| --- | --- | --- |
| Repeated program loads | Six programs, two preceding counter contexts | Complete bank/compiler tap seeds exact; index four; three counters retained |
| Depth composition | 72 cases: Depth 0/7/21/35/54/71 | Compiler and native tap state exact; no descriptor/coefficient inconsistency |
| First modulation calls | 2,838 calls from compiler seeds | Complete state exact at every actual call boundary |
| Direct program switches | All 30 directed pairs | Complete load modulation state exact |
| Physical Mod keys | 24 cases: six programs, four initial flag combinations | Recompile in both directions; native seed/counters exact |
| Physical Decay Opt keys | 24 cases | No graph reload; native modulation phase retained; all 12 frozen reference phases unchanged |
| Graph instructions across mode keys | 4,800 rows | All instruction fields invariant |
| Streamed native adapter | 360,000 passes, mode changes | Controller state/completed writes exact; 45 pending procedures cancelled; delay memory/counters retained |
| Allocation/property regression | Additional 1,080,000 passes | Zero allocations; history, comparator and program continuity checks pass |

The same new mode oracle, compiled against `f561f54`, fails with
`Native Mod transition differs from the compiler seed/counters`.
The initial program-load and direct-switch checks still pass in that negative
run. This distinguishes the newly corrected behavior from previously saved
work. Call-boundary oracles do not prove compiler transition audio timing.

## Startup sensitivity and historical fixture limits

The historical sound fixture used direct RAM flag writes to disable Mod,
after 400 ms of enabled operation. That freezes the reached tap phase and
**bypasses the physical key's program recompile**. The importer used physical
keys and captured the compiler seed. In addition, the fixture gave the
reference 300 ms of enabled settling before starting the native engine.
Those differences must be considered when interpreting older mode-off and
normal-startup reports. Historical results are retained for regression; they
are not measurements of the physical Mod-off transition.

`native_224_sound_compare --startup-probe` adds four explicitly offline
variants: equal extra warmup; reference modulation state at mode enable;
reference decay state at mode enable; and both states at mode enable. Each
receives the same extra 300 ms. Their native scan, converter and delay-memory
contexts remain independent. No state is restored at input start in these
four variants. The older `aligned` variant still restores controller state
at input start and remains diagnostic too.

The deterministic matrix has 24 fixtures: all six programs with both modes
enabled, noise seed 17/amplitude 0.08 at warmups 250/1000/1047 ms, and seed
224/amplitude 0.12 at 1000 ms. Each is stereo 48 kHz, 200 ms noise plus a tail,
12 seconds total. Seven broad bands give 42 accepted fits per group and
variant; every comparison below uses the same accepted bands. The overlapping
9–10.24 kHz diagnostic band is excluded from mean/max T20. Acceptance retains
R-squared at least 0.95 and the original end-of-capture margin. All fits,
including rejections, remain in the private CSV.

Mean absolute broad-band T20 error (%):

| Seed / warmup ms | Historical native | Equal warmup | Shared modulation at enable | Shared decay at enable | Both states at enable | Input-start aligned |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 17 / 250 | 4.2686 | 3.2456 | 1.0526 | 3.7514 | 1.1977 | 0.8076 |
| 17 / 1000 | 3.8745 | 3.8605 | 1.6266 | 3.8816 | 1.4924 | 1.3368 |
| 17 / 1047 | 3.2536 | 3.9581 | 1.7043 | 3.6348 | 1.5258 | 0.8050 |
| 224 / 1000 | 4.9799 | 4.4286 | 1.3708 | 4.2116 | 1.5770 | 0.6594 |

Across these 168 fits, sharing only the initial modulation state changes the
diagnostic mean from 4.0942% to 1.4386%. Equal warmup alone is inconsistent;
sharing decay alone has a much smaller average effect. Sharing both is not
uniformly better: Percussion Plate A, seed 224, 8–10.24 kHz has a 20.8637%
T20 error in that variant, versus 18.8550% historical maximum across programs.
No blanket equivalence or shipped T20 improvement follows from these probes.
Independent clocks and state-dependent branch timing still matter.

## Comparison from the compiler seed

`--compiler-start` makes the reference use the same physical-key seed capture
as bank preparation, then compiles controls and enables the requested flags.
It labels metadata `phase_protocol=compiler`. The default recipe remains
`historical`; neither recipe overwrites the other. This is a cold comparison
from a compiler seed, not a full recording of physical key transition audio.

With the same seed 17/amplitude 0.08/default controls/1000 ms warmup, all six
programs give:

| Mode | Accepted broad fits | Mean absolute T20 error | Maximum absolute T20 error | Maximum band-energy error |
| --- | ---: | ---: | ---: | ---: |
| Mod off, Opt off | 42 | 0.3393% | 2.7767% | 0.0381 dB |
| Mod off, Opt on | 42 | 0.3792% | 3.8545% | 0.0360 dB |

The historical recipes gave much larger errors when comparing different
frozen phases. This table demonstrates that those errors are not grounds for
compensating EQ or changing feedback. **The reference fixture changed here;
these percentages are not a before/after plugin optimization claim.**

## Regression, CPU and reproduction

Thirty paired historical fixtures against `f561f54` retain all 120
input/reference/native/aligned WAV files **byte-identically**. The Mod-edge
correction therefore leaves their steady startup measurements intact.
The comparison CSV now combines the union of old/new metadata columns. The
first aggregation failed on the added probe columns; aggregation was repaired
from the completed, verified paired fixtures without changing any audio data.

Twelve CTests pass, including portable/legacy profile, ADC/DAC, controller,
gain, fixed-graph queue and processing checks. The first full CTest run found
an unbuilt legacy test executable; building its desktop target resolved it.
VST3 and Standalone build and ad-hoc signature verification pass. The bank
processor check passes at 44.1/48/96 kHz, blocks 128/511/20000, state and mono,
with callback new/delete both zero. Original-224 Spillover passes all 30
directed pairs at all three rates, including rapid/disable, mono/stereo,
1/5/10-second retirement, dry and low latency cases.

The quiet paired CPU run against `f561f54` used 18 workloads, 15 samples per
case and five alternating baseline/current runs. ARM FZ matches the desktop
denormal policy; preparation/warmup are excluded. Summed median ratio is
**0.998354995** (-0.1645%), with case ratios **0.995168955–1.001966225**.
This is unchanged steady processing cost within measurement variability,
not a CPU speedup claim. All workload checksums match. `DesktopEngine48`
storage remains **82,416 bytes** in both revisions. Mod setters are bounded;
this steady run does not separately measure key-edge work or two-slot
Spillover CPU margin. Its functional overlap regression passes independently.

The frozen version-1 bank SHA-256 for these runs is
`15d14dcebbbafad35269e8159ddfedfb24ca9fdd203531b3d85cf094ce6e72ed`.
All banks, WCS images, state CSVs and WAVs stay under ignored
`build/validation/startup-20261005`. None is a source dependency or Git asset.

```sh
cmake --build build/sound-validation --config Release --target native_224_startup_phase_check native_224_sound_compare native_224_controllers_check
native_224_startup_phase_check "$ROM_DIRECTORY" "$BANK" build/startup-seeds.csv
python script/probe_startup_phase.py "$SOUND_COMPARE" "$ROM_DIRECTORY" "$BANK" build/startup-probes
native_224_sound_compare "$ROM_DIRECTORY" "$BANK" build/compiler-p0-m0 0 0 noise 17 .08 1000 --compiler-start
python script/analyze_sound.py build/compiler-p0-m0
python script/compare_sound_checkpoint.py f561f54 "$ROM_DIRECTORY" "$BANK" build/startup-regression
python script/benchmark_sound.py f561f54 "$BANK" build/startup-cpu
ctest --test-dir build/sound-validation/native-hall -C Release --output-on-failure
native_hall_plugin_check --bank "$BANK"
native_hall_plugin_check --spillover-224-check "$BANK"
```

Use built executable paths on the current host, a test-only
`CINEOL224_CACHE_DIR`, and original private ROMs outside Git. Compiler-start
tables use programs 0–5 and modes 0/2. Probe and comparator commands require
NumPy; DSP runtime and CPU measurements do not.

For listening validation, play a percussive burst and a sustained chord on
each program, switch Mod off/on during the tail, then change Decay Opt alone.
Check retained tail memory, repeatable Mod seed behavior, dry/latency behavior
and any transient at the key edge. Physical-unit/compiler transition timing
and pre-delay ramp acceptance remain separate next work; no aligned diagnostic
may be substituted for plugin behavior.
