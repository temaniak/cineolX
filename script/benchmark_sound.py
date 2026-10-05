"""Paired original-224 desktop CPU benchmark against a frozen Git revision.

Usage: python script/benchmark_sound.py BASELINE_REV BANK OUTPUT_DIRECTORY
Needs Git, CMake and a C++ compiler. No emulator/ROMs/audio device are needed.
Run with no other builds or measurement jobs active. Outputs remain private.
"""
import csv
import os
from pathlib import Path
import statistics
import subprocess
import sys

def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    repo = Path(__file__).resolve().parents[1]
    baseline = subprocess.check_output(["git", "rev-parse", "--verify", sys.argv[1] + "^{commit}"], cwd=repo, text=True).strip()
    bank = Path(sys.argv[2]).resolve()
    output = Path(sys.argv[3]).resolve()
    # Only produce build/checkpoint exports within the requested directory.
    output.mkdir(parents=True, exist_ok=True)
    env = {key.upper(): value for key, value in os.environ.items()} if os.name == "nt" else os.environ.copy()
    names = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", baseline,
                                     "native-hall/core", "native-hall/desktop"], cwd=repo, text=True).splitlines()
    executables = {}
    for label in ("baseline", "current"):
        root = output / label
        source_names = names if label == "baseline" else subprocess.check_output(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             "native-hall/core", "native-hall/desktop"], cwd=repo, text=True).splitlines()
        for name in source_names:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            data = subprocess.check_output(["git", "show", baseline + ":" + name], cwd=repo) if label == "baseline" else (repo / name).read_bytes()
            target.write_bytes(data)
        tool = root / "native-hall/tools/cpu_benchmark.cpp"
        tool.parent.mkdir(parents=True, exist_ok=True)
        tool.write_bytes((repo / "native-hall/tools/cpu_benchmark.cpp").read_bytes())
        (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(SoundCpuCheckpoint LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
if(MSVC)
  add_compile_options(/fp:strict)
else()
  add_compile_options(-ffp-contract=off)
endif()
add_executable(sound_cpu native-hall/tools/cpu_benchmark.cpp native-hall/core/hall.cpp)
''', encoding="utf-8")
        subprocess.run(["cmake", "-S", str(root), "-B", str(root / "compiled"), "-DCMAKE_BUILD_TYPE=Release"], env=env, check=True)
        subprocess.run(["cmake", "--build", str(root / "compiled"), "--config", "Release", "--parallel", "1"], env=env, check=True)
        candidates = [root / "compiled/Release/sound_cpu.exe", root / "compiled/sound_cpu.exe", root / "compiled/sound_cpu"]
        executables[label] = next(path for path in candidates if path.exists())
    samples = {}
    checksums = {}
    for repeat in range(5):
        for label in (("baseline", "current") if repeat % 2 == 0 else ("current", "baseline")):
            path = output / f"{label}-{repeat}.csv"
            subprocess.run([str(executables[label]), str(bank), str(path)], env=env, check=True)
            with path.open() as file:
                for row in csv.DictReader(file):
                    key = (int(row["program"]), int(row["mode"]))
                    samples.setdefault((label, key), []).append(float(row["seconds"]))
                    checksums.setdefault((label, key), set()).add(row["checksum"])
    rows = []
    for program in range(6):
        for mode in range(3):
            key = (program, mode)
            a = statistics.median(samples["baseline", key]); b = statistics.median(samples["current", key])
            if checksums["baseline", key] != checksums["current", key]:
                print(f"Output changed: program={program}, mode={mode}")
            rows.append(dict(program=program, mode=mode, baseline_median_s=a, current_median_s=b, ratio=b / a))
    with (output / "cpu.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, rows[0].keys()); writer.writeheader(); writer.writerows(rows)
    (output / "baseline.txt").write_text(baseline + "\n", encoding="utf-8")
    print("Summed median CPU ratio:", sum(r["current_median_s"] for r in rows) / sum(r["baseline_median_s"] for r in rows))
    print("Case ratio range:", min(r["ratio"] for r in rows), max(r["ratio"] for r in rows))

if __name__ == "__main__":
    main()
