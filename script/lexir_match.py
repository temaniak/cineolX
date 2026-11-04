#!/usr/bin/env python3
"""Offline analysis of private Little Devil 224XL IRs and native parameter fitting.

Requires NumPy only. Source archives remain in place; generated audio stays in build/.
This estimates useful settings, not the unknown original capture configuration.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import io
import json
import math
import re
import subprocess
import time
import wave
import zipfile
from pathlib import Path

import numpy as np

FS = 48000
BANDS = [(100, 250), (250, 500), (500, 1000), (1000, 2000),
         (2000, 4000), (4000, 8000), (8000, 12000)]
NFFT, HOP = 2048, 480
WINDOW = np.hanning(NFFT)
FREQ = np.fft.rfftfreq(NFFT, 1 / FS)
MASKS = [(FREQ >= lo) & (FREQ < hi) for lo, hi in BANDS]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def save_json(path, data):
    path.write_text(json.dumps(data, indent=2, allow_nan=False) + '\n')


def pcm(data):
    with wave.open(io.BytesIO(data)) as w:
        assert w.getsampwidth() == 3 and w.getnchannels() == 2
        rate = w.getframerate()
        b = np.frombuffer(w.readframes(w.getnframes()), np.uint8).reshape(-1, 3)
    a = b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)
    return ((a ^ 0x800000) - 0x800000).reshape(-1, 2).astype(float) / 8388608, rate


def downsample(x, rate):
    assert rate == 96000
    # Linear phase windowed-sinc antialiasing, compensated group delay.
    n = np.arange(255) - 127
    h = .46 * np.sinc(.46 * n) * np.kaiser(255, 9)
    h /= h.sum()
    size = 1 << (len(x) + len(h) - 2).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, size, axis=0) * np.fft.rfft(h, size)[:, None], size, axis=0)
    return y[127:127 + len(x):2].astype(np.float32)


def features(x):
    x = np.pad(x, ((NFFT // 2, NFFT // 2), (0, 0)))
    frames = np.lib.stride_tricks.sliding_window_view(x, NFFT, axis=0)[::HOP]
    spectrum = np.abs(np.fft.rfft(frames * WINDOW, axis=-1)) ** 2
    power = np.stack([spectrum[:, :, m].sum(axis=(1, 2)) for m in MASKS], axis=1)
    # Three-frame smoothing reduces phase-specific ripple without erasing onset.
    power = (np.pad(power, ((1, 1), (0, 0)))[:-2] + power +
             np.pad(power, ((1, 1), (0, 0)))[2:]) / 3
    return power


def describe(power, x=None):
    broad = power.sum(axis=1)
    total = broad.sum()
    cumulative = np.cumsum(broad) / max(total, 1e-30)
    result = dict(energy_quantiles_s={str(q): float(np.searchsorted(cumulative, q) * HOP / FS)
                                    for q in [.01, .1, .5, .9, .99]}, bands=[])
    for i, (lo, hi) in enumerate(BANDS):
        p = power[:, i]
        # Noise estimate from late capture; do not fit the flat recording floor.
        floor = float(np.median(p[-100:]))
        edc = np.maximum(np.cumsum((p - floor)[::-1])[::-1], 0)
        db = 10 * np.log10(np.maximum(edc / max(edc[0], 1e-30), 1e-30))
        valid = (db <= -5) & (db >= -25) & (p > floor * 10)
        index = np.flatnonzero(valid)
        fit = dict(T20_s=None, r2=None, accepted=False, reason='insufficient decay range')
        if len(index) >= 8:
            t = index * HOP / FS
            slope, intercept = np.polyfit(t, db[index], 1)
            residual = db[index] - (slope * t + intercept)
            r2 = float(1 - np.sum(residual ** 2) / max(np.sum((db[index] - db[index].mean()) ** 2), 1e-30))
            margin = len(power) * HOP / FS - float(t[-1])
            accepted = bool(slope < 0 and r2 >= .95 and margin >= .3 and np.ptp(db[index]) >= 17)
            fit = dict(T20_s=float(-60 / slope) if slope < 0 else None, r2=r2,
                       accepted=accepted, reason='accepted' if accepted else 'non-exponential or insufficient margin',
                       fit_start_s=float(t[0]), fit_end_s=float(t[-1]), noise_power=floor)
        result['bands'].append(dict(low=lo, high=hi, energy_db=float(10 * np.log10(max(p.sum() / total, 1e-30))), **fit))
    if x is not None:
        result.update(peak=float(np.max(np.abs(x))),
                      lr_correlation=float(np.corrcoef(x.T)[0, 1]),
                      final_100ms_relative_db=float(10 * np.log10(max(np.mean(x[-4800:] ** 2) / max(np.max(broad) / (NFFT * WINDOW.dot(WINDOW)), 1e-30), 1e-30))))
    return result


def read_pair(entry):
    with zipfile.ZipFile(entry['archive']) as z:
        left, fs = pcm(z.read(entry['left']))
        right, fs2 = pcm(z.read(entry['right']))
    assert fs == fs2 == 96000
    return downsample(left, fs), downsample(right, fs)


def inventory(args):
    catalog = json.loads((args.output / 'catalog.json').read_text())
    names = {p['name']: p['program'] for p in catalog}
    entries = []
    arrays = {}
    for archive in sorted(args.source.glob('*.zip')):
        with zipfile.ZipFile(archive) as z:
            members = z.namelist()
            for left in sorted(n for n in members if n.endswith('.L.wav') and not n.startswith('__MACOSX')):
                right = left[:-6] + '.R.wav'
                assert right in members
                name = Path(left).name[:-6]
                group = name.rsplit(' V', 1)[0]
                a, rate = pcm(z.read(left)); b, rb = pcm(z.read(right))
                entry = dict(name=name, group=group, program=names[group], archive=str(archive), left=left, right=right,
                             source_rate=rate, source_channels_per_file=2, source_bits=24, duration_s=len(a) / rate,
                             left_sha256=sha(z.read(left)), right_sha256=sha(z.read(right)))
                assert rate == rb == 96000
                a, b = downsample(a, rate), downsample(b, rb)
                pa, pb = features(a), features(b)
                entry['left_metrics'] = describe(pa, a)
                entry['right_metrics'] = describe(pb, b)
                entry['matrix_energy_db'] = [float(10 * np.log10(max(np.sum(v ** 2), 1e-30))) for v in (a[:, 0], a[:, 1], b[:, 0], b[:, 1])]
                entry['input_pair_relative_difference'] = float(np.linalg.norm(a - b) / max(np.linalg.norm(a), 1e-30))
                arrays[name] = pa.astype(np.float32)
                entries.append(entry)
                print('inventory', name, flush=True)
    assert len(entries) == 71
    np.savez_compressed(args.output / 'target_features.npz', **arrays)
    save_json(args.output / 'inventory.json', entries)
    with (args.output / 'measurements.csv').open('w') as f:
        fields = ['name', 'program', 'input', 'low', 'high', 'energy_db', 'T20_s', 'r2', 'accepted', 'reason', 'fit_start_s', 'fit_end_s', 'noise_power']
        w = csv.DictWriter(f, fields, extrasaction='ignore'); w.writeheader()
        for e in entries:
            for side in ['left', 'right']:
                for b in e[side + '_metrics']['bands']:
                    w.writerow(dict(name=e['name'], program=e['program'], input=side, **b))


class Renderer:
    def __init__(self, args):
        self.process = subprocess.Popen([str(args.renderer), str(args.bank)], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.count = 0

    def render(self, p, raw, x, mode=0, warm=48000, route=(0, 2), stability=1):
        recipe = [p, len(x), warm, bool(mode & 1), bool(mode & 2), bool(mode & 4), *route, stability, *raw]
        self.process.stdin.write((' '.join(str(int(v)) if isinstance(v, (bool, int, np.integer)) else str(v) for v in recipe) + '\n').encode())
        self.process.stdin.write(np.asarray(x, dtype='<f4').tobytes()); self.process.stdin.flush()
        # Read exact bytes; buffered reads may still end early after a failed renderer.
        data = self.process.stdout.read(len(x) * 8)
        if len(data) != len(x) * 8:
            raise RuntimeError('Renderer stopped with truncated output')
        y = np.frombuffer(data, '<f4').reshape(-1, 2).copy()
        if not np.isfinite(y).all(): raise ValueError('Non-finite renderer output')
        self.count += 1
        # Compensate native declared host latency; no matching-specific shift or EQ.
        return np.pad(y[57:], ((0, 57), (0, 0)))

    def close(self):
        self.process.stdin.close(); self.process.wait()
        assert self.process.returncode == 0


def loss(target, candidate):
    n = min(len(target), len(candidate)); a, b = target[:n], candidate[:n]
    a = a / max(a.sum(), 1e-30); b = b / max(b.sum(), 1e-30)
    energy = np.sqrt(np.mean((10 * np.log10(np.maximum(b.sum(axis=0), 1e-20) / np.maximum(a.sum(axis=0), 1e-20))) ** 2))
    # Smoothed frequency-dependent decay/envelope, with 40 dB measurement range.
    ad = 10 * np.log10(np.maximum(a, 1e-20)); bd = 10 * np.log10(np.maximum(b, 1e-20))
    weights = np.maximum(0, 1 - (np.max(ad) - ad) / 40)
    delta = np.clip(bd - ad, -30, 30)
    envelope = np.sqrt(np.sum(weights * delta ** 2) / max(weights.sum(), 1e-30))
    # Match direct/early buildup separately; helps distinguish Size and Depth.
    aa, bb = a.sum(axis=1), b.sum(axis=1)
    ca, cb = np.cumsum(aa), np.cumsum(bb)
    quantile = np.mean([abs(np.searchsorted(ca, q) - np.searchsorted(cb, q)) * .01 for q in [.1, .5, .9, .99]])
    return float(.35 * energy + .65 * envelope + 5 * quantile)


def variation_seed(meta, name):
    path = Path(__file__).resolve().parents[1] / 'deps/reflexion/web-demo/page/catalogs/eb3a7a765a703ece.json'
    data = json.loads(path.read_text())
    program = next(p for p in data['programs'] if p['name'].lower() == meta['name'].lower())
    variation = name.rsplit(' V', 1)[1]
    raw = meta['factory'].copy()
    for page_index, page in enumerate(program['pages']):
        for slot, control in enumerate(page['sliders']):
            label = control['name'].strip()
            native = next((c for row in meta['pages'] for c in row if c['name'].strip() == label), None)
            if native is None or native['cell'] >= 48: continue
            cell = native['cell']
            if cell in [43, 44]:
                # Firmware's SIZE record is scratch, not the slider value.
                # Recover size from the captured display, preserving factory raw if equal.
                text = program['presets'][variation][page_index][slot]
                match = re.search(r'[\d.]+', text)
                if not match: continue
                meters = float(match[0])
                candidates = [v for v, label in enumerate(native['values'])
                              if re.search(r'[\d.]+', label) and float(re.search(r'[\d.]+', label)[0]) == meters]
                if candidates: raw[cell] = min(candidates, key=lambda v: abs(v - raw[cell]))
            else:
                raw[cell] = program['raw'][variation][page_index][slot]
    if any(c['cell'] == 8 and c['active'] for row in meta['pages'] for c in row): raw[8] = min(248, raw[8] // 8 * 8)
    raw[10] = min(252, raw[10] // 4 * 4)
    return raw


def fit_group(args, entries, catalog, targets):
    renderer = Renderer(args)
    start = time.time()
    previous = None
    for entry in entries:
        destination = args.output / 'fits' / (entry['name'] + '.json')
        if destination.exists():
            previous = json.loads(destination.read_text()); continue
        p = entry['program']; meta = catalog[p]; factory = meta['factory'].copy()
        active = {c['cell'] for page in meta['pages'] for c in page if c['active']}
        # The plugin's shared aliases quantize these two controls to 8/4 raw steps.
        if 8 in active: factory[8] = min(248, factory[8] // 8 * 8)
        if 10 in active: factory[10] = min(252, factory[10] // 4 * 4)
        # Fit up to 3 seconds, extending long-tail records to 6 seconds.
        q99 = entry['left_metrics']['energy_quantiles_s']['0.99']
        seconds = 6 if q99 > 2 else 3
        target = targets[entry['name']][:seconds * 100]
        x = np.zeros((seconds * FS, 2), np.float32); x[0, 0] = .5
        cache = {}
        def evaluate(raw, mode):
            key = (tuple(raw), mode)
            if key not in cache:
                y = renderer.render(p, raw, x, mode)
                cache[key] = loss(target, features(y)[:len(target)])
            return cache[key]
        baseline = evaluate(factory, 0)
        variation = variation_seed(meta, entry['name'])
        variation_choices = [(evaluate(variation, mode), variation.copy(), mode) for mode in range(8)]
        variation_score, _, variation_mode = min(variation_choices, key=lambda c: c[0])
        choices = variation_choices + [(baseline, factory.copy(), 0)]
        seed_path = args.output / 'initial-fits' / (entry['name'] + '.json')
        seeds = ([previous] if previous else []) + ([json.loads(seed_path.read_text())] if seed_path.exists() else [])
        for seed in seeds:
            trial = seed['raw'].copy()
            if 8 in active: trial[8] = trial[8] // 8 * 8
            if 10 in active: trial[10] = trial[10] // 4 * 4
            choices.append((evaluate(trial, seed['mode']), trial, seed['mode']))
        score, raw, mode = min(choices, key=lambda c: c[0])
        # Repeatable bounded coordinate search, preserving the fixed algorithm.
        # No output EQ, clock change, envelope shaping or IR resynthesis.
        history = [dict(stage='switches', score=score, mode=mode, raw=raw.copy())]
        for cell in ([43, 0, 1, 4, 9, 10, 3] if score > 2 else []):
            if cell not in active: continue
            candidates = [(score, raw)]
            for v in [0, 64, 128, 192, 248]:
                trial = raw.copy(); trial[cell] = v
                candidates.append((evaluate(trial, mode), trial))
            score, raw = min(candidates, key=lambda c: c[0])
        if q99 > 4:
            trial = raw.copy(); trial[0] = trial[1] = 253
            if evaluate(trial, mode) < score: raw, score = trial, evaluate(trial, mode)
        history.append(dict(stage='global_coordinates', score=score, mode=mode, raw=raw.copy()))
        for step in ([32, 8] if score > 1 else [8]):
            for cell in [43, 0, 1, 3, 9, 4, 10, 5, 2, 11]:
                if cell not in active: continue
                values = sorted(set([max(0, raw[cell] - step), min(248 if cell in [0, 1] else 254, raw[cell] + step)]))
                candidates = [(score, raw)]
                for v in values:
                    trial = raw.copy(); trial[cell] = v
                    candidates.append((evaluate(trial, mode), trial))
                score, raw = min(candidates, key=lambda c: c[0])
            # Switches interact with decay; reconsider after each control pass.
            score, mode = min((evaluate(raw, m), m) for m in range(8))
            history.append(dict(stage=f'coordinates_{step}', score=score, mode=mode, raw=raw.copy()))
        destination.parent.mkdir(exist_ok=True)
        previous = dict(name=entry['name'], program=p, score=score, factory_score=baseline,
                                   factory_variation_score=variation_score, factory_variation_mode=variation_mode,
                                   factory_variation_raw=variation,
                                   raw=raw, mode=mode, evaluations=len(cache), seconds=seconds, history=history,
                                   fit_input='left impulse 0.5 FS', warmup_s=1, route=[0, 2], stability=1,
                                   native_latency_compensated_samples=57)
        save_json(destination, previous)
        print('fit', entry['name'], f'{baseline:.3f} -> {score:.3f}', 'mode', mode, 'renders', len(cache),
              'total_seconds', round(time.time() - start), flush=True)
    renderer.close()


def fit(args):
    catalog = json.loads((args.output / 'catalog.json').read_text())
    entries = json.loads((args.output / 'inventory.json').read_text())
    with np.load(args.output / 'target_features.npz') as data:
        targets = {name: data[name] for name in data.files}
    groups = [[e for e in entries if e['program'] == p] for p in range(14)]
    with ThreadPoolExecutor(max_workers=3) as pool:
        futures = [pool.submit(fit_group, args, group, catalog, targets) for group in groups]
        for future in futures: future.result()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['inventory', 'fit'])
    parser.add_argument('--source', type=Path, default=Path('/Volumes/Something/LexIRs'))
    parser.add_argument('--output', type=Path, default=Path('build/validation/lexirs-20261104'))
    parser.add_argument('--renderer', type=Path, default=Path('build/validation/lexirs-20261104/renderer'))
    parser.add_argument('--bank', type=Path, default=Path('build/clock-experiment/cache/programs-v821-native-v5.bankxl'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.command == 'inventory': inventory(args)
    if args.command == 'fit': fit(args)


if __name__ == '__main__':
    main()
