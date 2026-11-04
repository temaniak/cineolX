# LexIR comparisons rebuilt with Decay Opt disabled

Date: November 4, 2026. The user listened to the diagnostic variants, identified
enabled Decay Opt as the reported artifact source, and requested all comparisons
rebuilt with Opt off and the old generated files deleted locally and on the VPS.
This supersedes the pending-listening status of the earlier artifact report.

## Result

- All 71 Cineol comparisons and the 14 existing Clean alternatives were rendered
  again. All 85 downloadable presets have both `decay_opt=0` and the XL raw Opt
  bit clear. Other faders and the previously selected Mod/Dynamic bits are
  unchanged. The plugin DSP, global defaults, IDs and installed bundles are
  unchanged; this correction concerns the comparison renders and presets.
- All 71 true-stereo IR reference FLACs retain their exact prior SHA-256 hashes.
  The deterministic percussion/chord source, 3.67 s warmup, 57-sample native
  latency compensation, A/C routing, Dirt/Space zero and 14 s duration remain.
- All 156 FLACs decode to 672000 finite stereo frames at 48 kHz. Maximum absolute
  sample is 0.782046. The maximum active-window RMS difference within a case is
  approximately 0.00000051 dB. There is no EQ, limiter or per-channel gain.
  Fifteen sequential A-then-B WAVs were rebuilt and their 29 s durations checked.
- The existing processor harness loads all 85 presets, retains all 74 public
  parameter values and produces finite output at 48 kHz / 256 frames.
- Impulse band/decay metrics, stopped-input tail plots, listening metadata,
  settings tables and the local index were regenerated under the Opt-off policy.
  The report retains accepted/rejected decay fits rather than treating every
  tail as exponential. The user's diagnosis does not imply listening acceptance
  of every regenerated variation or complete agreement with the archived IRs.

The ordinary exporter now keeps Opt off when comparing musical mode candidates;
it cannot re-enable Opt merely to improve its envelope loss. The `opt-off`
command preserves the last published controls rather than fitting replacements:

```sh
python3 script/lexir_listening.py opt-off
```

`--resume` checks every completed case's hashes before reuse. When explicitly
authorized, `--delete-superseded` removes each replaced old generated case after
its new outputs and metadata are complete, recording paths in a cleanup log.
This was used after the volume ran out of space during staging; complete cases
were retained and verified on restart. No partial case was published.

## Publication and removal

The existing private collection URL is retained:
`https://45-128-204-208.sslip.io/listen/?collection=lexicon-224xl-2026-11-04`.
Its title and per-Cineol variant settings now explicitly say Opt is disabled.
The default example is Concert Hall V7. All 241 unique current media/preset
files were SHA-256 verified on the VPS before activation. Authenticated HTTPS
range requests pass for all 156 FLACs.

The old diagnostic collection `cineol-artifact-probe-2026-11-04` was removed and
returns 404. The library now lists only the current 71-case collection. On the
server, 112 superseded files totalling 93.67 MiB were removed; the active media
directory contains exactly the current manifest's files. Metadata audit history
is retained outside the listening library, under
`/home/tunnel/nk-listen/audits/opt-off-20261104/`.

Locally, 657 superseded generated files were removed, including old comparison
audio/presets, publication clones, combined WAVs and diagnostic float/FLAC/WAV
renders. The sum of their logical sizes is 637.19 MiB; APFS sharing means this
is not a claim of physical disk space recovered. Current identical reference
replacements, the new outputs and the reusable player remain. Original IR ZIPs,
ROMs, prepared banks, source tools, reports, metrics and reproduction recipes
were preserved. Historical diagnostic audio was deleted at the user's explicit
request; it can be regenerated from retained recipes and the source fixture.

## Reproduction evidence

Baseline Git revision and private bank/reference identity are unchanged from
[the artifact investigation](LEXIRS_ARTIFACT_DIAGNOSIS_2026-11-04.md).
Private evidence under `build/validation/lexirs-20261104/`:

- `opt-off-rerender-provenance.json`, `opt-off-rerender.log` and current
  `listening/manifest.json`, case settings/metrics and preset hashes.
- `verification-opt-off.json`, regenerated `paired-metrics.csv`, `summary.json`,
  `band-balance.png` and `stopped-tails.png`.
- `revisions/opt-on-metadata/`: prior recipes, measurements, manifest/file
  hashes and reports, without old audio copies.
- `opt-off-cleanup.json` and `opt-off-case-cleanup.ndjson`: exact local deletion
  paths and retained/rebuilt replacements. Staging replacement paths in the
  case log now reside at the final `listening/` path after directory activation.
- `server-prune-opt-off.py` and
  `server-opt-off-cleanup-verification.json`: bounded cleanup and HTTPS results.

Only generated outputs from these two named collections were removed. No
production DSP correction or CPU benchmark was needed for this render-policy
change. Further algorithm matching remains subject to listening evaluation.
