# Dark Hall runaway investigation — November 2, 2026

## User report and scope

The user accepted the seven-program Clean A/B prototype's noise reduction after
listening. They also reported Dark Hall self-excitation in **both** Original and
Clean, approximately above LF Decay 7 s, Mid Decay 5 s and Treble Decay 1 kHz.
This investigation changes no production DSP or installed prototype. Extending
Clean to the remaining algorithms is deferred while stability is considered.

## First-hand physical-hardware evidence

Bob Hodas, *Recording Engineer/Producer*, October 1985, reviewed a physical
Lexicon 224XL in an operational assessment starting on printed page 186. On
printed page 190, in the Dark Halls paragraph, he reports a problem with V5:
“a low feedback that built slowly even after input was removed”. He likens the
result to a closely miked gong. Page 188 describes using the unit on actual
recording material. This is a contemporary first-hand hardware review, not an
emulation report; ownership and the exact firmware revision are not established.

- [Original magazine scan](https://www.worldradiohistory.com/Archive-All-Audio/Archive-Recording-Engineer/80s/Recording-1985-10.pdf)
- [Mirror of the same scan](https://manuals.plus/m/bfa11458fb4f8757f60ef7eaa7d7fef8b814397425a594bc758512201170b67b.pdf)

Lexicon's troubleshooting notes, printed page 7, independently list Dark Hall
Variation 5 runaway among **version 8.2** software bugs. This alone does not
establish whether version 8.21 corrected it.

- [Lexicon documentation scan, including troubleshooting notes](https://therackofages.com/Lexicon/224x%20digital%20reverborator/Lexicon-224X-owner-manual.pdf)

No independently verified forum-owner report with exact firmware and settings
was located in this search. The magazine review is the strongest first-hand
physical-unit evidence found.

## Independent original-firmware check

The existing `cineol_xl_sound_compare` harness boots the private original v8.21
ROM set and operates its LARC protocol, independently of the native DSP engine.
These are firmware-emulator measurements, **not physical hardware captures**.
No static WCS injection is used. The complete control records agree between
reference and native. Outputs are wet A/C, 48 kHz, analog converter path enabled,
one-second silent warmup, 0.2 seconds of independent stereo noise (seed 17,
peak 0.03) followed by digital input silence. Mod Enhancement, Decay Optimization
and Dynamic Decay are off in both reference fixtures.

Evidence root: `build/validation/dark-hall-runaway-20261102/` (private, ignored).

The finite-decay `reference-near` fixture sets raw control cells 0=192, 1=176,
3=208, displaying LF 7.5 s, Mid 5.7 s and Treble 9 kHz. Remaining values are
factory defaults; this is **not** a claimed complete V5 preset reproduction.
Per-second AC RMS subtracts each channel mean before pooling channel power:

| Interval after stimulus start | Original v8.21 reference | Native Original |
| --- | ---: | ---: |
| 1–2 s | -60.83 dBFS | -60.83 dBFS |
| 5–6 s | -68.43 dBFS | -68.44 dBFS |
| 10–11 s | -62.30 dBFS | -62.39 dBFS |
| 20–21 s | -35.53 dBFS | -35.67 dBFS |
| 30–31 s | -6.91 dBFS | -7.02 dBFS |
| 39–40 s | -5.57 dBFS | -5.56 dBFS |

The `reference-high` fixture changes only LF and Treble to raw 242 and also
shows runaway in both engines, reaching approximately -3.75 dBFS AC RMS.
Both fixtures retain input/reference/native WAVs, control/WCS/startup CSVs,
fixture provenance, logs and envelope JSON. All are active unique evidence;
no cleanup is needed for this block. Existing private ROMs/banks were reused,
not copied or modified. Native audio allocation/release counts remain zero.

Reproduction, using the existing local private inputs:

```sh
build/plugin/native-hall/cineol_xl_sound_compare \
  build/validation/xl-catalog-20261005/rom-private-local \
  build/validation/hidden-controls-fix-20261102/preview-cache/programs-v821-native-v5.bankxl \
  build/validation/dark-hall-runaway-20261102/reference-near \
  2 0 noise 17 .03 1000 45 256 1 \
  --control=0:192 --control=1:176 --control=3:208
```

## Clean stability qualification

Two metric-only native A/B screens (`screen.cpp/.csv`, `threshold.cpp/.csv`)
exercise finite and extreme settings for 40 seconds. High finite-decay cases
grow in both A and B, consistent with the firmware comparison. Mid raw 253/254
requests infinite decay and must not be interpreted as a finite-decay failure.

At LF 7.5 s, Mid 5.7 s, Treble 1.13 kHz, Mod/Opt off, A settles around -105 dBFS
while B grows to approximately -25 dBFS in the final second. With Mod/Opt on,
this recipe reaches numerical silence in A but approximately -46 dBFS in B.
Thus higher precision does **not** preserve the stability boundary everywhere.
Reduced truncation damping is a plausible explanation, not an isolated cause.
This qualifies the earlier factory-setting noise/character results; those
results are not invalidated, but they were not extreme-control stability tests.

## Decision

Dark Hall runaway is demonstrably part of the original hardware/software
history, and independently reproduced by original v8.21 firmware for the tested
finite settings. It is not evidence by itself of a native-only implementation
bug. This does not certify all settings or rule out additional local defects.

Preserve the fidelity path's behavior. A separately evaluated Clean correction
could stabilize the offending internal feedback/filter behavior, with explicit
before/after frequency-dependent decay and listening comparisons. Its exact
cause and smallest correction remain open. No gate, limiter, parameter clamp
or feedback correction has been added by this investigation.

## Earlier-firmware follow-up

At the user's request, full reference-only Reflexion tests were run with the
available earlier firmware. All original chips were SHA-256 checked against
the pinned Reflexion catalog before loading. The available relevant complete
sets are **224X v8.1**, **224XL v8.1A** and **224XL v8.21**. There is no separate
v8.2 set in this local collection. The earlier original-224 sets do not supply
this X/XL Dark Hall program and are not substitutes for this comparison.

This follow-up uses Reflexion's `Engine`, emulating the original host CPU, ARU,
converters and analog I/O, without a native Cineol graph or prepared bank.
The original 224X panel operator selects Dark Hall with CALL + PROG 1+3;
the XL LARC operator selects Bank 1 / Program 3. Both then load Variation 5.
Control edits use those operators, including their established soft-pickup
activation helper; no audio, WCS, feedback coefficient or controller state is
injected. The test does not replace the user's installed firmware/cache.

Each case starts from a cold machine, boots for 16 simulated seconds, loads V5,
reads the actual firmware's pages and controls, waits 0.5 seconds, then renders
one additional second of silent warmup. All cases use the same 0.2-second stereo
noise burst (seed 17, peak 0.03), followed by input silence, at 48 kHz with analog
I/O enabled. All four DAC outputs are inspected; A/C AC RMS is reported below.
The panel/LARC setup actions, firmware displays and complete control record are
retained independently for every case. Reading pages changes setup duration,
so these are not claims of equal controller phase across firmware versions.

### Factory V5

| Firmware | Factory LF / Mid / Treble | Observation interval | Last-second A/C AC RMS | Result |
| --- | --- | ---: | ---: | --- |
| 224X v8.1 | 7.5 s / 5.7 s / 9 kHz | 60 s | -98.50 dBFS | Decays to a residual floor; no runaway observed |
| 224XL v8.1A | 7.5 s / 5.7 s / 9 kHz | 60 s | -97.78 dBFS | Decays to a residual floor; no runaway observed |
| 224XL v8.21 | 7.5 s / 5.7 s / 6.3 kHz | 60 s | -5.29 dBFS | Falls below -91 dBFS, then grows into overload |

Both XL factory cases display Dynamic Decay off, Mode Enhancement on and
Decay Optimization on. The X case retains its untouched factory mode record;
its mode byte is also 192, but individual modes were not changed through that
panel or independently read as named toggle states.

### Additional finite-control cases

| Case | Mod / Opt | Interval | Last-second A/C AC RMS | Observation |
| --- | --- | ---: | ---: | --- |
| XL v8.1A, LF 7.5 s / Mid 5.7 s / Treble 9 kHz | Off / Off | 90 s | -94.68 dBFS | Residual floor; no runaway observed |
| XL v8.21, LF 7.5 s / Mid 5.7 s / Treble 9 kHz | Off / Off | 60 s | -5.56 dBFS | Runaway |
| X v8.1, LF 35 s / Mid 22 s / Treble 19 kHz | Factory | 90 s | -86.57 dBFS | Long decay into a residual floor; no runaway observed |
| XL v8.1A, LF 35 s / Mid 22 s / Treble 19 kHz | Factory On / On | 90 s | -85.22 dBFS | Long decay into a residual floor; no runaway observed |
| XL v8.21, earlier V5 tone settings | Off / Off | 60 s | -5.56 dBFS | Runaway remains |
| XL v8.21, earlier V5 tone settings | On / On | 90 s | -83.47 dBFS | Much slower low-level growth; no large runaway within this interval |

The last two cases also match the earlier V5's LF 7.5 s, Mid 5.7 s, Treble 9 kHz,
Crossover 720 Hz, Depth 33, Predelay 24 ms, Chorus 53, HF Bandwidth 6.3 kHz,
Diffusion 25, Definition 0, and 3-second stop decays. Preecho levels remain zero.
Unused preecho delays differ, and v8.21 has its own Size control, kept at its
factory 40 m. This is matching audible controls, not claiming identical graphs.
The off/off comparison shows that the observed version difference cannot be
explained solely by the newer factory HF Bandwidth, Treble or Chorus settings.
The on/on case is not accepted as stable: after reaching approximately -93 dBFS,
its quiet residual changes and rises slowly; a longer or multi-seed study would
be needed to characterize that boundary.

### Updated interpretation and reproducibility

The available earlier versions **do not reproduce the runaway in these tested
conditions**. The evidence supports a version-dependent change in Dark Hall
behavior, rather than an invariant property of every X/XL revision. It does not
prove global stability of v8.1/v8.1A for every control, level or startup phase.
The exact change responsible, and v8.2 itself, remain untested. The earlier
physical-hardware review and Lexicon v8.2 bug note remain consistent with this
distinction; neither establishes that v8.1 already had the same failure.

Private evidence is under `build/validation/dark-hall-firmware-20261102/`:
`probe.cpp`, `analyze.py`, `provenance.json`, `summary.json`, and nine per-case
directories with `actions.csv`, `fixture.csv`, `pages.csv`, `record.csv` and
`envelope.csv`. Factory cases additionally retain four-channel reference WAVs.
No ROM was copied or modified. The dependency checkout remains clean. The
three WAVs are distinct active firmware references; metrics-only follow-ups
avoid repeated WAV campaigns. No generated material was removed in this block.

The harness is compiled against the existing build-only pinned Reflexion export:

```sh
/usr/bin/c++ -O3 -DNDEBUG -std=c++20 -ffp-contract=off \
  -Ibuild/deps/reflexion \
  build/validation/dark-hall-firmware-20261102/probe.cpp \
  -o build/validation/dark-hall-firmware-20261102/probe

# SET_INDEX: 5 = X v8.1, 6 = XL v8.1A, 7 = XL v8.21.
# Arguments: ROM_DIRECTORY SET_INDEX OUTPUT VARIATION MODE RECIPE SECONDS SAVE_WAV.
# MODE: -1 retains factory; 0 disables all; 3 enables Mod+Opt, disables Dynamic.
# RECIPE: 0 factory; 1 = 7.5 s/5.7 s/9 kHz; 3 = 35 s/22 s/19 kHz;
# 4 = recipe 1 plus the earlier V5 tone settings, for v8.21 only.
build/validation/dark-hall-firmware-20261102/probe \
  '/private/path/to/224XL v8_1A' 6 \
  build/validation/dark-hall-firmware-20261102/repeat-xl81a-off \
  5 0 1 90 0
python3 build/validation/dark-hall-firmware-20261102/analyze.py
```

No production DSP, Clean behavior, supported native firmware, plugin identity or
installed build has changed. The early Dark Hall implementation is a useful
reference for a future targeted Clean stability investigation, not an approved
wholesale replacement of the v8.21 algorithm.

The subsequent [cause and feedback-control study](DARK_HALL_FEEDBACK_CONTROL_2026-11-02.md)
identifies a growing small-signal mode near 298.213 Hz, independently confirms
it with a full-ROM running-WCS feedback intervention, and verifies an isolated
continuous fixed-point feedback-gain prototype. No GUI or installed build is
changed by that follow-up.
