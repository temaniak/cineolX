# XL coupled scan CPU checkpoint

October 6, 2026. The user requested CPU measurements now, before further panel
and startup work. This is an early checkpoint of the private Mod-off scan
prototype, not final production/plugin or sound acceptance.

The ordinary source is the actual frozen dynamic-envelope baseline, verified
against its manifest and still byte-identical to the five current production
control/runtime files. The candidate is the retained auxiliary/secondary-MID
corrected private scan used by the split sound comparison. The later rejected
parameter-compiler-state experiment is not the CPU candidate. Unused public
panel helpers do not enter either measured processing path.

## Method and scope

Two independently compiled Release executables use Apple Clang 21, arm64,
`-O3 -DNDEBUG -std=c++20 -ffp-contract=off`, identical FZ/FTZ and the same v5
bank, SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
Repository HEAD is `c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; Reflexion remains
clean at `f68ea1d069fef4a5663201693bfdfa1c579ffd69`.

Five alternating ordinary/candidate rounds cover 22 algorithms and six
workloads: **1,320 rows, 132 matched workload/program groups**. Processing is
48 kHz in 128-frame blocks, with one second of untimed warmup and two seconds
of timed deterministic bursts/tail. Program and workload order reverse on odd
rounds. Preparation, input generation, CSV writing and quantile sorting are
untimed. No task builds, renders or oracles run during sampling. Checksums and
peaks are exactly repeatable within each version/group; expected cross-version
changes reflect the candidate's different scan/coefficient chronology.

The host is MacBookPro18,3, eight physical CPUs, 16 GiB memory, macOS 27.2.
External activity is uncontrolled: the initial snapshot includes RustDesk,
WindowServer, Codex and coreaudiod. Both thread CPU time and elapsed wall time
are retained. Percentages below are median thread CPU seconds divided by
rendered audio seconds, multiplied by 100; they are an offline audio-budget
measure, not the macOS total-machine utilization percentage.

## Results

| Mod-off workload | Ordinary CPU range across programs | Candidate CPU range | Relative candidate cost |
| --- | ---: | ---: | ---: |
| Factory, Dynamic/Optimization off | 1.51–2.33% | 1.51–2.35% | +1.17% |
| Dynamic, factory controls | 1.51–2.34% | 1.51–2.35% | +1.33% |
| Dynamic + Optimization, gate controls | 1.51–2.33% | 1.51–2.35% | +1.22% |
| Two identical Dynamic gate networks | 3.00–4.67% | 2.99–4.73% | +1.32% |
| Dynamic gate network + Resonant Chords tail | 2.99–3.91% | 2.97–3.96% | +0.70% |
| Dynamic gate, LF update each block | 1.51–2.34% | 1.51–2.38% | +1.12% |

The sum of matched group medians gives a candidate/ordinary ratio of
**1.011244 for thread CPU** and **1.011289 for wall time**. This is about 1.12%
relative extra cost, not 1.12 percentage points of the audio budget. All cases
have finite bounded output and **zero processing/control allocations/releases**.
The largest candidate median single-network cost is 2.3752%; the largest
two-identical-network cost is 4.7268%, for Rich Chamber.

Requested heap storage for two prepared Runtime objects increases from
**6,457,216 to 6,628,688 bytes**, an additional **171,472 bytes (167.45 KiB,
2.66%)**. The bank payload remains 5,828,152 bytes. Shared converter tables,
the bank allocation and the private global trace buffer/counters are separate;
trace recording is disabled during timing. These figures are requested storage,
not process RSS or a proposed final shipping memory layout.

## Block timing and limits

Across programs, the largest median per-run p99 is 90.875 microseconds for a
single candidate network and 165.417 microseconds for two identical networks.
The nominal 128/48k block deadline is 2,666.667 microseconds. Nevertheless,
retained elapsed-time maxima reach **38.462 ms for the candidate and 35.916 ms
for the ordinary implementation**. Both exhibit large individual wall-time
outliers on this active host. Their causes were not isolated with a scheduling
trace; they cannot be discarded or claimed to be proven DSP-only peaks.
Low mean/median cost does not establish hard realtime or DAW underrun margin.

The overlap workloads run two native networks and omit plugin host-rate
converters, retirement/fades and host scheduling. The parameter workload
measures the prototype's actual atomic update/reset path, whose physical
transaction chronology is still unverified. Mod-on, complete text/panel startup,
legal IRQ entry, parameter transactions during compilation, full plugin/host
acceptance and Daisy margin remain open. CPU does not accept the unresolved
Hall / Hall envelope mismatch.

After this explicitly requested measurement, panel/startup work resumes. CPU
remains batched: the next campaign belongs to a changed assembled candidate or
an observed performance problem, not each unused helper edit.

## Evidence

Private source, binaries, compile commands, per-run CSV/logs, source-tree hashes,
host identity and alternating recipes are under
`build/validation/xl-coupled-cpu-20261006/`. `provenance.json`, `summary.json` and
`groups.csv` retain all ranges, checksums, peaks, block outliers and workload
limits. Reproduction uses the recorded `compile-commands.json` followed by
`python3 build/validation/xl-coupled-cpu-20261006/run_cpu.py` after all builds and
functional checks have stopped. ROM execution, plugin installation and normal
user cache replacement are unnecessary for this prepared-bank benchmark.
