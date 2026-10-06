#!/usr/bin/env python3
"""Analyze private cineol_xl_sound_compare output; requires NumPy.

Usage: python script/analyze_xl_sound.py OUTPUT_DIRECTORY [...]
Bands are exploratory XL measurements, not a sound-acceptance standard.
T20 fits extrapolate a -5..-25 dB slope to T60; unusable fits retain reasons.
"""
import csv
import json
import sys
from pathlib import Path

import numpy as np
from analyze_sound import filtered, wav

FS = 48000
ANALYSIS_POLICY = "xl-catalog-v2"
MEASURABLE_ENERGY = 1e-12
BANDS = [(100, 250), (250, 500), (500, 1000), (1000, 2000),
         (2000, 4000), (4000, 8000), (8000, 12000), (12000, 15000),
         (15000, 20000)]


def decay_fit(audio, start, duration, reverb):
    result = dict(t20_extrapolated_t60_s=None, r2=None, fit_last_s=None,
                  capture_margin_s=None, usable=False, reason="")
    if not reverb:
        result["reason"] = "non_exponential_or_non_reverb_program"
        return result
    samples = audio[int(start * FS):]
    energy = np.cumsum(np.sum(samples ** 2, axis=1)[::-1])[::-1]
    if not len(energy) or energy[0] <= 1e-24:
        result["reason"] = "silent_band"
        return result
    db = 10 * np.log10(np.maximum(energy / energy[0], 1e-30))
    selected = (db <= -5) & (db >= -25)
    if selected.sum() < 100:
        result["reason"] = "insufficient_fit_samples"
        return result
    time = np.arange(len(db)) / FS
    slope, intercept = np.polyfit(time[selected], db[selected], 1)
    error = db[selected] - slope * time[selected] - intercept
    variance = np.sum((db[selected] - db[selected].mean()) ** 2)
    r2 = float(1 - np.sum(error ** 2) / variance) if variance else 0.0
    last = float(time[selected][-1])
    margin = duration - start - last
    result.update(t20_extrapolated_t60_s=float(-60 / slope) if slope < 0 else None,
                  r2=r2, fit_last_s=last, capture_margin_s=margin)
    reasons = []
    if slope >= 0:
        reasons.append("nonnegative_slope")
    if r2 < .95:
        reasons.append("non_exponential_fit")
    if margin < 1.2:
        reasons.append("insufficient_capture_margin")
    # Retain a truncation/floor guard independently of the fit's R-squared.
    late = float(np.sum(samples[-FS:] ** 2))
    if late / energy[0] > 1e-4:
        reasons.append("late_energy_above_minus40db")
    result.update(usable=not reasons, reason=";".join(reasons) or "usable_exploratory_fit")
    return result


def energy_db(actual, reference):
    a, r = float(np.sum(actual ** 2)), float(np.sum(reference ** 2))
    # Ratios of float/filter residue in an unexcited split output are not
    # measurable signal differences. Use the same guard as signal observation.
    return float(10 * np.log10(a / r)) if a > MEASURABLE_ENERGY and r > MEASURABLE_ENERGY else None


def exponential_decay_applicable(meta):
    program, mode = int(meta["program"]), int(meta["mode"])
    if program in (9, 14, 15, 16) or mode & 4:
        return False
    # A/C baseline covers the reverb side. Plate/Chorus B/D is an effect;
    # Rich Split's auxiliary child has no exponential acceptance target here.
    # Mixed pairs containing either auxiliary output also stay unscored.
    return not (program in (20, 21) and any(int(meta[c]) in (1, 3) for c in ("left", "right")))


def response_metrics(audio, impulse=False):
    """Thresholded timing/envelope diagnostics, without exponential fitting."""
    result = []
    for channel in range(2):
        samples = audio[:, channel]
        power = samples ** 2
        total = float(np.sum(power))
        peak = float(np.max(np.abs(samples)))
        row = dict(channel=channel, energy=total, peak=peak, onset_ms=None,
                   peak_ms=None, energy10_ms=None, energy50_ms=None, energy90_ms=None,
                   final_second_rms_dbfs=None, impulse_peak_candidates_ms=[])
        if total > 1e-24 and peak > 1e-10:
            above = np.flatnonzero(np.abs(samples) >= max(1e-7, peak * .001))
            row["onset_ms"] = float(above[0] * 1000 / FS) if len(above) else None
            row["peak_ms"] = float(np.argmax(np.abs(samples)) * 1000 / FS)
            integrated = np.cumsum(power)
            for fraction in (.1, .5, .9):
                row[f"energy{int(fraction * 100)}_ms"] = float(np.searchsorted(integrated, total * fraction) * 1000 / FS)
            rms = float(np.sqrt(np.mean(power[-FS:])))
            row["final_second_rms_dbfs"] = float(20 * np.log10(rms)) if rms else None
            if impulse:
                # Candidates from 1 ms energy blocks, -20 dB relative threshold,
                # >=20 ms separation. They are not automatically echo identities.
                envelope = power[:len(power) // 48 * 48].reshape(-1, 48).mean(axis=1)
                peaks = np.flatnonzero((envelope[1:-1] > envelope[:-2]) &
                    (envelope[1:-1] >= envelope[2:]) & (envelope[1:-1] >= envelope.max() * .01)) + 1
                chosen = []
                for candidate in sorted(peaks, key=lambda n: envelope[n], reverse=True):
                    if all(abs(int(candidate) - old) >= 20 for old in chosen):
                        chosen.append(int(candidate))
                    if len(chosen) == 12:
                        break
                row["impulse_peak_candidates_ms"] = sorted(chosen)
        result.append(row)
    return result


def analyze(directory):
    with (directory / "fixture.csv").open() as file:
        meta = next(csv.DictReader(file))
    if meta["schema"] != "xl-independent-v1" or int(meta["rate"]) != FS:
        raise ValueError(f"Unsupported XL fixture: {directory}")
    reference, native = wav(directory / "reference.wav"), wav(directory / "native.wav")
    variants = {"native": native}
    if meta.get("static_wcs_diagnostic") == "1":
        variants["static_reference_wcs"] = wav(directory / "static_reference_wcs.wav")
    duration = int(meta["duration_s"])
    if any(audio.shape != reference.shape for audio in variants.values()) or len(native) != duration * FS:
        raise ValueError(f"Mismatched render lengths: {directory}")
    if not np.isfinite(reference).all() or any(not np.isfinite(audio).all() for audio in variants.values()):
        raise ValueError(f"Nonfinite render: {directory}")
    if meta["fixture"] == "file":
        start = float(meta["input_stop_s"]) + .2
        if not .2 < start < duration:
            raise ValueError(f"Invalid file-fixture stop time: {directory}")
    else:
        start = .3 if meta["fixture"] == "noise" else .1 if meta["fixture"].startswith("impulse") else 2.2
    # Inverse Room and the three non-reverbs need other envelope/delay metrics.
    # Dynamic gating also invalidates a blanket exponential-decay assumption.
    reverb = exponential_decay_applicable(meta)
    rows = []
    for low, high in BANDS:
        ref = filtered(reference, low, high)
        rf = decay_fit(ref, start, duration, reverb)
        for variant, audio in variants.items():
            actual = filtered(audio, low, high)
            nf = decay_fit(actual, start, duration, reverb)
            usable = rf["usable"] and nf["usable"] and high <= 15000
            rt, nt = rf["t20_extrapolated_t60_s"], nf["t20_extrapolated_t60_s"]
            row = dict(**meta, variant=variant, low_hz=low, high_hz=high,
                       energy_error_db=energy_db(actual, ref),
                       native_t20_extrapolated_t60_s=nt, reference_t20_extrapolated_t60_s=rt,
                       t20_delta_percent=100 * (nt / rt - 1) if rt and nt else None,
                       native_r2=nf["r2"], reference_r2=rf["r2"],
                       native_capture_margin_s=nf["capture_margin_s"],
                       reference_capture_margin_s=rf["capture_margin_s"],
                       native_fit_reason=nf["reason"], reference_fit_reason=rf["reason"],
                       paired_fit_usable=usable, spectral_image_band=high > 15000)
            rows.append(row)
    with (directory / "bands.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, rows[0].keys());writer.writeheader();writer.writerows(rows)
    usable = [row for row in rows if row["paired_fit_usable"] and row["variant"] == "native"]
    summary = dict(analysis_policy=ANALYSIS_POLICY, program=int(meta["program"]), mode=int(meta["mode"]), fixture=meta["fixture"],
                   level=float(meta["amplitude"]), seed=int(meta["seed"]),
                   overall_energy_error_db=energy_db(native, reference),
                   native_peak=float(np.max(np.abs(native))), reference_peak=float(np.max(np.abs(reference))),
                   usable_paired_bands=len(usable),
                   mean_abs_t20_delta_percent=float(np.mean([abs(row["t20_delta_percent"]) for row in usable])) if usable else None,
                   max_abs_t20_delta_percent=max((abs(row["t20_delta_percent"]) for row in usable), default=None),
                   acceptance="baseline_measurement_only")
    # Block envelopes remain useful when gating or quantization floors make
    # exponential fits unsuitable. Levels are measured, never normalized here.
    envelope_rows = []
    for variant, audio in {"reference": reference, **variants}.items():
        for begin in range(0, len(audio), 2400):
            segment = audio[begin:begin + 2400]
            rms = float(np.sqrt(np.mean(segment ** 2)))
            envelope_rows.append(dict(variant=variant, time_s=begin / FS,
                rms_dbfs=float(20 * np.log10(rms)) if rms > 0 else None,
                peak=float(np.max(np.abs(segment)))))
    with (directory / "envelope.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, envelope_rows[0].keys());writer.writeheader();writer.writerows(envelope_rows)
    summary["tail_energy_error_db"] = energy_db(native[int(start * FS):], reference[int(start * FS):])
    metrics = {variant: response_metrics(audio, meta["fixture"].startswith("impulse"))
               for variant, audio in {"reference": reference, **variants}.items()}
    (directory / "response.json").write_text(json.dumps(metrics, indent=2) + "\n")
    summary["signal_observed_both_paths"] = bool(np.sum(native ** 2) > MEASURABLE_ENERGY and np.sum(reference ** 2) > MEASURABLE_ENERGY)
    diagnostics = {}
    for variant, audio in variants.items():
        if variant == "native":
            continue
        fitted = [row for row in rows if row["variant"] == variant and row["paired_fit_usable"]]
        diagnostics[variant] = dict(overall_energy_error_db=energy_db(audio, reference),
            usable_paired_bands=len(fitted),
            mean_abs_t20_delta_percent=float(np.mean([abs(row["t20_delta_percent"]) for row in fitted])) if fitted else None,
            max_abs_t20_delta_percent=max((abs(row["t20_delta_percent"]) for row in fitted), default=None),
            interpretation="reference_settings_injected_diagnostic_only")
    if diagnostics:
        summary["diagnostics"] = diagnostics
    (directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(str(directory), json.dumps(summary))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for argument in sys.argv[1:]:
        analyze(Path(argument))
