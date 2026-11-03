#!/usr/bin/env python3
"""Summarize the bounded all-program precision comparison (requires NumPy)."""
import csv
import json
import sys
from pathlib import Path
import numpy as np

folder = Path(sys.argv[1])
rows = list(csv.DictReader((folder / 'catalog.csv').open()))
bands = [(40, 125), (125, 500), (500, 2000), (2000, 5000), (5000, 10000)]
spectra = []
for path in sorted(folder.glob('*.f32')):
    program, mode = map(int, path.stem.split('-'))
    data = np.fromfile(path, dtype='<f4').reshape(-1, 4).astype(float)
    fft = np.fft.rfft((data - data.mean(axis=0)) * np.hanning(len(data))[:, None], axis=0)
    frequency = np.fft.rfftfreq(len(data), 1/48000)
    power = np.abs(fft)**2
    ratios = []
    for low, high in bands:
        selected = power[(frequency >= low) & (frequency < high)].sum(axis=0)
        ratios.append(float(10*np.log10(max(selected[2:].sum(), 1e-30) / max(selected[:2].sum(), 1e-30))))
    original_rms = float(10*np.log10(max(np.var(data[:, :2], axis=0).mean(), 1e-36)))
    spectra.append(dict(program=program, mode=mode, original_active_dbfs=original_rms, band_delta_db=ratios))

tail = [r for r in rows if r['window'] == '2']
measurable = [r for r in tail if float(r['original_dbfs']) > -150]
changes = [float(r['original_dbfs']) - float(r['clean_dbfs']) for r in measurable]
summary = dict(
    cases=len(tail), tail_window_seconds=[28.5, 30.5],
    measurable_original_cases=len(measurable),
    original_floor_dbfs=[min(float(r['original_dbfs']) for r in measurable), max(float(r['original_dbfs']) for r in measurable)],
    clean_floor_dbfs=[min(float(r['clean_dbfs']) for r in measurable), max(float(r['clean_dbfs']) for r in measurable)],
    floor_reduction_db=dict(min=min(changes), median=float(np.median(changes)), max=max(changes)),
    active_window_seconds=[.25, .5], frequency_bands_hz=bands,
    max_abs_active_band_delta_db=max(abs(x) for s in spectra if s['original_active_dbfs'] > -70 for x in s['band_delta_db']),
    active_spectra=spectra,
    limitations='Finite factory-setting noise-burst fixtures, A/C raw ports, Mod/Opt off and on. Late RMS is residual signal plus noise, not an independent noise-floor measurement. Short-window spectral deltas are diagnostics; new programs still require listening review.'
)
(folder/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='active_spectra'}, indent=2))
