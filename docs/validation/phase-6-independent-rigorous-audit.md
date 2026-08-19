# Phase 6 Independent Rigorous Audit

Date: **2026-08-19**  
Prompt: `docs/prompts/phase-6/Auralis_PHASE_6_Independent_Deep_Rigorous_Test_and_Audit_Prompt.md`  
Scope: independent, report-only audit of the current working tree after the final Phase 6 correction pass. No production code or tests were changed as part of this audit.

## 1. Executive Summary

This audit reproduced a clean build and a clean default test suite on the current uncommitted correction-pass tree. The core Phase 6 software checks passed again under repetition, parallel execution, and a focused ASan/UBSan run. I did not reproduce a functional software defect in the session engine during this pass.

I did, however, confirm several limitations that matter for the final verdict:

- CTest still reports opt-in live Qt tests as `Passed` when the underlying Qt test body `QSKIP`s.
- The current environment could not provide reliable live validation: `tst_SessionLiveIntegration` failed when opt-in was enabled because PipeWire reported `Host is down`, and `bluetoothctl devices Connected` crashed during evidence capture.
- A few mandatory-looking race and persistence edge cases still lack deterministic automated coverage, even though adjacent code paths look reasonable on inspection.

## 2. Environment

Raw evidence: `audit-phase6-final/environment.txt`

- Host: Ubuntu 26.04 LTS, kernel `7.0.0-29-generic`
- Compiler: GCC/G++ `15.2.0`
- Build tools: CMake `4.2.3`, Ninja `1.13.2`
- Qt: `6.10.2`
- PipeWire: `1.6.2`
- D-Bus dev package: `1.16.2`
- BlueZ user tool: `bluetoothctl 5.85`
- GUI session variables were present: `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`, `XDG_SESSION_TYPE=wayland`

Environment caveats observed during the audit:

- `bluetoothctl devices Connected` crashed with `SIGSEGV` during evidence capture, so no trustworthy connected-device enumeration was obtained from that command.
- `systemctl --user is-active ...` produced no active-state lines in the captured evidence.
- PipeWire was not reachable from the live session integration test in this execution context (`Failed to connect to PipeWire: Host is down`).

## 3. Repository State

Raw evidence: `audit-phase6-final/git-status.txt`

- Branch: `master`
- Base commit at audit start: `ac40a5e`
- The working tree was intentionally **dirty** with uncommitted Phase 6 correction-pass changes, including:
  - reconnect idempotence / exhaustion work in `src/bluetooth/`
  - stop reentrancy / Policy None / persistence work in `src/session/`
  - new integration coverage in `tests/integration/tst_SessionManagedReconnectIntegration.cpp`

This audit treated prior Markdown reports only as context. All results below were reproduced from `build-rigorous`.

## 4. Build Result

Raw evidence:

- `audit-phase6-final/configure.log`
- `audit-phase6-final/build.log`

Commands:

```bash
rm -rf build-rigorous
cmake -S . -B build-rigorous -G Ninja
cmake --build build-rigorous
```

Result:

- Configure: **PASS**
- Build: **PASS**
- No Phase 6-specific build failure was reproduced.

## 5. Full Automated Test Result

Raw evidence:

- `audit-phase6-final/ctest-full.log`
- `audit-phase6-final/ctest-list.log`

Command:

```bash
ctest --test-dir build-rigorous --output-on-failure
ctest --test-dir build-rigorous -N
```

Observed result:

- Total CTest targets: **35**
- Failed CTest targets: **0**
- CTest summary: **35/35 passed**

Important nuance:

- The opt-in live targets `tst_BlueZLiveIntegration`, `tst_PipeWireLiveIntegration`, `tst_AudioRoutingLiveIntegration`, and `tst_SessionLiveIntegration` still appear as `Passed` in CTest when not opted in, even though their Qt test bodies are skip-gated. This is a reporting limitation, not a newly discovered Phase 6 product bug.

## 6. Repeated / Flaky Test Result

Raw evidence:

- `audit-phase6-final/ctest-repeat.log`
- `audit-phase6-final/ctest-session-repeat.log`
- `audit-phase6-final/ctest-parallel.log`

Commands:

```bash
for i in $(seq 1 20); do
  ctest --test-dir build-rigorous --output-on-failure || exit 1
done

for i in $(seq 1 100); do
  ctest --test-dir build-rigorous -R "Session|session" --output-on-failure || exit 1
done

ctest --test-dir build-rigorous -j "$(nproc)" --output-on-failure
```

Observed result:

- Full suite x20: **PASS**
- Session-focused repetition x100: **PASS**
- Parallel CTest: **PASS**

No flake was reproduced in these runs.

## 7. Recovery Policy Verification

Evidence sources:

- `audit-phase6-final/tst_SessionManager-v2.txt`
- `audit-phase6-final/tst_SessionManagedReconnectIntegration-v2.txt`
- `audit-phase6-final/requirements-matrix.md`

Verified behaviors:

- `RecoveryPolicy::None`
  - no automatic route restore when a member endpoint returns
  - no automatic reactivation of an inactive route
  - lower reconnect scheduling is cancelled when policy flips to `None`
- `RestoreRoutesOnly`
  - no Bluetooth reconnect request
  - route restore happens when the endpoint reappears
  - restored member receives current volume and mute state
- `ReconnectAndRestore`
  - SessionManager requests managed reconnect intent once
  - lower layer owns retries
  - restored member returns to `Active`

Assessment: **PASS**

## 8. Reconnect Ownership Verification

Code paths inspected:

- `src/bluetooth/ReconnectPolicy.cpp`
- `src/bluetooth/BluetoothManager.cpp`
- `src/bluetooth/DeviceLifecycleManager.cpp`
- `src/session/SessionManager.cpp`

Automated evidence:

- `tests/unit/bluetooth/tst_ReconnectPolicy.cpp::duplicateScheduleWhileTimerActiveDoesNotConsumeAttempt`
- `tests/integration/tst_SessionManagedReconnectIntegration.cpp::oneDisconnectSchedulesReconnectOnce`

Observed behavior:

- one disconnect episode produced one attempt count increment
- one scheduled retry state existed
- duplicate disconnect notifications did not double-burn the retry budget

Assessment: **PASS**

## 9. Reconnect Exhaustion

Automated evidence:

- `tests/unit/bluetooth/tst_ReconnectPolicy.cpp::emitsExhaustedOnceAfterBudget`
- `tests/integration/tst_SessionManagedReconnectIntegration.cpp::reconnectExhaustionLeavesPeerHealthyDegraded`
- `tests/integration/tst_SessionManagedReconnectIntegration.cpp::reconnectExhaustionWithNoHealthyMembersFails`

Observed behavior:

- `ReconnectPolicy` emits exhaustion once
- `BluetoothManager` forwards exhaustion
- `SessionManager` clears member recovery intent
- a healthy peer leaves the session `Degraded`
- with no healthy peer remaining, the session becomes `Failed`

Assessment: **PASS**

## 10. State Invariants

Inspected code:

- `src/session/SessionStateMachine.cpp`
- `src/session/SessionManager.cpp`
- `src/session/RoutingCoordinator.cpp`

Test evidence:

- `tests/unit/session/tst_SessionStateMachine.cpp`
- `tests/unit/session/tst_SessionManager.cpp`
- `tests/integration/tst_SessionLifecycleIntegration.cpp`

Confirmed invariants:

- `Idle` teardown paths end with zero owned routes in the existing tests.
- `Stopping` suppresses reconcile-driven route creation from route callbacks.
- `Failed` does not auto-recover without explicit `retrySession()`.
- stale generation work is rejected by `RoutingCoordinator::reconcile(...)` and `SessionManager::reconcileActiveSession(...)`.

Coverage limit:

- The single-session generation race is covered.
- A multi-session late-callback authority test was not present.

Assessment: **PASS with documented coverage gaps**

## 11. Route Reentrancy

Code evidence:

- `src/session/SessionManager.cpp`
  - `stopSession(...)` bumps generation before teardown
  - `handleRouteStateChanged(...)` ignores `Stopping`, `Idle`, and `Failed`
  - `handleRouteRemoved(...)` only performs bookkeeping during stop

Test evidence:

- `tests/unit/session/tst_SessionManager.cpp::deactivateSessionIsReentrancySafe`
- `tests/integration/tst_SessionLifecycleIntegration.cpp::twoDeviceHappyPathAndStopDuringRecovery`
- `audit-phase6-final/asan-session-noleak.log`

Observed result:

- No recursive stop or route recreation was reproduced.

Assessment: **PASS**

## 12. Source Recovery

Test evidence:

- `tests/unit/session/tst_SessionManager.cpp::midSessionSourceLossAndReturn`
- `tests/unit/session/tst_SessionManager.cpp::failedSourceStaysQuiescentUntilRetry`

Observed behavior:

- Mid-session source loss transitions to `Recovering`.
- Source return restores the session to `Active` without explicit retry.
- Start with missing source transitions to `Failed` and remains quiescent until retry.

Assessment: **PASS**

## 13. Persistence

Code evidence:

- `src/session/SessionPersistence.cpp`
- `src/session/SessionManager.cpp`

Test evidence:

- `tests/unit/session/tst_SessionPersistence.cpp`
- `tests/unit/session/tst_SessionManager.cpp::persistenceFailureSurfacesError`

Verified behaviors:

- first-run save can create nested parent directories
- invalid JSON returns safely with an error string
- duplicate device ids inside one session are normalized by skipping duplicates
- create-session persistence failure returns an empty id and rolls back in-memory state
- persisted authority uses stable logical fields such as `source` and `deviceId`; it does not persist PipeWire link ids or BlueZ object paths as the sole authority

Coverage limits:

- future-schema rejection exists in code but lacks a direct test
- duplicate session-id rejection exists in code but lacks a direct test
- wrong-root / truncated JSON variants were not individually tested

Assessment: **PASS with documented coverage gaps**

## 14. Volume / Mute

Test evidence:

- `tests/unit/session/tst_VolumeCoordinator.cpp`
- `tests/unit/session/tst_SessionManager.cpp::invalidVolumeRejected`
- `tests/integration/tst_SessionLifecycleIntegration.cpp::memberReturnsWithNewEndpointId`

Verified behaviors:

- effective volume combines group volume and per-device trim
- mute propagates through the router
- unavailable members retain intended volume semantics until recovery
- restored members receive current mute and volume settings
- `NaN` and infinities are rejected; finite out-of-range values are clamped

Assessment: **PASS**

## 15. Race / Stale Callback Tests

Clearly covered:

- stale generation after stop
- stop reentrancy
- duplicate reconnect scheduling
- recovery-policy changes to `None`
- disable-member-during-recovery

Not directly covered by deterministic tests in the current tree:

- activating a second session while the first later delivers stale callbacks
- deleting a recovering session and then delivering late callbacks
- toggling `autoReconnect=false` during active recovery
- removing a member during recovery and then delivering late callbacks

Assessment: **MEDIUM coverage gap**, not a reproduced software defect.

## 16. Sanitizer and Static Checks

Raw evidence:

- `audit-phase6-final/static-grep.txt`
- `audit-phase6-final/asan-configure.log`
- `audit-phase6-final/asan-build.log`
- `audit-phase6-final/asan-session.log`
- `audit-phase6-final/asan-session-noleak.log`

Static search findings:

- no `const_cast` in the audited session/bluetooth paths
- no `reinterpret_cast` in the session path
- no production shell-outs (`bluetoothctl`, `wpctl`, `pactl`, `pw-link`, `pw-cli`) in `src/`, `include/`, or `apps/`
- session code uses `QObject::connect(..., this, ...)` or direct member-slot connections, so the inspected connections had an owning QObject context

Sanitizer result:

- combined ASan/UBSan build: **PASS**
- focused `Session|ReconnectPolicy` CTest under `ASAN_OPTIONS=detect_leaks=0`: **7/7 PASS**

Tooling caveat:

- `ASAN_OPTIONS=detect_leaks=1` produced universal failure because LeakSanitizer does not function correctly under this harness’s ptrace/tracing environment. I did not classify that as a product defect.

## 17. Live Hardware Validation

Raw evidence:

- `audit-phase6-final/session-live-v2.txt`
- `audit-phase6-final/desktop-launch.log`
- `audit-phase6-final/desktop-launch-offscreen.log`

Observed results:

- `AURALIS_RUN_SESSION_INTEGRATION=1 ./build-rigorous/tests/integration/tst_SessionLiveIntegration -v2`
  - `sessionManagerInitializesWithLiveServices`: **FAIL (environment)** because PipeWire connection failed with `Host is down`
  - `twoDeviceLiveSessionIfConfigured`: **SKIP** because `AURALIS_EXPECT_DEVICE_ADDRESSES` was not provided
- direct desktop launch with Wayland plugin: **FAIL (environment)** because the platform plugin could not initialize in this execution context
- offscreen desktop launch: reached `QML root loaded`, then was terminated manually after the timeout wrapper failed to exit cleanly in this environment

Verdict for live coverage:

- `LIVE HARDWARE VALIDATION: NOT RUN`
- `MULTI-DEVICE LIVE VALIDATION: NOT RUN`

This audit does **not** claim live two-device proof.

## 18. Remaining Findings

### MEDIUM

1. Multi-session late-callback authority is not covered by a deterministic automated test.
2. Delete-during-recovery with late callbacks is not covered by a deterministic automated test.
3. `autoReconnect=false` during active recovery is implemented but not directly tested.
4. Persistence edge tests for future schema version, duplicate session ids, and additional malformed-root variants are missing.

### LOW

1. CTest still masks Qt `QSKIP` live tests as `Passed` at the CTest summary layer.
2. `bluetoothctl devices Connected` crashed during environment capture, reducing confidence in host-side Bluetooth enumeration.
3. Direct Wayland desktop launch is not viable in this execution context; offscreen launch was the only successful startup proof.

### BLOCKER

- None reproduced.

### MAJOR

- None reproduced.

## 19. Final Verdict

The software evidence from this audit supports the corrected Phase 6 session engine on the current tree:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 HARDWARE VALIDATION: NOT COMPLETE
PHASE 6 OVERALL STATUS: NOT COMPLETE
```

Reasoning:

- clean configure/build reproduced
- default regression suite reproduced cleanly
- no flake reproduced in repeated or parallel runs
- reconnect ownership, exhaustion, Policy None, source recovery, persistence rollback, and stop reentrancy all passed focused verification
- sanitizer coverage passed after disabling leak detection that is known-bad in this harness
- two-device live validation was not completed in this pass
