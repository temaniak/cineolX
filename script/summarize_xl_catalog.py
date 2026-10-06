#!/usr/bin/env python3
"""Summarize a private compare_xl_catalog campaign without declaring acceptance."""
import argparse
import csv
import json
from pathlib import Path
import statistics

import numpy as np
from analyze_sound import wav


def spectral_peaks(audio, start):
    spectra = np.sum(np.abs(np.fft.rfft(audio[int(start * 48000):], axis=0)) ** 2, axis=1)
    frequencies = np.fft.rfftfreq(len(audio) - int(start * 48000), 1 / 48000)
    peaks = np.flatnonzero((spectra[1:-1] > spectra[:-2]) & (spectra[1:-1] >= spectra[2:])) + 1
    peaks = [n for n in peaks if 20 <= frequencies[n] <= 15000]
    chosen = []
    for n in sorted(peaks, key=lambda n: spectra[n], reverse=True):
        if all(abs(frequencies[n] - frequencies[old]) >= 15 for old in chosen):
            chosen.append(n)
        if len(chosen) == 12:
            break
    return sorted(round(float(frequencies[n]), 3) for n in chosen)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix", type=Path)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()
    root = args.matrix
    plan = json.loads((root / "all-plan.json").read_text())
    completed, failed, pending = [], [], []
    summaries = {}
    for case in plan["cases"]:
        directory = root / case["name"]
        status = directory / "run.json"
        if not status.exists():
            pending.append(case["name"]);continue
        run = json.loads(status.read_text())
        if run["exit_code"]:
            failed.append(case["name"]);continue
        completed.append(case["name"])
        summaries[case["name"]] = json.loads((directory / "summary.json").read_text())
    rows = []
    for p in plan["programs"]:
        names = [c["name"] for c in plan["cases"] if c["index"] == p["index"]]
        baseline = root / f"p{p['index']:02d}-m0-noise17"
        result = summaries.get(baseline.name, {})
        common = [];native_mean = diagnostic_mean = None
        coeff_mismatch = address_mismatch = interpolation_mismatch = 0
        baseline_controls = None
        if (baseline / "bands.csv").exists():
            with (baseline / "bands.csv").open() as file:
                bands = list(csv.DictReader(file))
            native = {(r["low_hz"], r["high_hz"]): r for r in bands if r["variant"] == "native" and r["paired_fit_usable"] == "True"}
            frozen = {(r["low_hz"], r["high_hz"]): r for r in bands if r["variant"] == "static_reference_wcs" and r["paired_fit_usable"] == "True"}
            common = sorted(native.keys() & frozen.keys())
            if common:
                native_mean = statistics.mean(abs(float(native[key]["t20_delta_percent"])) for key in common)
                diagnostic_mean = statistics.mean(abs(float(frozen[key]["t20_delta_percent"])) for key in common)
            with (baseline / "wcs.csv").open() as file:
                for r in csv.DictReader(file):
                    c, a = r["bank_coefficient"] != r["reference_coefficient"], r["bank_offset"] != r["reference_offset"]
                    coeff_mismatch += c;address_mismatch += a
                    interpolation_mismatch += bool(c or a) and r["interpolation_row"] == "1"
            with (baseline / "fixture.csv").open() as file:
                baseline_controls = next(csv.DictReader(file)).get("control_fixture", "factory")
        rows.append(dict(index=p["index"], name=p["name"], reverb=p["reverb"], planned=len(names),
            completed=sum(name in completed for name in names), failed=sum(name in failed for name in names),
            pending=sum(name in pending for name in names), baseline_energy_error_db=result.get("overall_energy_error_db"),
            baseline_signal=result.get("signal_observed_both_paths"), common_decay_bands=len(common),
            native_mean_abs_decay_error_common_percent=native_mean,
            injected_wcs_mean_abs_decay_error_common_percent=diagnostic_mean,
            bank_reference_coefficient_differences=coeff_mismatch, bank_reference_offset_differences=address_mismatch,
            bank_reference_interpolation_row_differences=interpolation_mismatch, baseline_controls=baseline_controls,
            interpretation="independent_baseline_and_injected_WCS_diagnostic; no shipped correction"))
    with (root / "catalog-summary.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, rows[0].keys());writer.writeheader();writer.writerows(rows)
    summary = dict(planned=len(plan["cases"]), completed=len(completed), failed=failed, pending=pending,
                   programs=rows, identity=plan["identity"], acceptance="not_established")
    (root / "catalog-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    # Resonant Chords and delay effects use response/envelope/peak diagnostics,
    # not exponential decay fits. Retain spectral candidates, not pitch passes.
    effects = {}
    for p in plan["programs"]:
        if p["reverb"]:
            continue
        for fixture, start in (("noise17", .3), ("music", 2.2)):
            d = root / f"p{p['index']:02d}-m0-{fixture}"
            if d.name in completed:
                effects[d.name] = {variant: spectral_peaks(wav(d / f"{variant}.wav"), start)
                                  for variant in ("native", "reference", "static_reference_wcs")}
    (root / "effects-spectral-candidates.json").write_text(json.dumps(effects, indent=2) + "\n")
    print(json.dumps(dict(planned=summary["planned"], completed=summary["completed"],
                         failed=len(failed), pending=len(pending), programs_with_baseline=sum(r["baseline_signal"] is not None for r in rows))))
    return bool(args.require_complete and (failed or pending))


if __name__ == "__main__":
    raise SystemExit(main())
