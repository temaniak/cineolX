#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "${1:-}" in '') TASK_DEPS=(reflexion);; --daisy) TASK_DEPS=(reflexion libDaisy);;
  *) echo 'usage: setup_dependencies.sh [--daisy]' >&2;exit 2;; esac
[[ $# -le 1 ]] || { echo 'Only one option is accepted.' >&2;exit 2; }
for TASK_DEP in "${TASK_DEPS[@]}"; do
  if [[ "$(git -C "$TASK_ROOT" rev-parse --show-toplevel 2>/dev/null || true)" == "$TASK_ROOT" ]]; then
    git -C "$TASK_ROOT" submodule update --init --recursive "deps/$TASK_DEP"
  else
    TASK_URL="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]]["url"])' "$TASK_ROOT/dependencies.json" "$TASK_DEP")"
    TASK_PIN="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]]["commit"])' "$TASK_ROOT/dependencies.json" "$TASK_DEP")"
    if [[ ! -d "$TASK_ROOT/deps/$TASK_DEP/.git" ]]; then
      git clone "$TASK_URL" "$TASK_ROOT/deps/$TASK_DEP"
    fi
    git -C "$TASK_ROOT/deps/$TASK_DEP" checkout --detach "$TASK_PIN"
    git -C "$TASK_ROOT/deps/$TASK_DEP" submodule update --init --recursive
  fi
done
