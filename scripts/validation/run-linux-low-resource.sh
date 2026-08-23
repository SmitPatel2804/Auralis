#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "This validation runner requires Linux." >&2
    exit 2
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd -- "$repo_root"

build_jobs="${AURALIS_BUILD_JOBS:-1}"
case "$build_jobs" in
    1|2) ;;
    *)
        echo "AURALIS_BUILD_JOBS must be 1 or 2 for this bounded runner." >&2
        exit 2
        ;;
esac

cmake --preset linux-low-resource
cmake --build --preset linux-low-resource --parallel "$build_jobs"
ctest --preset linux-low-resource

echo "Low-resource deterministic validation completed."
echo "Live hardware tests remain opt-in; run them individually with ctest -j1."
