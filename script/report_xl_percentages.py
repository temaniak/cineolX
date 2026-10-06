#!/usr/bin/env python3
"""Report XL band RMS-level and valid decay differences from retained analyses.

Usage: report_xl_percentages.py OUTPUT_DIRECTORY CASE_DIRECTORY [...]
The 5% criterion is provisional numerical triage, not listening acceptance.
Energy ratios are converted to RMS amplitude ratios before percentage scoring.
Unusable decay fits and spectral-image bands remain separately labelled.
"""
import csv
import json
import math
import statistics
import sys
from pathlib import Path


def percent(db):
    return None if db is None or db == '' else 100 * math.expm1(float(db) * math.log(10) / 20)


def envelope_metrics(directory):
    records = list(csv.DictReader((directory / 'envelope.csv').open()))
    reference = {r['time_s']: r['rms_dbfs'] for r in records if r['variant'] == 'reference'}
    native = {r['time_s']: r['rms_dbfs'] for r in records if r['variant'] == 'native'}
    peak = max((float(v) for v in reference.values() if v), default=None)
    # Score audible-response windows; retain the separate late-floor levels.
    # A native silence/dropout in an active reference window is -100%.
    threshold = max(peak - 40, -90) if peak is not None else None
    errors = [abs(percent(str(float(native[t]) - float(db)))) if native[t] else 100.0
              for t, db in reference.items()
              if threshold is not None and db and float(db) >= threshold]
    ordered = sorted(errors)
    position = .95 * (len(ordered) - 1)
    first = math.floor(position)
    p95 = (ordered[first] + (ordered[min(first + 1, len(ordered) - 1)] - ordered[first]) *
           (position - first)) if ordered else None
    responses = json.loads((directory / 'response.json').read_text())
    late = {}
    for variant in ('native', 'reference'):
        dbs = [r['final_second_rms_dbfs'] for r in responses[variant]]
        power = sum(10 ** (db / 10) if db is not None else 0 for db in dbs) / len(dbs)
        late[variant] = 10 * math.log10(power) if power else None
    return dict(envelope_reference_threshold_dbfs=threshold,
                envelope_active_50ms_windows=len(errors),
                envelope_median_abs_level_delta_percent=statistics.median(errors) if errors else None,
                envelope_p95_abs_level_delta_percent=p95,
                envelope_max_abs_level_delta_percent=max(errors, default=None),
                native_final_second_rms_dbfs=late['native'],
                reference_final_second_rms_dbfs=late['reference'])


def report(output, directories):
    rows, cases = [], []
    for directory in directories:
        bands = list(csv.DictReader((directory / 'bands.csv').open()))
        primary = []
        for band in bands:
            level = percent(band['energy_error_db'])
            image = band['spectral_image_band'] == 'True'
            usable = band['paired_fit_usable'] == 'True'
            decay = float(band['t20_delta_percent']) if usable else None
            row = dict(case=directory.name, directory=str(directory.resolve()),
                       program=int(band['program']), mode=int(band['mode']),
                       fixture=band['fixture'], variant=band['variant'],
                       low_hz=int(band['low_hz']), high_hz=int(band['high_hz']),
                       rms_level_delta_percent=level, spectral_image_band=image,
                       level_status='unmeasurable' if level is None else
                           'image_diagnostic' if image else
                           'provisionally_small' if abs(level) <= 5 else 'investigate',
                       decay_delta_percent=decay,
                       decay_status='unavailable' if not usable else
                           'provisionally_small' if abs(decay) <= 5 else 'investigate',
                       native_fit_reason=band['native_fit_reason'],
                       reference_fit_reason=band['reference_fit_reason'])
            rows.append(row)
            if band['variant'] == 'native' and not image:
                primary.append(row)
        levels = [abs(r['rms_level_delta_percent']) for r in primary
                  if r['rms_level_delta_percent'] is not None]
        decays = [abs(r['decay_delta_percent']) for r in primary
                  if r['decay_delta_percent'] is not None]
        cases.append(dict(case=directory.name, program=primary[0]['program'],
                          mode=primary[0]['mode'], fixture=primary[0]['fixture'],
                          measurable_level_bands=len(levels),
                          max_abs_level_delta_percent=max(levels, default=None),
                          bands_over_5_percent=sum(v > 5 for v in levels),
                          usable_decay_bands=len(decays),
                          max_abs_decay_delta_percent=max(decays, default=None),
                          decay_bands_over_5_percent=sum(v > 5 for v in decays),
                          **envelope_metrics(directory)))
    output.mkdir(parents=True, exist_ok=True)
    with (output / 'band-percentages.csv').open('w', newline='') as file:
        writer = csv.DictWriter(file, rows[0].keys())
        writer.writeheader(); writer.writerows(rows)
    with (output / 'case-percentages.csv').open('w', newline='') as file:
        writer = csv.DictWriter(file, cases[0].keys())
        writer.writeheader(); writer.writerows(cases)
    summary = dict(policy='xl-provisional-rms-and-valid-decay-5-percent-v2',
                   rms_formula='100 * (sqrt(native_energy/reference_energy) - 1)',
                   decay_formula='100 * (native_T20_slope_T60/reference_T20_slope_T60 - 1)',
                   provisional_threshold_percent=5, listening_acceptance=False,
                   envelope_policy='50ms block RMS; reference >= max(peak - 40 dB, -90 dBFS); native silence = -100%; retain final-second absolute floors; envelope errors diagnostic, no automatic pass',
                   cases=cases)
    (output / 'percentages.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(dict(cases=len(cases),
                          level_bands_over_5_percent=sum(c['bands_over_5_percent'] for c in cases),
                          usable_decay_bands=sum(c['usable_decay_bands'] for c in cases),
                          decay_bands_over_5_percent=sum(c['decay_bands_over_5_percent'] for c in cases))))


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    report(Path(sys.argv[1]), [Path(p) for p in sys.argv[2:]])
