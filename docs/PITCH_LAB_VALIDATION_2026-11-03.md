# Cineol Pitch Lab reference bench — 2026-11-03

## Scope

Implemented a separate AU/VST3/standalone laboratory using Signalsmith
Stretch 1.3.2, external filters/sends/feedback and one complete original
224 Large Hall B engine. The vendor core is replaceable through `PitchEngine`.
No independently developed proprietary pitch algorithm is present yet.
Existing plugin identifiers, main plugin sources, Hall algorithms and
dependency checkouts are unchanged. Only two optional subdirectory entries
were added to the root build. No ROM, bank, capture or binary is added to
source control. This is not a release or subjective sound acceptance.

Signalsmith Stretch pin: `88061fd4eda56a2353c5d65c74aa3f4ccf76f9da`.
Signalsmith Linear pin: `7bb1c8f36794d6980080ce817c91f8da8fe56a66`.
Both are MIT; their complete notices are included in built bundles. See the
[upstream reference](https://github.com/Signalsmith-Audio/signalsmith-stretch)
and the isolated adapter `native-hall/pitch-lab/signalsmith_reference.cpp`.

## Executed checks

Build: Release arm64, cached JUCE 8.0.14, Apple Accelerate, CMake. AU, VST3,
standalone and both check executables built and signed. Installed AU/VST3
binary hashes match the final build. System `auval -v aufx CpLB Rflx` passed
on the final installed AU. Hashes of the existing installed Cineol-X AU and
VST3 before/after are identical.

- Core: all nine window/overlap configurations, including first processing,
  formants, finite guards, six routes, resets and stopped-input intervals.
  Monitor selections produced identical Hall and return power histories.
- Processor: full parameter save/load, mono versus duplicated stereo,
  44.1/48/96 kHz and blocks 1/17/127/128/511/2048. Host partition error was
  **zero** at every tested rate. Input monitor impulse was sample-exact at
  the reported 10374-frame latency at 48 kHz.
- Instrumented C++ new/delete counters observed **zero allocations and zero
  releases** during tested processing, parameter changes and Panic. This
  counter does not claim to intercept every platform-library allocator.
- Processor checks also passed with a deliberately missing Hall bank; pitch
  remained operational. The editor correctly reported Hall unavailable.
- Final editor snapshot rendered and inspected. All host parameters appeared
  in REAPER with names, formatted values and stable plugin parameter IDs.

Evidence: `build/validation/pitch-lab-20261103/{core.log,plugin.log,
plugin-no-bank.log,auval-final.log,editor.png,summary.json}`. These generated
artifacts remain local and outside source control.

## Actual REAPER measurements

REAPER 7.40/macOS-arm64 loaded **AU: Cineol Pitch Lab (NK Stuff)** in a new
fourth project tab. The three pre-existing project tabs were not modified.
The dedicated project is `build/pitch-lab/reaper-bench/Cineol-Pitch-Lab-Bench.RPP`.
The file bridge operates through the
[official ReaScript API](https://www.reaper.fm/sdk/reascript/reascripthelp.html).
Status, parameter readback, fixture replacement, reset, render and save were
exercised. Restart was tested after fixing stale-request replay. The bridge
reports the master mute through `GetMasterMuteSoloFlags`; it leaves the owned
project stopped and muted. WAV float32, stereo 48 kHz, eight-second bounds,
no normalization/dither or added render tail. REAPER's PDC was active.

After a targeted new-plugin scan, the final VST3 was also loaded. The owned
test chain was reduced to one VST3 instance; a repeated +12-st sine render
matched the earlier AU sample for sample (maximum difference zero). The
saved audition project now uses VST3. The final resizable editor was visually
verified in REAPER with all controls, mixer and instruments visible. This
also avoids requiring a restart of REAPER's unrelated unsaved projects to
refresh its previously loaded AU library. Evidence: `vst3-final.json`.

The sine fixture has L=440 Hz at 0.08 peak and R=660 Hz at 0.06 peak, active
0.5–2.5 seconds with 10 ms fades. The pitch input/output are unity, master
is -6 dB, filters and feedback are bypassed unless specified. Peaks below
include the master attenuation. Frequencies use a Hann analysis over
0.8–2.2 seconds with zero padding; extra decimal places are diagnostic
interpolation, not a guarantee of pitch accuracy.

| Case | Measured L / R | Result |
|---|---|---|
| Pitch +12 st | 881.20 / 1321.20 Hz | +2.35 / +1.57 cents relative to 880/1320 Hz |
| Pitch -12 st | 218.72 / 330.69 Hz | -10.11 / +3.61 cents relative to 220/330 Hz |
| Pitch 0 st | 440.00 / 660.00 Hz | Within 0.02 cents at this analysis resolution |
| Post filter, +12 st, LP=1000 Hz, 24 dB/oct | -10.12 / -17.63 dB versus bypass | External filter responds independently |
| Input monitor, impulse at frame 24000 | Peak at frame 24000 in rendered file | Host compensates the reported delay |

The -12-st frequency offset also reproduced with a standalone direct vendor
instance configured identically, without the plugin, rate bridge, filters or
Hall: **218.72 / 330.69 Hz**. It is a reference-core characteristic for this
fixture/configuration, not evidence that the wrapper adds detuning. The first
strict 1-Hz frequency assertion exposed this. The bounded campaign now records
cents explicitly with a 15-cent reference sanity bound; it does not assert
ideal tuning. Direct reproduction source and numeric result are retained in
`build/validation/pitch-lab-20261103/direct_reference.cpp` and
`direct-reference.json`.

A deterministic stereo noise burst at 0.5–1.0 seconds exercised Pitch into
Hall and Shimmer Loop with **Original precision**, stock decay settings and
15% filtered Hall return. Both were finite and declined into an approximately
-100 dBFS residual region. Last-two-second AC RMS was -100.76 dBFS and
-100.10 dBFS respectively. These are observed residuals, not a measured RT60
or proof of stability at arbitrary settings. The original quantized tank is
retained. Percussive pulses at 0.5/1.5/2.5 seconds exercised Pitch Only and
Pitch Delay (250 ms external delay, 15% return). Both reached exact zero in
the final two seconds. All measured output peaks remained below unity.

The PDC-compensated pitched onset shows some precursor energy before the
input onset. The reference is not accepted as artifact-free on transients;
listen to the preserved percussive pair. The plots deliberately show raw
levels including the -6 dB master, not a level-matched listening verdict.

Measured renders took approximately 0.83–0.90 seconds per eight seconds,
including bridge polling and analysis. REAPER reported 16.8x realtime for the
Pitch Delay render. This is offline throughput, not a guarantee of realtime
deadline margin or complete process CPU measurement. No CPU optimization or
hardware/Daisy readiness is claimed.

Full readbacks, render statistics, hashes and envelopes are retained in
`build/validation/pitch-lab-20261103/ref-v2-reaper.json`, `up12.json` and
`bridge-restart.json`. Current WAVs are under the bench's `renders` folder;
input fixtures remain beside the project. `measurements.png` contains tail
and first-transient plots. Cleanup decisions are recorded in `cleanup.json`;
active measurements, listening files, logs, provenance and sources remain.

## Listening and subsequent development

Start with **Pitch Only**, then **Pitch into Hall**, then **Shimmer Loop**.
Change Monitor to audition internal nodes without resetting the return.
Compare 2048/4096/8192 windows at fixed host latency. Smaller windows trade
frequency resolution for a shorter internal pitch response. The current
216.125 ms host padding is a laboratory choice, not a proposed production
latency target. Loop-delay automation still uses an integer tap and can
introduce artifacts.

The user owns listening acceptance. The next independent proprietary engine
should be evaluated against the same fixture/readback pipeline for pitch
error, transient precursor/smearing, stereo coherence, repeated-return
spectral buildup, stopped-input decay, latency and CPU. It should be authored
as a new implementation rather than renaming the third-party core.
