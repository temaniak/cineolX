#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TASK_ROM="${NATIVE_HALL_ROM_DIR:-$TASK_ROOT/firmware/224 v4_4}"
TASK_BOARD="${DAISY_BOARD:-seed}"
TASK_MODE="${1:---check}"
case "$TASK_MODE" in --check|--build-only|--flash) ;; *) echo 'usage: build_daisy.sh [--check|--build-only|--flash]' >&2;exit 2;; esac
[[ $# -le 1 ]] || { echo 'Only one mode is accepted.' >&2;exit 2; }
python3 "$TASK_ROOT/native-hall/daisy/tools/verify_roms.py" "$TASK_ROM"
python3 "$TASK_ROOT/script/prepare_dependency.py" libDaisy \
    --source "${LIBDAISY_DIR:-$TASK_ROOT/deps/libDaisy}" --output "$TASK_ROOT/build/deps/libDaisy"
cmake -S "$TASK_ROOT" -B "$TASK_ROOT/build/rom" -DCMAKE_BUILD_TYPE=Release \
    -DCINEOL_BUILD_PLUGIN=OFF -DNATIVE_HALL_BUILD_TOOLS=ON "-DNATIVE_HALL_ROM_DIR=$TASK_ROM"
cmake --build "$TASK_ROOT/build/rom" --parallel "${BUILD_JOBS:-6}" --target native_224_bank_check
if [[ "$TASK_MODE" != --build-only ]]; then
    python3 "$TASK_ROOT/native-hall/daisy/tests/rom_build_check.py" "$TASK_ROM"
    "$TASK_ROOT/build/rom/native-hall/native_224_bank_check" "$TASK_ROOT/build/rom/native-hall/programs-v44.bank224"
    clang++ -std=c++17 -O2 -ffp-contract=off "$TASK_ROOT/native-hall/daisy/tests/controls_check.cpp" \
        "$TASK_ROOT/native-hall/core/hall.cpp" -o "$TASK_ROOT/build/rom/controls_check"
    "$TASK_ROOT/build/rom/controls_check" "$TASK_ROOT/build/rom/native-hall/programs-v44.bank224"
    clang++ -std=c++17 -O2 "$TASK_ROOT/native-hall/daisy/tests/adapter_check.cpp" \
        -o "$TASK_ROOT/build/rom/adapter_check"
    "$TASK_ROOT/build/rom/adapter_check"
fi
mkdir -p "$TASK_ROOT/build/daisy/libdaisy" "$TASK_ROOT/build/daisy/$TASK_BOARD"
make -C "$TASK_ROOT/native-hall/daisy" -j"${BUILD_JOBS:-6}" all \
    "BUILD_DIR=$TASK_ROOT/build/daisy/$TASK_BOARD" "LIBDAISY_DIR=$TASK_ROOT/build/deps/libDaisy" \
    "PROFILE=$TASK_ROOT/build/rom/native-hall/programs-v44.bank224" \
    "NATIVE_HALL_ROM_DIR=$TASK_ROM" "DAISY_BOARD=$TASK_BOARD"
if [[ "$TASK_MODE" == --flash ]]; then
    make -C "$TASK_ROOT/native-hall/daisy" flash "BUILD_DIR=$TASK_ROOT/build/daisy/$TASK_BOARD" \
        "LIBDAISY_DIR=$TASK_ROOT/build/deps/libDaisy" "PROFILE=$TASK_ROOT/build/rom/native-hall/programs-v44.bank224" \
        "NATIVE_HALL_ROM_DIR=$TASK_ROM" "DAISY_BOARD=$TASK_BOARD"
fi
echo "Firmware: $TASK_ROOT/build/daisy/$TASK_BOARD/Cineol224_Daisy.bin"
