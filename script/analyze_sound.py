"""Analyze private native_224_sound_compare output (requires NumPy only).

All results, including rejected fits, are retained. Aligned is diagnostic.
With --startup-probe renders, warmup/modulation/decay/load are also diagnostics.
Usage: python script/analyze_sound.py OUTPUT_DIRECTORY [OUTPUT_DIRECTORY ...]
"""
import csv
import json
import sys
from pathlib import Path
import numpy as np

FS = 48000
BANDS = [(100, 250), (250, 500), (500, 1000), (1000, 2000),
         (2000, 4000), (4000, 8000), (8000, 10240), (9000, 10240)]

def wav(path):
    raw = path.read_bytes()
    if raw[:4] != b"RIFF" or raw[20:22] != b"\x03\x00" or raw[36:40] != b"data":
        raise ValueError(f"Expected harness stereo float WAV: {path}")
    return np.frombuffer(raw[44:], dtype="<f4").reshape(-1, 2).astype(float)

def filtered(x, low, high):
    taps = 4097
    time = np.arange(taps) - (taps - 1) / 2
    h = (2 * high / FS * np.sinc(2 * high / FS * time)
         - 2 * low / FS * np.sinc(2 * low / FS * time)) * np.hanning(taps)
    size = 1 << (len(x) + taps - 2).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, size, axis=0) * np.fft.rfft(h, size)[:, None], size, axis=0)
    return y[(taps - 1) // 2:(taps - 1) // 2 + len(x)]

def fit(x, start):
    energy = np.cumsum(np.sum(x[int(start * FS):] ** 2, axis=1)[::-1])[::-1]
    if not len(energy) or energy[0] <= 0:
        return 0.0, 0.0, 99.0
    db = 10 * np.log10(np.maximum(energy / energy[0], 1e-30))
    selected = (db <= -5) & (db >= -25)
    time = np.arange(len(db)) / FS
    if selected.sum() < 100:
        return 0.0, 0.0, 99.0
    slope, intercept = np.polyfit(time[selected], db[selected], 1)
    error = db[selected] - slope * time[selected] - intercept
    variance = np.sum((db[selected] - db[selected].mean()) ** 2)
    r2 = 1 - np.sum(error ** 2) / variance if variance > 0 else 0
    return float(-60 / slope) if slope < 0 else 0.0, float(r2), float(time[selected][-1])

def analyze(directory):
    meta = next(csv.DictReader((directory / "fixture.csv").open()))
    start = .3 if meta["fixture"] == "noise" else 2.2
    reference = wav(directory / "reference.wav")
    rows = []
    names = ["native", "aligned"]
    if meta.get("startup_probe") == "1":
        names += ["warmup", "modulation", "decay", "load"]
    variants = {name: wav(directory / f"{name}.wav") for name in names}
    for low, high in BANDS:
        ref = filtered(reference, low, high)
        rt, rr, rlast = fit(ref, start)
        for name, audio in variants.items():
            data = filtered(audio, low, high)
            nt, nr, nlast = fit(data, start)
            accepted = min(nr, rr) >= .95 and max(nlast, rlast) < 12 - start - 1.2 and nt > 0 and rt > 0
            rows.append(dict(**meta, variant=name, low=low, high=high,
                             energy_db=float(10 * np.log10(np.sum(data ** 2) / np.sum(ref ** 2))),
                             native_T20_s=nt, reference_T20_s=rt,
                             delta_percent=100 * (nt / rt - 1) if rt else 0,
                             r2_min=min(nr, rr), fit_last_s=max(nlast, rlast), accepted=accepted))
    with (directory / "bands.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, rows[0].keys()); writer.writeheader(); writer.writerows(rows)
    broad = [r for r in rows if r["variant"] == "native" and r["low"] != 9000 and r["accepted"]]
    print(str(directory), json.dumps(dict(accepted=len(broad),
          max_T20_error_percent=max((abs(r["delta_percent"]) for r in broad), default=None),
          max_energy_error_db=max((abs(r["energy_db"]) for r in rows if r["variant"] == "native"), default=None))))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for arg in sys.argv[1:]:
        analyze(Path(arg))
