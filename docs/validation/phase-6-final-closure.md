# Phase 6 Final Closure — Remaining Issues 100% Correction

Date: **2026-08-19**  
Prompt: `docs/prompts/phase-6/Auralis_PHASE_6_Final_Remaining_Issues_100_Percent_Correction_Prompt.md`  
Prior hardening: this file previously recorded the A–N correction pass; this document now records the **final remaining-issue pass** (duplicate reconnect, exhaustion, stop reentrancy, Policy None reactivation, JSON UB, createSession contract, mid-session source loss).

## Baseline

```text
git: master @ ac40a5e (working tree then edited)
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Pre-change default ctest: **34/34 PASS**.

Post-change clean tree: **35/35 PASS** (added `tst_SessionManagedReconnectIntegration`).

## Remaining issues

### A. Duplicate managed reconnect scheduling

- **Root cause:** `ReconnectPolicy::scheduleReconnect` always incremented `attempt` and restarted the timer. `DeviceLifecycleManager::handleUnexpectedDisconnect` and `BluetoothManager::requestManagedReconnect` (from `SessionManager`) both scheduled on one `Connected=false`.
- **Files:** `include/auralis/bluetooth/ReconnectPolicy.h`, `src/bluetooth/ReconnectPolicy.cpp`, `src/bluetooth/DeviceLifecycleManager.cpp`, `src/bluetooth/BluetoothManager.cpp`
- **Fix:** If a timer is active or a due attempt is in-flight, `scheduleReconnect` is a no-op (`ManagedReconnectAlreadyScheduled`). Callers skip when `isScheduled` or `isReconnectInProgress`. DLM calls `completeReconnectAttempt` before a retry so the next attempt can start.
- **Tests:** `tst_ReconnectPolicy::duplicateScheduleWhileTimerActiveDoesNotConsumeAttempt`; `tst_SessionManagedReconnectIntegration::oneDisconnectSchedulesReconnectOnce`
- **Result:** PASS

### B. Reconnect exhaustion must reach session state

- **Root cause:** At `maxAttempts`, `scheduleReconnect` returned silently. Session members could stay `recovering`.
- **Files:** `ReconnectPolicy` (`reconnectExhausted`), `BluetoothManager::managedReconnectExhausted`, `SessionManager::handleManagedReconnectExhausted`
- **Fix:** Emit exhaustion once; SessionManager clears recovering / managed request, sets `RecoveryExhausted`, emits `sessionError`, recomputes. Exhausted members are not rescheduled until `retrySession()`. Healthy peer → `Degraded`; no healthy members → `Failed`.
- **Tests:** `tst_ReconnectPolicy::emitsExhaustedOnceAfterBudget`; `tst_SessionManagedReconnectIntegration::reconnectExhaustionLeavesPeerHealthyDegraded`; `reconnectExhaustionWithNoHealthyMembersFails`
- **Result:** PASS

### C. Stop/deactivate reentrancy

- **Root cause:** `AudioRouter::deactivateRoute` emits Inactive synchronously. `handleRouteStateChanged` still reconciled during `Stopping`, which called `stopSessionRoutes` again.
- **Files:** `src/session/SessionManager.cpp`
- **Fix:** `stopSession` bumps generation first. Route callbacks in Stopping/Idle/Failed return after optional bookkeeping (`RouteCallbackIgnoredDuringStopping`). Reconcile does not own stop teardown.
- **Tests:** `tst_SessionManager::deactivateSessionIsReentrancySafe`
- **Result:** PASS (also ASan/UBSan)

### D. RecoveryPolicy::None existing-route reactivation

- **Root cause:** Create was gated by `autoRestoreAllowed`; `else if (!routeActive) activateRoute` was not. Runtime flags were stale until after reconcile.
- **Files:** `src/session/RoutingCoordinator.cpp`, `src/session/SessionManager.cpp` (`refreshRuntime` before `applyAutoRestoreFlags`)
- **Fix:** Reactivation uses the same gate as create. None never auto-restores missing or inactive routes; `retrySession()` sets `autoRestoreAllowed`.
- **Tests:** `tst_RoutingCoordinator::policyNoneDoesNotReactivateExistingRoute`; `tst_SessionManager::recoveryPolicyNoneDoesNotReactivateInactiveRoute`
- **Result:** PASS

### E. JSON parse const-cast UB

- **Root cause:** `const QJsonParseError` + `const_cast` in `SessionPersistence::load`.
- **Files:** `src/session/SessionPersistence.cpp`
- **Fix:** Mutable `QJsonParseError parseError`; `fromJson(data, &parseError)`.
- **Tests:** `tst_SessionPersistence::invalidJsonDoesNotCrash` (now asserts error string)
- **Result:** PASS; `rg const_cast` on `*.{h,cpp}` is empty

### F. createSession persistence failure contract

- **Root cause:** Returned a UUID and kept the in-memory session on persist failure (`sessionError` only).
- **Files:** `src/session/SessionManager.cpp`
- **Fix:** Keep `Q_INVOKABLE QString createSession`. On failure: log `SessionPersistenceCreateFailed`, emit `sessionError` via `persistAll`, pop the in-memory session, return `{}`.
- **Tests:** `tst_SessionManager::persistenceFailureSurfacesError`
- **Result:** PASS

### G. Mid-session source loss/return

- **Root cause:** `SuppressCreate` only refreshed runtime, so routes stayed Active; recovering flags were cleared because `routeActive` was still true. State went `Degraded` instead of `Recovering`.
- **Files:** `RoutingCoordinator` SuppressCreate now tears down routes but keeps `routeRequested`; SessionManager sets recovering when policy allows restore.
- **Semantics (locked):** Active → source disappears → Recovering → source returns → Active without `retrySession`. Activate with no source still Failed until retry (`failedSourceStaysQuiescentUntilRetry`).
- **Tests:** `tst_SessionManager::midSessionSourceLossAndReturn`
- **Result:** PASS

## Reconnect ownership

`ReconnectPolicy` is the single schedule state: one attempt counter, one backoff timer, one in-flight flag per device path. DLM is the primary scheduler on unexpected disconnect. SessionManager only requests managed reconnect as session recovery intent; both paths are idempotent. Duplicate `Connected=false` cannot increment attempts while a timer or in-flight attempt exists.

## Recovery exhaustion path

```text
ReconnectPolicy::reconnectExhausted
  -> BluetoothManager::managedReconnectExhausted
    -> SessionManager::handleManagedReconnectExhausted
```

## Stop/reentrancy safety

Outer `stopSession` owns teardown. Synchronous router Inactive/Removed during Stopping is ignored for reconcile/create/reconnect.

## Policy None

Create **and** reactivate of inactive/failed routes are suppressed outside `Starting` unless `autoRestoreAllowed` (set by `retrySession` / explicit start).

## Persistence

Parser has no const-cast. `createSession` does not report success on persist failure.

## Test commands

```bash
ctest --test-dir build --output-on-failure
# 35/35 PASS

./build/tests/unit/bluetooth/tst_ReconnectPolicy
./build/tests/unit/session/tst_SessionManager
./build/tests/integration/tst_SessionManagedReconnectIntegration
```

## Sanitizer

Temporary `build-asan` with `-fsanitize=address,undefined` (not a project CMake policy change):

```text
tst_ReconnectPolicy: 9 passed
tst_SessionManager: 21 passed
tst_SessionLifecycleIntegration: 4 passed
tst_SessionManagedReconnectIntegration: 6 passed
```

No ASan/UBSan failures.

## Hardware

Not re-run in this software-gate pass. Prior live validation: single BT device (`88:08:94:9D:B4:22`); two-device live session remains opt-in via `AURALIS_RUN_SESSION_INTEGRATION` + `AURALIS_EXPECT_DEVICE_ADDRESSES`.

## Final verdict

```text
PHASE 6 STATUS: COMPLETE
```

Software gates from the remaining-issues prompt are closed. Phase 7 Sessions UI is still out of scope. Two-device hardware live activate is opt-in, not a remaining software blocker.
