# Cineol Pitch Lab

Separate AU, VST3 and standalone test bench. The existing Cineol-X plugin,
Dual Hall prototype and Hall algorithm sources are unchanged. AU identity is
`aufx / CpLB / Rflx`, bundle prefix `net.nkstuff.cineolpitchlab`, and plugin
state root `CineolPitchLab`. This is an experiment, not a release.

## Reference pitch engine

Version one uses **Signalsmith Stretch 1.3.2** as a named reference. Only
`signalsmith_reference.cpp` includes its headers. `PitchEngine` in
`pitch_engine.hpp` defines preparation, configuration, processing, reset and
latency; routing, filters, Hall and measurement UI do not depend on vendor
types. A future independently developed proprietary engine can implement
this interface. Signalsmith code and its MIT notices remain attributed;
wrapping or modifying it does not establish a new independently authored
algorithm. No proprietary pitch core has been implemented in this version.

Controls: linked stereo or mono input; +/-24 semitones, +/-100 cents; glide;
2048/4096/8192 sample windows; 4x/8x/16x overlap; formant preservation and
independent formant shift. All nine configurations are prepared in advance;
only the selected one processes audio. Changing window or overlap clears
histories. No pitch detector, transient threshold or vendor internals are
exposed as invented controls.

## Signal flow and audition

Fresh input and the selected feedback return enter the pre-pitch filter,
pitch engine and post-pitch filter. Independent direct-input and pitch sends
feed one complete Large Hall B through an external Hall input filter/trim.
The Hall algorithm itself receives no pitch code or topology changes.
Original and Clean precision use the existing engines; only the selected
engine processes audio. Hall requires the existing original v4.4 bank cache;
Pitch Only works without a bank. The bench cannot import ROMs or write banks.

Routing presets: Pitch Only, Hall Only, Pitch into Hall, Parallel, Shimmer
Loop and Pitch Delay. Presets change routing while retaining the pitch and
filter settings in the editor. The command-line preset fixture loads the
complete documented default state for repeatable comparisons.

The return source is Off, filtered Pitch, or Hall Wet. It passes through the
loop filter and a causal 64-frame minimum delay plus 0–2000 ms user delay,
then re-enters the pitch input. Hall feedback produces repeated pitch shifts
without opening the Hall tank. Pitch Delay uses the same external loop with
Hall disabled. The loop delay uses a slewed integer tap in version one;
changing it during playback can introduce modulation artifacts. It is not
a finished interpolating delay effect.

Four bypassable HP/LP groups use smoothed, nonresonant 12/24 dB/oct cascades.
Output dry/pitch/Hall gains and master gain are independent of return gain.
Monitor selects Mix or seven internal nodes without changing routing or
clearing tails. A/B stores complete parameter snapshots, persisted with host
state; recalling them starts fresh histories. Panic clears histories and
clip counters. Routing/precision/Hall-enable changes also clear as indicated
in the editor.

Return gain is limited to 80%. The optional feedback guard latches the return
off after sustained high levels; Panic releases it. The optional output
ceiling bends peaks above 0.9. Both affect sound when active and can be
disabled. These controls do not prove unconditional loop stability for all
settings. Meter clips refer to individual node samples above unity, not DAW
xruns. Meter power is pooled across stereo; the spectrum is a 2048-point
Hann analysis of the selected monitor, computed off the audio thread.

## Latency and realtime behavior

The whole graph runs at a fixed 48 kHz boundary through the existing host-rate
bridge. A 64-frame processing quantum makes return timing independent of
host block size. All audition paths are padded to the maximum reference
configuration plus Hall rate-bridge delay. At 48 kHz the reported total is
**10374 frames / 216.125 ms**; the default 4096/8x pitch engine contributes
4608 frames / 96 ms. This laboratory intentionally keeps host latency fixed
when quality changes. REAPER compensates it in offline renders. A production
low-latency pitch effect needs a different latency policy.

`Align Hall Input Sends` delays the direct Hall send by the selected pitch
latency. Disable it to hear the deliberately staggered excitations. Hall
predelay remains part of the audible effect. Monitoring a loop node does not
make its recirculated history equivalent to a single feed-forward latency.

Audio processing, setters and Panic reset preallocated buffers. No audio
thread file access, locks, waits or preparation occur. Analyzer FIFO overflow
drops visualization samples. Callback percentage is the most recent callback
duration divided by its audio duration, not total process CPU or an xrun
guarantee.

## Build and checks

Run `bash script/build_pitch_lab.sh`. The option
`CINEOL_BUILD_PITCH_LAB` is OFF by default. Enabling it requires CMake 3.24+
and the pinned Signalsmith dependencies; existing builds retain their normal
paths and dependency pins. The script reuses cached JUCE and vendor sources
when available. Binaries are under
`build/pitch-lab/native-hall/pitch-lab/CineolPitchLab_artefacts/Release`.
No automatic installation overwrites the main plugin.

The editor starts at 1088 x 800 and can be resized; the full laboratory scales
uniformly so its mixer and instruments remain accessible in a DAW window.

The script also builds `cineol_pitch_lab_core_check`,
`cineol_pitch_lab_plugin_check`, and writes `build/pitch-lab/parameters.json`.
See `docs/PITCH_LAB_VALIDATION_2026-11-03.md` for results and limits.

## REAPER control

1. Generate deterministic test signals with
   `python3 script/pitch_lab_fixtures.py create` (requires NumPy).
2. Create an empty **new project tab**. In Actions, load and run
   `script/reaper_pitch_lab.lua`. It creates a dedicated project in
   `build/pitch-lab/reaper-bench`, never repurposes another project, and
   refuses commands unless this project is active. Its master is muted for
   instrumented playback; offline renders temporarily unmute the master.
3. In Render to File choose WAV, 32-bit FP and enable automatic closing of
   completed render windows. Save settings. The bridge sets 48 kHz stereo,
   bounds and disables normalization/dither for each measurement.
4. Use the local command interface, for example:

```sh
python3 script/pitch_lab_control.py status
python3 script/pitch_lab_control.py preset 0
python3 script/pitch_lab_control.py set semitones -12 --reset
python3 script/pitch_lab_control.py monitor 3
python3 script/pitch_lab_control.py render unique-pitch-test --seconds 8
python3 script/pitch_lab_control.py panic
python3 script/pitch_lab_control.py stop
```

`set` accepts raw values from the parameter manifest (feedback is 0–0.8, not
0–80). Render names must be unique. The bridge uses local files and official
ReaScript APIs on REAPER's main thread; it has no network listener or
arbitrary-code command. Only use one command client at a time. Restart ignores
the previous request. `close_bridge` stops the polling action. Start/stop
commands target only the owned project. For subjective listening, unmute
this project's master manually; `play` from the instrumentation client
always mutes it again. Audio quality remains for the user to judge.

`python3 script/pitch_lab_reaper_check.py --prefix unique-name` runs eight
bounded cases, saves full host parameter readbacks, hashes and numeric WAV
measurements. It leaves Pitch Only with the transient fixture, stopped and
muted. The default-fixture names are generated signals, not user recordings.
