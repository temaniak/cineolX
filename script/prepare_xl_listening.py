#!/usr/bin/env python3
"""Create level-matched private XL listening pairs without changing raw evidence."""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

import numpy as np
from analyze_sound import wav


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_wav(path, audio):
    data = np.asarray(audio, dtype='<f4').tobytes()
    header = b'RIFF' + struct.pack('<I', len(data) + 36) + b'WAVEfmt '
    header += struct.pack('<IHHIIHH', 16, 3, 2, 48000, 48000 * 8, 8, 32)
    path.write_bytes(header + b'data' + struct.pack('<I', len(data)) + data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('matrix', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.matrix = args.matrix.resolve(); args.output = args.output.resolve()
    if root / 'build' not in args.output.parents:
        parser.error('Private listening WAVs must stay under ignored build/')
    plan = json.loads((args.matrix / 'all-plan.json').read_text())
    cases = sorted((c for c in plan['cases'] if c['fixture'] == 'music' and c['mode'] == 0
                    and '--gate-controls' not in c['options']), key=lambda c: c['index'])
    if [c['index'] for c in cases] != list(range(22)):
        parser.error('Expected one mode-0 music fixture for each XL graph')
    args.output.mkdir(parents=True, exist_ok=True)
    result = []
    for case in cases:
        source = args.matrix / case['name']
        run = json.loads((source / 'run.json').read_text())
        if run['exit_code'] or run['identity']['bank_sha256'] != plan['identity']['bank_sha256']:
            raise ValueError('Incomplete or mismatched listening source: ' + case['name'])
        audio = {name: wav(source / (name + '.wav')) for name in ('reference', 'native')}
        rms = {name: math.sqrt(float(np.mean(value[:96000] ** 2))) for name, value in audio.items()}
        peaks = {name: float(np.max(np.abs(value))) for name, value in audio.items()}
        if any(not math.isfinite(rms[name]) or rms[name] <= 1e-12 for name in audio):
            raise ValueError('Unexcited listening source: ' + case['name'])
        target = min(10 ** (-24 / 20), *(0.95 * rms[name] / peaks[name] for name in audio))
        gains = {name: target / rms[name] for name in audio}
        destination = args.output / ('p%02d' % case['index']); destination.mkdir(exist_ok=True)
        normalized = {name: value * gains[name] for name, value in audio.items()}
        for name, value in normalized.items():
            if not np.isfinite(value).all() or np.max(np.abs(value)) > 0.950001:
                raise ValueError('Invalid normalized output')
            write_wav(destination / (name + '.wav'), value)
        paired = np.concatenate((normalized['reference'], np.zeros((24000, 2)), normalized['native']))
        write_wav(destination / 'reference-then-native.wav', paired)
        result.append(dict(program=case['index'], case=case['name'], command=run['command'], gain=gains,
                           source_sha256={name: sha(source / (name + '.wav')) for name in audio},
                           output_sha256={p.name: sha(p) for p in sorted(destination.glob('*.wav'))}))
    (args.output / 'manifest.json').write_text(json.dumps(dict(bank_sha256=plan['identity']['bank_sha256'],
        generator_sha256=sha(Path(__file__)), method='Common RMS target -24 dBFS over the first two seconds, reduced if either full capture needs peak headroom; per-variant scalar gain only; no EQ/time alignment/fades; paired file reference first, 500 ms silence, native second',
        listened=False, cases=result), indent=2) + '\n')
    print('22 level-matched music pairs prepared; raw WAVs preserved; listening not performed.')


if __name__ == '__main__':
    main()
