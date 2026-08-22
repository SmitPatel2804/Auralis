#!/usr/bin/env bash

set -euo pipefail

mode="${1:-runtime}"
if [[ "$mode" != "runtime" && "$mode" != "persistent" ]]; then
    echo "usage: $0 runtime|persistent [build-dir]" >&2
    exit 2
fi

# Prefer an explicit argument, then AURALIS_BUILD_DIR, then the CI default.
# Do not assume a user-owned tree named `build`.
build_dir="${2:-${AURALIS_BUILD_DIR:-build}}"
if [[ ! -d "$build_dir" ]]; then
    echo "error: build directory '$build_dir' does not exist" >&2
    exit 1
fi
# Resolve before dbus-run-session so a relative path still works.
build_dir="$(cd -- "$build_dir" && pwd)"

runtime_dir="$(mktemp -d)"
config_home="$(mktemp -d)"
cleanup() {
    fusermount -u "$runtime_dir/gvfs" 2>/dev/null || true
    rm -rf -- "$runtime_dir" "$config_home" 2>/dev/null || true
}
trap cleanup EXIT
chmod 700 "$runtime_dir" "$config_home"

if [[ "$mode" == "persistent" ]]; then
    mkdir -p "$config_home/pipewire/pipewire.conf.d"
    cp data/pipewire/90-auralis-virtual-output.conf \
        "$config_home/pipewire/pipewire.conf.d/"
fi

XDG_RUNTIME_DIR="$runtime_dir" \
XDG_CONFIG_HOME="$config_home" \
AURALIS_EXPECT_VIRTUAL_OUTPUT_PERSISTENCE="$mode" \
AURALIS_CTEST_DIR="$build_dir" \
dbus-run-session -- bash -euo pipefail -c '
    pipewire >"$XDG_RUNTIME_DIR/pipewire.log" 2>&1 &
    pipewire_pid=$!
    trap "kill $pipewire_pid 2>/dev/null || true" EXIT

    for attempt in {1..100}; do
        test -S "$XDG_RUNTIME_DIR/pipewire-0" && break
        sleep 0.05
    done
    if ! test -S "$XDG_RUNTIME_DIR/pipewire-0"; then
        cat "$XDG_RUNTIME_DIR/pipewire.log" >&2
        exit 1
    fi

    # Loopback nodes get audio ports from the session manager. A bare PipeWire
    # daemon on 1.6+ can publish the nodes with zero ports, which Auralis
    # correctly treats as not ready.
    wireplumber >"$XDG_RUNTIME_DIR/wireplumber.log" 2>&1 &
    wireplumber_pid=$!
    trap "kill $pipewire_pid $wireplumber_pid 2>/dev/null || true" EXIT

    AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
    ctest --test-dir "$AURALIS_CTEST_DIR" -R "^tst_PipeWireLiveIntegration$" \
        --output-on-failure --timeout 30
'
