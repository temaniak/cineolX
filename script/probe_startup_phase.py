"""Offline startup sensitivity matrix; none of its variants changes the plugin.

Usage: python script/probe_startup_phase.py EXECUTABLE ROM_DIRECTORY BANK OUTPUT
Requires the built native_224_sound_compare tool and NumPy. Captures and
ROM-derived state remain in OUTPUT, which must be a private build directory.
"""
import concurrent.futures
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import analyze_sound


def summarize(rows):
    groups = {}
    for row in rows:
        key = (row["seed"], row["warmup_ms"])
        groups.setdefault(key, []).append(row)
    summary = []
    for (seed, warmup), group in groups.items():
        common = {}
        for row in group:
            if row["variant"] in ("native", "warmup", "modulation", "decay", "load") and row["low"] != "9000":
                key = (row["program"], row["low"], row["high"])
                common.setdefault(key, []).append(row["accepted"] == "True")
        accepted_keys = {key for key, values in common.items() if len(values) == 5 and all(values)}
        for variant in ("native", "warmup", "modulation", "decay", "load", "aligned"):
            selected = [r for r in group if r["variant"] == variant]
            accepted = [r for r in selected if r["low"] != "9000" and r["accepted"] == "True"]
            paired = [r for r in accepted if (r["program"], r["low"], r["high"]) in accepted_keys]
            errors = [abs(float(r["delta_percent"])) for r in accepted]
            paired_errors = [abs(float(r["delta_percent"])) for r in paired]
            summary.append(dict(seed=seed, warmup_ms=warmup, variant=variant, accepted=len(accepted),
                                mean_T20_error_percent=sum(errors)/len(errors) if errors else None,
                                max_T20_error_percent=max(errors, default=None),
                                max_energy_error_db=max(abs(float(r["energy_db"])) for r in selected),
                                common_fits=len(paired),
                                common_mean_T20_error_percent=sum(paired_errors)/len(paired_errors) if paired_errors else None))
    return summary


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    executable, roms, bank, output = (Path(x).resolve() for x in sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    fixtures = [(p, 17, ".08", ms) for ms in (250, 1000, 1047) for p in range(6)]
    fixtures += [(p, 224, ".12", 1000) for p in range(6)]

    def render(case):
        program, seed, amplitude, warmup = case
        directory = output / f"p{program}-s{seed}-w{warmup}"
        with (output / f"p{program}-s{seed}-w{warmup}.log").open("w") as log:
            subprocess.run([str(executable), str(roms), str(bank), str(directory), str(program), "3",
                            "noise", str(seed), amplitude, str(warmup), "--startup-probe"],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        analyze_sound.analyze(directory)
        with (directory / "bands.csv").open() as file:
            return list(csv.DictReader(file))

    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for result in pool.map(render, fixtures):
            rows.extend(result)
    with (output / "bands.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, rows[0].keys());writer.writeheader();writer.writerows(rows)
    summary = summarize(rows)
    with (output / "summary.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, summary[0].keys());writer.writeheader();writer.writerows(summary)
    manifest = dict(cases=len(fixtures), bank_sha256=hashlib.sha256(bank.read_bytes()).hexdigest(),
                    executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    variants=dict(native="Unchanged historical normal startup",
                                  warmup="Bank state with 300 ms extra native warmup",
                                  modulation="Reference modulation state at mode enable, then equal warmup; bank decay state",
                                  decay="Reference decay state at mode enable, then equal warmup; bank modulation state",
                                  load="Reference controller state at mode enable, then equal warmup; independent native clocks",
                                  aligned="Reference controller state restored at input start; independent native clocks"))
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
    print("24 fixtures saved; all extra variants remain offline diagnostics", flush=True)


if __name__ == "__main__":
    main()
