#!/usr/bin/env bash

set -euo pipefail

mode="${1:-runtime}"
if [[ "$mode" != "runtime" && "$mode" != "persistent" ]]; then
    echo "usage: $0 runtime|persistent" >&2
    exit 2
fi

runtime_dir="$(mktemp -d)"
config_home="$(mktemp -d)"
cleanup() {
    rm -rf -- "$runtime_dir" "$config_home"
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

    AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
    ctest --test-dir build -R "^tst_PipeWireLiveIntegration$" \
        --output-on-failure --timeout 30
'
