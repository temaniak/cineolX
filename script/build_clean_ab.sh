#!/bin/bash
set -euo pipefail
clean_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$clean_root"
clean_run=false
if [[ "${1:-}" == "--run" ]]; then clean_run=true; shift; fi
clean_options=()
# Reuse the already pinned local JUCE source when available; otherwise CMake
# resolves the repository's pinned version normally.
clean_juce="$(python3 - <<'PY'
from pathlib import Path
for name in ['build/plugin/CMakeCache.txt']:
 p=Path(name)
 if not p.exists():continue
 for line in p.read_text().splitlines():
  if line.startswith('FETCHCONTENT_SOURCE_DIR_JUCE:PATH='):
   value=line.split('=',1)[1]
   if Path(value,'CMakeLists.txt').exists():print(value);raise SystemExit
PY
)"
if [[ -n "$clean_juce" ]]; then clean_options+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$clean_juce"); fi
cmake -S . -B build/clean-ab -DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_CLEAN_AB=ON -DNATIVE_HALL_BUILD_TOOLS=OFF "${clean_options[@]}" "$@"
cmake --build build/clean-ab --config Release --parallel 4 --target CineolCleanAB_AU CineolCleanAB_VST3 CineolCleanAB_Standalone cineol_clean_check cineol_clean_zero_check cineol_clean_plugin_check
if $clean_run; then
    pkill -x 'Cineol Clean AB' || true
    open -n "$clean_root/build/clean-ab/native-hall/clean-ab/CineolCleanAB_artefacts/Release/Standalone/Cineol Clean AB.app"
fi
