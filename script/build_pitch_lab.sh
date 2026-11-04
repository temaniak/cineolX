#!/bin/bash
set -euo pipefail
pitch_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$pitch_root"
pitch_args=()
pitch_juce="$(python3 - <<'PY'
from pathlib import Path
for name in ['build/dual-hall/CMakeCache.txt','build/clean-main/CMakeCache.txt','build/plugin/CMakeCache.txt']:
    p=Path(name)
    if not p.exists():continue
    for line in p.read_text().splitlines():
        if line.startswith('FETCHCONTENT_SOURCE_DIR_JUCE:PATH='):
            v=line.split('=',1)[1]
            if Path(v,'CMakeLists.txt').exists():print(v);raise SystemExit
PY
)"
if [[ -n "$pitch_juce" ]]; then pitch_args+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$pitch_juce"); fi
for pitch_dependency in linear stretch; do
    pitch_source="$pitch_root/build/deps/signalsmith-$pitch_dependency"
    if [[ -f "$pitch_source/CMakeLists.txt" ]]; then
        if [[ "$pitch_dependency" == linear ]]; then pitch_key=PITCH_SIGNALSMITH_LINEAR; else pitch_key=PITCH_SIGNALSMITH_STRETCH; fi
        pitch_args+=("-DFETCHCONTENT_SOURCE_DIR_$pitch_key=$pitch_source")
    fi
done
cmake -S . -B build/pitch-lab -DCMAKE_BUILD_TYPE=Release -DNATIVE_HALL_BUILD_TOOLS=OFF -DCINEOL_BUILD_PITCH_LAB=ON "${pitch_args[@]}" "$@"
pitch_targets=(CineolPitchLab_VST3 CineolPitchLab_Standalone cineol_pitch_lab_core_check cineol_pitch_lab_plugin_check)
if [[ "$(uname -s)" == Darwin ]]; then pitch_targets+=(CineolPitchLab_AU); fi
cmake --build build/pitch-lab --config Release --parallel 3 --target "${pitch_targets[@]}"
"$pitch_root/build/pitch-lab/native-hall/pitch-lab/cineol_pitch_lab_plugin_check_artefacts/Release/cineol_pitch_lab_plugin_check" --manifest "$pitch_root/build/pitch-lab/parameters.json"
