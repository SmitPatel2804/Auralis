# Auralis — Phase 6 Final Actual-Machine Rigorous Validation & Independent Audit Prompt
## Run on the Real Ubuntu Development Machine After the Absolute Final Correction Pass

**Project:** Auralis  
**Phase:** Phase 6 — Multi-Device Session Engine  
**Execution target:** The actual Linux/Ubuntu machine used to build and run Auralis  
**Purpose:** Independently prove Phase 6 software correctness using the real compiler, Qt installation, BlueZ, PipeWire, desktop session, repeated automated tests, sanitizers, raw logs, and—when available—real Bluetooth audio devices.

---

# 1. Role

You are the independent QA/audit agent.

Do **not** trust:

- implementation-agent reports;
- prior Markdown audit conclusions;
- claimed CTest totals;
- claimed sanitizer results;
- comments saying an issue is fixed.

Reproduce all important evidence on the actual machine.

Your role is:

```text
verify
stress
break
collect evidence
report truthfully
```

Do not modify production code unless the user explicitly asks you to fix findings.

If a bug is discovered, record it precisely.

---

# 2. Absolute Rule

A Phase 6 audit report is not sufficient evidence by itself.

You must create raw machine evidence.

Create:

```bash
mkdir -p audit-phase6-actual-machine
```

Store every important command output there.

Required artifacts include at minimum:

```text
environment.txt
git-state.txt
configure.log
build.log
ctest-list.txt
ctest-full.log
ctest-repeat-full.log
ctest-session-repeat.log
ctest-parallel.log
new-boundary-tests.log
asan-configure.log
asan-build.log
asan-tests.log
ubsan-configure.log
ubsan-build.log
ubsan-tests.log
desktop-launch.log
bluetooth-state.txt
pipewire-state.txt
hardware-validation.log
requirements-matrix.md
FINAL_AUDIT.md
```

If a test cannot run, record why.

---

# 3. Environment Capture

Run:

```bash
{
  echo "===== DATE ====="
  date --iso-8601=seconds

  echo "===== KERNEL ====="
  uname -a

  echo "===== OS ====="
  cat /etc/os-release

  echo "===== GCC ====="
  gcc --version | head -1

  echo "===== G++ ====="
  g++ --version | head -1

  echo "===== CMAKE ====="
  cmake --version | head -1

  echo "===== NINJA ====="
  ninja --version

  echo "===== GIT ====="
  git --version

  echo "===== QT QMAKE ====="
  qmake6 --version 2>&1 || true

  echo "===== QT CORE ====="
  pkg-config --modversion Qt6Core 2>&1 || true

  echo "===== QT BLUETOOTH ====="
  pkg-config --modversion Qt6Bluetooth 2>&1 || true

  echo "===== QT MULTIMEDIA ====="
  pkg-config --modversion Qt6Multimedia 2>&1 || true

  echo "===== PIPEWIRE DEV ====="
  pkg-config --modversion libpipewire-0.3 2>&1 || true

  echo "===== DBUS DEV ====="
  pkg-config --modversion dbus-1 2>&1 || true

  echo "===== BLUEZ ====="
  bluetoothctl --version 2>&1 || true
} | tee audit-phase6-actual-machine/environment.txt
```

---

# 4. Repository State

Run:

```bash
{
  echo "===== STATUS ====="
  git status --short

  echo "===== BRANCH ====="
  git branch --show-current

  echo "===== HEAD ====="
  git rev-parse HEAD

  echo "===== LOG ====="
  git log --oneline -n 30

  echo "===== DIFF STAT ====="
  git diff --stat
} | tee audit-phase6-actual-machine/git-state.txt
```

Record whether the tree is clean.

If not clean, identify whether changes are intentional.

---

# 5. Source Sanity Search

Run:

```bash
{
  echo "===== QTIMERS ====="
  grep -R "new QTimer" -n src include || true

  echo "===== RECONNECT CLEANUP ====="
  grep -R "cancelReconnect\|cancelAll\|deleteLater" -n src/bluetooth include/auralis/bluetooth || true

  echo "===== SUPPRESSION ====="
  grep -R "suppressAutoReconnect\|unsuppressAutoReconnect\|autoReconnectSuppressed" -n src include tests || true

  echo "===== DEVICE PREF ====="
  grep -R "autoReconnectEnabled" -n src include tests || true

  echo "===== RECOVERY POLICY ====="
  grep -R "RecoveryPolicy::" -n src/session tests || true

  echo "===== CONST CAST ====="
  grep -R "const_cast" -n src/session src/bluetooth include/auralis/session include/auralis/bluetooth || true
} > audit-phase6-actual-machine/source-sanity.txt
```

Review manually.

---

# 6. Clean Build From Scratch

Do not reuse an old build directory.

```bash
rm -rf build-phase6-final
```

Configure:

```bash
cmake -S . -B build-phase6-final -G Ninja \
  2>&1 | tee audit-phase6-actual-machine/configure.log
```

Build:

```bash
cmake --build build-phase6-final \
  2>&1 | tee audit-phase6-actual-machine/build.log
```

The audit fails if build fails due to repository code.

Missing environment packages must be reported separately.

---

# 7. Enumerate Tests

```bash
ctest --test-dir build-phase6-final -N \
  2>&1 | tee audit-phase6-actual-machine/ctest-list.txt
```

Manually identify all:

```text
Session
Reconnect
Bluetooth
AudioRouter
Endpoint
Persistence
```

tests.

---

# 8. Full Serial CTest

Run:

```bash
ctest --test-dir build-phase6-final \
  --output-on-failure \
  2>&1 | tee audit-phase6-actual-machine/ctest-full.log
```

Record:

```text
Total
Passed
Failed
Skipped
```

No mandatory Phase 0–6 test may fail.

---

# 9. Full Suite Repetition

Run 20 complete repetitions:

```bash
{
  for i in $(seq 1 20); do
    echo "===== FULL RUN $i ====="
    ctest --test-dir build-phase6-final \
      --output-on-failure || exit 1
  done
} 2>&1 | tee audit-phase6-actual-machine/ctest-repeat-full.log
```

Any flaky failure blocks Phase 6 completion.

---

# 10. Phase 6 / Reconnect Repetition

Run 100 focused repetitions:

```bash
{
  for i in $(seq 1 100); do
    echo "===== SESSION/RECONNECT RUN $i ====="
    ctest --test-dir build-phase6-final \
      -R "Session|Reconnect|Bluetooth" \
      --output-on-failure || exit 1
  done
} 2>&1 | tee audit-phase6-actual-machine/ctest-session-repeat.log
```

If runtime is acceptable, increase to 250 or 500.

---

# 11. Parallel CTest

Run:

```bash
ctest --test-dir build-phase6-final \
  -j "$(nproc)" \
  --output-on-failure \
  2>&1 | tee audit-phase6-actual-machine/ctest-parallel.log
```

Serial pass + parallel failure = investigate shared-state/test isolation.

---

# 12. Explicit Final Boundary Tests

Identify the exact test names covering:

1. reconnect timer cancellation cleanup;
2. 1000-cycle timer stress;
3. DeviceLifecycle operation contention;
4. deferred retry cancellation;
5. member disable suppression cleanup;
6. member re-enable suppression reapplication;
7. device autoReconnect preference matrix;
8. shutdown suppression cleanup;
9. terminal reconnect failure;
10. reconnect exhaustion;
11. RecoveryPolicy::None before disconnect;
12. RestoreRoutesOnly before disconnect;
13. ReconnectAndRestore exactly-once scheduling.

Run those tests explicitly and tee results:

```bash
ctest --test-dir build-phase6-final \
  -R "Reconnect|Suppression|Contention|RecoveryPolicy|SessionManagedReconnect" \
  --output-on-failure \
  2>&1 | tee audit-phase6-actual-machine/new-boundary-tests.log
```

Adjust regex to real test names.

---

# 13. Test Coverage Mapping

Create:

```text
audit-phase6-actual-machine/requirements-matrix.md
```

with:

| Requirement | Source implementation | Unit test | Integration test | Actual-machine result |
|---|---|---|---|---|
| Session CRUD | | | | |
| State machine | | | | |
| Multi-device routes | | | | |
| Degraded operation | | | | |
| Group volume | | | | |
| Per-member volume | | | | |
| None policy | | | | |
| RestoreRoutesOnly | | | | |
| ReconnectAndRestore | | | | |
| Reconnect de-duplication | | | | |
| Terminal reconnect | | | | |
| Retry exhaustion | | | | |
| Contention deferral | | | | |
| Timer cleanup | | | | |
| Suppression lifecycle | | | | |
| Device preference matrix | | | | |
| Source loss/return | | | | |
| New endpoint ID return | | | | |
| Persistence | | | | |
| Stable source restore | | | | |
| Stop reentrancy | | | | |
| Generation race | | | | |
| Shutdown | | | | |

No mandatory software row should remain unproven.

---

# 14. Reconnect Timer Lifetime Verification

Do not rely only on test PASS.

Inspect test logic.

Confirm it asserts:

```text
entries == 0
retained retry timers do not increase
```

after repeated:

```text
schedule/cancel
```

If possible run the timer stress test under:

```text
ASan
Valgrind
```

---

# 15. Device Preference Matrix Verification

Review the implementation and test.

Confirm exact semantics for:

```text
device.autoReconnectEnabled
session.autoReconnect
RecoveryPolicy
member.enabled
```

Record the effective matrix in the final audit.

---

# 16. Session Suppression Lifecycle Verification

Explicitly verify automated tests for:

```text
activate
disable member
enable member
remove member
stop
delete
switch session
shutdown
```

Ensure no suppression survives incorrectly.

---

# 17. Reconnect Operation Contention

Inspect the actual test.

It must involve:

```text
DeviceLifecycleManager
```

not only ReconnectPolicy.

Verify:

```text
reconnect due
device busy
no BlueZ connect yet
retry still alive
attempt not consumed incorrectly
busy operation completes
connect proceeds
```

---

# 18. Deferred Retry Cancellation

Verify tests for:

```text
session stop
policy -> None
member disable
```

while reconnect is deferred.

Late device operation completion must not resurrect reconnect.

---

# 19. Terminal Failure

Verify a real non-retryable fake BlueZ error.

Examples:

```text
AuthenticationFailed
NotAuthorized
NotSupported
```

Do not use a retryable `ConnectionAttemptFailed` for the terminal-failure test.

---

# 20. Exhaustion

Set a small retry budget in test.

Verify exact attempt count.

The session must leave `RECOVERING`.

---

# 21. Source Loss/Return

Verify:

```text
ACTIVE
source disappears
state changes as documented
source returns with changed runtime ID
routes restored or explicit retry required
```

No:

```text
FAILED + active routes
```

contradiction.

---

# 22. Endpoint ID Churn

Verify existing recovery test uses:

```text
old endpoint ID != new endpoint ID
```

and no stale runtime ID survives.

---

# 23. Stop Reentrancy

Verify test uses synchronous route callbacks.

Run focused:

```bash
ctest --test-dir build-phase6-final \
  -R "Reentrancy|Session.*Stop|Route" \
  --output-on-failure
```

Adjust regex.

---

# 24. Generation Races

Run rapid:

```text
start-stop-start
```

test repeatedly.

Use 500–1000 iterations inside test if reasonable.

---

# 25. Persistence

Verify:

- missing parent directory;
- atomic save;
- malformed JSON;
- unsupported schema;
- duplicate IDs;
- persistence failure propagation;
- createSession rollback/failure semantics;
- runtime IDs not trusted;
- stable source re-resolution.

---

# 26. Volume

Verify:

```text
NaN
+Inf
-Inf
```

are rejected.

Verify returning member receives current group volume/mute.

---

# 27. AddressSanitizer Build

Create a separate build.

```bash
rm -rf build-phase6-asan

cmake -S . -B build-phase6-asan -G Ninja \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  2>&1 | tee audit-phase6-actual-machine/asan-configure.log
```

Build:

```bash
cmake --build build-phase6-asan \
  2>&1 | tee audit-phase6-actual-machine/asan-build.log
```

Run focused Phase 6 tests:

```bash
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
ctest --test-dir build-phase6-asan \
  -R "Session|Reconnect|Bluetooth" \
  --output-on-failure \
  2>&1 | tee audit-phase6-actual-machine/asan-tests.log
```

If Qt/third-party leak noise appears, distinguish real project leaks from framework globals.

Do not hide genuine Auralis leaks.

---

# 28. UBSan Build

```bash
rm -rf build-phase6-ubsan

cmake -S . -B build-phase6-ubsan -G Ninja \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" \
  2>&1 | tee audit-phase6-actual-machine/ubsan-configure.log
```

Build:

```bash
cmake --build build-phase6-ubsan \
  2>&1 | tee audit-phase6-actual-machine/ubsan-build.log
```

Run:

```bash
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-phase6-ubsan \
  -R "Session|Reconnect|Bluetooth" \
  --output-on-failure \
  2>&1 | tee audit-phase6-actual-machine/ubsan-tests.log
```

---

# 29. Optional Valgrind Focus

If installed:

```bash
valgrind --version
```

Run the reconnect timer stress binary directly if feasible.

Focus on:

```text
definitely lost
indirectly lost
invalid read/write
```

Do not require Valgrind if ASan gives reliable coverage.

---

# 30. Bluetooth Service State

Capture:

```bash
{
  echo "===== BLUETOOTH SERVICE ====="
  systemctl status bluetooth --no-pager 2>&1 || true

  echo "===== CONTROLLERS ====="
  bluetoothctl list 2>&1 || true

  echo "===== DEVICES ====="
  bluetoothctl devices 2>&1 || true

  echo "===== PAIRED ====="
  bluetoothctl paired-devices 2>&1 || true
} | tee audit-phase6-actual-machine/bluetooth-state.txt
```

---

# 31. PipeWire State

Capture:

```bash
{
  echo "===== PIPEWIRE ====="
  systemctl --user status pipewire --no-pager 2>&1 || true

  echo "===== WIREPLUMBER ====="
  systemctl --user status wireplumber --no-pager 2>&1 || true

  echo "===== WPCTL STATUS ====="
  wpctl status 2>&1 || true
} | tee audit-phase6-actual-machine/pipewire-state.txt
```

These commands are diagnostics only.

They are not a production implementation dependency.

---

# 32. Desktop Launch

Run:

```bash
timeout 20s ./build-phase6-final/apps/desktop/auralis-desktop \
  > audit-phase6-actual-machine/desktop-launch.log 2>&1
```

If app is intended to remain open, timeout exit code alone is not a failure.

Inspect log for:

```text
crash
assert
QObject warnings
DBus errors
PipeWire errors
session-load errors
```

If application binary path differs, adapt.

---

# 33. Real Hardware Validation — Preflight

If two Bluetooth audio/hearing devices are available, record their addresses privately.

Do not write placeholders with `<...>`.

Use:

```bash
export AURALIS_DEVICE_A="AA:BB:CC:DD:EE:FF"
export AURALIS_DEVICE_B="11:22:33:44:55:66"
```

Confirm both are paired.

---

# 34. Real Hardware — Two Device Happy Path

1. connect A;
2. connect B;
3. verify PipeWire endpoints;
4. launch Auralis;
5. create/load a session containing both;
6. select source;
7. activate;
8. confirm both receive audio;
9. change group volume;
10. confirm both react;
11. stop;
12. verify routes removed.

Record:

```text
PASS / FAIL
actual observations
```

---

# 35. Real Hardware — Degraded Operation

Start both ACTIVE.

Disconnect B physically.

Verify:

```text
A continues audio
session DEGRADED or RECOVERING
A route not unnecessarily recreated
```

Reconnect B.

Verify:

```text
new endpoint resolved if runtime ID changed
B route restored
volume/mute restored
ACTIVE
```

---

# 36. Real Hardware — Policy None

Set session policy:

```text
None
```

Disconnect B.

Verify Auralis does not initiate Bluetooth reconnect.

Observe with:

```bash
bluetoothctl info "$AURALIS_DEVICE_B"
```

and Auralis logs.

When B reconnects externally, verify route does **not** auto-restore until explicit retry/start.

---

# 37. Real Hardware — RestoreRoutesOnly

Set:

```text
RestoreRoutesOnly
```

Disconnect B.

Verify Auralis does not initiate Bluetooth reconnect.

Reconnect B externally.

Verify route automatically restores.

---

# 38. Real Hardware — ReconnectAndRestore

Set:

```text
ReconnectAndRestore
```

Disconnect B.

Verify:

```text
Auralis lower managed reconnect starts
one backoff sequence
route returns
ACTIVE
```

---

# 39. Real Hardware — Device AutoReconnect Preference

If UI/backend exposes device autoReconnect preference:

Set:

```text
device.autoReconnectEnabled=false
session ReconnectAndRestore
```

Disconnect.

Verify behavior matches documented matrix.

Then restore preference.

---

# 40. Real Hardware — Restart Persistence

Save session.

Exit Auralis cleanly.

Restart.

Verify:

- session restored;
- membership restored;
- volume restored;
- recovery settings restored;
- source resolved;
- activation works.

---

# 41. Hardware Test Logging

Write all manual/live results to:

```text
audit-phase6-actual-machine/hardware-validation.log
```

If two devices are unavailable, write:

```text
MULTI-DEVICE HARDWARE VALIDATION: NOT RUN
Reason: ...
```

Do not fake PASS.

---

# 42. Optional Live CTest

If repository contains opt-in live test:

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="${AURALIS_DEVICE_A};${AURALIS_DEVICE_B}" \
ctest --test-dir build-phase6-final \
  -R "SessionLiveIntegration" \
  --output-on-failure \
  2>&1 | tee -a audit-phase6-actual-machine/hardware-validation.log
```

Adapt exact test/environment names from repository.

---

# 43. Review All Skips

Search:

```bash
grep -R "QSKIP\|SKIP" -n tests
```

Every skip must be justified.

A mandatory deterministic software test must not be hidden behind hardware gating.

---

# 44. Flakiness Standard

Any failure across:

```text
20 full-suite runs
100 focused Phase 6 runs
parallel run
ASan
UBSan
```

must be investigated.

Do not dismiss it as “probably timing”.

Phase 6 is asynchronous and timing failures are meaningful.

---

# 45. Code Review: ReconnectPolicy

Review every removal path.

For each:

```text
cancel
success
terminal
exhaustion
cancelAll
```

verify:

```text
entry removed
timer stopped
timer destroyed
attempt state removed
```

---

# 46. Code Review: Session Suppression

For each member lifecycle:

```text
activate
disable
enable
remove
stop
delete
switch
shutdown
```

verify suppression symmetry.

---

# 47. Code Review: Effective Reconnect Rule

Write the actual production condition in the audit.

Example:

```text
device preference
AND session autoReconnect
AND ReconnectAndRestore
AND member enabled
AND state allows recovery
```

Compare code and tests.

---

# 48. Code Review: RECOVERING Invariant

Every state:

```text
RECOVERING
```

must correspond to real work:

```text
scheduled reconnect
in-flight reconnect
deferred reconnect
waiting for endpoint under allowed route recovery
```

No dead-end RECOVERING.

---

# 49. Code Review: Shutdown

Verify service destruction order.

SessionManager must not produce reconnect work after shutdown begins.

---

# 50. Final Severity Classification

Classify every finding:

```text
BLOCKER
MAJOR
MEDIUM
LOW
INFO
```

Software COMPLETE requires:

```text
0 BLOCKER
0 MAJOR
```

---

# 51. Final Audit Report

Create:

```text
audit-phase6-actual-machine/FINAL_AUDIT.md
```

and also copy or summarize into:

```text
docs/validation/phase-6-actual-machine-rigorous-audit.md
```

Include:

## Executive Summary

## Machine Environment

## Git State

## Clean Configure/Build

## CTest Results

## Repeated Test Results

## Parallel Test Result

## Boundary Tests

## Timer Lifetime

## Contention/Deferred Reconnect

## Suppression Lifecycle

## Device Preference Matrix

## Terminal Failure

## Retry Exhaustion

## Source Recovery

## Persistence

## Volume/Mute

## ASan

## UBSan

## Desktop Launch

## Live Two-Device Validation

## Findings by Severity

## Final Verdict

---

# 52. Required Raw Results Table

In FINAL_AUDIT.md include:

| Check | Result | Raw Log |
|---|---|---|
| Clean configure | | configure.log |
| Clean build | | build.log |
| Full CTest | | ctest-full.log |
| 20x full CTest | | ctest-repeat-full.log |
| 100x session/reconnect | | ctest-session-repeat.log |
| Parallel CTest | | ctest-parallel.log |
| Boundary tests | | new-boundary-tests.log |
| ASan | | asan-tests.log |
| UBSan | | ubsan-tests.log |
| Desktop launch | | desktop-launch.log |
| Hardware validation | | hardware-validation.log |

---

# 53. Final Verdict Format

Use exactly:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 LIVE HARDWARE VALIDATION: COMPLETE
PHASE 6 OVERALL STATUS: COMPLETE
```

only if all three are genuinely complete.

If software is complete but hardware is unavailable:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 LIVE HARDWARE VALIDATION: NOT RUN
PHASE 6 OVERALL STATUS: <follow official roadmap gate>
```

If software has blocker/major issues:

```text
PHASE 6 SOFTWARE STATUS: NOT COMPLETE
```

List blockers immediately.

---

# 54. Do Not Modify Source During Audit

This document is for validation.

If you discover a defect:

1. record file/line;
2. record reproduction;
3. record severity;
4. stop calling Phase 6 complete.

Do not silently patch the source and continue unless the user explicitly asks for repair.

That preserves independence between implementation and audit.

---

# 55. Final Principle

The goal is not to produce a green Markdown report.

The goal is to generate reproducible evidence from the actual machine proving that:

```text
multi-device sessions
routing
degraded operation
recovery
policy authority
reconnect lifetime
contention handling
persistence
state invariants
race safety
```

all work reliably.

If the implementation survives this audit with no BLOCKER or MAJOR findings, Phase 6 software can be considered genuinely ready to close.
