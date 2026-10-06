#!/usr/bin/env python3
"""Pair private XL campaigns, retaining common decay fits and reference identity."""
import argparse
import csv
import hashlib
import json
import math
import statistics
from pathlib import Path


def read_json(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open() as file:
        return list(csv.DictReader(file))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def decay_bands(directory):
    return {(r['low_hz'], r['high_hz']): r for r in rows(directory / 'bands.csv')
            if r['variant'] == 'native' and r['paired_fit_usable'] == 'True'}


def envelope_error(directory):
    envelope = rows(directory / 'envelope.csv')
    value = lambda r: float(r['rms_dbfs']) if r['rms_dbfs'] else None
    reference = {r['time_s']: value(r) for r in envelope if r['variant'] == 'reference'}
    native = {r['time_s']: value(r) for r in envelope if r['variant'] == 'native'}
    require(reference.keys() == native.keys(), 'Envelope window times differ')
    peak = max((v for v in reference.values() if v is not None), default=-300)
    active = [time for time, db in reference.items() if peak > -200 and db is not None and db >= peak - 60]
    paired = [time for time in active if native[time] is not None]
    extra = [time for time in reference if time not in active and native[time] is not None
             and (peak <= -200 or native[time] >= peak - 60)]
    return dict(mean_abs_db=statistics.mean(abs(native[t] - reference[t]) for t in paired) if paired else None,
                paired_windows=len(paired), active_reference_windows=len(active),
                missing_native_windows=len(active) - len(paired), extra_native_windows=len(extra),
                excluded_reference_windows=len(reference) - len(active),
                policy='reference RMS peak above -200 dBFS, windows within 60 dB of that peak; missing/extra native windows separate; no acceptance threshold')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('current', type=Path)
    parser.add_argument('--require-complete', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.baseline = args.baseline.resolve(); args.current = args.current.resolve()
    require(root / 'build' in args.current.parents, 'Private outputs must stay under ignored build/')
    before = read_json(args.baseline / 'all-plan.json')
    after = read_json(args.current / 'all-plan.json')
    require(before['cases'] == after['cases'], 'Campaign recipes differ')
    for field in ('reference_revision', 'export_stamp', 'rom_sha256'):
        require(before['identity'][field] == after['identity'][field], 'Reference identity differs: ' + field)
    paired, pending, failed = [], [], []
    fit_changes = dict(improved=0, unchanged=0, worsened=0, unavailable=0)
    metadata_fields = ('program', 'mode', 'fixture', 'seed', 'amplitude', 'warmup_ms',
                       'duration_s', 'rate', 'block', 'analog', 'input_db', 'mix', 'left', 'right',
                       'protocol', 'reference_setup_frames', 'reference_start_cycles', 'reference_end_cycles',
                       'mod_calls', 'slow_calls', 'fast_calls', 'control_fixture')
    for case in after['cases']:
        name = case['name']; old = args.baseline / name; new = args.current / name
        if not (old / 'run.json').exists() or not (new / 'run.json').exists():
            pending.append(name); continue
        old_run, new_run = read_json(old / 'run.json'), read_json(new / 'run.json')
        if old_run['exit_code'] or new_run['exit_code']:
            failed.append(name); continue
        require(old_run['command'][4:] == new_run['command'][4:], name + ': actual commands differ')
        hashes = {variant: {'old': sha(old / (variant + '.wav')), 'new': sha(new / (variant + '.wav'))}
                  for variant in ('input', 'reference', 'native')}
        for variant in ('input', 'reference'):
            require(hashes[variant]['old'] == hashes[variant]['new'], name + ': ' + variant + ' WAV differs')
        old_meta, new_meta = rows(old / 'fixture.csv')[0], rows(new / 'fixture.csv')[0]
        for field in metadata_fields:
            require(old_meta[field] == new_meta[field], name + ': fixture metadata differs: ' + field)
        for meta, plan in ((old_meta, before), (new_meta, after)):
            require(meta['allocations'] == meta['releases'] == '0', name + ': native heap activity')
            require(meta['bank_sha256'] == plan['identity']['bank_sha256'], name + ': bank metadata mismatch')
        old_summary, new_summary = read_json(old / 'summary.json'), read_json(new / 'summary.json')
        require(old_summary['analysis_policy'] == new_summary['analysis_policy'], name + ': analysis policy differs')
        old_bands, new_bands = decay_bands(old), decay_bands(new)
        common = sorted(old_bands.keys() & new_bands.keys())
        means = [statistics.mean(abs(float(bands[key]['t20_delta_percent'])) for key in common)
                 if common else None for bands in (old_bands, new_bands)]
        if not common:
            change = 'unavailable'
        elif math.isclose(means[0], means[1], rel_tol=1e-12, abs_tol=1e-12):
            change = 'unchanged'
        else:
            change = 'improved' if means[1] < means[0] else 'worsened'
        fit_changes[change] += 1
        # Aggregate rates characterize the shared firmware loop. They do not
        # prove individual native call times or WCS visibility boundaries.
        rates = {controller: dict(reference_hz=int(new_meta[controller + '_calls']) / float(new_meta['duration_s']),
                                  native_nominal_hz=float(new_meta['native_' + controller + '_rate_hz']))
                 for controller in ('mod', 'slow', 'fast')}
        spectra = [{(r['low_hz'], r['high_hz']): r for r in rows(directory / 'bands.csv')
                    if r['variant'] == 'native'} for directory in (old, new)]
        require(spectra[0].keys() == spectra[1].keys(), name + ': spectral band definitions differ')
        band_energy = [dict(low_hz=key[0], high_hz=key[1],
                           spectral_image_band=spectra[1][key]['spectral_image_band'] == 'True',
                           old_error_db=float(spectra[0][key]['energy_error_db']) if spectra[0][key]['energy_error_db'] else None,
                           new_error_db=float(spectra[1][key]['energy_error_db']) if spectra[1][key]['energy_error_db'] else None)
                       for key in sorted(spectra[0], key=lambda key: float(key[0]))]
        paired.append(dict(case=name, index=case['index'], mode=case['mode'],
            fixture=case['fixture'], common_bands=len(common), old_usable=len(old_bands),
            new_usable=len(new_bands), band_set_changed=old_bands.keys() != new_bands.keys(),
            old_mean_abs_delta_percent=means[0], new_mean_abs_delta_percent=means[1], decay_fit_change=change,
            old_energy_error_db=old_summary['overall_energy_error_db'],
            new_energy_error_db=new_summary['overall_energy_error_db'],
            old_tail_energy_error_db=old_summary['tail_energy_error_db'],
            new_tail_energy_error_db=new_summary['tail_energy_error_db'],
            signal=old_summary['signal_observed_both_paths'] and new_summary['signal_observed_both_paths'],
            native_byte_identical=hashes['native']['old'] == hashes['native']['new'],
            old_envelope_error=envelope_error(old), new_envelope_error=envelope_error(new),
            band_energy=band_energy, controller_rates=rates, hashes=hashes))
    require(not failed, 'Failed campaign cases: ' + ', '.join(failed))
    if args.require_complete:
        require(not pending, str(len(pending)) + ' campaign cases are pending')
    result = dict(planned=len(after['cases']), completed=len(paired), pending=pending,
                  baseline_bank_sha256=before['identity']['bank_sha256'],
                  current_bank_sha256=after['identity']['bank_sha256'],
                  comparison_source_sha256=sha(Path(__file__)), decay_fit_changes=fit_changes,
                  interpretation='identical-reference paired measurement; unavailable fits are not passes',
                  cases=paired)
    (args.current / 'paired-correction-summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: result[key] for key in ('planned', 'completed', 'decay_fit_changes')}, indent=2))
    print('Input/reference WAVs and physical fixture metadata identical for every completed pair.')


if __name__ == '__main__':
    main()
