# Remaining XL musical examples and final desktop checks

October 6, 2026. Eight distinct algorithms already have positive user listening
feedback for selected examples. This block completes fourteen remaining musical
comparisons and runs the current desktop plugin regression/build plus one batched
CPU measurement. No DSP correction or private-controller-pilot integration occurs.
The new fourteen examples await listening; numerical coverage is not auditory
acceptance of all 22 programs or every mode/control setting.

## Independent musical coverage

Reuse the existing four-second peak-0.2 dual-mono riff, SHA-256
`7fcbbf61a8cfd1e789843e0e4c796ed94426db2133572a61a2576200e7b6029d`.
Only the unreviewed programs are newly rendered. Each capture is twelve seconds,
48 kHz, 256-frame blocks, analog on, 0 dB input gain, 1,000 ms warmup, wet-only,
factory controls and Mod/Dynamic/Decay Opt off. Input is exactly zero after four
seconds. Reference preparation uses physical operations and independent clocks;
no WCS, controller state or phase is injected into native.

Use A/C for the nine ordinary reverbs and **A/B for all five split programs**.
The plugin's split factory route selects one output from each child; A/C alone
would cover the left child twice. An unfinished Hall/Hall A/C attempt was stopped
before accepting the split block, then resumed with A/B. Its alternate-output
raw/preparation files are retained separately, with attempted argv and interruption
record; they are excluded from the fourteen-case metrics/listening. An interrupted
stdout log path was reused, so that original stdout is unavailable and not claimed
as retained. Unique alternate-output files remain protected; their identical input
shares canonical storage.

| Algorithm | Maximum primary band RMS deviation | Active-envelope p95 | Usable paired decay fits / 8 | Maximum valid decay deviation |
| --- | ---: | ---: | ---: | ---: |
| Bright Hall | 1.1193% | 1.3997% | 3 | 4.2636% |
| Dark Hall | 0.6428% | 1.3365% | 3 | 0.2316% |
| Rich Chamber | 0.2288% | 1.5852% | 3 | 0.0527% |
| Small Room | 0.3238% | 1.9999% | 0 | unavailable |
| Chamber | 0.1896% | 1.4825% | 2 | 0.1837% |
| Dark Chamber | 0.1424% | 1.5916% | 3 | 0.2262% |
| Small Plate | 0.6123% | 1.4904% | 1 | 0.0649% |
| CD Plate B | 16.9695% | 1.3143% | 1 | 0.0449% |
| Rich Plate | 0.2488% | 1.3613% | 3 | 0.1906% |
| Hall / Hall, A/B | 0.2200% | 2.2986% | 3 | 0.2958% |
| Plate / Plate, A/B | 0.6498% | 2.0619% | 2 | 0.0764% |
| Plate / Hall, A/B | 0.3481% | 1.7114% | 3 | 0.2416% |
| Plate / Chorus, A/B | 0.8774% | 2.3830% | 0 | inapplicable |
| Rich Split, A/B | 1.5272% | 1.3683% | 0 | inapplicable |

111 of 112 primary level bands are within the provisional 5% triage band.
CD Plate B's exception is 8–12 kHz, native −16.9695% / −1.6152 dB relative to
reference, at very quiet whole-capture RMS of −104.2240 / −102.6087 dBFS. This
does not represent a 17% overall sound mismatch. No EQ/floor suppression is
applied. All 27 usable paired decay fits are within 5%; the other 85 fits remain
unavailable or inapplicable, not passes. All fourteen outputs are finite and
report zero tracked native processing allocations/releases. Input sample identity
and active logical control audits pass. Short-window deviations remain diagnostics.

## Grouped listening

Five files under ignored `build/validation/xl-remaining-musical-20261006/listening/`
contain fourteen pairs, with no redundant individual paired WAVs. For every
algorithm: eight seconds reference, 0.5 seconds silence, eight seconds native.
One second separates algorithms, so consecutive pair starts are 17.5 seconds
apart. Every segment covers four seconds of riff and four seconds of post-input
response; full twelve-second raw recordings remain available.

| Group | Algorithm ordering and start times |
| --- | --- |
| Halls, 34 s | Bright Hall 0; Dark Hall 17.5 s |
| Rooms, 69 s | Rich Chamber 0; Small Room 17.5; Chamber 35; Dark Chamber 52.5 s |
| Plates, 51.5 s | Small Plate 0; CD Plate B 17.5; Rich Plate 35 s |
| Reverb splits, 51.5 s | Hall/Hall 0; Plate/Plate 17.5; Plate/Hall 35 s |
| Mixed splits, 34 s | Plate/Chorus 0; Rich Split 17.5 s |

Each full variant receives one scalar gain to match its input-active RMS to
−24 dBFS, reduced for common peak headroom. Saved segments are exactly float32
raw prefixes times the recorded scalar; peak bounds pass. No EQ, alignment,
denoising or output tail fade is added. Bounds, ordering, source and output hashes
are retained in `listening/manifest.json`. Human review is pending. Retain cases
the user finds close; pursue only material, repeatable differences rather than
continuing sample/envelope fitting for accepted examples.

## Desktop plugin checks and builds

Current Release `native_hall_plugin_check --banks ORIGINAL_BANK XL_BANK` passes
in its self-created temporary cache, covering all 28 original-224/XL programs:
selection, cross-engine presets/session restoration, preset audio/tail behavior,
618 active fader endpoints, mono/stereo, 44.1/48/96 kHz, 128/511/20,000-frame
block invariance, low latency/dry path, Dirt and Spillover/rapid retirement/disable.
The final log reports zero callback allocations/releases. Original-224 support
and parameter/plugin identities remain intact.

AU, VST3 and Standalone Release builds pass in `build/plugin/`. Each executable
contains arm64 and x86_64 slices; bundle identities, versions and hashes are in
`plugin-bundles.json`. `COPY_PLUGIN_AFTER_BUILD` is false. These are built bundles
and processor-regression checks; no new bundle is installed or validated inside
the user's DAW. System `auval` cannot target an arbitrary uninstalled bundle by
path, so an installed older plugin is not presented as validation of this build.

## Installed AU follow-up, October 6

At the user's request, the current universal AU is now installed at
`~/Library/Audio/Plug-Ins/Components/Cineol-X 224.component`. Its executable SHA-256
is `b792f7025690687ca53aadc5eaaa1858625b078eba35f8df1869c32358a229bc`, identical
to the built bundle; strict deep signature verification passes. The previous AU
is retained under `build/validation/au-install-20261006/previous/` for rollback.
The installed bundle includes both the updated original 224 and XL engines.

A fresh XL v5 cache was extracted from the same verified private ROM set and
matches the accepted bank SHA-256
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.
The original-224 and previous XL v4 user caches remain byte-identical.
System `/usr/bin/auval -v aufx Nh24 Rflx -strict` exits 0 with
`AU VALIDATION SUCCEEDED`, including render, channel, parameter and scheduling
checks. Installation, cache provenance and validation logs are retained in
`build/validation/au-install-20261006/`. The optional full `auval -al` inventory
was stopped after slow enumeration; it produced no registration-path result.
Targeted strict validation completed successfully before that inventory, and
installed binary/cache hashes were verified again afterwards.
A DAW listening session has not been
performed; restart an already running host to load the replacement bundle.

## Batched CPU checkpoint

Run the rebuilt Release native XL benchmark only after this task's builds,
renders, analyses and plugin checks finish. Five repeats of 22 programs × six
workloads give 660 rows at 48 kHz / 128-frame control intervals, each processing
one second of signal and one second of zero input. Preparation and one-second
warmup are untimed; flush-to-zero matches the desktop processing policy.

| Workload | Range of program medians, CPU budget | Largest individual observation |
| --- | ---: | ---: |
| Single engine, modes off | 1.4967–2.7098% | 5.2342% |
| Single engine, modes on | 1.5135–2.6310% | 2.8521% |
| Two copies of the same program | 3.0039–5.0343% | 6.5949% |
| Current program plus Resonant Chords tail | 3.0072–4.3424% | 12.4384% |
| Decay control updates every 128 frames | 1.5400–2.5412% | 2.6330% |
| Mod/Size updates every 128 frames | 1.5343–2.7491% | 2.8029% |

No processing/warmup/control-edge allocations or releases occur. Each program/
workload checksum is identical across its five repeats. Prepared two-Runtime
storage is 6,457,216 bytes, and in-memory bank size is 5,828,152 bytes (the bank
file also has its header). The benchmark is arm64; CPU-model metadata is unavailable
because the sandbox denies that `sysctl` read. Do not infer a specific host CPU.

Percentages are elapsed native processing time divided by simulated audio duration;
they are not the complete DAW/plugin meter or worst-callback latency. Overlap omits
plugin retirement fades and host-rate converters. Outliers are retained. This
single current-source checkpoint is not a before/after speedup or Daisy realtime
margin claim. No repeated benchmark is needed without a new change or regression.

## Provenance and cleanup

Private root: `build/validation/xl-remaining-musical-20261006/`. Retain exact
commands/exits, unchanged renderer and bank identities, source hashes, controls,
raw captures, metrics, playlist manifest, plugin build/test logs and bundle hashes,
660 CPU rows/summary, interruption record and checkpoint. HEAD is
`c6454a317b92b2a52d0cad17ebd3bdcd9f190d86`; clean Reflexion is
`f68ea1d069fef4a5663201693bfdfa1c579ffd69`. The v5 bank SHA remains
`3314cb99201ed91c430411514a197400421a8e5dec6964d45619b4fda7824238`.

Cleanup shares all byte-identical full inputs with the existing canonical fixture.
It retains current references, pending listening and uncertain unique alternate-
output evidence, and creates no normalized per-variant or individual-pair copies.
No ROM, bank, source asset or dependency resource is deleted. Next obtain actual
listening feedback on these fourteen cases; then preserve the accepted sound and
address only a confirmed difference or the user-requested installed-host workflow.
