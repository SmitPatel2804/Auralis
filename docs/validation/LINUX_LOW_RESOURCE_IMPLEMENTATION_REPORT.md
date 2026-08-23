# Auralis Linux Low-Resource Implementation Report

## Status

Auralis has been updated to reduce Linux build-time and runtime resource
consumption without intentionally changing its user-facing behavior or audio,
Bluetooth, session, logging, and diagnostics functionality.

This repository was modified and compiled successfully on the available Windows
development machine. The Linux-specific PipeWire, BlueZ, `/dev/input`, packaging,
latency, and Bluetooth hardware checks must still be executed on an actual Linux
desktop.

## Implemented improvements

### PipeWire and audio graph processing

- Identical PipeWire snapshots no longer trigger unnecessary graph changes.
- PipeWire node, port, link, and device bursts are coalesced into bounded graph
  refreshes.
- BlueZ updates schedule a coalesced graph refresh instead of rebuilding the
  audio graph immediately for every event.
- Audio-source collections are compared before updating the model, preventing
  identical model resets and redundant UI notifications.

### Session processing

- Healthy active sessions no longer run a continuous one-second reconciliation
  sweep.
- Graph, route, and Bluetooth events remain the primary reconciliation triggers.
- A five-second, very-coarse safety timer operates only while a session is
  starting, degraded, or recovering.
- The safety timer stops when the session becomes healthy, idle, stopped, or
  failed.

### Bluetooth and Linux input handling

- BlueZ system-bus health polling was reduced from two seconds to five seconds
  and changed to a coarse fallback timer.
- The logind subscription retry interval was increased from two seconds to ten
  seconds.
- Bluetooth count notifications are no longer emitted for irrelevant updates
  such as routine RSSI changes. Relevant connection and transport changes still
  update the exposed counts.
- Linux Bluetooth media-button capability checks cache the parsed input-device
  list for two seconds instead of reparsing `/proc/bus/input/devices` for every
  QML role read.
- The cache is invalidated on device synchronization and expires automatically,
  allowing late-created or removed `/dev/input` endpoints to be detected.
- A DISALLOW action forces a fresh probe so a policy change never relies on an
  earlier stale capability result.

### Logging and diagnostics

- Informational and debug log records are no longer explicitly flushed after
  every record.
- Warnings, critical failures, and fatal records are still flushed immediately.
- Buffered records are flushed during normal logger shutdown and rotation.
- The diagnostics model evicts old entries incrementally instead of resetting
  and rebuilding the complete model whenever it reaches capacity.
- The diagnostics log remains bounded, and its existing capture behavior is
  preserved.

## Low-resource Linux build and test workflow

The repository now contains a `linux-low-resource` CMake preset and a bounded
validation runner.

Default limits:

- One build job.
- One test process.
- Interprocedural optimization disabled for the validation build.
- Sanitizers disabled during the normal validation pass.
- A hard maximum of two build jobs in the runner.

Run this command from the repository root on Linux:

```bash
AURALIS_BUILD_JOBS=1 bash scripts/validation/run-linux-low-resource.sh
```

The equivalent individual commands are:

```bash
cmake --preset linux-low-resource
cmake --build --preset linux-low-resource
ctest --preset linux-low-resource
```

Use two build jobs only when the machine has sufficient free memory:

```bash
AURALIS_BUILD_JOBS=2 bash scripts/validation/run-linux-low-resource.sh
```

The runner rejects values other than `1` or `2` so an automated IDE cannot
accidentally use every CPU core and exhaust the host.

## Validation completed in the current environment

- Final MSVC/Qt build: **PASS**.
- Runnable automated tests: **38/38 PASS**, executed serially.
- New regression coverage verifies:
  - duplicate PipeWire snapshots do not report graph changes;
  - an unchanged source refresh does not emit a redundant update;
  - diagnostics capacity eviction uses incremental row operations;
  - buffered informational logs are durable after normal shutdown;
  - repeated Linux button-capability reads reuse the input probe cache.
- CMake preset JSON parsing: **PASS**.
- Bash syntax validation: **PASS**.
- Non-Linux runner safety guard: **PASS** with the expected exit code.
- `git diff --check`: **PASS**.

Two Windows test executables, `tst_SessionPersistence.exe` and
`tst_RoutingCoordinator.exe`, were explicitly blocked from launching by Windows
Application Control because they are locally built unsigned executables. This
was a host security-policy block, not a test assertion or compilation failure.

## Required actual-Linux verification

The following work cannot be truthfully marked as passed until it is performed
on the target Linux desktop:

1. Compile all Linux-only PipeWire, BlueZ, logind, and evdev sources.
2. Run the complete Linux CTest suite serially.
3. Launch the unpackaged and packaged desktop application.
4. Verify PipeWire and WirePlumber service integration.
5. Confirm that **Auralis Virtual Output** appears and that **Auralis System
   Audio** is available to the application.
6. Test discovery while another Bluetooth device is already connected through
   the OS.
7. Test Classic and BLE list separation.
8. Test connect, disconnect, pair, trust, forget, ALLOW, and DISALLOW behavior.
9. Route real browser and application playback streams to one and multiple
   Bluetooth outputs.
10. Measure latency, synchronization, drift, underruns, CPU use, and memory
    growth.
11. Test suspend/resume, PipeWire restart, WirePlumber restart, BlueZ restart,
    device loss, and reconnection recovery.
12. Build the DEB and TGZ packages and launch the extracted package.

Use the complete procedure in
`docs/validation/LINUX_ACTUAL_MACHINE_VALIDATION_PROMPT.md`. Its commands now use
one build job and one test job by default, avoid duplicate validation builds,
and require resource headroom before sanitizer or endurance testing.

## Resource-safety requirements for the Linux AI IDE

- Record `free -h`, `df -h .`, and `ulimit -a` before beginning.
- Do not run builds, tests, packaging, sanitizers, or the Auralis GUI
  concurrently.
- Never calculate build or test concurrency from `nproc`.
- Keep the deterministic tests at `-j1`.
- Do not create another equivalent build after the low-resource preset succeeds.
- Do not start sanitizer validation automatically. Ask first and confirm that
  the machine has adequate RAM and swap.
- Stop the active validation cleanly if available memory falls below 1 GiB, the
  machine swaps continuously, or the desktop becomes unresponsive.
- Inspect large logs with `tail` or `rg`; do not load entire log files into the
  IDE or chat context.

## Important conclusion

The major known sources of unnecessary event processing, periodic work, model
resets, input probing, logging I/O, and unbounded test concurrency have been
addressed. The current automated results establish that the portable code and
existing functionality remain stable in the available environment. Final Linux
readiness still depends on completing and documenting the actual-machine checks
listed above.
