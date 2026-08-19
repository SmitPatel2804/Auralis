# Phase 6 Actual-Machine Rigorous Validation Audit

Independent QA on the real Ubuntu development machine after the Absolute Final Correction pass. Production source was **not** modified during this audit.

Raw evidence: `audit-phase6-actual-machine/` (gitignored).

## Executive Summary

Clean configure/build succeeded. Default CTest is **35/35 passed, 0 failed**. Twenty full-suite repetitions and 100 Session/Reconnect/Bluetooth repetitions passed. Parallel CTest (`-j 4`) passed. Focused ASan (`detect_leaks=1`) and UBSan (`halt_on_error=1`) Session/Reconnect/Bluetooth/DeviceLifecycle runs passed with no sanitizer diagnostics. Desktop launched on the real Wayland session, reached `QML root loaded`, BlueZ snapshot, and PipeWire Connected; `timeout 20s` exit 124 is not a crash.

Two-device live hardware validation was **not run**: only one Bluetooth audio device is present (`88:08:94:9D:B4:22`).

No BLOCKER or MAJOR software defects were reproduced. Remaining items are coverage gaps and environment limits (MEDIUM / LOW / INFO).

## Machine Environment

Captured in `environment.txt` at `2026-08-19T15:30:52+05:30`.

| Item | Value |
|---|---|
| Host | Linux smit 7.0.0-29-generic x86_64 |
| OS | Ubuntu 26.04 LTS (resolute) |
| gcc/g++ | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Qt | 6.10.2 |
| PipeWire | 1.6.2 (user service active) |
| dbus-1 | 1.16.2 |
| bluetoothctl | 5.85 |
| DISPLAY | `:0` |
| WAYLAND_DISPLAY | `wayland-0` |
| Valgrind | not installed |

## Git State

Recorded in `git-state.txt`.

- Branch: `master`
- HEAD: `aa28ff965cee6bca61b76971ef72dfa57c4ffb8b`
- Tree: **not clean** — uncommitted Absolute Final Correction delta (intentional):

  - `src/bluetooth/ReconnectPolicy.cpp` / `.h` — `destroyEntryTimer`, test timer count
  - `src/session/SessionManager.cpp` — disable/enable suppression; device `autoReconnectEnabled` gate
  - `src/bluetooth/BluetoothManager.cpp` / `.h` — `isAutoReconnectSuppressed`
  - tests: ReconnectPolicy timer lifetime, DeviceLifecycle contention, SessionManagedReconnect suppression + preference

- Untracked prompts under `docs/prompts/phase-6/`

## Clean Configure/Build

- Configure: `build-phase6-final` via Ninja — success (`configure.log`)
- Build: 252 compile/link steps — success (`build.log`)
- Warning: `-Wconversion` in `ReconnectPolicy::liveRetryTimerCountForTesting()` (`qsizetype` → `int`). Test-only accessor. Severity: **LOW**

## CTest Results

From `ctest-list.txt` / `ctest-full.log`:

| Metric | Value |
|---|---|
| Total CTest targets | 35 |
| Passed | 35 |
| Failed | 0 |
| CTest skipped targets | 0 |

Live targets (`tst_SessionLiveIntegration`, `tst_BlueZLiveIntegration`, `tst_PipeWireLiveIntegration`, `tst_AudioRoutingLiveIntegration`) **pass at CTest level** because internal `QSKIP` runs unless opt-in env vars are set. That is justified gating, not hidden mandatory software coverage.

Opt-in `AURALIS_RUN_SESSION_INTEGRATION=1`: `sessionManagerInitializesWithLiveServices` **PASS** (BlueZ + PipeWire + SessionManager). Two-device slot **SKIP**.

## Repeated Test Results

| Loop | Result | Log |
|---|---|---|
| 20× full suite | 20/20 `35/35 PASS` | `ctest-repeat-full.log` |
| 100× `-R Session\|Reconnect\|Bluetooth` | 100/100 `10/10 PASS` | `ctest-session-repeat.log` |

No flake reproduced. Focused loop was not increased to 250/500; 100 runs already exceed the mandatory floor.

## Parallel Test Result

`ctest -j 4` → 35/35 PASS (`ctest-parallel.log`). Serial + parallel agree. Isolation issue: none observed.

## Boundary Tests

`new-boundary-tests.log`: `tst_DeviceLifecycle`, `tst_ReconnectPolicy`, `tst_SessionManagedReconnectIntegration` PASS; also `tst_SessionManager` / `tst_SessionStateMachine` / router tests PASS.

Mapped slots:

1. Timer cancel cleanup — `cancelDestroysRetryTimer`, `cancelAllDestroysTimers`
2. 1000-cycle timer stress — `repeatedScheduleCancelDoesNotAccumulateTimers` (asserts child `QTimer` count == 0)
3. DLM contention — `busyOperationDefersReconnect`
4. Deferred retry cancel — `deferredReconnectCancelled`; policy None — `deferredReconnectPolicyChangeToNone`
5–6. Suppression disable/enable — `disableRemovesSuppression`, `enableReappliesSuppression`
7. Device preference — `devicePreferenceBlocksReconnect` / `AllowsReconnect` / `sessionPolicyNoneBlocksReconnect`
8. Shutdown/stop suppression — `stopRestoresDeviceBehavior`; `SessionManager::shutdown` calls `stopSession` → `removeReconnectSuppressions`
9. Terminal failure — `terminalReconnectFailureEmitsSignal` uses `org.bluez.Error.AuthenticationFailed` (non-retryable)
10. Exhaustion — `exhaustionCleansUpEntry`, `reconnectExhaustionLeavesPeerHealthyDegraded`
11–13. Policies — `sessionPolicyNoneBlocksReconnect`, `restoreRoutesOnlyRestoresWithoutBluetoothReconnect`, `oneDisconnectSchedulesReconnectOnce`

## Timer Lifetime

Production: every entry removal path (`cancelReconnect`, `cancelAll`, exhaustion, `reportTerminalFailure`, `onConnected` via cancel) calls `destroyEntryTimer` (`stop` + `deleteLater` + nullptr).

Tests assert `liveRetryTimerCountForTesting() == 0` after cancel/cancelAll/1000-cycle stress (after event processing). ASan run included `tst_ReconnectPolicy`. Valgrind: **NOT RUN** (package not installed); ASan leak detection was enabled and passed.

## Contention/Deferred Reconnect

`DeviceLifecycleManager` `reconnectDue` handler (`src/bluetooth/DeviceLifecycleManager.cpp` ~40–48): if `beginOperation(..., Reconnecting)` fails, `retryAfterContention` is called and BlueZ `connectDevice` is skipped.

`busyOperationDefersReconnect` uses the real DLM + FakeBlueZClient: unexpected disconnect schedules reconnect; user `connectDevice` occupies pending; due fires; connect request count stays 1. Completing the user connect **successfully** then calls `onConnected`, which **cancels** the deferred retry. Therefore the prompt’s “busy completes → reconnect connect exactly once” continuation is **not** asserted. Deferred cancel and policy-disable paths **are** asserted.

## Suppression Lifecycle

| Event | Production | Automated test |
|---|---|---|
| activate | `installReconnectSuppressions` (enabled members only) | `activeSessionSuppressesLowerLayerAutoReconnect` |
| disable member | `unsuppressAutoReconnect` | `disableRemovesSuppression` |
| enable member | `suppressAutoReconnect` if session active | `enableReappliesSuppression` |
| remove member | `unsuppressAutoReconnect` | no dedicated slot (code at `removeDevice`) |
| stop | `removeReconnectSuppressions` (all members) | `stopRestoresDeviceBehavior` |
| delete | `stopSession` then drop session | no dedicated suppression slot |
| switch | `activateSession` rejects if another active; deactivate first | no dedicated slot |
| shutdown | `cancelAllRecovery` + `stopSession` | live init shutdown |

## Device Preference Matrix

Effective production rule in `requestManagedReconnect`:

```text
member.enabled
AND session.autoReconnect
AND session.recoveryPolicy == ReconnectAndRestore
AND device registry autoReconnectEnabled (if registry entry exists)
AND path resolvable
AND not already managedReconnectRequested
```

`policyAllowsBluetoothReconnect` is session-level only. Device preference is checked after that, via `DeviceRegistry::findByObjectPath`.

| device.autoReconnectEnabled | session.autoReconnect | Policy | Expected session BT reconnect | Evidence |
|---|---|---|---|---|
| true | true | ReconnectAndRestore | yes | `devicePreferenceAllowsReconnect` |
| true | false | ReconnectAndRestore | no | `policyAllowsBluetoothReconnect` + manager reconnect tests |
| false | true | ReconnectAndRestore | no | `devicePreferenceBlocksReconnect` |
| true | true | None | no | `sessionPolicyNoneBlocksReconnect` |
| true | true | RestoreRoutesOnly | no BT reconnect; routes may restore | `restoreRoutesOnlyRestoresWithoutBluetoothReconnect` |

Preference survival: `devicePreferenceSurvivesSessionLifecycle` — `autoReconnectEnabled=false` remains after deactivate.

If the registry has no device entry, the preference check is skipped (reconnect may still proceed). That is the only incomplete AND in the matrix. Severity: **INFO** (missing device cannot be session-managed via BlueZ path anyway if path is empty; if path exists but registry miss, theoretically possible).

`requestManagedReconnect` sets `recovering = true` before preference failure returns. Reconcile also sets `recovering = true` for ReconnectAndRestore while waiting for the member. That matches “waiting for endpoint under allowed route recovery,” not a dead-end with Policy None (None clears recovering).

## Terminal Failure

`tst_DeviceLifecycle::terminalReconnectFailureEmitsSignal` emits `org.bluez.Error.AuthenticationFailed`. Session integration `terminalReconnectFailureReachesSessionState` exists. Not using retryable `ConnectionAttemptFailed` for the terminal test.

## Retry Exhaustion

Small budget in unit/integration tests. Exhaustion removes the entry (`attempt` reads as 0). Session: `reconnectExhaustionLeavesPeerHealthyDegraded` — peer route stays active, recovering clears on exhausted member.

## Source Recovery

`midSessionSourceLossAndReturn` and `failedSourceStaysQuiescentUntilRetry` exist. FAILED + active routes contradiction is gated by state machine (`tst_SessionStateMachine`) plus Stopping/Failed reconcile suppression.

## Persistence

Serialized identity: session `source` string and Bluetooth `deviceId` (address), plus policy/volume. No PipeWire link IDs. Atomic write via `QSaveFile`. Tests: round-trip (state saved Active loads Idle), malformed JSON, duplicate **device** IDs skipped, missing parent `mkpath`, save fail when parent is a file, manager persistence error surface.

Missing dedicated tests (code exists): future `schemaVersion`, duplicate **session** ids (`seenIds` in load). **MEDIUM** coverage.

## Volume/Mute

`invalidVolumeRejected` rejects NaN, +Inf, -Inf. Clamp 1.4 → 1.0, -0.2 → 0.0. Returning-member volume reapply: `tst_VolumeCoordinator` + RestoreRoutesOnly restore path.

## ASan

Separate `build-phase6-asan`. `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` focused ctest: **PASS**, no AddressSanitizer summaries in `asan-tests.log`. Additional `DeviceLifecycle|ReconnectPolicy` ASan run: PASS.

## UBSan

Separate `build-phase6-ubsan`. `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`: **PASS**, no runtime errors in `ubsan-tests.log`.

## Desktop Launch

`timeout 20s ./build-phase6-final/apps/desktop/auralis-desktop` (`desktop-launch.log`):

- Logger, Bluetooth SystemBusConnected, BlueZAvailable
- PipeWire Connecting → Connected
- Session manager initialized
- Application core ready
- QML root loaded
- BlueZSnapshotApplied adapters=1 devices=1
- Endpoint mapped for `88:08:94:9D:B4:22`
- PipeWire initial registry sync complete `error=none`
- Exit 124 from timeout (expected; app stays open)

No crash, assert, or session-load error observed.

## Live Two-Device Validation

**NOT RUN.** One paired audio device only. Details in `hardware-validation.log`.

Single-device live **init** with `AURALIS_RUN_SESSION_INTEGRATION=1` passed. That is not a substitute for two-device routing/degraded/policy playthrough.

## Findings by Severity

### BLOCKER

None.

### MAJOR

None.

### MEDIUM

1. **Contention continuation gap.** `busyOperationDefersReconnect` proves due-while-busy does not issue a second BlueZ connect, but completing the blocking op as a **successful** connect cancels reconnect. The “then reconnect fires and connect is called once” continuation is unproven.
2. **Deferred reconnect + member disable** is not a DLM-level slot (session `disableMemberDuringRecoveryDoesNotRestore` exists). Prompt §18 listed member disable while deferred.
3. **Persistence edge slots** missing for unsupported schema version and duplicate session IDs (load code handles both).
4. **No 500–1000 start-stop** generation stress iteration inside a test (generation gate is unit-tested once).

### LOW

1. `-Wconversion` on test-only `liveRetryTimerCountForTesting()`.
2. CTest reports QSKIP live binaries as Passed.
3. Preference matrix is split across tests rather than one table-driven `devicePreferenceMatrix` slot; RestoreRoutesOnly row is in `tst_SessionManager`.

### INFO

1. Valgrind not installed.
2. Dedicated suppression tests missing for remove/delete/switch/shutdown (code review: stop/delete/shutdown unsuppress).
3. If `findByObjectPath` misses, device preference is not applied.
4. Tree dirty with intentional uncommitted correction + untracked prompts.
5. Two-device hardware unavailable.

## Final Verdict

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 LIVE HARDWARE VALIDATION: NOT RUN
PHASE 6 OVERALL STATUS: NOT COMPLETE
```

Software COMPLETE: clean build, 35/35 default CTest, no flake in 20× full / 100× focused / parallel, ASan+UBSan focused PASS, 0 BLOCKER, 0 MAJOR.

OVERALL NOT COMPLETE because the official two-device live hardware gate was not executed.

## Required raw results table

| Check | Result | Raw Log |
|---|---|---|
| Clean configure | PASS | configure.log |
| Clean build | PASS (1 test-only -Wconversion) | build.log |
| Full CTest | 35/35 PASS | ctest-full.log |
| 20× full CTest | 20/20 PASS | ctest-repeat-full.log |
| 100× session/reconnect | 100/100 PASS | ctest-session-repeat.log |
| Parallel CTest | 35/35 PASS | ctest-parallel.log |
| Boundary tests | PASS | new-boundary-tests.log |
| ASan | PASS (leaks on) | asan-tests.log |
| UBSan | PASS | ubsan-tests.log |
| Desktop launch | PASS (timeout 124, QML loaded) | desktop-launch.log |
| Hardware validation | NOT RUN (one BT audio device) | hardware-validation.log |
