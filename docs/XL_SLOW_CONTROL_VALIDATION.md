# XL slow controller stage validation

October 6, 2026. This continues the measured Dynamic Decay correction in
[XL_DYNAMIC_ENVELOPE_VALIDATION.md](XL_DYNAMIC_ENVELOPE_VALIDATION.md). The
ordinary native CD Plate A envelope still differs materially from the physical
reference. These components derive the work needed to couple its polls to the
native scan; they are not yet integrated in production `Native48`.

## Native stage laws

`SlowEntryClockXL` yields separate left/right headroom reads, combines the fast
software accumulator, performs level encoding and updates held level, average,
trigger flags and Optimization history. It preserves intermediate feedback
compilations and their returns. It ends at the gradual-transition entry.

`HeadroomDisplayClockXL` derives both channels' rise, peak hold and decay work,
the display divider, DIP sample and publication requests. Glyph values do not
select timing branches and no glyph ROM table is retained. A separate native
publication law derives interrupt-disabled request work from current panel
flags and receive mode. The enclosing scan must own subsequent serial service.
Display work matters because it postpones the next sound-control poll.

`SlowTailClockXL` handles parameter-reconciliation countdown, feedback period
and amount, display work, software headroom clearing, held-level release and
panel timers. Release uses the compiler's cached decay time. The stage ends
at the panel-service dispatch boundary, before the slow routine's actual return.
Nested compiler, display and configuration events remain explicit.

All three laws retain fixed storage, borrow their caller's state and contain
no ROM interpreter, allocation, I/O, lock or wait. Their work counts represent
scheduled computation, not blocking delays in audio processing. Production
DSP, filters, converter processing and bank layout are unchanged.

## Independent local checks

The reference boots the original hash-validated XL v8.21 firmware and physically
selects each program. Entry/tail checks exercise all four Dynamic/Optimization
combinations with Mod off. One second per variant uses alternating comparator
masks 15, 3, 0, 31 and 0, with different left/right stimulation in the second
half of each cycle. Display checks use two seconds per program with switches
off. These are controller component fixtures, not acoustic acceptance tests.

| Check | Physical cases | Calls | Relevant observations | Differences |
| --- | ---: | ---: | --- | ---: |
| Headroom display | 22 | 3,330 | 1,664 publication requests; 832 DIP reads | 0 |
| Slow entry | 88 | 5,995 | 11,990 headroom reads; 36 feedback compilations | 0 |
| Post-ramp slow stage | 88 | 6,005 | 6,005 display calls; 408 compilations; 754 DIP reads | 0 |
| Main compiler, unequal stop values | 19 | 2,359 | 12,466 coefficient writes | 0 |

State, event order and CPU work match at the checked boundaries. The compiler
check also verifies write payloads, instruction starts, READY grants and commit
times. Tracked native calls allocate/release nothing. Both direct optimized
builds and the CMake Release targets compile successfully.

The release check exposed an error in the shared division work helper:
each of its 16 bounded iterations was charged two extra states. The corrected
count removes **32 states (15.625 microseconds)** per division without changing
the quotient. An independently frozen pre-correction helper fails the retained
Concert Hall tail fixture at those boundaries; the corrected helper passes
the same recipe. This proves a component timing correction, not a measured
audio improvement. The preceding compiler campaign did not exercise that
division timing path. Additional physical Low/Mid stop values 8/56 now verify
the main compiler across all 19 reverbs, including unequal decay arithmetic,
signed writes and cache behavior; earlier equal-stop evidence is retained.

## Reproduction and scope

Baseline processing remains byte-identical to the frozen five production
control/runtime files from `xl-dynamic-envelope-20261006`. Repository HEAD is
`c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion remains the clean pinned
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. The unchanged v5 bank SHA-256 is
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Commands, current/frozen source and binary hashes, detailed logs and limits are
under ignored `build/validation/xl-headroom-display-20261006/checkpoint.json`.
ROMs, bank data and private listings stay outside Git.

```sh
cmake -S . -B build/xl-sound-validation
cmake --build build/xl-sound-validation --target cineol_xl_headroom_display_check cineol_xl_slow_entry_check cineol_xl_slow_tail_check cineol_xl_decay_compiler_check -j2
build/xl-sound-validation/native-hall/cineol_xl_headroom_display_check ROM_DIRECTORY
build/xl-sound-validation/native-hall/cineol_xl_slow_entry_check ROM_DIRECTORY
build/xl-sound-validation/native-hall/cineol_xl_slow_tail_check ROM_DIRECTORY
build/xl-sound-validation/native-hall/cineol_xl_decay_compiler_check ROM_DIRECTORY PREPARED_BANK --unequal-stops
```

Local entry memory and serial IRQ spans are observed oracle inputs. Entry/tail
checks also supply nested compiler/display/menu durations; display publication
work and the main compiler have their separate native checks. This is not yet
an independent free-running scan. Settled windows do not cover panel receive
mode 2, pending parameter reconciliation or changed DIP/menu configuration.
The panel-service dispatch after the tail boundary and serial chronology still
need their own native coupling. No source copies an observed poll rate or a
measured millisecond duration into these laws.

Next, assemble these stages with the validated gradual, compiler, Fast and
nine-pass monitor clocks, derive the remaining caller/serial work, then render
the retained CD Plate A gate recipe with identical input/reference WAVs. Keep
the previous 4.9177% band / 22.8037% envelope-p95 prototype result separate.
The ordinary 313-case matrix remains unchanged. CPU, full sound/plugin and
host/listening acceptance remain deferred until the assembled correction is
stable, under the user's agreed batch cadence.

The subsequent [coupled scan sound checkpoint](XL_COUPLED_SCAN_VALIDATION.md)
now connects these stages in a private independent pilot. It verifies the
settled native TX law and substantially improves the canonical CD Plate A
envelope. Initial scan/read phase and split-program retriggers remain open;
the coupled pilot is not yet production behavior. The earlier display event
labelled `DIP` is actually DSP arithmetic-monitor port 3; its observed-byte
and timing checks remain valid. The slow-tail DIP read is address FFFF.
