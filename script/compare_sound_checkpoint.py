"""Paired normal-startup sound regression against a frozen Git checkpoint.

Usage: python script/compare_sound_checkpoint.py REV ROM_DIRECTORY BANK OUTPUT
Requires CMake, a C++ compiler, NumPy, and the configured pinned Reflexion export.
All ROM-derived data and renders stay in the requested private output directory.
"""
import concurrent.futures
import csv
import os
from pathlib import Path
import subprocess
import sys
import analyze_sound


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    repo = Path(__file__).resolve().parents[1]
    revision = subprocess.check_output(["git", "rev-parse", "--verify", sys.argv[1] + "^{commit}"], cwd=repo, text=True).strip()
    roms, bank, output = (Path(x).resolve() for x in sys.argv[2:])
    output.mkdir(parents=True, exist_ok=True)
    reference = repo / "build/deps/reflexion"
    if not (reference / "emulator/host.hpp").exists():
        raise SystemExit("Configure the repository first to prepare its pinned Reflexion export")
    env = {key.upper(): value for key, value in os.environ.items()} if os.name == "nt" else os.environ.copy()
    paths = ["native-hall/core", "native-hall/desktop", "native-hall/import",
             "native-hall/tools/sound_compare.cpp", "native-hall/tools/reference224.hpp"]
    executables = {}
    for label in ("baseline", "current"):
        root = output / label
        args = ["git", "ls-tree", "-r", "--name-only", revision, *paths] if label == "baseline" else [
            "git", "ls-files", "--cached", "--others", "--exclude-standard", *paths]
        for name in subprocess.check_output(args, cwd=repo, text=True).splitlines():
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            data = subprocess.check_output(["git", "show", revision + ":" + name], cwd=repo) if label == "baseline" else (repo / name).read_bytes()
            target.write_bytes(data)
        (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(SoundCheckpoint LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
if(MSVC)
  add_compile_options(/fp:strict)
  add_compile_definitions(_USE_MATH_DEFINES)
else()
  add_compile_options(-ffp-contract=off)
endif()
include_directories("''' + reference.as_posix() + '''")
add_executable(sound_compare native-hall/tools/sound_compare.cpp
  native-hall/core/hall.cpp native-hall/import/bank_import.cpp)
''', encoding="utf-8")
        subprocess.run(["cmake", "-S", str(root), "-B", str(root / "compiled"), "-DCMAKE_BUILD_TYPE=Release"], env=env, check=True)
        subprocess.run(["cmake", "--build", str(root / "compiled"), "--config", "Release", "--parallel", "2"], env=env, check=True)
        executables[label] = next(p for p in (root / "compiled/Release/sound_compare.exe",
            root / "compiled/sound_compare.exe", root / "compiled/sound_compare") if p.exists())
    fixtures = [(p, m, "noise", 17, ".08", 1000) for p in range(6) for m in (0, 2, 3)]
    fixtures += [(p, 3, "noise", 224, ".12", 1000) for p in range(6)]
    fixtures += [(p, 3, "music", 73, ".12", 1047) for p in range(6)]

    def render(case):
        p, m, fixture, seed, amplitude, warmup = case
        name = f"p{p}-m{m}-{fixture}-s{seed}"
        directories = {}
        for label in ("baseline", "current"):
            directory = output / "renders" / label / name
            subprocess.run([str(executables[label]), str(roms), str(bank), str(directory), str(p), str(m),
                            fixture, str(seed), amplitude, str(warmup)], env=env, check=True, stdout=subprocess.DEVNULL)
            directories[label] = directory
        for filename in ("input.wav", "reference.wav"):
            if (directories["baseline"] / filename).read_bytes() != (directories["current"] / filename).read_bytes():
                raise RuntimeError(f"The paired fixture/reference changed: {name}/{filename}")
        rows = []
        for label, directory in directories.items():
            analyze_sound.analyze(directory)
            with (directory / "bands.csv").open() as file:
                rows.extend(dict(row, checkpoint=label) for row in csv.DictReader(file))
        print("Paired fixture complete:", name, flush=True)
        return rows

    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for result in pool.map(render, fixtures):
            rows.extend(result)
    with (output / "bands.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    (output / "baseline.txt").write_text(revision + "\n", encoding="utf-8")
    print("Thirty paired fixtures saved; aligned variants remain diagnostic", flush=True)


if __name__ == "__main__":
    main()
