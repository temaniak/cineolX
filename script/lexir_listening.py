#!/usr/bin/env python3
"""Identify XL factory variations and prepare private, level-matched listening files."""
import argparse
import copy
from concurrent.futures import ThreadPoolExecutor
import csv
import html
import json
import math
import re
import shutil
import subprocess
import time
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from lexir_match import (FS, BANDS, Renderer, describe, features, loss, read_pair,
                         save_json, sha, variation_seed)


def identify_group(args, entries, catalog):
    if not entries: return
    p = entries[0]['program']; meta = catalog[p]
    renderer = Renderer(args)
    seconds = 6 if p in [5, 13] else 3
    x = np.zeros((seconds * FS, 2), np.float32); x[0, 0] = .5
    lookup = {}; controls = {}
    cache_path = args.output / f'factory-p{p:02}.npz'
    if cache_path.exists():
        with np.load(cache_path) as data: lookup = {k: data[k] for k in data.files}
    else:
        for entry in entries:
            variant = entry['name'].rsplit(' V', 1)[1]
            raw = variation_seed(meta, entry['name']); controls[variant] = raw
            for mode in range(8):
                for side in range(2):
                    stimulus = x if side == 0 else x[:, ::-1].copy()
                    lookup[f'{variant}-{mode}-{side}'] = features(renderer.render(p, raw, stimulus, mode)).astype(np.float32)
        np.savez_compressed(cache_path, **lookup)
    for entry in entries:
        dest = args.output / 'matched' / (entry['name'] + '.json')
        if dest.exists(): continue
        n = (6 if entry['left_metrics']['energy_quantiles_s']['0.99'] > 2 else 3) * 100
        pair = read_pair(entry); target = [features(a)[:n] for a in pair]
        ranking = []
        for variant in [e['name'].rsplit(' V', 1)[1] for e in entries]:
            for mode in range(8):
                scores = [loss(target[s], lookup[f'{variant}-{mode}-{s}'][:n]) for s in range(2)]
                ranking.append(dict(variant=int(variant), mode=mode, score=float(np.mean(scores)), sides=scores))
        ranking.sort(key=lambda a: a['score'])
        best = ranking[0]
        initial = json.loads((args.output / 'fits' / (entry['name'] + '.json')).read_text())
        current = []
        for side in range(2):
            stimulus = x if side == 0 else x[:, ::-1].copy()
            current.append(loss(target[side], features(renderer.render(p, initial['raw'], stimulus, initial['mode']))[:n]))
        raw = initial['raw']; mode = initial['mode']; score = float(np.mean(current))
        if best['score'] < score:
            raw = variation_seed(meta, entry['group'] + ' V' + str(best['variant']))
            mode, score = best['mode'], best['score']
        # A second, narrow pass uses both driven inputs, rather than one favorable phase.
        for cell in [0, 1, 3, 9, 10]:
            candidates = [(score, raw)]
            for delta in [-8, 8]:
                trial = raw.copy(); trial[cell] = int(np.clip(raw[cell] + delta, 0, 248 if cell in [0, 1] else 252 if cell == 10 else 254))
                scores = []
                for side in range(2):
                    stimulus = x if side == 0 else x[:, ::-1].copy()
                    scores.append(loss(target[side], features(renderer.render(p, trial, stimulus, mode))[:n]))
                candidates.append((float(np.mean(scores)), trial))
            score, raw = min(candidates, key=lambda a: a[0])
        dest.parent.mkdir(exist_ok=True)
        save_json(dest, dict(name=entry['name'], program=p, raw=raw, mode=mode, score=score,
                             initial_left_fit_score=initial['score'], initial_matrix_score=float(np.mean(current)),
                             factory_ranking=ranking, identified_factory_variation=best['variant'],
                             factory_variation_score=best['score'], stability=1, route=[0, 2],
                             known_capture_settings=False, native_latency_compensated_samples=57))
        print('matched', entry['name'], 'closest factory', best['variant'], f'matrix score {score:.3f}', flush=True)
    renderer.close()


def identify(args):
    entries = json.loads((args.output / 'inventory.json').read_text())
    catalog = json.loads((args.output / 'catalog.json').read_text())
    with ThreadPoolExecutor(max_workers=3) as pool:
        futures = [pool.submit(identify_group, args, [e for e in entries if e['program'] == p], catalog) for p in range(14)]
        for future in futures: future.result()


def refine(args):
    entries = json.loads((args.output / 'inventory.json').read_text())
    renderer = Renderer(args)
    for entry in entries:
        if entry['name'] not in ['Small Room V3', 'Inverse Room V1', 'Inverse Room V2']: continue
        path = args.output / 'matched' / (entry['name'] + '.json')
        match = json.loads(path.read_text())
        target = [features(a)[:300] for a in read_pair(entry)]
        x = np.zeros((FS * 3, 2), np.float32); x[0, 0] = .5
        cache = {}
        def evaluate(raw, mode):
            key = (tuple(raw), mode)
            if key not in cache:
                cache[key] = float(np.mean([loss(target[s], features(renderer.render(entry['program'], raw,
                                      x if s == 0 else x[:, ::-1].copy(), mode))[:300]) for s in range(2)]))
            return cache[key]
        raw, mode = match['raw'], match['mode']; before = score = evaluate(raw, mode)
        cells = [12, 11, 10, 43, 4] if entry['program'] == 9 else [12, 13, 14, 15, 4, 10, 43]
        for cell in cells:
            candidates = [(score, raw)]
            for value in [0, 32, 64, 96, 128, 160, 192, 224, 248]:
                trial = raw.copy(); trial[cell] = value
                candidates.append((evaluate(trial, mode), trial))
            score, raw = min(candidates, key=lambda a: a[0])
        score, mode = min((evaluate(raw, m), m) for m in range(8))
        for cell in cells + [0, 1, 9, 5]:
            candidates = [(score, raw)]
            for delta in [-8, 8]:
                trial = raw.copy(); trial[cell] = int(np.clip(raw[cell] + delta, 0, 248 if cell in [0, 1] else 252 if cell == 10 else 254))
                candidates.append((evaluate(trial, mode), trial))
            score, raw = min(candidates, key=lambda a: a[0])
        match.update(raw=raw, mode=mode, score=score, special_refinement=dict(before=before, after=score, evaluations=len(cache)))
        save_json(path, match)
        print('refined', entry['name'], before, '->', score, flush=True)
    renderer.close()


def fixture():
    rng = np.random.default_rng(224821)
    x = np.zeros((FS * 14, 2), np.float64)
    # Alternating isolated percussion makes stereo crossfeed and buildup audible.
    for start, pan in [(0.15, 0), (.55, 1), (.95, .5), (1.35, .5)]:
        t = np.arange(int(FS * .35)) / FS
        noise = rng.standard_normal(len(t))
        hit = .18 * np.sin(2 * np.pi * (64 * t + 1.6 * (1 - np.exp(-t * 40)))) * np.exp(-t * 23)
        hit += .12 * noise * np.exp(-t * 60)
        hit *= np.minimum(t / .001, 1)
        at = int(start * FS)
        x[at:at + len(t), 0] += hit * math.cos(pan * math.pi / 2)
        x[at:at + len(t), 1] += hit * math.sin(pan * math.pi / 2)
    # Two harmonic chords, then an abrupt, click-free stop at 4.5 seconds.
    for start, notes in [(1.9, [130.8128, 164.8138, 195.9977, 246.9417]),
                         (3.2, [146.8324, 174.6141, 220, 261.6256])]:
        length = 1.15 if start < 3 else 1.3
        t = np.arange(round(length * FS)) / FS
        envelope = np.minimum(t / .025, 1) * np.minimum((length - t) / .015, 1)
        for channel in range(2):
            voice = sum(sum(np.sin(2 * np.pi * f * h * t + channel * .31 * h) / h ** 1.5
                            for h in range(1, 9)) for f in notes) * .027
            x[round(start * FS):round(start * FS) + len(t), channel] += voice * envelope
    assert np.max(np.abs(x)) < 1 and not x[round(4.5 * FS):].any()
    return x.astype(np.float32)


def convolve(x, left, right):
    n = len(x) + len(left) - 1; size = 1 << (n - 1).bit_length()
    a = np.fft.rfft(x, size, axis=0)
    l, r = np.fft.rfft(left, size, axis=0), np.fft.rfft(right, size, axis=0)
    y = np.fft.irfft(a[:, 0, None] * l + a[:, 1, None] * r, size, axis=0)
    return y[:len(x)].astype(np.float32)


def write_audio(path, x):
    assert np.isfinite(x).all() and np.max(np.abs(x)) < 1
    codec = ['-c:a', 'pcm_s24le'] if path.suffix == '.wav' else ['-c:a', 'flac', '-sample_fmt', 's32']
    subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-y', '-f', 'f32le', '-ar', str(FS), '-ac', '2',
                    '-i', 'pipe:0', *codec, str(path)], input=np.asarray(x, dtype='<f4').tobytes(), check=True)


def preset(path, match):
    defaults = dict(bass=17, mid=14, crossover=5, treble=23, depth=21, predelay=24, diffusion=1,
                    input_db=0, mix=1, analog=1, mode_enh=int(bool(match['mode'] & 1)),
                    decay_opt=int(bool(match['mode'] & 2)), output_l=0, output_r=2,
                    algorithm=match['program'] + 6, xl_chorus=match['raw'][8] // 8,
                    xl_diffusion=match['raw'][10] // 4, space=0,
                    filters_follow_clock=0, output_rear_l=1, output_rear_r=3,
                    mod_rate=2, mod_depth=1, optimization_response=5, clean_mode=0, dark_stability=1)
    raw = match['raw'].copy()
    raw[42] = (raw[42] & ~193) | (64 if match['mode'] & 1 else 0) | (128 if match['mode'] & 2 else 0) | (1 if match['mode'] & 4 else 0)
    defaults.update({f'xl_{i:02}': v for i, v in enumerate(raw)})
    source = Path(__file__).resolve().parents[1] / 'native-hall/plugin/Processor.hpp'
    text = source.read_text().split('static constexpr const char* ids[]={', 1)[1].split('};', 1)[0]
    ids = re.findall(r'"([^"]+)"', text)
    assert len(ids) == 74 and set(ids) == set(defaults)
    root = ET.Element('Cineol224Preset', version='7', name='LexIR Match - ' + match['name'], firmware='224xl-v8.21')
    for key in ids: ET.SubElement(root, 'PARAM', id=key, value=str(defaults[key]))
    ET.indent(root)
    ET.ElementTree(root).write(path, encoding='utf-8', xml_declaration=True)


def export(args):
    entries = json.loads((args.output / 'inventory.json').read_text())
    catalog = json.loads((args.output / 'catalog.json').read_text())
    matches = [json.loads((args.output / 'matched' / (e['name'] + '.json')).read_text()) for e in entries]
    # The user rejected the audible Decay Opt behavior in listening review.
    # Never re-enable it merely because a numerical envelope loss improves.
    for match in matches:
        match['mode'] &= ~2
        match['raw'][42] &= ~128
    command = [str(args.output / 'renderer-display'), str(args.bank), '--display']
    text = ''.join(' '.join(map(str, [m['program'], *m['raw']])) + '\n' for m in matches)
    displays = [json.loads(line) for line in subprocess.check_output(command, input=text.encode()).decode().splitlines()]
    renderer = Renderer(args)
    x = fixture(); listen = args.output / 'listening'; listen.mkdir(exist_ok=True)
    write_audio(listen / '00-DRY.wav', x)
    manifests = []; settings = []
    for entry, match, display in zip(entries, matches, displays):
        folder = listen / entry['name']; folder.mkdir(exist_ok=True)
        left, right = read_pair(entry)
        reference = convolve(x, left, right)
        # A separate phase/warmup from the fitting impulses, using the actual algorithm.
        native = renderer.render(entry['program'], match['raw'], x, match['mode'], warm=176160)
        reference_features = features(reference)
        mode_scores = [(loss(reference_features, features(native)), match['mode'])]
        rendered_modes = {match['mode']: native}
        # Dynamic settings inferred from one impulse can be ambiguous.
        # Check the musical response while keeping the user's Opt-off policy.
        for alternative in [match['mode'] ^ 4]:
            candidate = renderer.render(entry['program'], match['raw'], x, alternative, warm=176160)
            score = loss(reference_features, features(candidate))
            rendered_modes[alternative] = candidate
            mode_scores.append((score, alternative))
        music_score, selected_mode = min(mode_scores)
        match['impulse_fitted_mode'] = match['mode']
        match['mode'] = selected_mode
        match['music_mode_selection'] = dict(candidates=mode_scores, selected=selected_mode, score=music_score)
        match['optimization_policy'] = 'off: user listening decision 2026-11-04'
        native = rendered_modes[selected_mode]
        active = slice(0, round(4.5 * FS))
        outputs = [reference, native]
        rms = [float(np.sqrt(np.mean(y[active] ** 2))) for y in outputs]
        peak = [float(np.max(np.abs(y))) for y in outputs]
        target = min(10 ** (-22 / 20), *(0.89 * r / p for r, p in zip(rms, peak)))
        gains = [target / r for r in rms]
        normalized = [y * gain for y, gain in zip(outputs, gains)]
        write_audio(folder / 'A-Lexicon-IR.flac', normalized[0])
        write_audio(folder / 'B-Cineol.flac', normalized[1])
        # One convenient WAV per family, plus the suspected Bright Hall label anomaly.
        if entry['name'].endswith(' V1') or entry['name'] == 'Bright Hall V4':
            pair = np.concatenate([normalized[0], np.zeros((FS, 2)), normalized[1]])
            write_audio(folder / 'A-then-B.wav', pair)
        preset(folder / 'Matched.cineol224', match)
        path_metrics = []
        impulse = np.zeros((FS * 8, 2), np.float32); impulse[0, 0] = .5
        for side, ref in enumerate([left, right]):
            stimulus = impulse if side == 0 else impulse[:, ::-1].copy()
            rendered = renderer.render(entry['program'], match['raw'], stimulus, match['mode'], warm=176160)
            path_metrics.append(dict(input=['left', 'right'][side], reference=describe(features(ref)),
                                     native=describe(features(rendered)),
                                     score=loss(features(ref)[:300], features(rendered)[:300])))
        for pi, page in enumerate(catalog[entry['program']]['pages']):
            for si, control in enumerate(page):
                if control['cell'] >= 48: continue
                settings.append(dict(name=entry['name'], program=entry['program'] + 6, page=pi + 1, fader=si + 1,
                                     control=control['name'].strip(), cell=control['cell'], raw=match['raw'][control['cell']],
                                     display=display[pi][si], active=control['active']))
        residual = [{label: float(10 * np.log10(max(np.mean(y[int(start * FS):int(stop * FS)] ** 2), 1e-30)))
                     for label, start, stop in [('early_tail_dbfs', 4.6, 5.6), ('late_tail_dbfs', 11, 12), ('final_dbfs', 13, 14)]}
                    for y in normalized]
        row = dict(name=entry['name'], matched=match, gain=dict(reference=gains[0], native=gains[1]),
                   music_score=music_score,
                   active_rms_dbfs=20 * math.log10(target), peak=[float(np.max(np.abs(y))) for y in normalized],
                   residual_windows=residual, impulse_validation=path_metrics, display=display,
                   files={p.name: sha(p.read_bytes()) for p in sorted(folder.iterdir()) if p.name != 'settings-and-metrics.json'}, listened=False)
        manifests.append(row)
        save_json(folder / 'settings-and-metrics.json', row)
        print('exported', entry['name'], flush=True)
    renderer.close()
    save_json(listen / 'manifest.json', dict(cases=manifests, source_end_s=4.5, duration_s=14,
              normalization='One scalar per variant, common active-window stereo RMS at or below -22 dBFS; no EQ or per-channel normalization',
              order='A: four-path true-stereo convolution; B: native Cineol XL, Clean off, Dirt 0, Space 0, A/C outputs',
              fit_warmup_s=1, validation_warmup_s=3.67, listened=False))
    with (listen / 'settings.csv').open('w') as f:
        w = csv.DictWriter(f, settings[0]); w.writeheader(); w.writerows(settings)
    save_json(args.output / 'provenance.json', dict(
        revision=subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode().strip(),
        bank=str(args.bank.resolve()), bank_sha256=sha(args.bank.read_bytes()),
        renderer_sha256=sha(args.renderer.read_bytes()),
        sources={str(p): sha(p.read_bytes()) for p in [Path(__file__), Path(__file__).with_name('lexir_match.py'),
                 Path('native-hall/tools/lexir_render.cpp'), Path('deps/reflexion/web-demo/page/catalogs/eb3a7a765a703ece.json')]},
        archives={str(p): sha(p.read_bytes()) for p in sorted(args.source.glob('*.zip'))}))
    make_index(listen, manifests)


def rerender_opt_off(args):
    """Replace listening candidates with Opt off, retaining all other controls."""
    source = args.output / 'listening'
    destination = args.destination or args.output / 'listening-opt-off'
    if destination.exists() and not args.resume:
        raise ValueError('Choose a new destination; existing output is never overwritten')
    previous = json.loads((source / 'manifest.json').read_text())
    inventory = {e['name']: e for e in json.loads((args.output / 'inventory.json').read_text())}
    assert len(previous['cases']) == 71
    destination.mkdir(parents=True, exist_ok=True)
    renderer = Renderer(args)
    clean_renderer = Renderer(argparse.Namespace(renderer=args.clean_renderer, bank=args.bank))
    x = fixture()
    write_audio(destination / '00-DRY.wav', x)
    rows = []
    def remove_superseded_case(row):
        if not args.delete_superseded:
            return
        old_folder = source / row['name']
        if not old_folder.exists():
            return
        assert old_folder.parent == source and not old_folder.is_symlink()
        records = [dict(path=str(f.resolve()), bytes=f.stat().st_size,
                        replacement=str((destination / row['name'] / f.name).resolve()))
                   for f in old_folder.iterdir() if f.is_file()]
        with (args.output / 'opt-off-case-cleanup.ndjson').open('a') as log:
            log.write(json.dumps(dict(case=row['name'], removed=records)) + '\n')
        shutil.rmtree(old_folder)
    try:
        for old in previous['cases']:
            completed = destination / old['name'] / 'settings-and-metrics.json'
            if args.resume and completed.exists():
                row = json.loads(completed.read_text())
                assert not row['matched']['mode'] & 2
                assert not row['matched']['raw'][42] & 128
                for name, digest in row['files'].items():
                    assert sha((completed.parent / name).read_bytes()) == digest
                rows.append(row)
                remove_superseded_case(row)
                print('Verified completed', len(rows), '/71', row['name'], flush=True)
                continue
            row = copy.deepcopy(old)
            match = row['matched']
            before = match['mode']
            match['mode'] &= ~2
            match['raw'][42] = ((match['raw'][42] & ~193)
                                | (64 if match['mode'] & 1 else 0)
                                | (1 if match['mode'] & 4 else 0))
            match['previous_selected_mode'] = before
            match['optimization_policy'] = 'off: user listening decision 2026-11-04'
            match.pop('music_mode_selection', None)
            match['historical_impulse_fit_score'] = match.pop('score')
            folder = destination / row['name']
            folder.mkdir(exist_ok=True)
            reference_path = source / row['name'] / 'A-Lexicon-IR.flac'
            # Reuse the identical true-stereo reference, rather than recapture it.
            clone = subprocess.run(['cp', '-c', str(reference_path), str(folder / reference_path.name)],
                                   stderr=subprocess.DEVNULL)
            if clone.returncode:
                shutil.copyfile(reference_path, folder / reference_path.name)
            reference = np.frombuffer(subprocess.check_output([
                'ffmpeg', '-v', 'error', '-nostdin', '-i', str(reference_path),
                '-f', 'f32le', '-ac', '2', 'pipe:1']), '<f4').reshape(-1, 2)
            target = float(np.sqrt(np.mean(reference[:216000].astype(float) ** 2)))
            native = renderer.render(match['program'], match['raw'], x, match['mode'], warm=176160)
            gain = target / float(np.sqrt(np.mean(native[:216000].astype(float) ** 2)))
            native *= gain
            write_audio(folder / 'B-Cineol.flac', native)
            preset(folder / 'Matched.cineol224', match)
            if row['name'].endswith(' V1') or row['name'] == 'Bright Hall V4':
                write_audio(folder / 'A-then-B.wav', np.concatenate([
                    reference, np.zeros((FS, 2), np.float32), native]))
            row['gain']['native'] = gain
            row['active_rms_dbfs'] = 20 * math.log10(target)
            row['peak'] = [float(np.max(np.abs(y))) for y in [reference, native]]
            row['music_score'] = loss(features(reference), features(native))
            row['residual_windows'] = [
                {label: float(10 * np.log10(max(np.mean(y[int(start*FS):int(stop*FS)].astype(float) ** 2), 1e-30)))
                 for label, start, stop in [('early_tail_dbfs', 4.6, 5.6), ('late_tail_dbfs', 11, 12), ('final_dbfs', 13, 14)]}
                for y in [reference, native]]
            impulse = np.zeros((FS * 8, 2), np.float32)
            impulse[0, 0] = .5
            scores = []
            reference_pair = read_pair(inventory[row['name']])
            for side, validation in enumerate(row['impulse_validation']):
                y = renderer.render(match['program'], match['raw'],
                                    impulse if side == 0 else impulse[:, ::-1].copy(),
                                    match['mode'], warm=176160)
                native_features = features(y)
                validation['native'] = describe(native_features)
                # The inventory's archived reference is already measured;
                # its per-band values and accepted/rejected fits remain intact.
                ref = reference_pair[side]
                validation['score'] = loss(features(ref)[:300], native_features[:300])
                scores.append(validation['score'])
            match['score'] = float(np.mean(scores))
            if 'optional_clean' in old:
                clean = clean_renderer.render(match['program'], match['raw'], x, match['mode'], warm=176160)
                clean_gain = target / float(np.sqrt(np.mean(clean[:216000].astype(float) ** 2)))
                clean *= clean_gain
                write_audio(folder / 'C-Cineol-Clean.flac', clean)
                preset(folder / 'Matched-Clean.cineol224', match)
                tree = ET.parse(folder / 'Matched-Clean.cineol224')
                tree.getroot().set('name', 'LexIR Opt Off Clean - ' + row['name'])
                tree.getroot().find("PARAM[@id='clean_mode']").set('value', '1')
                tree.write(folder / 'Matched-Clean.cineol224', encoding='utf-8', xml_declaration=True)
                row['optional_clean'] = dict(name=row['name'], gain=clean_gain,
                    music_diagnostic=loss(features(reference), features(clean)),
                    final_window_dbfs=float(10*np.log10(max(np.mean(clean[-FS:].astype(float)**2), 1e-30))),
                    same_faders_and_switches_as_B=True,
                    files={name: sha((folder / name).read_bytes())
                           for name in ['C-Cineol-Clean.flac', 'Matched-Clean.cineol224']})
            row['files'] = {f.name: sha(f.read_bytes()) for f in sorted(folder.iterdir())}
            row['listened'] = False
            row['user_feedback'] = 'User identified Decay Opt as the artifact source; regenerated with Opt off.'
            save_json(folder / 'settings-and-metrics.json', row)
            rows.append(row)
            remove_superseded_case(row)
            print('Opt off', len(rows), '/71', row['name'], flush=True)
    finally:
        renderer.close()
        clean_renderer.close()
    previous.update(cases=rows, optimization_policy='Disabled for every Cineol render and preset by user request',
                    user_identified_artifact_source='Decay Opt', listened=False)
    save_json(destination / 'manifest.json', previous)
    shutil.copyfile(source / 'settings.csv', destination / 'settings.csv')
    make_index(destination, rows)
    save_json(args.output / 'opt-off-rerender-provenance.json', dict(
        bank_sha256=sha(args.bank.read_bytes()), renderer_sha256=sha(args.renderer.read_bytes()),
        clean_renderer_sha256=sha(args.clean_renderer.read_bytes()),
        generator_sha256=sha(Path(__file__).read_bytes()),
        previous_manifest_sha256=sha((source / 'manifest.json').read_bytes()),
        cases=71, native_renders=71, clean_renders=sum('optional_clean' in row for row in rows),
        warmup_frames=176160, optimization=False,
        retained_controls='All controls and Mod/Dynamic bits from the previous published match, except Opt'))


def make_index(listen, rows):
    body = []
    for row in rows:
        name = row['name']; match = row['matched']; e = html.escape
        clean = (f'<label>C · Cineol Clean <audio controls preload="none" src="{e(name)}/C-Cineol-Clean.flac"></audio>'
                 f'<a href="{e(name)}/Matched-Clean.cineol224">Clean preset</a></label>'
                 if (listen / name / 'C-Cineol-Clean.flac').exists() else '')
        body.append(f'<section><h2>{e(name)}</h2><p>Closest factory variation: V{match["identified_factory_variation"]}; '
                    f'fit diagnostic: {match["score"]:.2f}. <a href="{e(name)}/Matched.cineol224">Cineol preset</a></p>'
                    f'<label>A · Lexicon IR <audio controls preload="none" src="{e(name)}/A-Lexicon-IR.flac"></audio></label>'
                    f'<label>B · Cineol <audio controls preload="none" src="{e(name)}/B-Cineol.flac"></audio></label>{clean}</section>')
    (listen / 'index.html').write_text('''<!doctype html><meta charset="utf-8"><title>LexIR / Cineol comparisons</title>
<style>body{font:17px system-ui;max-width:1000px;margin:40px auto;padding:0 24px;background:#14171c;color:#e8edf5}a{color:#8dc1ff}section{border-top:1px solid #46515c;padding:20px 0}label{display:inline-flex;flex-direction:column;margin:5px 20px 5px 0;gap:8px}h2{font-size:21px}</style>
<h1>Lexicon 224XL IR / Cineol</h1><p>71 matched sets · true stereo · 48 kHz · level matched · wet only.</p>
<p>Input stops at 4.5 s; every file lasts 14 s. A = archived IR convolution, B = running Cineol algorithm.
Decay Opt is disabled in every Cineol render and preset by user request.
The score is a fitting diagnostic, not a percentage of sound similarity. Listening acceptance is pending.</p>
<p><a href="00-DRY.wav">Dry percussion and chords</a> · <a href="settings.csv">All settings</a></p>''' + '\n'.join(body))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('command', choices=['identify', 'refine', 'export', 'opt-off'])
    p.add_argument('--source', type=Path, default=Path('/Volumes/Something/LexIRs'))
    p.add_argument('--output', type=Path, default=Path('build/validation/lexirs-20261104'))
    p.add_argument('--renderer', type=Path, default=Path('build/validation/lexirs-20261104/renderer'))
    p.add_argument('--bank', type=Path, default=Path('build/clock-experiment/cache/programs-v821-native-v5.bankxl'))
    p.add_argument('--destination', type=Path)
    p.add_argument('--clean-renderer', type=Path, default=Path('build/validation/lexirs-20261104/renderer-clean'))
    p.add_argument('--resume', action='store_true', help='Verify and reuse complete Opt-off cases')
    p.add_argument('--delete-superseded', action='store_true',
                   help='Delete each old generated case after its replacement is complete; requires user authorization')
    args = p.parse_args()
    if args.command == 'identify': identify(args)
    elif args.command == 'refine': refine(args)
    elif args.command == 'opt-off': rerender_opt_off(args)
    else: export(args)


if __name__ == '__main__': main()
