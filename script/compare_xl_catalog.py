#!/usr/bin/env python3
"""Run ROM-private XL comparisons per program, retaining failures and provenance.

All recipes run independent reference/native clocks. This produces measurements,
not an automatic sound-acceptance verdict. Never use the user's plugin cache.
"""
import argparse
import csv
import hashlib
import json
import re
import subprocess
import sys
import platform
import concurrent.futures
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NON_REVERBS = {"chorus_echo", "resonant_chords", "multiband_delay"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def catalog():
    pattern = r'CINEOL_XL_GRAPH\((\w+),"([^"]+)",(\d+),(\d+),(\d+)\)'
    records = re.findall(pattern, (ROOT / "native-hall/desktop/graph_list.inc").read_text())
    if len(records) != 22:
        raise ValueError("Expected the existing 22-program XL catalog")
    return [dict(index=i, id=row[0], name=row[1], rows=int(row[2]), bank=int(row[3]),
                 physical_program=int(row[4]), reverb=row[0] not in NON_REVERBS)
            for i, row in enumerate(records)]


def recipes(programs, scope):
    cases = []

    def add(p, suffix, mode=0, fixture="noise", seed=17, level=".08", warmup=1000,
            options=(), seconds=12):
        opts = list(options)
        if p["id"] == "inverse_room":
            opts.append("--audible-levels")
        if mode == 0:
            opts.append("--static-wcs")
        cases.append(dict(name=f"p{p['index']:02d}-{suffix}", index=p["index"], mode=mode,
                          fixture=fixture, seed=seed, amplitude=level, warmup_ms=warmup,
                          duration_s=seconds, block=256, analog=1, options=opts))

    # Every program is reached early, rather than completing all modes of one
    # program before discovering a catalog-specific preparation failure.
    if scope in ("baseline", "all"):
        for p in programs:
            add(p, "m0-noise17")
        for p in programs:
            add(p, "m0-noise991-quiet250", seed=991, level=".02", warmup=250)
    if scope in ("modes", "all"):
        for mode in range(1, 8):
            for p in programs:
                if p["reverb"] or mode == 1:
                    add(p, f"m{mode}-noise17", mode=mode)
    if scope in ("listening", "all"):
        for fixture in ("impulse", "music"):
            for p in programs:
                add(p, f"m0-{fixture}", fixture=fixture)
    if scope in ("routing", "all"):
        for p in programs:
            if p["index"] < 14:
                continue
            for side in ("left", "right"):
                for pair, channels in (("ac", "0:2"), ("bd", "1:3")):
                    add(p, f"m0-impulse-{side}-{pair}", fixture=f"impulse-{side}",
                        options=[f"--outputs={channels}"])
    if scope in ("gate", "all"):
        for mode in (0, 4, 7):
            for p in programs:
                if p["reverb"]:
                    add(p, f"m{mode}-gate", mode=mode, fixture="music",
                        options=["--gate-controls"])
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("bank", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--tool", type=Path, default=ROOT / "build/xl-sound-validation/native-hall/cineol_xl_sound_compare")
    parser.add_argument("--scope", choices=["baseline", "modes", "listening", "routing", "gate", "all"], default="all")
    parser.add_argument("--indices", type=int, nargs="+")
    parser.add_argument("--jobs", type=int, default=2, choices=[1, 2],
                        help="Independent offline render processes; CPU benchmarks must run separately")
    args = parser.parse_args()
    args.output = args.output.resolve();args.bank = args.bank.resolve();args.tool = args.tool.resolve()
    args.rom_directory = args.rom_directory.resolve()
    if ROOT / "build" not in args.output.parents:
        parser.error("Private catalog evidence must be under this repository's ignored build/")
    programs = catalog()
    if args.indices is not None:
        if not args.indices or any(index not in range(22) for index in args.indices):
            parser.error("XL indices must be 0..21")
        programs = [p for p in programs if p["index"] in args.indices]
    cases = recipes(programs, args.scope)
    args.output.mkdir(parents=True, exist_ok=True)
    identity = dict(bank_sha256=sha(args.bank), renderer_sha256=sha(args.tool),
                    analyzer_sha256=sha(ROOT / "script/analyze_xl_sound.py"),
                    runner_sha256=sha(Path(__file__)), native_revision=subprocess.check_output(
                        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                    reference_revision=subprocess.check_output(["git", "-C", "deps/reflexion", "rev-parse", "HEAD"],
                        cwd=ROOT, text=True).strip(), architecture=platform.machine(), python=sys.version,
                    compiler=subprocess.check_output(["c++", "--version"],text=True).splitlines()[0],
                    export_stamp=(ROOT / "build/deps/reflexion/.cineol-export").read_text(),
                    rom_sha256={p.name:sha(p) for p in sorted(args.rom_directory.glob("*.BIN"))},
                    source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ("native-hall/desktop", "native-hall/core")
                                   for p in sorted((ROOT / folder).iterdir()) if p.suffix in (".hpp", ".inc", ".cpp")})
    def command_for(case):
        output = args.output / case["name"]
        return [str(args.tool), str(args.rom_directory), str(args.bank), str(output),
                   str(case["index"]), str(case["mode"]), case["fixture"], str(case["seed"]),
                   case["amplitude"], str(case["warmup_ms"]), str(case["duration_s"]),
                   str(case["block"]), str(case["analog"]), *case["options"]]
    def matches(previous, command):
        comparable = lambda value: {k:v for k,v in value.items() if k != "runner_sha256"}
        same_command = previous["command"][:1] + previous["command"][2:] == command[:1] + command[2:]
        return same_command and comparable(previous["identity"]) == comparable(identity)
    # Reject changed recipes/tools before replacing a completed campaign plan.
    for case in cases:
        saved = args.output / case["name"] / "run.json"
        if saved.exists() and not matches(json.loads(saved.read_text()), command_for(case)):
            raise ValueError(f"Changed recipe/tool identity for {saved.parent}; use a fresh output directory")
    (args.output / f"{args.scope}-plan.json").write_text(json.dumps(dict(identity=identity, programs=programs, cases=cases), indent=2) + "\n")
    def run_case(item):
        number, case = item
        output = args.output / case["name"]
        command = command_for(case)
        saved = output / "run.json"
        if saved.exists():
            previous = json.loads(saved.read_text())
            # Orchestration-only changes do not alter a completed renderer or
            # analyzer recipe. Preserve the previous runner hash in its record.
            # A byte-identical private ROM copy may move out of cloud storage.
            # Compare every other argument and the complete chip hash map; keep
            # the original actual command in each retained case's provenance.
            if not matches(previous, command):
                raise ValueError(f"Changed recipe/tool identity for {output}; use a fresh output directory")
            if previous["exit_code"] == 0:
                print(f"[{number}/{len(cases)}] retained {case['name']}", flush=True)
                return None
        output.mkdir(exist_ok=True)
        print(f"[{number}/{len(cases)}] render {case['name']}", flush=True)
        if saved.exists() and previous.get("renderer_exit_code") == 0 and (output / "fixture.csv").exists():
            code=0;log=(output / "run.log").read_text()
        else:
            try:
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
                log = result.stdout + result.stderr;code = result.returncode
            except subprocess.TimeoutExpired as error:
                def decode(value):
                    return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
                log = decode(error.stdout) + decode(error.stderr) + "\nRenderer exceeded the 180-second wall-time limit.\n"
                code = 124
        renderer_code=code
        if not code:
            analysis = subprocess.run([sys.executable, str(ROOT / "script/analyze_xl_sound.py"), str(output)],
                                      cwd=ROOT, capture_output=True, text=True)
            log += analysis.stdout + analysis.stderr;code = analysis.returncode
        (output / "run.log").write_text(log)
        saved.write_text(json.dumps(dict(command=command, identity=identity, render_timeout_s=180,
                                        renderer_exit_code=renderer_code, exit_code=code), indent=2) + "\n")
        if code:
            print(f"  FAILED {code}: {log[-600:].strip()}", flush=True)
        else:
            summary = json.loads((output / "summary.json").read_text())
            print(f"  measured energy={summary['overall_energy_error_db']} dB; fitted bands={summary['usable_paired_bands']}; signal={summary['signal_observed_both_paths']}", flush=True)
        return case["name"] if code else None
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        failed = [name for name in executor.map(run_case, enumerate(cases, 1)) if name is not None]
    (args.output / f"{args.scope}-status.json").write_text(json.dumps(dict(total=len(cases), failed=failed, identity=identity), indent=2) + "\n")
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
