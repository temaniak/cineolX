# Original-224 WCS audio boundary isolation

October 5, 2026. Desktop follow-up to `0417cf1` on
`codex/original-224-sound-accuracy`. Reference: Reflexion
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`, original Lexicon 224 v4.4
ROM1-ROM5. Host: macOS arm64, AppleClang Release with strict FP.

## Write arbitration

`native_224_wcs_padding_check` compares two independent row machines running
the privately generated stock program graphs. Both receive the same changing
coefficients, addresses and input. One also uses the pinned reference
`Scheduler` and `WcsAccess` board model to request unchanged-byte writes.
Keeping the payload unchanged isolates displaced fetches and held operand
clocks from coefficient changes. No firmware CPU or native grant model is
used to choose the actual slots in this test.

Each of the six programs runs 12,000 passes at 20.48 kHz: 9,000 full-scale
stereo noise passes followed by 3,000 silent passes. The test cycles 47 control
sets, advances fractional tap modulation every three passes, uses all four
byte lanes and exercises every allowed grant slot. A deliberately displaced
consumed memory-read row must fail the same state comparator in every program.

| Program | Grants / displaced fetches | Held ARU edges | Result |
| --- | ---: | ---: | --- |
| Small Hall B | 8,691 | 26,073 | Exact audio and pass state |
| Vocal Plate | 9,590 | 28,770 | Exact audio and pass state |
| Large Hall B | 8,691 | 26,073 | Exact audio and pass state |
| Acoustic Chamber | 8,129 | 24,387 | Exact audio and pass state |
| Percussion Plate A | 9,590 | 28,770 | Exact audio and pass state |
| Small Hall A | 8,691 | 26,073 | Exact audio and pass state |

All **53,382** writes committed, displaced **53,382** fetches and held
**160,146** ARU edges. DAC capture words/gains/channels, row arithmetic and
register/XREG data match. Complete delay memory and ARU state match at every
pass boundary. The operand register does differ transiently, confirming that
the hold perturbation actually ran.

The stock allowed slots displace zero-coefficient padding. Their operand
holds recover before a subsequent consumed product. For these stable stock
graphs, reproducing the write-side displacement/holds adds no demonstrated
audio benefit. Their protect-pair transitions still matter to later CPU
write waits; the native scheduler retains that separate timing model.

This result excludes neither coefficient payload timing nor WCS reads.
Compiler/predelay transitions, altered programs and halted-DSP accesses are
outside the test. No runtime graph or sonic behavior was changed by this
investigation.

## Coefficient/address visibility experiment

`native_224_wcs_update_check` isolates update batching with actual firmware
writes and shared startup. It reconstructs three DSP machines from one
reference pass boundary: immutable fetched-word execution, whole groups
visible after modulation/level calls, and the same groups visible only at the
next 100-row pass. CPU events, input holds, graph topology and initial DSP
memory are shared deliberately. The immutable reconstruction must reproduce
the reference's arithmetic/register state on every row and delay memory at
each pass boundary before any difference metric is accepted.

For each program, both-off and both-on fixtures use Bass/Mid 3/15, Depth 35,
minimum predelay and a fixed digital noise seed of 224. Approximately 500 ms
of noise is followed by silence, for a three-second run. A group that finishes
between a row's fetch and execute cannot become visible retroactively.
Output differences are measured at the raw RR words offered to the DAC,
before its mantissa quantization, analog output filtering or host resampling.

The twelve fixtures covered 737,194 passes, 52,099 completed calls and 67,768
actual WCS writes. The immutable fetched-word reconstruction had **zero** row
arithmetic/register or pass-memory mismatches. All six both-off fixtures had
zero output differences and no observed writes.

| Both-on program | Call-completion batching: relative RMS error | Pass batching: relative RMS error | Peak raw-word difference, call/pass |
| --- | ---: | ---: | ---: |
| Small Hall B | 0.7784% | 0.8247% | 473 / 480 |
| Vocal Plate | 0.4765% | 0.5585% | 832 / 839 |
| Large Hall B | 0.7797% | 0.8224% | 450 / 457 |
| Acoustic Chamber | 0.9397% | 0.9909% | 388 / 450 |
| Percussion Plate A | 0.4781% | 0.5663% | 703 / 703 |
| Small Hall A | 0.9688% | 1.0228% | 800 / 820 |

The relative RMS percentage is `100 * sqrt(sum(error^2) / sum(reference^2))`
over the offered output words, including the tail. These results demonstrate
an audio effect from grouping individual writes at completed calls. Deferring
that group until a pass boundary adds a smaller error in this fixture. They
justify investigating native individual-write visibility; they establish no
new decay-length or spectral acceptance result.

These machines share reference CPU state and startup. This is a cause-
isolation experiment, not an independently predicted native controller, a
normal-startup sound acceptance test, or a measured T20 improvement.

The next implementation should preserve the native grant clock and predict
each changed coefficient/address payload before its commit. A fixed graph
need not acquire a row interpreter: a target's fetch happens once per pass,
so its old/new value can be selected before rendering that pass, then its
committed state retained for the next one. First validate payloads and fetch
visibility against the independent ROM; measure runtime cost only after that
proof. Initial/program-load phase remains a separate open issue.

## Reproduction

```sh
cmake -S . -B build/sound-validation -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON -DNATIVE_HALL_ROM_DIR="/private/path/224 v4_4"
cmake --build build/sound-validation --config Release --target native_224_wcs_padding_check native_224_wcs_update_check
ctest --test-dir build/sound-validation/native-hall --output-on-failure -V -R native_224_wcs_padding
build/sound-validation/native-hall/native_224_wcs_update_check "/private/path/224 v4_4" build/wcs-update.csv
```

Banks and WCS fixtures are generated privately from validated ROMs. No ROMs,
images, captures or firmware listings belong in Git. Private evidence is in
`build/validation/scheduler-20261005/wcs-padding-ctest.log` and `wcs-update.csv`.
The shipped control-clock/sound/CPU results remain in the
[native scan report](NATIVE_CONTROL_SCAN_VALIDATION.md).
