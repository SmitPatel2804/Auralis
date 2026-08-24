#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Phase 9 Linux RC qualification requires Linux." >&2
    exit 2
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd -- "$repo_root"

usage() {
    cat <<'EOF'
Usage: scripts/validation/run-phase9-linux-rc.sh MODE

Modes:
  preflight   Capture read-only repository, host, Bluetooth, PipeWire, and input state.
  software    Fresh RelWithDebInfo build, default CTest, and bounded stress runs.
  sanitizer   Fresh ASan/UBSan build and hardware-independent CTest.
  live        Opt-in live integration tests. Requires AURALIS_PHASE9_RUN_LIVE=1.
  package     Fresh Release build/test, staged install, CPack, DEB inspection/extraction/smoke.
  all-safe    Run preflight, software, sanitizer, and package; never runs live/disruptive tests.

Important environment variables:
  AURALIS_PHASE9_EVIDENCE_DIR       Default: audit-phase9-linux-rc
  AURALIS_PHASE9_BASELINE_BUILD     Default: build-phase9-baseline
  AURALIS_PHASE9_ASAN_BUILD         Default: build-phase9-asan
  AURALIS_PHASE9_RELEASE_BUILD      Default: build-phase9-release
  AURALIS_BUILD_JOBS                1 or 2; default: 1
  AURALIS_PHASE9_RUN_LIVE           Must be 1 for live mode
  AURALIS_EXPECT_DEVICE_ADDRESS     One real connected Bluetooth address
  AURALIS_EXPECT_DEVICE_ADDRESSES   Two addresses separated by comma or semicolon

The runner never uses sudo, restarts services, changes adapter power, suspends,
reboots, pairs, forgets, connects, or disconnects devices. Those operations and
physical button/audio observations remain operator-controlled.
EOF
}

mode="${1:-}"
case "$mode" in
    preflight|software|sanitizer|live|package|all-safe) ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

build_jobs="${AURALIS_BUILD_JOBS:-1}"
case "$build_jobs" in
    1|2) ;;
    *) echo "AURALIS_BUILD_JOBS must be 1 or 2." >&2; exit 2 ;;
esac

evidence_root="${AURALIS_PHASE9_EVIDENCE_DIR:-audit-phase9-linux-rc}"
baseline_build="${AURALIS_PHASE9_BASELINE_BUILD:-build-phase9-baseline}"
asan_build="${AURALIS_PHASE9_ASAN_BUILD:-build-phase9-asan}"
release_build="${AURALIS_PHASE9_RELEASE_BUILD:-build-phase9-release}"

case "$evidence_root" in
    /*) ;;
    *) evidence_root="$repo_root/$evidence_root" ;;
esac

mkdir -p \
    "$evidence_root/baseline" \
    "$evidence_root/builds" \
    "$evidence_root/ctest" \
    "$evidence_root/sanitizer" \
    "$evidence_root/bluetooth" \
    "$evidence_root/pipewire" \
    "$evidence_root/routing" \
    "$evidence_root/buttons" \
    "$evidence_root/recovery" \
    "$evidence_root/suspend-resume" \
    "$evidence_root/soak" \
    "$evidence_root/package" \
    "$evidence_root/diagnostics" \
    "$evidence_root/summary"

run_id="$(date +%Y%m%d-%H%M%S)"
summary_file="$evidence_root/summary/run-$run_id.tsv"
printf 'stage\tresult\texit_code\tevidence\n' > "$summary_file"
failures=0

record() {
    printf '%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" >> "$summary_file"
}

run_logged() {
    local stage="$1"
    local log_file="$2"
    shift 2
    echo
    echo "==> $stage"
    set +e
    "$@" 2>&1 | tee "$log_file"
    local status="${PIPESTATUS[0]}"
    set -e
    if [[ "$status" -eq 0 ]]; then
        record "$stage" PASS "$status" "$log_file"
    else
        record "$stage" FAIL "$status" "$log_file"
        failures=$((failures + 1))
    fi
    # Keep collecting evidence after a failed executed stage. The aggregate
    # exit code at the end remains non-zero, and dependent stages will make
    # any follow-on impact explicit instead of silently disappearing.
    return 0
}

require_fresh_build_dir() {
    local dir="$1"
    if [[ -e "$dir" ]]; then
        echo "Refusing non-fresh build directory: $dir" >&2
        echo "Move/remove it deliberately or choose another AURALIS_PHASE9_*_BUILD path." >&2
        exit 2
    fi
}

capture_preflight() {
    local baseline_log="$evidence_root/baseline/baseline-$run_id.txt"
    local host_log="$evidence_root/baseline/live-preflight-$run_id.txt"

    run_logged "baseline-capture" "$baseline_log" bash -c '
        set -u
        date --iso-8601=seconds
        pwd
        git status --short
        git branch --show-current
        git rev-parse HEAD
        git log -1 --oneline
        git diff --stat
        git diff --cached --stat
        git submodule status 2>/dev/null || true
        cmake --version | head -1
        ninja --version
        g++ --version | head -1
        qmake6 --version || true
        bluetoothctl --version || true
        pkg-config --modversion libpipewire-0.3 || true
        pkg-config --modversion dbus-1 || true
        wireplumber --version || true
        uname -a
        sed -n "1,40p" /etc/os-release
    ' || true

    run_logged "live-preflight" "$host_log" bash -c '
        set -u
        bluetoothctl list || true
        bluetoothctl show || true
        bluetoothctl devices || true
        bluetoothctl devices Connected || true
        systemctl status bluetooth --no-pager || true
        systemctl --user status pipewire --no-pager || true
        systemctl --user status pipewire-pulse --no-pager || true
        systemctl --user status wireplumber --no-pager || true
        wpctl status || true
        pw-cli info 0 || true
        pactl info || true
        pgrep -a auralis-desktop || true
        sed -n "1,320p" /proc/bus/input/devices || true
        ls -l /dev/input/by-id /dev/input/by-path 2>/dev/null || true
    ' || true
}

run_software() {
    require_fresh_build_dir "$baseline_build"
    run_logged "baseline-configure" "$evidence_root/builds/baseline-configure-$run_id.log" \
        cmake -S . -B "$baseline_build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    run_logged "baseline-build" "$evidence_root/builds/baseline-build-$run_id.log" \
        cmake --build "$baseline_build" --parallel "$build_jobs"
    run_logged "default-test-inventory" "$evidence_root/ctest/default-inventory-$run_id.json" \
        ctest --test-dir "$baseline_build" --show-only=json-v1
    run_logged "default-ctest" "$evidence_root/ctest/default-$run_id.log" \
        ctest --test-dir "$baseline_build" --output-on-failure -j1
    run_logged "stress-default" "$evidence_root/ctest/stress-default-$run_id.log" \
        ctest --test-dir "$baseline_build" -L stress --output-on-failure -j1
    run_logged "stress-extended-1000" "$evidence_root/ctest/stress-extended-1000-$run_id.log" \
        env AURALIS_RUN_STRESS=1 ctest --test-dir "$baseline_build" -L stress --output-on-failure -j1
    run_logged "stress-bounded-2500" "$evidence_root/ctest/stress-bounded-2500-$run_id.log" \
        env AURALIS_STRESS_ITERATIONS=2500 ctest --test-dir "$baseline_build" -L stress --output-on-failure -j1
}

run_sanitizer() {
    require_fresh_build_dir "$asan_build"
    run_logged "sanitizer-configure" "$evidence_root/sanitizer/configure-$run_id.log" \
        cmake -S . -B "$asan_build" -G Ninja \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAURALIS_ENABLE_SANITIZERS=ON
    run_logged "sanitizer-build" "$evidence_root/sanitizer/build-$run_id.log" \
        cmake --build "$asan_build" --parallel "$build_jobs"
    run_logged "sanitizer-ctest-leaks-enabled" "$evidence_root/sanitizer/ctest-$run_id.log" \
        env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            ctest --test-dir "$asan_build" --output-on-failure -j1
}

run_live() {
    if [[ "${AURALIS_PHASE9_RUN_LIVE:-0}" != 1 ]]; then
        echo "live mode requires AURALIS_PHASE9_RUN_LIVE=1" >&2
        exit 2
    fi
    if [[ ! -x "$baseline_build/tests/integration/tst_PipeWireLiveIntegration" ]]; then
        echo "Live mode requires an existing Phase 9 baseline build at $baseline_build." >&2
        exit 2
    fi

    capture_preflight
    run_logged "live-pipewire" "$evidence_root/pipewire/live-$run_id.log" \
        env AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
            AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_EXPECT_DEVICE_ADDRESS:-}" \
            ctest --test-dir "$baseline_build" -R '^tst_PipeWireLiveIntegration$' --output-on-failure -V -j1
    run_logged "live-audio-routing" "$evidence_root/routing/live-$run_id.log" \
        env AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
            AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_EXPECT_DEVICE_ADDRESS:-}" \
            ctest --test-dir "$baseline_build" -R '^tst_AudioRoutingLiveIntegration$' --output-on-failure -V -j1

    if [[ -n "${AURALIS_EXPECT_DEVICE_ADDRESSES:-}" ]]; then
        run_logged "live-bluez-two-device" "$evidence_root/bluetooth/live-$run_id.log" \
            env AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
                AURALIS_EXPECT_DEVICE_ADDRESSES="$AURALIS_EXPECT_DEVICE_ADDRESSES" \
                ctest --test-dir "$baseline_build" -R '^tst_BlueZLiveIntegration$' --output-on-failure -V -j1
        run_logged "live-session-two-device" "$evidence_root/routing/session-live-$run_id.log" \
            env AURALIS_RUN_SESSION_INTEGRATION=1 \
                AURALIS_EXPECT_DEVICE_ADDRESSES="$AURALIS_EXPECT_DEVICE_ADDRESSES" \
                ctest --test-dir "$baseline_build" -R '^tst_SessionLiveIntegration$' --output-on-failure -V -j1
    else
        record "live-bluez-two-device" "NOT EXERCISED" 0 "AURALIS_EXPECT_DEVICE_ADDRESSES not set"
        record "live-session-two-device" "NOT EXERCISED" 0 "AURALIS_EXPECT_DEVICE_ADDRESSES not set"
    fi

    if [[ -n "${AURALIS_EXPECT_DEVICE_ADDRESS:-}" ]]; then
        run_logged "live-phase7-hardware" "$evidence_root/routing/phase7-hardware-$run_id.log" \
            env AURALIS_RUN_PHASE7_HARDWARE=1 \
                AURALIS_EXPECT_DEVICE_ADDRESS="$AURALIS_EXPECT_DEVICE_ADDRESS" \
                AURALIS_EXPECT_DEVICE_ADDRESSES="${AURALIS_EXPECT_DEVICE_ADDRESSES:-}" \
                ctest --test-dir "$baseline_build" -R '^tst_Phase7HardwareLive$' --output-on-failure -V -j1
    else
        record "live-phase7-hardware" "NOT EXERCISED" 0 "AURALIS_EXPECT_DEVICE_ADDRESS not set"
    fi
}

run_package() {
    require_fresh_build_dir "$release_build"
    local stage_dir="$evidence_root/package/stage-$run_id"
    local extracted_dir="$evidence_root/package/extracted-$run_id"
    mkdir -p "$stage_dir" "$extracted_dir"

    run_logged "release-configure" "$evidence_root/package/configure-$run_id.log" \
        cmake -S . -B "$release_build" -G Ninja -DCMAKE_BUILD_TYPE=Release
    run_logged "release-build" "$evidence_root/package/build-$run_id.log" \
        cmake --build "$release_build" --parallel "$build_jobs"
    run_logged "release-ctest" "$evidence_root/package/ctest-$run_id.log" \
        ctest --test-dir "$release_build" --output-on-failure -j1
    run_logged "staged-install" "$evidence_root/package/install-$run_id.log" \
        env DESTDIR="$stage_dir" cmake --install "$release_build" --prefix /usr
    find "$stage_dir" -type f -o -type l | sort > "$evidence_root/package/stage-tree-$run_id.txt"
    run_logged "cpack" "$evidence_root/package/cpack-$run_id.log" \
        cpack --config "$release_build/CPackConfig.cmake" -B "$evidence_root/package/artifacts-$run_id"

    local deb
    deb="$(find "$evidence_root/package/artifacts-$run_id" -maxdepth 1 -type f -name '*.deb' -print -quit)"
    if [[ -z "$deb" ]]; then
        echo "CPack did not produce a DEB." >&2
        record "deb-artifact" FAIL 1 "$evidence_root/package/artifacts-$run_id"
        failures=$((failures + 1))
        return
    fi
    run_logged "deb-metadata" "$evidence_root/package/deb-info-$run_id.txt" dpkg-deb -I "$deb"
    run_logged "deb-contents" "$evidence_root/package/deb-contents-$run_id.txt" dpkg-deb -c "$deb"
    run_logged "deb-extract" "$evidence_root/package/deb-extract-$run_id.log" dpkg-deb -x "$deb" "$extracted_dir"
    run_logged "extracted-ldd" "$evidence_root/package/extracted-ldd-$run_id.txt" \
        ldd "$extracted_dir/usr/bin/auralis-desktop"
    run_logged "extracted-dependency-audit" "$evidence_root/package/extracted-dependency-audit-$run_id.log" \
        bash -c '
            if grep -Fq "not found" "$1" || grep -Fq "$2" "$1"; then
                echo "Unresolved dependency or build-tree reference found in ldd output." >&2
                exit 1
            fi
            echo "No unresolved dependency or build-tree reference found."
        ' _ "$evidence_root/package/extracted-ldd-$run_id.txt" "$repo_root"

    local smoke_log="$evidence_root/package/extracted-smoke-$run_id.log"
    echo "==> extracted-package-smoke"
    set +e
    timeout 5 env QT_QPA_PLATFORM=offscreen \
        "$extracted_dir/usr/bin/auralis-desktop" > "$smoke_log" 2>&1
    local smoke_status=$?
    set -e
    if { [[ "$smoke_status" -eq 0 || "$smoke_status" -eq 124 ]]; } \
        && grep -Fq "QML root loaded" "$smoke_log" \
        && ! grep -Eiq \
            'TypeError|ReferenceError|QQmlApplicationEngine failed|module .* not installed|symbol lookup error|Segmentation fault' \
            "$smoke_log"; then
        record "extracted-package-smoke" PASS "$smoke_status" "$smoke_log"
    else
        record "extracted-package-smoke" FAIL "$smoke_status" "$smoke_log"
        failures=$((failures + 1))
    fi
}

case "$mode" in
    preflight) capture_preflight ;;
    software) run_software ;;
    sanitizer) run_sanitizer ;;
    live) run_live ;;
    package) run_package ;;
    all-safe)
        capture_preflight
        run_software
        run_sanitizer
        run_package
        ;;
esac

echo
echo "Phase 9 run summary: $summary_file"
cat "$summary_file"
echo
echo "Operator-controlled gates were not inferred from this runner."
echo "See docs/validation/phase-9-linux-rc.md for hardware, recovery, suspend, and soak steps."

if [[ "$failures" -ne 0 ]]; then
    echo "$failures executed stage(s) failed." >&2
    exit 1
fi
