#!/bin/bash
set -euo pipefail
dual_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$dual_root"
dual_options=()
dual_juce="$(python3 - <<'PY'
from pathlib import Path
for name in ['build/clean-main/CMakeCache.txt','build/plugin/CMakeCache.txt','build/float-ab/CMakeCache.txt']:
    path=Path(name)
    if not path.exists():continue
    for line in path.read_text().splitlines():
        if line.startswith('FETCHCONTENT_SOURCE_DIR_JUCE:PATH='):
            value=line.split('=',1)[1]
            if Path(value,'CMakeLists.txt').exists():print(value);raise SystemExit
PY
)"
if [[ -n "$dual_juce" ]]; then dual_options+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$dual_juce"); fi
cmake -S . -B build/dual-hall -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_DUAL_HALL=ON \
    -DNATIVE_HALL_BUILD_TOOLS=OFF "${dual_options[@]}" "$@"
dual_targets=(CineolDualHallB_VST3 CineolDualHallB_Standalone cineol_dual_hall_check cineol_dual_hall_plugin_check)
if [[ "$(uname -s)" == Darwin ]]; then dual_targets+=(CineolDualHallB_AU); fi
cmake --build build/dual-hall --config Release --parallel 3 --target "${dual_targets[@]}"
