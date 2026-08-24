# Phase 9 Linux RC1 qualification

This procedure qualifies the current Auralis checkout as an internal Linux RC1
technical candidate. It is deliberately evidence-driven: a skipped opt-in test
is not hardware evidence, an unavailable device is not a pass, and operator-only
actions stay under operator control.

The machine-local evidence root is `audit-phase9-linux-rc/`. It is gitignored
because raw BlueZ, input, process, and PipeWire dumps can contain machine and
Bluetooth identifiers. The durable, redacted result belongs in
`docs/PHASE_9_LINUX_RC1_QUALIFICATION_AUDIT.md`.

## Prerequisites and safety

- Run from the repository root on the intended Ubuntu Linux reference machine.
- Use a normal desktop user. Never launch the full application with `sudo`.
- Have BlueZ, PipeWire, pipewire-pulse, and WirePlumber available.
- Use two already-paired Bluetooth audio devices for the mandatory live gate.
- Keep full Bluetooth addresses only in the gitignored evidence directory.
- Do not forget/unpair a device unless destructive testing was separately
  approved with `AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1`.
- Do not restart BlueZ, suspend, reboot, or change adapter power unexpectedly.
- Stop audio-service tests during calls, recordings, or other important audio.

The Phase 9 runner never uses `sudo`, connects or disconnects devices, changes
adapter power, restarts services, suspends, reboots, pairs, or forgets devices.
It also refuses to reuse an existing Phase 9 build directory, so stale objects
cannot turn a qualification build into a false pass.

## Automated safe qualification

Inspect the runner contract:

```bash
scripts/validation/run-phase9-linux-rc.sh --help
```

Run the safe stages serially on the reference laptop:

```bash
AURALIS_BUILD_JOBS=1 scripts/validation/run-phase9-linux-rc.sh preflight
AURALIS_BUILD_JOBS=1 scripts/validation/run-phase9-linux-rc.sh software
AURALIS_BUILD_JOBS=1 scripts/validation/run-phase9-linux-rc.sh sanitizer
AURALIS_BUILD_JOBS=1 scripts/validation/run-phase9-linux-rc.sh package
```

Or run all four with `all-safe`. That mode intentionally excludes live tests.
The default build paths are:

```text
build-phase9-baseline
build-phase9-asan
build-phase9-release
```

Choose unused paths with `AURALIS_PHASE9_BASELINE_BUILD`,
`AURALIS_PHASE9_ASAN_BUILD`, and `AURALIS_PHASE9_RELEASE_BUILD` when rerunning.
Do not delete a tree merely to hide or overwrite earlier evidence.

The software stage executes:

- fresh RelWithDebInfo configure and build;
- full default CTest;
- default stress;
- extended 1,000-iteration stress;
- a bounded 2,500-iteration stress run.

The sanitizer stage runs ASan and UBSan with LeakSanitizer enabled:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

If this host genuinely requires `detect_leaks=0`, rerun explicitly, retain both
logs, and classify LeakSanitizer as not exercised. Do not describe that result
as fully leak-clean.

The package stage executes a fresh Release build and CTest, installs into a
non-root `DESTDIR`, produces DEB and TGZ artifacts, inspects the DEB, extracts
it, runs `ldd`, and performs a five-second offscreen smoke. Timeout exit 124 is
a pass only when the log proves the application reached a stable event loop and
contains no fatal QML/runtime error.

Default CTest includes opt-in live executables whose Qt tests call `QSKIP` when
their environment flag is absent. CTest may display those executables as
`Passed`; that is only a safe default-suite pass, never live hardware evidence.

## Live test environment

Set real addresses in the shell. Do not copy placeholder addresses literally.
The two-address parser accepts comma or semicolon separation:

```bash
export AURALIS_EXPECT_DEVICE_ADDRESS="DEVICE_A_REAL_ADDRESS"
export AURALIS_EXPECT_DEVICE_ADDRESSES="DEVICE_A_REAL_ADDRESS;DEVICE_B_REAL_ADDRESS"
export AURALIS_PHASE9_RUN_LIVE=1
scripts/validation/run-phase9-linux-rc.sh live
```

Before running, prove both devices are connected:

```bash
bluetoothctl devices Connected
wpctl status
```

The live runner executes real PipeWire and routing tests and, when addresses are
provided, the existing BlueZ, session, and Phase 7 hardware tests. It never sets
`AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS`.

Expected live evidence includes:

- BlueZ system bus and service available;
- powered usable adapter and real discovery activity;
- both expected devices observed and simultaneously connected;
- PipeWire Connected plus completed initial registry sync;
- Bluetooth endpoints correlated to stable addresses;
- route creation, Active state, and removal of only owned links;
- two session destinations active from one source without sibling teardown.

## Desktop, fan-out, and system audio

Launch the final build normally:

```bash
./build-phase9-baseline/apps/desktop/auralis-desktop
```

Assign neutral aliases `Device A` and `Device B` in committed notes. Verify the
Devices, Audio Routing, Sessions, Settings, and Diagnostics pages; truthful
states; no duplicate rows; no toast/log storm; and no QML error flood.

For the mandatory fan-out gate:

1. Connect Device A and Device B simultaneously.
2. Select one source and add both devices to the same session.
3. Capture `wpctl status` and an appropriate PipeWire graph/link dump before,
   during, and after activation.
4. Confirm both stereo routes are Active at the same time.
5. Confirm adding Device B does not remove Device A links.
6. Confirm real content is audible on both devices.
7. Remove or stop one destination and confirm the healthy sibling survives.
8. Deactivate the session and confirm only Auralis-owned links disappear.

For system audio, use **USE AS SYSTEM OUTPUT**, select **Auralis System Audio**
as the Auralis source, and play deterministic local or browser media. Prove one
system mix reaches both devices. Do not call simultaneous audible playback
sample-perfect synchronization; acoustic latency is not measured in Phase 9.

## Physical button-policy validation

Never select an arbitrary keyboard, mouse, or laptop hotkey node. With each
headset connected, inspect:

```bash
sed -n '1,320p' /proc/bus/input/devices
ls -l /dev/input/by-id /dev/input/by-path 2>/dev/null || true
udevadm info --query=property --name=/dev/input/eventX
```

Only use `evtest` when `eventX` is correlated to the target device by a headset
Bluetooth address in `Uniq`/`Phys` or equivalent narrow udev Bluetooth identity.
A name-only match or adapter-address-only match is insufficient.

For each supported device:

1. Set ALLOW, play media, and verify repeated Play/Pause presses reach the host.
2. Set DISALLOW during playback and verify repeated presses no longer affect
   host media while A2DP and the Auralis route remain active.
3. Set A=DISALLOW/B=ALLOW, then swap, and verify per-device isolation.
4. Toggle ALLOW/DISALLOW at least ten times under playback.
5. Disconnect/reconnect a DISALLOW device and verify release/reapply.
6. Exit/restart Auralis and verify the grab releases, policy persists, and
   suppression reapplies; return to ALLOW and verify normal behavior.

If no safely correlated endpoint exists, record `NOT EXERCISED` for physical
suppression and require the UI to say Unsupported. If open or `EVIOCGRAB` fails,
record the Permission Denied state. Do not run Auralis as root or make all of
`/dev/input` broadly readable; use a narrow session `uaccess` rule only after
reviewing the exact device identity model.

## Recovery and churn

Begin with both destinations playing and capture initial process, graph, and
session state. For every trigger, record detection time, state transitions,
retry attempts, recovery time, final state, and whether the healthy peer stayed
active.

- Power off Device B, recover it, then repeat for Device A.
- Perform bounded alternating device power/connect cycles.
- Restart PipeWire only with operator awareness:

  ```bash
  systemctl --user restart pipewire
  ```

- Restart WirePlumber only after confirming its user service exists:

  ```bash
  systemctl --user restart wireplumber
  ```

- Test adapter off/on through the normal user control path when safe.
- For a BlueZ daemon restart, prepare evidence first and have the operator run
  the exact privileged command. Never invoke privileged restart silently.

Required behavior is bounded recovery, fresh graph/device identity, no stale
callback mutation, no duplicate links or retry engines, and preservation of the
healthy sibling wherever the host stack permits it.

## Suspend/resume and persistence

Suspend is operator-controlled. Before each cycle, persist notes to
`audit-phase9-linux-rc/suspend-resume/`, connect both devices, activate a
representative session, and record `restoreOnResume`.

After resume, verify BlueZ and PipeWire refresh, volatile IDs are not reused,
the UI settles, no duplicate links remain, and restoration follows the setting.
Run at least two normal cycles and one `restoreOnResume=false` cycle.

Separately test clean application exit/restart with both devices healthy, one
member absent, and after a recovery cycle. Confirm no process or evdev grab is
left behind and sessions, preferences, and per-device policy persist.

## Hardware stress, soak, and resources

Target at least:

```text
10 connect/disconnect cycles per device
10 alternating A/B cycles
10 two-device activate/deactivate cycles
10 button-policy toggle cycles per supported device
2 hours continuous two-device playback
```

Sample the application periodically:

```bash
pidof auralis-desktop
ps -o pid,etime,%cpu,%mem,rss,vsz,nlwp,cmd -p PID
find /proc/PID/fd -mindepth 1 -maxdepth 1 | wc -l
sed -n '1,200p' /proc/PID/status
```

Record actual duration, Device A/B continuity, dropouts, reconnects, final
session state, RSS/FD deltas, and log rotation. An interrupted soak is its actual
duration, not a two-hour pass.

## Result labels and exit rule

Use only:

```text
PASS
FAIL
SKIPPED
NOT EXERCISED
PENDING OPERATOR ACTION
BLOCKED
```

The Phase 9 Linux RC1 technical exit gate passes only when Gates A–G in the
master prompt all pass. In particular, safe software tests and a good package
cannot substitute for the mandatory two-device hardware, recovery, lifecycle,
hardware churn, and two-hour soak gates.

Licensing and maintainer contact are still owner decisions. Until both are
approved, retain:

```text
PUBLIC DISTRIBUTION READINESS: BLOCKED — OWNER LICENSE/CONTACT METADATA PENDING
```

