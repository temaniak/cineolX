#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TASK_BUILD="$TASK_ROOT/build/plugin"
TASK_ROM="${NATIVE_HALL_ROM_DIR:-$TASK_ROOT/firmware/224 v4_4}"
TASK_MODE="${1:---build-only}"
case "$TASK_MODE" in --build-only|--check|--compare) ;; *) echo 'usage: build_plugin.sh [--build-only|--check|--compare]' >&2;exit 2;; esac
[[ $# -le 1 ]] || { echo 'Only one mode is accepted.' >&2;exit 2; }
if [[ "$TASK_MODE" != --build-only ]]; then
    python3 "$TASK_ROOT/native-hall/daisy/tools/verify_roms.py" "$TASK_ROM"
fi
TASK_ARGS=(-DCMAKE_BUILD_TYPE=Release -DCINEOL_BUILD_PLUGIN=ON -DNATIVE_HALL_BUILD_TOOLS=ON
           "-DNATIVE_HALL_ROM_DIR=$TASK_ROM")
if [[ -n "${JUCE_DIR:-}" ]]; then TASK_ARGS+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_DIR");fi
cmake -S "$TASK_ROOT" -B "$TASK_BUILD" "${TASK_ARGS[@]}"
TASK_FORMATS=(NativeHall224_VST3 NativeHall224_Standalone)
if [[ "$(uname -s)" == Darwin ]]; then TASK_FORMATS+=(NativeHall224_AU);fi
cmake --build "$TASK_BUILD" --parallel "${BUILD_JOBS:-6}" --target \
    "${TASK_FORMATS[@]}" native_hall_plugin_check cineol_rom_import_check
TASK_CHECK="$TASK_BUILD/cineol_rom_import_check_artefacts/Release/cineol_rom_import_check"
TASK_CACHE="$(mktemp -d "${TMPDIR:-/tmp}/cineol-import-check.XXXXXX")"
trap 'rm -rf "$TASK_CACHE"' EXIT
CINEOL224_CACHE_DIR="$TASK_CACHE" "$TASK_CHECK" --empty
if [[ "$TASK_MODE" != --build-only ]]; then
    cmake --build "$TASK_BUILD" --parallel "${BUILD_JOBS:-6}" --target \
        native_hall_core_check native_224_bank_check native_224_dac_check native_224_adc_check native_224_controllers_check native_hall_compare
    "$TASK_BUILD/native-hall/native_hall_core_check" "$TASK_BUILD/native-hall/hall-v44.hall224"
    "$TASK_BUILD/native-hall/native_224_bank_check" "$TASK_BUILD/native-hall/programs-v44.bank224"
    "$TASK_BUILD/native-hall/native_224_dac_check" "$TASK_BUILD/native-hall/programs-v44.bank224"
    "$TASK_BUILD/native-hall/native_224_adc_check" "$TASK_BUILD/native-hall/programs-v44.bank224"
    "$TASK_BUILD/native-hall/native_224_controllers_check" "$TASK_BUILD/native-hall/programs-v44.bank224"
    "$TASK_BUILD/native_hall_plugin_check_artefacts/Release/native_hall_plugin_check" \
        --bank "$TASK_BUILD/native-hall/programs-v44.bank224"
    CINEOL224_CACHE_DIR="$TASK_CACHE" "$TASK_CHECK" --cancel "$TASK_ROM"
    CINEOL224_CACHE_DIR="$TASK_CACHE" "$TASK_CHECK" --import "$TASK_ROM" "$TASK_BUILD/native-hall/programs-v44.bank224"
    CINEOL224_CACHE_DIR="$TASK_CACHE" "$TASK_CHECK" --cached
fi
if [[ "$TASK_MODE" == --compare ]]; then
    "$TASK_BUILD/native-hall/native_hall_compare" "$TASK_ROM" "$TASK_BUILD/native-hall/programs-v44.bank224" \
        "$TASK_ROOT/build/validation"
fi
echo "Cineol-X 224 bundles: $TASK_BUILD/NativeHall224_artefacts/Release"
