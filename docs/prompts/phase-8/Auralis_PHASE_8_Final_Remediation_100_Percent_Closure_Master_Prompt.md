# AURALIS — PHASE 8 FINAL REMEDIATION + 100% CLOSURE MASTER PROMPT

**Project:** Auralis  
**Target phase:** Phase 8 — Reliability, Testing and Production Hardening  
**Purpose of this prompt:** Final remediation of the remaining Phase 8 defects and evidence-driven closure of the Phase 8 software exit gate  
**Target environment:** Cursor AI IDE running inside the actual Auralis repository on the Linux development machine  
**Important:** Phases 0–7 are already implemented. A substantial Phase 8 implementation also already exists. This is **NOT** a greenfield Phase 8 implementation prompt. It is a surgical correction, hardening, test-expansion, and closure prompt.

---

# 0. ROLE AND OPERATING MODE

You are acting as a **principal C++/Qt/Linux reliability engineer** responsible for bringing the existing Auralis Phase 8 implementation from a strong but incomplete state to a genuinely production-hardened and test-evidenced state.

You must work directly from the repository in front of you.

Do **not** assume that a previous audit, README, test result, or comment is correct merely because it exists in the repository.

Your job is to:

1. inspect the current implementation;
2. reproduce and validate the issues described in this prompt;
3. fix the underlying production behavior;
4. add deterministic regression tests for every software-level defect;
5. expand Phase 8 integration/stress coverage;
6. harden packaging metadata/install rules;
7. run the complete hardware-independent validation matrix;
8. update the Phase 8 audit documents with truthful evidence;
9. leave hardware-only checks explicitly pending unless they are actually executed on the real machine with real devices;
10. stop only when the **software portion** of the Phase 8 exit gate can be defended with code, tests, and command output.

Do not merely modify documentation so that Phase 8 appears complete.

Do not weaken tests to make failures disappear.

Do not add arbitrary sleeps to hide races.

Do not replace existing working architecture without a compelling reason.

Prefer small, cohesive, testable corrections.

---

# 1. AUTHORITATIVE CONTEXT

The repository already contains substantial Phase 8 work, including at least:

- `RecoveryManager`
- `RecoveryTypes`
- `ServiceRetryPolicy`
- `SystemPowerMonitor`
- BlueZ service watching
- Bluetooth reconnect pause/resume hooks
- PipeWire reconnect logic
- generation fencing
- session reconciliation
- recovery preferences
- diagnostics status
- logger rotation
- atomic persistence using `QSaveFile`
- `.desktop` metadata
- AppStream metadata
- CPack DEB configuration
- Phase 8 unit/integration tests
- Phase 8 implementation/validation audit documents

The current implementation must therefore be **preserved where correct** and **corrected where incomplete**.

The goal is not to create a second recovery system.

The architectural principle remains:

> **Centralized orchestration, decentralized ownership.**

That means:

- `RecoveryManager` coordinates system-wide recovery state and cross-service reconciliation.
- `BluetoothManager`, `ReconnectPolicy`, and `DeviceLifecycleManager` continue owning Bluetooth/device-specific behavior.
- `PipeWireManager` may own its own bounded reconnect timing.
- `SessionManager` continues owning session semantics.
- `AudioRouter` / routing components continue owning routing.
- `SystemPowerMonitor` owns detection of suspend/resume lifecycle events.
- `ConfigurationManager` owns persisted preferences.
- `Logger` owns file logging/rotation.
- CMake/CPack owns packaging.

Do not create duplicate retry engines for the same subsystem.

---

# 2. CURRENT INDEPENDENT AUDIT VERDICT

Treat the current state as:

```text
PHASE 8 SOFTWARE IMPLEMENTATION: PARTIALLY COMPLETE
PHASE 8 EXIT GATE: FAIL — REMEDIATION REQUIRED
```

The existing repository audit may currently say that software implementation is complete and that only live/hardware validation is pending. That statement is too optimistic.

There are still software-level defects and mandatory hardware-independent tests that must be addressed.

The highest-priority findings are:

1. `restoreOnResume=false` can leave Bluetooth reconnect permanently paused.
2. `autoRecoverServices` is not authoritative over PipeWire auto-reconnect.
3. runtime system D-Bus loss/reconnect is not truly monitored/recovered by the production BlueZ client.
4. PipeWire reconnect attempt accounting can reset too early, weakening bounded retry guarantees.
5. `RecoveryManager` contains dormant/duplicate PipeWire retry machinery despite `PipeWireManager` being the actual retry owner.
6. central recovery diagnostics do not accurately reflect PipeWire reconnect attempt count and exhaustion.
7. BlueZ/service return can trigger redundant snapshot refreshes.
8. Phase 8 cross-layer integration coverage is too thin.
9. required stress/soak/failure-injection coverage is incomplete.
10. sanitizer evidence is missing.
11. packaging has icon/license/contact/install-metadata issues.
12. live/hardware validation is still pending and must remain explicitly pending until actually performed.

---

# 3. NON-NEGOTIABLE SAFETY RULES FOR THE REPOSITORY

## 3.1 Do not regress Phases 0–7

Before editing, inspect the current architecture and tests.

The following subsystems are considered established and should not be rewritten casually:

- Bluetooth device discovery
- pairing / connection ownership
- `ReconnectPolicy`
- `DeviceLifecycleManager`
- `DeviceRegistry`
- PipeWire graph/object-store infrastructure
- endpoint resolution
- routing
- stable endpoint identity logic
- session management
- session persistence
- Phase 6 managed reconnect/session behavior
- Phase 7 diagnostics/logging behavior
- existing live integration test gates

If a Phase 8 fix touches any of these areas, add regression coverage.

---

## 3.2 Do not invent a second device reconnect policy

Bluetooth reconnect remains owned by the existing Bluetooth lifecycle/reconnect components.

`RecoveryManager` may:

- pause the reconnect engine;
- resume it;
- request a fresh BlueZ snapshot;
- request session reconciliation;
- expose status.

It must not independently call `org.bluez.Device1.Connect` as a replacement for the existing reconnect system.

---

## 3.3 Do not create competing PipeWire retry owners

Decide on one authoritative owner for PipeWire retry scheduling.

The current code indicates that `PipeWireManager` already owns:

- reconnect attempt counter;
- retry delay;
- retry timer;
- actual reconnect operation;
- `reconnectAttemptStarted`;
- `reconnectExhausted`.

That is a reasonable ownership model.

If you retain this model:

- remove/deprecate dormant retry scheduling from `RecoveryManager`;
- make `RecoveryManager` observe PipeWire retry progress and exhaustion;
- let `RecoveryManager` coordinate status/session reconciliation;
- do not maintain a second independent PipeWire retry timer.

---

## 3.4 No fake completion

Never mark any of the following as PASS unless actually run:

- real BlueZ daemon bounce;
- real Bluetooth adapter disappearance/reappearance;
- real device reconnect;
- real PipeWire daemon restart;
- real WirePlumber restart;
- real two-device session recovery;
- real laptop suspend/resume;
- long hardware soak.

A skipped opt-in test is **not** a pass.

A fake backend test is **not** a hardware pass.

An offscreen launch in a sandbox is **not** proof of hardware recovery.

---

# 4. REQUIRED INITIAL RECONNAISSANCE

Before modifying code, perform and record the following.

## 4.1 Repository state

Run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
```

Record the result in the final remediation audit.

Do not discard unrelated user changes.

---

## 4.2 Toolchain

Record:

```bash
gcc --version | head -1
g++ --version | head -1
cmake --version | head -1
ninja --version
qmake6 --version || true
bluetoothctl --version || true
pipewire --version || true
wireplumber --version || true
uname -a
```

Also record relevant `pkg-config` versions when available.

---

## 4.3 Baseline build and test

Use the repository's normal build workflow.

Prefer a clean baseline build:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If the repository requires explicit options, inspect its current documented build commands and use them.

Record:

- total tests;
- passed;
- failed;
- skipped;
- duration;
- any live-test skips.

If baseline fails before your changes, determine whether the failure is pre-existing.

---

# 5. REMEDIATION WORKSTREAM A — FIX SUSPEND/RESUME SEMANTICS

## 5.1 Confirm the current bug

Inspect:

```text
src/recovery/RecoveryManager.cpp
RecoveryManager::onPreparingForSleep(bool)
```

The current sequence is effectively:

```cpp
if (sleeping) {
    pauseBluetoothReconnect();
    ...
    return;
}

status_.suspended = false;
...
if (!restoreOnResume_) {
    ...
    return;
}

resumeBluetoothReconnect();
```

This creates a defect:

```text
sleep
→ Bluetooth reconnect engine paused
→ resume with restoreOnResume=false
→ early return
→ reconnect engine never resumed
```

This must be fixed.

---

## 5.2 Required semantic contract

Define and document the meaning of `restoreOnResume`.

Recommended contract:

```text
restoreOnResume = true
    Automatically trigger service/session recovery and reconciliation after resume.

restoreOnResume = false
    Do NOT proactively restore the prior session/routes solely because a resume occurred,
    but restore internal subsystem ability to operate normally.
```

Therefore:

- the Bluetooth reconnect engine must not remain permanently paused;
- timers that are merely paused for suspend must return to a sane post-resume state;
- stale pre-suspend callbacks must remain fenced;
- no forced session restoration should occur if preference says not to restore;
- normal future user/device activity must still function.

---

## 5.3 Implement the fix

Refactor resume handling into explicit steps such as:

1. leave suspended state;
2. increment suspend epoch;
3. bump recovery generation;
4. resume internal reconnect machinery that was paused solely for suspend;
5. if `restoreOnResume == false`:
   - do not trigger proactive BlueZ refresh/session restore/PipeWire reconnect solely for resume;
   - place recovery status into an accurate non-suspended state;
   - return;
6. otherwise trigger coalesced recovery.

Do not blindly use this exact ordering if source ownership proves another ordering safer, but preserve the semantics above.

---

## 5.4 Required regression tests

Extend `tests/unit/recovery/tst_RecoveryManager.cpp`.

Add a test equivalent to:

```text
resumeWithRestoreDisabledDoesNotLeaveBluetoothReconnectPaused
```

It must prove:

- sleep calls pause exactly once;
- resume calls resume exactly once even when `restoreOnResume=false`;
- proactive session refresh is not invoked because restore is disabled;
- no PipeWire reconnect is forced solely by resume when disabled;
- `suspended == false` after resume;
- suspend epoch increments;
- stale pre-suspend reconciliation does not run.

Also test repeated cycles:

```text
sleep → resume(no restore) → sleep → resume(no restore)
```

and verify no cumulative pause/resume imbalance.

---

# 6. REMEDIATION WORKSTREAM B — MAKE `autoRecoverServices` AUTHORITATIVE

## 6.1 Current problem

Inspect:

```text
src/core/ApplicationCore.cpp
apps/desktop/DesktopApplication.cpp
src/audio/PipeWireManager.cpp
src/recovery/RecoveryManager.cpp
```

Current configuration wiring updates:

```cpp
RecoveryManager::setAutoRecoverEnabled(...)
```

But `PipeWireManager` separately defaults to automatic reconnect and exposes:

```cpp
setAutoReconnectEnabled(bool)
```

with no authoritative production configuration link.

This means the UI can say:

```text
Auto-recover Bluetooth and audio services = OFF
```

while PipeWire continues to automatically reconnect.

That is unacceptable.

---

## 6.2 Define exact policy semantics

Create one explicit behavioral contract for `autoRecoverServices`.

Recommended:

When `autoRecoverServices == true`:

- service-level BlueZ recovery may trigger refresh/reconciliation;
- PipeWire may automatically reconnect;
- system-wide recovery manager may coordinate restoration;
- device-level reconnect still obeys its own per-device/session policies.

When `autoRecoverServices == false`:

- no automatic PipeWire reconnect loop;
- no automatic service-return recovery orchestration that violates the preference;
- system status may reflect degraded/unavailable state;
- explicit user actions such as manual refresh/reconnect must remain possible;
- disabling service recovery must not permanently disable normal subsystem APIs.

Carefully distinguish:

- **service recovery policy**
- **device reconnect preference**
- **session recovery policy**

Do not collapse them into one flag.

---

## 6.3 Wire the preference through the actual owner

When `ConfigurationManager::autoRecoverServicesChanged` fires, ensure it reaches both:

- `RecoveryManager`;
- `PipeWireManager` or whichever component actually owns PipeWire retry scheduling.

Possible implementation options include:

- wiring in `ApplicationCore`;
- exposing a coordinated setter in application composition;
- connecting configuration directly where service ownership permits.

Use the architecture that best matches existing object ownership.

---

## 6.4 Disabling during an active retry

Test this edge case:

```text
PipeWire error
→ auto reconnect timer scheduled
→ user turns autoRecoverServices OFF
→ pending automatic retry must be cancelled
```

Then:

```text
user turns autoRecoverServices ON
```

Do not automatically trigger an unexpected retry unless the designed policy clearly says enabling recovery while degraded should start recovery.

Whichever behavior you choose, document and test it.

---

## 6.5 BlueZ recovery gating

Audit:

```text
RecoveryManager::notifyBlueZAvailable
RecoveryManager::notifySystemBusConnected
BluetoothManager::handleBlueZAvailable
BluetoothManager::handleSystemBusStateChanged
```

Avoid a situation where disabling service recovery in `RecoveryManager` is meaningless because `BluetoothManager` independently performs all automatic restore work.

Clarify ownership:

- low-level manager may update its internal state when service availability changes;
- service availability re-subscription may be mandatory regardless of preference;
- proactive snapshot/session recovery must obey policy where appropriate.

Do not disable required internal D-Bus correctness in the name of user preference.

---

## 6.6 Required tests

Add tests covering:

```text
autoRecoverDisabledStopsPipeWireRetry
autoRecoverPreferencePropagatesToPipeWireOwner
disableAutoRecoverCancelsPendingPipeWireRetry
manualPipeWireReconnectStillWorksWhenAutoRecoverDisabled
blueZReturnWithAutoRecoverDisabledDoesNotTriggerForbiddenRecovery
reEnablingAutoRecoverUsesDocumentedSemantics
```

Use the right test layer for each.

---

# 7. REMEDIATION WORKSTREAM C — REAL RUNTIME SYSTEM D-BUS LOSS/RECOVERY

## 7.1 Current problem

Inspect:

```text
src/bluetooth/BlueZDbusClient.cpp
```

Current initialization does approximately:

```cpp
connection_ = QDBusConnection::systemBus();
systemBusConnected_ = connection_.isConnected();
emit systemBusStateChanged(systemBusConnected_);
```

Then it creates the BlueZ `QDBusServiceWatcher` only if the bus is connected.

This mostly captures startup state.

The production implementation must also handle runtime loss/recovery of the system D-Bus connection.

Tests that manually call:

```cpp
RecoveryManager::notifySystemBusConnected(false)
```

do not prove production detection.

---

## 7.2 Required production behavior

A robust runtime contract is required:

### On system bus loss

The BlueZ/D-Bus layer must:

- detect loss;
- emit bus-disconnected state exactly once per transition;
- mark BlueZ unavailable;
- prevent new D-Bus method calls from being treated as usable;
- detach/clear or invalidate signal subscriptions as necessary;
- stop assuming the existing BlueZ service watcher remains valid;
- ensure Bluetooth reconnect actions do not keep hammering an unavailable bus;
- preserve high-level user/session intent outside the transient D-Bus object graph.

### On bus return

It must:

- obtain/revalidate a usable system bus connection;
- recreate any watcher/subscription objects that became invalid;
- re-subscribe to ObjectManager / Properties signals;
- re-check BlueZ service registration;
- request exactly one authoritative fresh snapshot when appropriate;
- emit bus-connected state exactly once per transition;
- allow normal device discovery/reconnect behavior to resume;
- avoid duplicate signal subscriptions;
- avoid stale callback corruption.

---

## 7.3 Choose a defensible Qt D-Bus monitoring mechanism

Research the actual Qt 6 APIs already available in the project's target environment.

Do not invent signals that `QDBusConnection` does not provide.

Potential approaches may involve:

- monitoring the bus service / local disconnect behavior;
- explicit health checks;
- reconstructing `QDBusConnection::systemBus()` state;
- a small retry/health timer;
- watcher lifecycle recreation;
- a dedicated wrapper around system bus availability.

The implementation must be:

- deterministic;
- bounded;
- low-frequency;
- non-busy-looping;
- testable behind a seam.

If a timer is needed, keep it modest and only active while disconnected.

---

## 7.4 Separate bus recovery from BlueZ service recovery

Treat these as different states:

```text
System bus connected?
BlueZ service registered?
Bluetooth adapter present?
```

Examples:

```text
bus down
    → BlueZ cannot be trusted regardless of previous registration

bus up + BlueZ absent
    → bus healthy, BlueZ waiting

bus up + BlueZ present + adapter absent
    → BlueZ healthy, adapter degraded
```

Recovery diagnostics must reflect this distinction.

---

## 7.5 Test seam

Do not rely solely on stopping the host's real system D-Bus inside automated tests.

Create a hardware-independent seam around the bus state/connection acquisition if needed.

Add tests that prove:

```text
connected → disconnected
disconnected → connected
reconnect recreates subscriptions once
BlueZ service check occurs after bus return
one snapshot follows successful recovery
duplicate disconnect notifications are idempotent
duplicate reconnect notifications are idempotent
shutdown while bus recovery is pending is safe
stale callbacks from pre-reconnect generation are ignored
```

If the Qt D-Bus layer is difficult to fake directly, isolate the state machine from the raw connection object.

---

# 8. REMEDIATION WORKSTREAM D — PIPEWIRE RETRY ACCOUNTING AND SUCCESS BOUNDARY

## 8.1 Inspect current behavior

Inspect:

```text
src/audio/PipeWireManager.cpp
src/audio/PipeWireConnection.cpp
tests/unit/audio/tst_PipeWireManager.cpp
```

Identify where `reconnectAttempt_` is reset.

The current concern is that an attempt may be treated as successful at transport-level `Connected`, before the PipeWire graph reaches initial synchronization.

That can create:

```text
failure
→ attempt 1
→ transport Connected
→ counter reset
→ graph never becomes ready
→ failure
→ attempt 1 again
→ ...
```

which weakens bounded retry semantics.

---

## 8.2 Define "successful reconnect"

The Phase 8 recovery contract should consider PipeWire recovery successful only after the application has a usable graph.

A reasonable boundary is:

```text
PipeWire connection alive
AND
initial graph synchronization complete
```

or the equivalent existing state:

```text
Connected + InitialSyncDone
```

Use the actual event/state names in this codebase.

Do not reset retry history merely because the socket/core briefly connected if graph initialization then fails.

---

## 8.3 Required changes

Ensure:

- attempt count increases monotonically across unsuccessful reconnect cycles;
- transient `Connected` does not reset attempts prematurely;
- successful graph-ready state resets the counter;
- maximum attempts really means maximum attempts;
- exhaustion fires exactly once per exhaustion episode;
- retry timer does not run after shutdown;
- disabling auto-recovery cancels pending retry;
- explicit manual retry after exhaustion has documented behavior.

---

## 8.4 Manual recovery after exhaustion

Define a clear API contract.

Recommended:

- automatic retries stop at exhaustion;
- status becomes `Exhausted`;
- an explicit user/manual reconnect may reset the retry episode and try again;
- successful recovery clears exhausted state;
- a new independent service failure after a healthy period starts a fresh retry episode.

Add tests.

---

## 8.5 Required PipeWire tests

Add or extend tests to cover:

```text
reconnectAttemptNotResetAtTransportConnected
reconnectAttemptResetsOnlyAfterGraphReady
boundedRetryExhaustsAfterConfiguredMaximum
reconnectExhaustedEmittedExactlyOncePerEpisode
manualReconnectAfterExhaustionStartsFreshEpisode
healthyRecoveryStartsFreshFutureEpisode
disableAutoReconnectCancelsScheduledAttempt
shutdownCancelsScheduledReconnect
staleGenerationEventsDoNotResurrectOldGraph
graphNeverReadyCannotRetryForever
```

Tests must use deterministic fake events and avoid wall-clock sleeps where possible.

---

# 9. REMEDIATION WORKSTREAM E — REMOVE DUPLICATE PIPEWIRE RETRY OWNERSHIP

## 9.1 Current architecture smell

Inspect:

```text
src/recovery/RecoveryManager.cpp
include/auralis/recovery/RecoveryManager.h
```

The current manager contains items such as:

```text
pipeWireRetry_
pipeWireRetryTimer_
beginPipeWireRecovery(...)
```

while actual production error handling comments indicate:

```text
PipeWireManager owns bounded reconnect timing
```

and `beginPipeWireRecovery()` is not the active production path.

This creates two conceptual owners.

---

## 9.2 Required decision

Prefer:

```text
PipeWireManager = retry owner
RecoveryManager = orchestration/status owner
```

unless a careful architecture review demonstrates a better existing pattern.

If retaining `PipeWireManager` ownership:

remove or simplify dead/dormant retry scheduling from `RecoveryManager`.

At minimum:

- remove unused timer;
- remove unused policy state;
- remove unreachable retry method;
- remove misleading public configuration if no longer needed;
- update tests;
- update comments.

Do not keep dead code "for future use."

---

## 9.3 RecoveryManager should observe

Add explicit RecoveryManager APIs/signals/slots as needed to observe:

```text
PipeWire reconnect attempt started
PipeWire reconnect exhausted
PipeWire connected
PipeWire graph ready
PipeWire error
```

It should then update central status accurately without performing a second retry schedule.

---

# 10. REMEDIATION WORKSTREAM F — CENTRAL RECOVERY STATUS MUST MATCH REALITY

## 10.1 Current problem

`RecoveryStatus` exposes a PipeWire attempt count, but production wiring does not fully propagate:

```cpp
PipeWireManager::reconnectAttemptStarted(int)
```

into central recovery state.

Similarly, PipeWire exhaustion may emit a user notification but not place central status into `RecoveryState::Exhausted`.

This can produce misleading UI such as:

```text
Reconnecting audio service (attempt 0)
```

during a real retry.

---

## 10.2 Required status invariants

At all times:

```text
RecoveryStatus.pipeWireAttempts
```

must equal the authoritative retry owner's current retry attempt for the current episode.

When PipeWire retry exhausts:

```text
status.pipeWire = Exhausted
status.overall = Exhausted or an accurately defined degraded/exhausted aggregate
last error/cause populated
```

When a manual or new recovery episode begins:

- attempt count transitions according to the retry policy;
- exhausted state is cleared only when semantically appropriate.

On successful graph-ready recovery:

- PipeWire state becomes healthy;
- attempt count returns to zero at the correct boundary;
- overall state becomes healthy only if other dependencies are healthy.

---

## 10.3 Add explicit methods if useful

For example, consider state notifications conceptually equivalent to:

```text
notifyPipeWireReconnectAttempt(int attempt)
notifyPipeWireReconnectExhausted(QString reason)
```

Names may differ.

Keep the public interface small and meaningful.

---

## 10.4 Required status tests

Verify:

```text
attempt 1 visible centrally
attempt N visible centrally
exhaustion visible centrally
userFacingStatus mentions correct attempt
userFacingStatus never displays impossible attempt 0 while actively retrying
successful graph sync resets central attempt count
BlueZ degradation + PipeWire exhaustion aggregate correctly
```

---

# 11. REMEDIATION WORKSTREAM G — ELIMINATE BLUEZ SNAPSHOT STORMS

## 11.1 Current duplicate paths

Audit all callers of:

```cpp
requestSnapshot()
BluetoothManager::refresh()
```

The current service-return chain may involve:

```text
BlueZDbusClient::onBlueZRegistered()
    → requestSnapshot()

BlueZDbusClient::setBlueZAvailable(true)
    → signal
    → BluetoothManager::handleBlueZAvailable(true)
        → requestSnapshot()

RecoveryManager observes availability
    → requestBlueZRefresh
    → BluetoothManager::refresh()
        → requestSnapshot()
```

This can create redundant fresh-snapshot calls.

---

## 11.2 Pick one authoritative snapshot owner per event

A preferred design is:

- low-level client owns D-Bus service registration/subscription correctness;
- Bluetooth manager owns model population request;
- recovery manager asks for a high-level refresh only when cross-service orchestration requires it.

But the exact owner may differ.

What matters:

```text
one service-return transition
→ one required snapshot
```

unless a failed snapshot requires a documented retry.

---

## 11.3 Snapshot in-flight protection

Consider whether the client needs:

```text
snapshotInFlight
pendingSnapshotRefresh
generation
```

to coalesce concurrent refresh requests.

Do not drop a genuinely required later refresh, but do not issue parallel identical `GetManagedObjects` calls unnecessarily.

A robust pattern can be:

```text
if no snapshot in flight:
    issue request
else:
    mark one pending refresh

when request completes:
    if pending refresh:
        issue one more request
```

Only add this if current lifecycle warrants it.

---

## 11.4 Required tests

Add counters to fake BlueZ client tests and prove:

```text
BlueZ registration causes exactly one effective snapshot
bus reconnect + BlueZ registration coalesces correctly
RecoveryManager coalescing does not create duplicate snapshots
repeated identical availability=true events are idempotent
failed snapshot permits a later retry
snapshot completion after shutdown is harmless
```

---

# 12. REMEDIATION WORKSTREAM H — SUSPEND/RESUME HARDENING BEYOND THE EARLY-RETURN BUG

Audit:

```text
src/recovery/SystemPowerMonitor.cpp
RecoveryManager suspend generation logic
Bluetooth reconnect timers
PipeWire timers
session reconciliation timers
```

Required properties:

- no blocking work in `PrepareForSleep(true)`;
- timers are stopped/paused promptly;
- stale queued work from before suspend is fenced;
- resume creates a new epoch;
- repeated `PrepareForSleep(true)` is idempotent;
- repeated `PrepareForSleep(false)` is idempotent;
- shutdown while suspended is safe;
- initialization without logind/system bus degrades gracefully;
- if logind subscription is unavailable at startup but becomes available later, the architecture should have a reasonable re-subscription strategy or explicitly document the limitation.

If you implement retry for logind subscription:

- make it bounded/low frequency;
- do not spam logs;
- stop retrying on shutdown;
- add tests.

---

# 13. REMEDIATION WORKSTREAM I — PHASE 8 CROSS-LAYER INTEGRATION TESTS

The existing `tst_ServiceRecoveryIntegration.cpp` is too narrow if it primarily exercises `RecoveryManager` with lambdas.

Phase 8 requires tests through real production managers with fake external boundaries.

Do not delete useful existing tests; expand coverage.

---

## 13.1 Create a reusable Phase 8 integration harness if helpful

A good harness should instantiate as many real production components as practical:

```text
ConfigurationManager
RecoveryManager
BluetoothManager
fake BlueZ boundary/client
PipeWireManager
fake PipeWire connection/backend
endpoint/object store
SessionManager
routing/session components needed for reconciliation
```

External services may be faked.

Internal state machines should be real.

---

## 13.2 Mandatory hardware-independent scenarios

Implement deterministic scenarios covering at least:

### Scenario 1 — BlueZ daemon bounce

```text
healthy
→ BlueZ unavailable
→ reconnect/device work paused
→ BlueZ available
→ exactly one fresh snapshot
→ registry repopulated
→ recovery reconcile
→ healthy
```

Assert no duplicate reconnect storm.

---

### Scenario 2 — System bus loss and return

```text
healthy
→ bus down
→ BlueZ invalidated
→ recovery degraded/waiting
→ bus up
→ watchers/signals restored
→ BlueZ checked
→ snapshot
→ models recover
```

---

### Scenario 3 — PipeWire bounce with new graph IDs

Simulate:

```text
stable endpoint identity A mapped to graph node 42
PipeWire restart
node 42 disappears
new graph
same physical/logical endpoint appears as node 108
```

Assert:

- stale graph IDs are not reused;
- endpoint is re-resolved by stable identity;
- prior route intent is reconciled;
- route recreated exactly once;
- old-generation events cannot overwrite new state.

---

### Scenario 4 — PipeWire repeatedly connects but never reaches graph ready

Assert retry count reaches exhaustion and stops.

---

### Scenario 5 — Combined BlueZ + PipeWire bounce

Vary event ordering:

```text
BlueZ down then PipeWire down
PipeWire down then BlueZ down
BlueZ up first
PipeWire graph ready first
both return nearly together
```

Assert exactly one coalesced session reconciliation after dependencies are actually usable.

---

### Scenario 6 — Recovery disabled

Trigger failures with:

```text
autoRecoverServices=false
```

Assert:

- automatic retries do not occur;
- state remains truthful;
- manual operations still work.

---

### Scenario 7 — Suspend/resume restore enabled

Assert:

- pause on sleep;
- stale work fenced;
- resume;
- subsystem machinery resumes;
- one recovery cycle;
- one session reconcile.

---

### Scenario 8 — Suspend/resume restore disabled

Assert:

- subsystem machinery is resumed;
- no proactive restoration;
- user can later manually refresh/recover.

---

### Scenario 9 — Shutdown in every recovery stage

At minimum:

```text
shutdown while retry timer pending
shutdown while snapshot in flight
shutdown while coalesce timer pending
shutdown after bus disconnected
shutdown while suspended
shutdown during PipeWire reconnect
```

No use-after-free, crash, resurrection, or post-shutdown state mutation.

---

### Scenario 10 — Session member disappears while another survives

For an active multi-device session:

```text
A + B active
B disappears
A survives
```

Assert behavior respects the existing session recovery policy and does not unnecessarily destroy surviving valid state.

---

# 14. REMEDIATION WORKSTREAM J — STRESS / CHURN / SOAK TESTS

Phase 8 is specifically a reliability phase. Add actual repeat-loop tests.

Avoid making normal CI excessively slow, but provide a useful deterministic default and optional extended mode.

---

## 14.1 BlueZ churn stress

Run at least hundreds of fake cycles, e.g.:

```text
BlueZ unavailable
BlueZ available
snapshot
device model reconstruction
```

Assert after each or periodic batch:

- no duplicate devices;
- no duplicate subscriptions;
- no runaway timers;
- no monotonically growing pending state;
- reconnect policy remains sane.

---

## 14.2 PipeWire graph churn stress

Repeatedly:

```text
graph ready
error
reconnect
new generation
new node IDs
initial sync
```

Assert:

- object store is cleared correctly;
- endpoint model does not duplicate stale endpoints;
- generation monotonically advances;
- routes reference only current graph objects;
- attempt counter resets only on real success.

---

## 14.3 Suspend/resume stress

Run many injected cycles:

```text
sleep
resume
```

with both preference modes.

Assert:

- pause/resume counts remain balanced;
- no queued reconciliation explosion;
- suspend epoch matches expected count;
- generation increases;
- no timer remains unexpectedly active.

---

## 14.4 Persistence loop

Repeatedly:

```text
save session/config
reload
modify
save again
```

Assert:

- atomic writes;
- no malformed state;
- no accidental loss of stable identities;
- no unbounded file growth;
- corrupt/truncated input handled gracefully if existing contract requires it.

---

## 14.5 Logger rotation stress

Generate enough controlled log output to force multiple rotations.

Verify:

- active log remains writable;
- retention limit honored;
- old files removed in correct order;
- failed rotation does not crash application;
- retention zero/one/boundary cases documented.

---

## 14.6 Optional long-running target

If appropriate, provide an opt-in CTest label or executable for an extended stress run.

Examples:

```text
AURALIS_RUN_STRESS=1
AURALIS_STRESS_ITERATIONS=10000
```

Do not force a 30-minute soak into ordinary developer CTest unless project policy wants that.

Document how to run it.

---

# 15. REMEDIATION WORKSTREAM K — SANITIZERS

The previous Phase 8 audit explicitly states AddressSanitizer was not run.

For software closure, add and/or run a sanitizer configuration if supported.

At minimum target:

- AddressSanitizer;
- UndefinedBehaviorSanitizer where compatible.

Example concept:

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

Prefer project-level CMake options if they already exist or if adding clean sanitizer options is appropriate.

Do not blindly hardcode flags in production release builds.

Record:

- exact command;
- compiler;
- number of tests;
- any tests skipped;
- sanitizer findings.

If a Qt/external-library sanitizer false positive exists, investigate before suppressing.

Any suppression must be narrow and documented.

---

# 16. REMEDIATION WORKSTREAM L — PACKAGING HARDENING

Inspect:

```text
apps/desktop/CMakeLists.txt
cmake/AuralisPackaging.cmake
data/io.github.auralis.Auralis.desktop
data/io.github.auralis.Auralis.metainfo.xml
LICENSE
```

---

## 16.1 Application icon

Current desktop metadata references:

```ini
Icon=io.github.auralis.Auralis
```

Ensure a matching icon is actually present and installed.

Recommended freedesktop path:

```text
/usr/share/icons/hicolor/<size>/apps/io.github.auralis.Auralis.png
```

or SVG/scalable equivalent.

Requirements:

- use a real project-owned icon;
- install it via CMake;
- desktop entry name must match installed icon name;
- package content test must assert icon presence.

Do not generate or copy an unrelated copyrighted icon.

If the repository has no approved icon asset, do not fabricate brand identity silently. In that case either:
- add a clearly project-owned minimal asset only if authorized by repository context, or
- change packaging metadata in a defensible way and flag the owner decision.

---

## 16.2 License contradiction

Current metadata may say:

```xml
<project_license>MIT</project_license>
```

while root `LICENSE` states licensing terms have not been selected and software should not be distributed until the owner selects a license.

This contradiction must not remain.

Do **not** unilaterally choose a legal license for the project.

Correct behavior:

1. inspect current repository ownership/documentation for an authoritative license;
2. if no license has actually been selected:
   - do not falsely claim MIT;
   - adjust AppStream metadata to avoid false licensing claims in a schema-valid way;
   - clearly flag license selection as an owner decision blocking public distribution;
3. do not rewrite the root license to MIT without explicit project-owner authorization.

If AppStream requires a valid project license field, determine a truthful SPDX/metadata approach or document that public distribution packaging cannot be considered release-ready until the project owner selects a license.

This is a **release governance blocker**, not merely a cosmetic issue.

---

## 16.3 Install LICENSE where appropriate

Once licensing is truthful, package legal/project documentation under a standard location such as:

```text
/usr/share/doc/auralis/
```

Do not package misleading terms.

---

## 16.4 Maintainer/contact metadata

Current CPack may use:

```text
auralis@localhost
```

Do not invent a real person's email address.

If the repository contains an authoritative project contact, use it.

Otherwise:

- use a neutral documented project contact only if one exists;
- or leave a clearly documented placeholder and mark package release metadata pending owner input.

A locally buildable `.deb` can still be tested, but do not call it publication-ready with fake metadata.

---

## 16.5 Packaging tests

After building the `.deb`, run:

```bash
cpack --config build/CPackConfig.cmake
```

or the repository's correct equivalent.

Then inspect:

```bash
dpkg-deb -I <package>.deb
dpkg-deb -c <package>.deb
```

Assert presence of:

- application binary;
- `.desktop`;
- AppStream metainfo;
- icon if declared;
- project documentation/license as appropriate.

---

## 16.6 Clean-prefix launch

Extract the package into a temporary root:

```bash
rm -rf /tmp/auralis-deb-root
mkdir -p /tmp/auralis-deb-root
dpkg-deb -x <package>.deb /tmp/auralis-deb-root
```

Launch from extracted prefix without relying on build-tree resources.

For headless smoke testing, use an appropriate Qt platform such as `offscreen` only if needed.

Verify the executable does not accidentally load project resources from the source/build directory.

---

# 17. REMEDIATION WORKSTREAM M — DOCUMENTATION TRUTHFULNESS

Update:

```text
docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
README.md
```

only after implementation/testing.

The docs must distinguish:

```text
implemented
tested hardware-independently
tested with actual host services
tested with actual Bluetooth hardware
not run
skipped
blocked
```

Never use one word "PASS" to conflate those categories.

---

# 18. REQUIRED TEST INVENTORY AFTER REMEDIATION

The exact file layout may evolve, but final coverage should include tests equivalent to the following.

## RecoveryManager unit tests

```text
initializesFromHostState
blueZLossTransitionsToRecovering
blueZReturnSchedulesOneReconcile
systemBusLossPausesBluetoothWork
systemBusReturnRestoresAppropriateWork
sleepPausesReconnect
resumeRestoreEnabledRecovers
resumeRestoreDisabledResumesInfrastructureWithoutRestoringSession
repeatedSuspendResumeIsIdempotent
shutdownCancelsPendingReconcile
pipeWireAttemptStatusTracksOwner
pipeWireExhaustionTracksOwner
coalescedMultiFailureReconcilesOnce
```

## PipeWireManager unit tests

```text
autoReconnectCanBeDisabled
disablingAutoReconnectStopsPendingTimer
attemptCountDoesNotResetAtTransportConnected
attemptCountResetsAtGraphReady
boundedRetryExhaustion
manualRetryAfterExhaustion
staleGenerationIgnored
shutdownDuringReconnectSafe
```

## BlueZ/Bluetooth unit tests

```text
serviceRegistrationRequestsOneSnapshot
duplicateAvailabilitySignalIsIdempotent
systemBusDisconnectInvalidatesBlueZ
systemBusReconnectRebuildsSubscriptionState
snapshotCoalescing
shutdownWithSnapshotPendingSafe
```

## Integration tests

```text
blueZBounceRecoversRealManagers
systemBusBounceRecoversBlueZLayer
pipeWireBounceRebindsStableEndpointIdentity
combinedServiceBounceCoalescesSessionRestore
autoRecoveryDisabledPreventsAutomaticServiceRecovery
suspendResumeWithRestore
suspendResumeWithoutRestore
shutdownDuringEachRecoveryStage
sessionMemberChurnPreservesValidPeer
```

## Stress tests

```text
blueZBounceHundredsOfCycles
pipeWireGraphBounceHundredsOfCycles
suspendResumeHundredsOfCycles
persistenceRepeatedSaveLoad
loggerRepeatedRotation
```

---

# 19. TEST DESIGN QUALITY REQUIREMENTS

Every new test must be deterministic.

Prefer:

- signal spies;
- fake clocks/timers if current architecture supports them;
- explicit fake service events;
- direct event-loop draining;
- bounded `QTRY_*` only when truly asynchronous.

Avoid:

```cpp
QTest::qWait(5000);
```

as a substitute for synchronization.

No random sleeps.

No tests that pass only on one machine because of timing.

For stress tests, make iteration counts deterministic and configurable.

---

# 20. EVENT/STATE MACHINE INVARIANTS TO ENFORCE

Implement tests around these invariants.

## 20.1 RecoveryManager

```text
shutdown is terminal for the current object lifetime
suspended state prevents active recovery work
resume cannot leave subsystem timers accidentally paused
overall Healthy requires all required dependencies healthy
Exhausted is not silently converted to Healthy without recovery
coalesced reconciliation executes at most once per coalescing window
```

## 20.2 Bluetooth

```text
no D-Bus calls treated as valid while bus is unavailable
one logical availability transition → one state transition
service return does not duplicate registry objects
device reconnect policy is not duplicated by RecoveryManager
```

## 20.3 PipeWire

```text
old generation events cannot mutate new graph
retry budget cannot reset before actual usable graph recovery
one retry timer at a time
exhaustion stops automatic reconnect
shutdown prevents reconnection
```

## 20.4 Session/routing

```text
stable identity, not ephemeral PipeWire ID, drives restoration
one recovery event does not create duplicate routes
explicitly stopped user intent is not resurrected
valid surviving session members are preserved where policy allows
```

---

# 21. FULL HARDWARE-INDEPENDENT VALIDATION SEQUENCE

After all corrections:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Then run targeted suites verbosely, adapting names to actual CTest registration:

```bash
ctest --test-dir build -R Recovery --output-on-failure -V
ctest --test-dir build -R PipeWire --output-on-failure -V
ctest --test-dir build -R BlueZ --output-on-failure -V
ctest --test-dir build -R Session --output-on-failure -V
ctest --test-dir build -R Logger --output-on-failure -V
```

If stress tests use labels:

```bash
ctest --test-dir build -L stress --output-on-failure
```

or use the environment gate defined by the repository.

Then run sanitizer build/test.

Then produce/package `.deb`.

Then inspect package content.

Then smoke-launch extracted package.

---

# 22. ACTUAL-MACHINE SERVICE VALIDATION

If the Cursor session is running on the actual Linux development laptop and permissions allow safe service restart, you may run **non-destructive service validation**, but never infer permission.

Do not suspend the host automatically unless explicitly intended by the user.

Do not disrupt unrelated user work without warning.

For service tests, preserve safety.

Potential service validation includes:

```bash
systemctl --user status pipewire.service pipewire-pulse.service wireplumber.service
```

and controlled user-service restarts.

For BlueZ, restarting the system daemon may require privilege and can disconnect devices. Only run if appropriate and permitted.

If not run, mark:

```text
PENDING ACTUAL-MACHINE VALIDATION
```

Do not mark PASS.

---

# 23. ACTUAL HARDWARE EXIT-GATE MATRIX TO PREPARE FOR

The remediation code must be ready for a later rigorous actual-machine test containing at least:

1. application cold start with BlueZ/PipeWire healthy;
2. application cold start with Bluetooth disabled;
3. BlueZ daemon restart during active app;
4. Bluetooth adapter power off/on;
5. paired device disappears and returns;
6. PipeWire restart during active session;
7. WirePlumber restart during active session;
8. endpoint IDs change after graph rebuild;
9. two-device session with one device temporarily removed;
10. two-device session with both devices lost and restored;
11. laptop suspend/resume with restore enabled;
12. laptop suspend/resume with restore disabled;
13. repeated service bounces;
14. package installation and launch;
15. long-running soak.

Do not fake results for this matrix.

---

# 24. REQUIRED FINAL AUDIT DOCUMENT

Create/update a final audit with a table like:

| Requirement | Before | Change | Automated Evidence | Live Evidence | Final |
|---|---|---|---|---|---|
| Suspend restore disabled | FAIL | fixed resume semantics | test name | N/A | PASS |
| Auto recovery preference | FAIL | wired to PipeWire owner | tests | N/A | PASS |
| Runtime system bus recovery | FAIL | monitoring/rebind state machine | tests | pending/actual | ... |
| PipeWire bounded retry | FAIL | graph-ready reset boundary | tests | ... | ... |
| Central retry diagnostics | FAIL | signal wiring | tests | N/A | PASS |
| BlueZ snapshot coalescing | FAIL | single owner/coalescing | tests | ... | PASS |
| Stress tests | MISSING | added | command/results | N/A | PASS |
| ASan/UBSan | NOT RUN | executed | command/results | N/A | PASS/FAIL |
| `.deb` icon | FAIL | installed | dpkg-deb content | N/A | PASS |
| License metadata | CONTRADICTORY | corrected truthfully | metadata validation | owner decision if needed | ... |

Include exact test names and command outputs.

---

# 25. EXIT GATE — SOFTWARE

You may declare:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
```

only if all of the following are true:

- baseline Phase 0–7 regressions remain green;
- suspend/resume early-return bug fixed;
- `autoRecoverServices` actually controls automatic PipeWire service recovery;
- runtime D-Bus disconnect/reconnect behavior has a real production implementation and deterministic test coverage;
- PipeWire retry budget cannot reset before usable graph recovery;
- there is one authoritative PipeWire retry owner;
- RecoveryManager accurately reports PipeWire attempts and exhaustion;
- BlueZ refresh/snapshot storm is removed/coalesced;
- cross-layer integration tests exist and pass;
- stress/churn tests exist and pass;
- sanitizer suite is run and clean, or any blocker is explicitly unresolved;
- package content/install rules are correct;
- package metadata is truthful;
- documentation matches evidence.

---

# 26. EXIT GATE — FULL PHASE 8

Do **not** declare full Phase 8 complete unless actual-machine/hardware requirements have also been executed.

Use one of the following final states.

### State A

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

This is acceptable if all software work is truly complete but live hardware validation has not been run.

### State B

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PASS
PHASE 8 OVERALL: COMPLETE
```

Use only with real evidence.

### State C

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 OVERALL: REMEDIATION REQUIRED
```

Use if any mandatory software defect/test remains.

---

# 27. SPECIFIC FILES TO INSPECT BEFORE EDITING

At minimum inspect these before making changes:

```text
src/recovery/RecoveryManager.cpp
include/auralis/recovery/RecoveryManager.h
src/recovery/RecoveryTypes.cpp
include/auralis/recovery/RecoveryTypes.h
src/recovery/SystemPowerMonitor.cpp
include/auralis/recovery/SystemPowerMonitor.h

src/audio/PipeWireManager.cpp
include/auralis/audio/PipeWireManager.h
src/audio/PipeWireConnection.cpp
src/audio/PipeWireObjectStore.cpp
src/audio/AudioRouter.cpp

src/bluetooth/BlueZDbusClient.cpp
include/auralis/bluetooth/BlueZDbusClient.h
src/bluetooth/BluetoothManager.cpp
src/bluetooth/ReconnectPolicy.cpp
src/bluetooth/DeviceLifecycleManager.cpp
src/bluetooth/DeviceRegistry.cpp

src/core/ApplicationCore.cpp
src/core/ConfigurationManager.cpp
src/core/Logger.cpp

apps/desktop/DesktopApplication.cpp
apps/desktop/CMakeLists.txt

tests/unit/recovery/tst_RecoveryManager.cpp
tests/unit/audio/tst_PipeWireManager.cpp
tests/unit/bluetooth/tst_BluetoothManager.cpp
tests/unit/bluetooth/FakeBlueZClient.h
tests/integration/tst_ServiceRecoveryIntegration.cpp
tests/integration/tst_SessionManagedReconnectIntegration.cpp
tests/CMakeLists.txt

cmake/AuralisPackaging.cmake
data/io.github.auralis.Auralis.desktop
data/io.github.auralis.Auralis.metainfo.xml
LICENSE

docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
README.md
```

Also use `rg` to find every signal/caller of:

```text
requestSnapshot
refresh
reconnectAttemptStarted
reconnectExhausted
setAutoReconnectEnabled
autoRecoverServices
notifyPipeWireError
notifyPipeWireConnected
notifyBlueZAvailable
notifySystemBusConnected
PrepareForSleep
```

---

# 28. RECOMMENDED IMPLEMENTATION ORDER

Follow this sequence unless source dependencies justify a small deviation.

## Stage 0 — Baseline and architecture map

No code edits until current ownership is understood.

## Stage 1 — Suspend/resume bug

Small, isolated, easy to regression-test.

## Stage 2 — Recovery preference authority

Wire `autoRecoverServices` into actual service retry owner.

## Stage 3 — PipeWire retry semantics

Fix graph-ready success boundary and exhaustion behavior.

## Stage 4 — RecoveryManager cleanup/status wiring

Remove duplicate retry machinery and consume PipeWire attempt/exhaustion signals.

## Stage 5 — BlueZ snapshot ownership/coalescing

Remove duplicate refresh paths.

## Stage 6 — System D-Bus runtime recovery

Implement robust reattachment lifecycle.

## Stage 7 — Cross-layer integration tests

Validate real manager interactions with fake boundaries.

## Stage 8 — Stress tests

Add repeated churn loops.

## Stage 9 — Sanitizers

Run full hardware-independent matrix.

## Stage 10 — Packaging

Fix icon/license/contact/install metadata.

## Stage 11 — Full regression

Clean build, all CTest, package, extracted launch.

## Stage 12 — Audit

Only now update Phase 8 status.

---

# 29. CODE QUALITY REQUIREMENTS

All new code must follow existing project conventions.

Use:

- RAII;
- Qt object ownership correctly;
- parented `QObject`s where appropriate;
- `QPointer`/generation fencing when lifecycle requires it;
- explicit state transitions;
- existing logging categories;
- bounded timers;
- meaningful signal names;
- no raw unmanaged thread lifecycle;
- no silent catch-all failure swallowing.

Avoid:

- magic sleeps;
- busy polling;
- duplicate state flags representing the same truth;
- recovery loops with no cap;
- static mutable global recovery state;
- forcing user session state that conflicts with explicit stop intent;
- modifying unrelated files merely to increase apparent progress.

---

# 30. LOGGING REQUIREMENTS FOR RECOVERY

Ensure important transitions are logged with enough structure to debug actual-machine failures.

At minimum log:

```text
Recovery generation
Suspend epoch
System bus state transition
BlueZ service state transition
Adapter state transition
PipeWire retry attempt
PipeWire retry exhaustion
PipeWire graph-ready recovery
Session reconciliation start/result
Auto-recovery enabled/disabled transition
Snapshot request/coalescing decision
Shutdown cancellation of pending recovery
```

Do not spam logs for repeated identical state notifications.

Use consistent category/prefix conventions already in the project.

---

# 31. FAILURE INJECTION REQUIREMENTS

Add deterministic test-only injection seams where necessary.

Acceptable examples:

- fake bus availability source;
- fake BlueZ service availability;
- fake snapshot responses;
- fake PipeWire client events;
- injected PrepareForSleep;
- fake session members;
- fake endpoint graph IDs.

Do not put testing-only conditionals throughout production logic.

Prefer dependency injection at boundaries.

---

# 32. RECOVERY ORDERING CONTRACT

When multiple services fail, restoration must respect dependencies.

Conceptually:

```text
System bus
    ↓
BlueZ service
    ↓
Bluetooth adapter/device state

PipeWire transport
    ↓
PipeWire graph ready
    ↓
Endpoint resolution
    ↓
Routing/session reconciliation
```

Do not reconcile routes against an incomplete graph.

Do not attempt Bluetooth operations against a dead system bus.

Do not mark overall state Healthy merely because one service has returned.

---

# 33. COALESCING CONTRACT

A burst such as:

```text
bus returned
BlueZ returned
snapshot received
PipeWire connected
PipeWire graph ready
adapter returned
```

must not result in six full session reconstructions.

Use the existing coalescing mechanism or improve it.

Target behavior:

```text
many dependency events in a short recovery burst
→ one final reconciliation after usable state
```

without indefinitely postponing recovery.

Add tests with different event orders.

---

# 34. USER INTENT PRESERVATION

Reliability recovery must restore **system capability and previously valid session intent**, not override explicit user decisions.

Audit interactions with:

- per-device auto-reconnect;
- explicit user disconnect/stop;
- session recovery policy;
- route removal;
- selected device state.

Examples:

```text
User explicitly disabled auto-reconnect for device B
→ service bounce
→ B must not suddenly be forced to reconnect by RecoveryManager
```

```text
User stopped an active session
→ PipeWire bounce
→ session must not be resurrected merely because recovery ran
```

Use existing Phase 6/session contracts.

---

# 35. PERSISTENCE HARDENING CHECK

Do not rewrite persistence if already correct.

Validate:

- atomic session write remains `QSaveFile` or equivalent;
- configuration recovery flags persist;
- corrupted settings/session state does not crash startup;
- stable identities survive restart;
- recovery transient state such as "currently retry attempt 3" is **not** persisted unless explicitly required;
- no stale ephemeral PipeWire object IDs are persisted as durable identity.

Add tests only where coverage is currently absent.

---

# 36. PACKAGE RELEASE GOVERNANCE

Distinguish:

```text
locally buildable package
```

from:

```text
publicly distributable release package
```

A `.deb` that builds is a software engineering success.

But if project licensing is not selected, public distribution may still be blocked.

The audit must say so clearly rather than changing legal metadata without authorization.

---

# 37. WHAT NOT TO DO

Do not:

- start Phase 9 work;
- redesign the UI unrelated to recovery;
- migrate frameworks;
- replace Qt D-Bus with another stack without necessity;
- replace PipeWire ownership;
- add cloud/network dependencies;
- add telemetry;
- rewrite all tests;
- remove hardware opt-in gates;
- mark skipped tests as pass;
- select a legal license on behalf of the owner;
- suppress sanitizer findings without investigation;
- use `sudo` automatically for destructive system changes;
- suspend the host automatically during generic CI;
- restart BlueZ on the user's machine without making the action explicit in the validation record.

---

# 38. FINAL COMMAND/EVIDENCE BLOCK TO PRODUCE

At the end of your work, produce a concise but complete evidence section containing commands and results for:

```text
git revision
compiler/CMake/Qt versions
clean configure
clean build
CTest count
CTest pass/fail/skip
targeted recovery tests
stress tests
ASan/UBSan build
ASan/UBSan CTest
CPack DEB
dpkg-deb metadata
dpkg-deb file listing
extracted-prefix launch
live tests actually run
live tests not run
hardware tests actually run
hardware tests pending
```

Do not omit failures.

---

# 39. REQUIRED FINAL RESPONSE FORMAT FROM CURSOR

When implementation is complete, respond with:

## A. Files changed

Grouped by:

```text
Recovery
Bluetooth/D-Bus
PipeWire
Tests
Packaging
Documentation
```

## B. Defects fixed

For each original defect:

```text
Problem
Root cause
Fix
Test proving fix
```

## C. Test results

Include exact totals.

## D. Sanitizer results

Include exact configuration and findings.

## E. Packaging results

Include package path, content verification, launch result.

## F. Remaining live/hardware items

Explicitly list anything not actually run.

## G. Exit gate

Use only one truthful state from Section 26.

---

# 40. MINIMUM ACCEPTANCE CHECKLIST

Before considering this prompt complete, every box below must be resolved.

## Suspend / resume

- [ ] `restoreOnResume=false` no longer leaves Bluetooth reconnect paused.
- [ ] restore-disabled resume does not proactively resurrect the previous session.
- [ ] repeated suspend/resume is idempotent.
- [ ] stale pre-suspend work is fenced.

## Recovery policy

- [ ] `autoRecoverServices` controls PipeWire automatic reconnect.
- [ ] disabling preference cancels pending service retry.
- [ ] manual recovery still works.
- [ ] BlueZ/service recovery obeys documented policy.

## D-Bus / BlueZ

- [ ] runtime system-bus loss detected in production layer.
- [ ] bus return rebuilds required watchers/subscriptions.
- [ ] BlueZ state rechecked after bus return.
- [ ] exactly one effective snapshot refresh per recovery transition.
- [ ] no duplicate subscriptions.
- [ ] shutdown during bus recovery safe.

## PipeWire

- [ ] one authoritative retry owner.
- [ ] retry counter resets only after usable graph recovery.
- [ ] retry budget truly bounded.
- [ ] exhaustion is observable centrally.
- [ ] manual retry after exhaustion tested.
- [ ] stale generation ignored.

## Recovery diagnostics

- [ ] attempt number is accurate.
- [ ] exhausted state is accurate.
- [ ] overall state aggregates dependencies correctly.
- [ ] user-facing text is truthful.

## Integration

- [ ] BlueZ bounce cross-layer test.
- [ ] system-bus bounce cross-layer test.
- [ ] PipeWire bounce/rebind cross-layer test.
- [ ] combined failure-order test.
- [ ] recovery-disabled test.
- [ ] suspend/resume enabled test.
- [ ] suspend/resume disabled test.
- [ ] shutdown-during-recovery tests.
- [ ] session-member churn test.

## Stress

- [ ] repeated BlueZ churn.
- [ ] repeated PipeWire churn.
- [ ] repeated suspend/resume.
- [ ] persistence repeated save/load.
- [ ] logger repeated rotation.
- [ ] no timer/listener/state explosion found.

## Sanitizers

- [ ] ASan run.
- [ ] UBSan run where supported.
- [ ] no unresolved sanitizer defect.

## Packaging

- [ ] `.deb` builds.
- [ ] binary installed.
- [ ] desktop file installed.
- [ ] AppStream installed.
- [ ] declared icon is installed or metadata corrected.
- [ ] license metadata is truthful.
- [ ] no unauthorized license selection.
- [ ] documentation/license install decision handled.
- [ ] maintainer/contact metadata not falsely represented.
- [ ] extracted package launches without build tree.

## Regression

- [ ] Phase 0–7 automated tests remain green.
- [ ] Phase 8 automated tests green.
- [ ] live skip is not counted as hardware pass.

---

# 41. FINAL PRINCIPLE

The objective is not to make the Phase 8 audit look green.

The objective is to make Auralis behave predictably when Linux desktop reality becomes hostile:

```text
Bluetooth daemon disappears.
System D-Bus is interrupted.
PipeWire restarts.
WirePlumber rebuilds the graph.
Endpoint IDs change.
A device leaves and returns.
The laptop suspends.
The user disables auto-recovery.
Recovery retries exhaust.
The application shuts down mid-recovery.
```

After this remediation, each of those cases must have:

1. one clear owner;
2. bounded behavior;
3. accurate state;
4. preserved user intent;
5. deterministic automated coverage where hardware is not essential;
6. explicit live/hardware evidence where hardware is essential.

Only then is the Phase 8 software exit gate defensible.

---

# 42. START NOW

Begin with repository reconnaissance and the baseline test run.

Then implement the remediation in the staged order above.

Do not stop after the first green CTest run.

Complete the code, targeted tests, stress tests, sanitizer run, package verification, and truthful audit update.

If actual hardware/live checks cannot be run in this Cursor session, finish all software work and end with:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

only if every mandatory software-level requirement in this prompt is genuinely satisfied.

If any mandatory software requirement remains unresolved, state:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 OVERALL: REMEDIATION REQUIRED
```

and list the exact blocker.
