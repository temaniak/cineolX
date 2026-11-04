#!/usr/bin/env python3
"""Summarize the private LexIR listening manifest and plot measured decay evidence."""
import csv
import json
import os
import subprocess
from pathlib import Path

import numpy as np


def main():
    root = Path(__file__).resolve().parents[1]
    work = root / 'build/validation/lexirs-20261104'
    listen = work / 'listening'
    data = json.loads((listen / 'manifest.json').read_text())
    rows = data['cases']; assert len(rows) == 71
    metrics = []; table = []
    for row in rows:
        name = row['name']; match = row['matched']; display = row['display']
        for side in row['impulse_validation']:
            for a, b in zip(side['reference']['bands'], side['native']['bands']):
                ordinary = not name.startswith('Inverse') and name not in ['Rich Plate V7', 'Rich Chamber V7']
                accepted = ordinary and a['accepted'] and b['accepted']
                metrics.append(dict(name=name, input=side['input'], low=a['low'], high=a['high'],
                                    reference_T20_s=a['T20_s'], native_T20_s=b['T20_s'],
                                    paired_T20_accepted=accepted,
                                    T20_delta_percent=100 * (b['T20_s'] / a['T20_s'] - 1) if accepted else None,
                                    band_balance_delta_db=b['energy_db'] - a['energy_db'],
                                    reference_r2=a['r2'], native_r2=b['r2']))
        cells = {c['cell']: display[pi][si] for pi, page in enumerate(json.loads((work / 'catalog.json').read_text())[match['program']]['pages'])
                 for si, c in enumerate(page) if c['cell'] < 48}
        table.append(dict(name=name, closest_factory_variation=match['identified_factory_variation'],
                          LF=cells[0], MID=cells[1], crossover=cells[2], treble=cells[3], depth_attack=cells[4],
                          predelay=cells[5], bandwidth=cells[9], diffusion=cells[10], size=cells.get(43, ''),
                          mod=bool(match['mode'] & 1), optimization=bool(match['mode'] & 2), dynamic=bool(match['mode'] & 4),
                          fit_diagnostic=match['score'], music_diagnostic=row['music_score']))
    for path, records in [(work / 'paired-metrics.csv', metrics), (listen / 'settings-summary.csv', table)]:
        with path.open('w') as f:
            w = csv.DictWriter(f, records[0]); w.writeheader(); w.writerows(records)
    accepted = [abs(m['T20_delta_percent']) for m in metrics if m['paired_T20_accepted']]
    summary = dict(sets=len(rows), family_count=14, band_input_cases=len(metrics), paired_T20_cases=len(accepted),
                   T20_absolute_delta_percent_median=float(np.median(accepted)),
                   T20_absolute_delta_percent_p90=float(np.percentile(accepted, 90)),
                   T20_absolute_delta_percent_max=float(np.max(accepted)),
                   band_balance_absolute_delta_db_median=float(np.median([abs(m['band_balance_delta_db']) for m in metrics])),
                   fit_diagnostic_median=float(np.median([r['matched']['score'] for r in rows])),
                   unresolved=[r['name'] for r in rows if r['matched']['score'] > 1],
                   listening_performed=False)
    (work / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    lines = ['# Lexicon 224XL / Cineol listening comparisons', '',
             'Open `index.html` for all 71 sets, or play the V1 `A-then-B.wav` files. A is the archived IR; B is the running Cineol algorithm. Both use the same stereo percussion and chords. Input stops at 4.5 s. A/B files contain 14 s of A, 1 s silence, then 14 s of B.', '',
             'Each folder contains a separate `Matched.cineol224` preset. Load it using Cineol\'s preset Load command; an existing XL v8.21 import is required. Presets keep Clean off, Dirt/Space zero and A/C outputs. The output gains used for listening are external scalar loudness matching, recorded in the JSON; they are not a change to the input gain in the preset.', '',
             '**Decay Opt is disabled in every Cineol render and preset by user request after listening review.** All other fitted faders and Mod/Dynamic switch settings are preserved. This removes the rejected Opt behavior without changing the plugin algorithm or its global defaults.', '',
             'The 14 V1 examples also include `C-Cineol-Clean.flac` and `Matched-Clean.cineol224`: identical faders and switches, with the existing Clean precision mode enabled and its active loudness matched independently. The HTML page exposes C next to A/B. Clean reduces the precision noise floor; it does not reconstruct captured modulation or correct unmatched envelope shapes.', '',
             'The four true-stereo convolution paths are summed correctly. Files are 48 kHz / 24 bit, wet only. One scalar per variant matches active-window stereo RMS; no EQ or independent L/R gain is applied.', '',
             '**Known limitations:** these are 224XL captures, not original-224 evidence. Hardware firmware and capture levels are unknown. Static IRs lose time-varying chorus/dynamics and some near-infinite behavior. Bright Hall V1 matches factory V4 much better than factory V1. Some other variation numbers are ambiguous because they differ only in stop-decay behavior. Small Room V3, Inverse Room V1/V2, Rich Chamber/Plate V7 and Dark Hall V5 require particular attention. No listening acceptance has been claimed.', '',
             '## Candidate settings', '',
             'The score is a numerical fitting diagnostic, not a percentage of sound similarity. Full fader values are in `settings.csv`; this table is a compact summary.', '',
             '| Capture | Closest factory V | LF | Mid | Treble | Bandwidth | Size | Mod / Opt / Dyn |',
             '|---|---:|---:|---:|---:|---:|---:|---|']
    for t in table:
        switches = ' / '.join('on' if t[k] else 'off' for k in ['mod', 'optimization', 'dynamic'])
        lines.append(f'| {t["name"]} | {t["closest_factory_variation"]} | {t["LF"]} | {t["MID"]} | {t["treble"]} | {t["bandwidth"]} | {t["size"]} | {switches} |')
    (listen / 'README.md').write_text('\n'.join(lines) + '\n')
    os.environ['MPLCONFIGDIR'] = str(work / 'matplotlib-cache')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    chosen = [r for r in rows if r['name'].endswith(' V1')]
    fig, axes = plt.subplots(4, 4, figsize=(16, 12), constrained_layout=True)
    for ax, row in zip(axes.flat, chosen):
        side = row['impulse_validation'][0]
        a, b = side['reference']['bands'], side['native']['bands']
        center = [np.sqrt(v['low'] * v['high']) for v in a]
        ax.semilogx(center, [v['energy_db'] for v in a], 'o-', color='#2769a8', label='Archived IR')
        ax.semilogx(center, [v['energy_db'] for v in b], 's--', color='#c55a25', label='Cineol')
        ax.set_title(row['name'], fontsize=10); ax.set_ylim(-25, 0); ax.set_xlim(100, 12000); ax.grid(alpha=.2)
        ax.set_xlabel('Band center (Hz)', fontsize=8); ax.set_ylabel('Relative band energy (dB)', fontsize=8)
    for ax in list(axes.flat)[len(chosen):]: ax.axis('off')
    axes.flat[0].legend(fontsize=8)
    fig.suptitle('Independent left-driven impulse validation · matched settings · relative band balance\nDifferent warmup from fitting; this measures archived IR agreement, not listening acceptance', fontsize=14)
    fig.savefig(work / 'band-balance.png', dpi=140); plt.close(fig)
    fig, axes = plt.subplots(4, 4, figsize=(16, 12), constrained_layout=True)
    for ax, row in zip(axes.flat, chosen):
        sources = [('Archived IR', 'A-Lexicon-IR.flac', '#2769a8'), ('Cineol', 'B-Cineol.flac', '#c55a25')]
        if (listen / row['name'] / 'C-Cineol-Clean.flac').exists(): sources.append(('Cineol Clean', 'C-Cineol-Clean.flac', '#26845b'))
        for label, file, color in sources:
            raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-nostdin', '-i', str(listen / row['name'] / file),
                                           '-f', 'f32le', '-c:a', 'pcm_f32le', 'pipe:1'])
            x = np.frombuffer(raw, '<f4').reshape(-1, 2)[216000:]
            # Non-overlapping 100 ms windows, after the identical input stop.
            power = np.mean(x[:len(x) // 4800 * 4800].astype(float).reshape(-1, 4800, 2) ** 2, axis=(1, 2))
            ax.plot(np.arange(len(power)) * .1 + .05, 10 * np.log10(np.maximum(power, 1e-12)), color=color, label=label)
        ax.set_title(row['name'], fontsize=10); ax.set_ylim(-120, -15); ax.set_xlim(0, 9.5); ax.grid(alpha=.2)
        ax.set_xlabel('Seconds after input stops', fontsize=8); ax.set_ylabel('Wet RMS (dBFS)', fontsize=8)
    for ax in list(axes.flat)[len(chosen):]: ax.axis('off')
    axes.flat[0].legend(fontsize=8)
    fig.suptitle('Input-silent tails of the actual listening files · 100 ms RMS windows\nOne active-window gain per variant; original precision noise remains; graph floor −120 dBFS', fontsize=14)
    fig.savefig(work / 'stopped-tails.png', dpi=140); plt.close(fig)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__': main()
