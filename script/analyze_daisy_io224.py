#!/usr/bin/env python3
"""Summarize the Daisy vs Cineol-X boundary check (numpy + matplotlib).

Usage: python3 script/analyze_daisy_io224.py build/validation/daisy-io-20261011
The C++ check supplies all metrics; this script plots gain and inspects the
two SAME-Hall listening pairs, including their input-silent intervals.
"""
import csv
import json
import math
import statistics
import struct
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

directory = Path(sys.argv[1])
boundaries = list(csv.DictReader((directory/'boundaries.csv').open()))
cpu = list(csv.DictReader((directory/'cpu.csv').open()))
summary = {'boundary': {}, 'cpu': [], 'listening_pairs': []}

fig, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
for boundary, ax in zip(('input', 'output'), axes):
    rows = [r for r in boundaries if r['boundary'] == boundary and int(r['frequency']) > 0
            and int(r['frequency']) <= 10000 and r['network'] == '0' and r['channel'] == '0']
    ax.plot([int(r['frequency']) for r in rows], [float(r['old_gain_error_db']) for r in rows],
            'o-', label='Previous Daisy')
    ax.plot([int(r['frequency']) for r in rows], [float(r['new_gain_error_db']) for r in rows],
            'o-', label='Compact Daisy')
    ax.set_ylabel(f'{boundary.capitalize()} gain error (dB)')
    ax.grid(alpha=.3)
    ax.legend()
    all_rows = [r for r in boundaries if r['boundary'] == boundary]
    passband = [r for r in all_rows if 0 < int(r['frequency']) <= 9600]
    summary['boundary'][boundary] = {
        'max_peak_error': max(float(r['peak_error']) for r in all_rows),
        'max_gain_error_db_40_to_9600': max(abs(float(r['new_gain_error_db'])) for r in passband),
        'worst_relative_error_db_40_to_9600': max(float(r['new_relative_error_db']) for r in passband),
        'tested_values': sum(int(r['count']) for r in all_rows),
        'adc_word_differences': sum(int(r['adc_word_differences']) for r in all_rows),
        'detector_differences': sum(int(r['detector_differences']) for r in all_rows),
    }
axes[-1].set_xlabel('Frequency (Hz)')
fig.suptitle('Original 224 boundaries relative to current Cineol-X\nIdentical input/converter streams; algorithms excluded')
fig.tight_layout()
fig.savefig(directory/'frequency-response.png', dpi=170)
plt.close(fig)

for program in range(6):
    for mode in range(3):
        timings = {v: statistics.median(float(r['seconds']) for r in cpu
                   if int(r['program']) == program and int(r['mode']) == mode and int(r['variant']) == v)
                   for v in range(3)}
        summary['cpu'].append({'program': program+1, 'mode': mode,
                              'new_over_old': timings[1]/timings[0],
                              'new_over_desktop_boundaries': timings[1]/timings[2]})


def read_float_wav(path):
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:12] == b'WAVE'
    offset = 12
    while offset+8 <= len(data):
        tag, size = data[offset:offset+4], struct.unpack_from('<I', data, offset+4)[0]
        if tag == b'data':
            return np.frombuffer(data, dtype='<f4', offset=offset+8, count=size//4).reshape(-1, 2).astype(float)
        offset += 8+size+(size % 2)
    raise ValueError('missing WAV data')


def db(value):
    return 10*math.log10(max(float(value), 1e-30))


fig, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
with (directory/'tail-windows.csv').open('w') as out:
    writer = csv.writer(out)
    writer.writerow(['program', 'start_s', 'end_s', 'cineol_rms_dbfs', 'daisy_rms_dbfs', 'gain_delta_db'])
    for program, ax in zip((3, 5), axes):
        ref = read_float_wav(directory/f'program-{program}-cineol-boundaries.wav')
        emb = read_float_wav(directory/f'program-{program}-daisy.wav')
        row = {'program': program, 'overall_gain_delta_db': db(np.mean(emb*emb)/np.mean(ref*ref)),
               'relative_error_db': db(np.mean((emb-ref)**2)/np.mean(ref*ref)),
               'bands': []}
        # Band power uses the same complete input-silent 1..6 s window, no
        # alignment/normalization, and the unchanged identical Hall on both sides.
        rt = np.fft.rfft(ref[48000:], axis=0)
        et = np.fft.rfft(emb[48000:], axis=0)
        frequencies = np.fft.rfftfreq(len(ref)-48000, 1/48000)
        for lo, hi in ((20, 250), (250, 1000), (1000, 4000), (4000, 8000), (8000, 10000)):
            mask = (frequencies >= lo) & (frequencies < hi)
            row['bands'].append({'low_hz': lo, 'high_hz': hi,
                                 'tail_gain_delta_db': db(np.sum(abs(et[mask])**2)/np.sum(abs(rt[mask])**2))})
        time, rd, ed = [], [], []
        for begin in range(0, len(ref), 12000):
            end = min(begin+12000, len(ref))
            rpower, epower = np.mean(ref[begin:end]**2), np.mean(emb[begin:end]**2)
            writer.writerow([program, begin/48000, end/48000, db(rpower), db(epower), db(epower/rpower) if rpower else 0])
            time.append((begin+end)/96000);rd.append(db(rpower));ed.append(db(epower))
        row['final_half_second_cineol_dbfs'] = db(np.mean(ref[-24000:]**2))
        row['final_half_second_daisy_dbfs'] = db(np.mean(emb[-24000:]**2))
        summary['listening_pairs'].append(row)
        ax.plot(time, rd, label='Cineol-X boundaries / same Hall')
        ax.plot(time, ed, '--', label='Compact Daisy / same Hall')
        ax.axvline(1, color='gray', linestyle=':', label='Input becomes silent')
        ax.set_ylabel(f'Program {program}: RMS dBFS')
        ax.grid(alpha=.3);ax.legend()
axes[-1].set_xlabel('Time (s)')
fig.suptitle('Narrow same-algorithm check: response and input-silent tail\n250 ms RMS windows; no gain normalization')
fig.tight_layout();fig.savefig(directory/'tail-envelope.png', dpi=170);plt.close(fig)
(directory/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps(summary, indent=2))
