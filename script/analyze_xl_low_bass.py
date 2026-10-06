#!/usr/bin/env python3
"""Measure independent XL 20–60/60–100 Hz RMS levels; never normalize audio.

Usage: analyze_xl_low_bass.py OUTPUT_CSV CASE_DIRECTORY [...]
Complements the catalog bands for investigating bass/boominess. Requires NumPy.
No exponential-decay or listening acceptance is inferred from these levels.
"""
import csv
import sys
from pathlib import Path

import numpy as np
from analyze_sound import filtered, wav
from analyze_xl_sound import MEASURABLE_ENERGY


def analyze(output, directories):
    rows = []
    for directory in directories:
        with (directory / 'fixture.csv').open() as file:
            meta = next(csv.DictReader(file))
        if meta['schema'] != 'xl-independent-v1' or int(meta['rate']) != 48000:
            raise ValueError('Unsupported XL fixture: ' + str(directory))
        native, reference = (wav(directory / (v + '.wav')) for v in ('native', 'reference'))
        if native.shape != reference.shape or not all(np.isfinite(a).all() for a in (native, reference)):
            raise ValueError('Invalid paired audio: ' + str(directory))
        for low, high in ((20, 60), (60, 100)):
            actual, original = (filtered(a, low, high) for a in (native, reference))
            a, r = (float(np.sum(x ** 2)) for x in (actual, original))
            measurable = min(a, r) > MEASURABLE_ENERGY
            rows.append(dict(case=directory.name, directory=str(directory.resolve()),
                             program=int(meta['program']), mode=int(meta['mode']),
                             fixture=meta['fixture'], low_hz=low, high_hz=high,
                             native_energy=a, reference_energy=r,
                             rms_level_delta_percent=100 * (np.sqrt(a / r) - 1) if measurable else None,
                             status='measurement_only' if measurable else 'unmeasurable'))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('w', newline='') as file:
        writer = csv.DictWriter(file, rows[0].keys())
        writer.writeheader(); writer.writerows(rows)
    measured = [abs(r['rms_level_delta_percent']) for r in rows if r['rms_level_delta_percent'] is not None]
    print(f'{len(directories)} cases, {len(measured)} measurable low-bass bands; '
          f'maximum absolute RMS difference {max(measured, default=0):.6f}%')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    analyze(Path(sys.argv[1]), [Path(p) for p in sys.argv[2:]])
