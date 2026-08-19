# Auralis — Phase 6 Final Recovery-Boundary Closure Prompt
## Last Surgical Correction Pass for Session ↔ Bluetooth Recovery Ownership, Terminal Failure Propagation, Operation-Contention Recovery, and Timer Lifetime

**Project:** Auralis  
**Target:** Current repository after the latest Phase 6 correction and rigorous-audit pass  
**Phase:** Phase 6 — Multi-Device Session Engine  
**Current status entering this task:** Phase 6 is very close to complete, but a small set of production-path recovery-boundary defects remain  
**Primary stack:** Modern C++, Qt 6, BlueZ D-Bus, PipeWire, CMake/Ninja  
**Purpose:** Fix the final recovery integration defects without rewriting the working Phase 6 architecture, add focused regression tests that exercise the production event path, and leave the repository ready for a final independent rigorous audit.

---

# 1. Role and Operating Mode

You are acting as the senior C++/Qt engineer performing the **last Phase 6 correctness pass**.

The repository already contains a mature implementation of:

- `SessionManager`
- `SessionStateMachine`
- `RoutingCoordinator`
- `VolumeCoordinator`
- session persistence
- `BluetoothManager`
- `DeviceLifecycleManager`
- `ReconnectPolicy`
- managed reconnect APIs
- reconnect exhaustion propagation
- recovery policy handling
- source recovery
- persistence error handling
- unit tests
- integration tests
- Phase 6 validation documentation

Do **not** rebuild Phase 6.

Do **not** introduce new parallel managers.

Do **not** move responsibilities away from their current architectural owners unless required to repair a correctness boundary.

The task is now:

```text
preserve architecture
    +
fix final recovery ownership defects
    +
add production-path regression tests
    +
run full regression
    =
defensible Phase 6 completion
```

---

# 2. Known Final Defects

The latest source-level audit found four remaining issues.

Treat all four as mandatory until code inspection proves one is already fixed.

They are:

1. **Session recovery policy does not fully override Phase 3 device-level auto reconnect.**
2. **Terminal/non-retryable reconnect failures are not propagated to SessionManager.**
3. **A due reconnect attempt can be silently abandoned when the lower lifecycle layer cannot begin the reconnect operation.**
4. **Stopped reconnect timers accumulate until `ReconnectPolicy` destruction.**

Additionally, add regression coverage for each production path and re-run the entire Phase 0–6 suite.

---

# 3. Non-Negotiable Architecture Rule

The final ownership model should be explicit.

Preferred architecture:

```text
SessionManager
    owns:
        session recovery intent
        recovery policy
        member desired participation
        whether Bluetooth reconnect is allowed for this session/member
        route restoration policy
        session state

BluetoothManager / DeviceLifecycleManager / ReconnectPolicy
    owns:
        actual Bluetooth reconnect execution
        retry timing
        retry backoff
        retry attempt counting
        connection operation serialization
        terminal/retryable Bluetooth failure classification

RoutingCoordinator
    owns:
        desired route set
        route creation/removal/reactivation
        policy-aware route restoration

VolumeCoordinator
    owns:
        current intended group/member volume
        mute restoration after route recovery
```

There must be no ambiguity about which layer owns Bluetooth retry timing.

---

# 4. Mandatory Baseline Audit

Before editing anything, inspect the exact repository.

Run:

```bash
git status --short
git branch --show-current
git log --oneline -n 25
```

Inspect recovery-related symbols:

```bash
grep -R \
  "autoReconnectEnabled\|RecoveryPolicy\|requestManagedReconnect\|managedReconnect\|scheduleReconnect\|completeReconnectAttempt\|cancelReconnect\|reconnectExhausted\|beginOperation\|DeviceOperation::Reconnecting" \
  -n include src tests docs 2>/dev/null
```

Inspect relevant files in full, especially:

```text
BluetoothManager.*
DeviceLifecycleManager.*
ReconnectPolicy.*
SessionManager.*
RoutingCoordinator.*
BlueZTypes.*
session integration tests
Bluetooth lifecycle/reconnect tests
```

Then run the baseline suite:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If needed:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record:

- test count;
- pass/fail/skip;
- existing reconnect tests;
- warnings;
- any environmental limitations.

---

# 5. FINAL DEFECT A — Session Recovery Policy Does Not Fully Override Device-Level Auto-Reconnect

## 5.1 Observed Problem

The Bluetooth/device domain has a persistent device-level flag similar to:

```cpp
bool autoReconnectEnabled = true;
```

The lower lifecycle layer may independently perform:

```cpp
if (!device.userDisconnectRequested
    && device.autoReconnectEnabled
    && reconnect_ != nullptr) {
    reconnect_->scheduleReconnect(devicePath);
}
```

This means the lower Phase 3 lifecycle layer may schedule Bluetooth reconnect after an unexpected disconnect **even when the active Phase 6 session policy is**:

```text
RecoveryPolicy::None
```

or:

```text
RecoveryPolicy::RestoreRoutesOnly
```

That makes Phase 6 session recovery policy non-authoritative.

## 5.2 Required Semantics

For a member participating in the active session:

### `RecoveryPolicy::None`

```text
Bluetooth reconnect initiated by Auralis session: NO
automatic route restoration: NO
healthy peers continue: YES
state after member loss: DEGRADED
```

### `RecoveryPolicy::RestoreRoutesOnly`

```text
Bluetooth reconnect initiated by Auralis session: NO
wait for device to reconnect externally: YES
wait for endpoint return: YES
automatic route restoration after endpoint return: YES
```

### `RecoveryPolicy::ReconnectAndRestore`

```text
Bluetooth managed reconnect: YES
retry/backoff owned by lower lifecycle layer
route restoration after endpoint return: YES
```

These semantics must hold **regardless of the device’s previous generic autoReconnectEnabled default**.

---

# 6. Design the Override Carefully

Do not permanently disable a user's generic Bluetooth auto-reconnect preference just because a session temporarily uses a restrictive recovery policy.

Prefer a session-scoped suppression/override mechanism.

Possible approaches:

## Option A — Managed Reconnect Authorization Callback/Policy

DeviceLifecycleManager asks:

```text
is automatic reconnect currently allowed for this device?
```

before scheduling.

SessionManager or an application-level recovery policy service can temporarily deny it.

## Option B — BluetoothManager Per-Device Reconnect Suppression

Add APIs such as:

```cpp
void setManagedReconnectSuppressed(
    const DeviceId& deviceId,
    bool suppressed);
```

or:

```cpp
ReconnectPermissionToken suppressManagedReconnect(...);
```

SessionManager applies suppression while the device belongs to an active session whose policy forbids reconnect.

## Option C — Session-Aware Reconnect Gate

Centralize in `BluetoothManager`:

```cpp
bool shouldAutoReconnect(const DeviceId&) const;
```

which evaluates:

- user disconnect requested;
- generic device auto reconnect preference;
- active session override.

Choose the least invasive design consistent with existing ownership.

---

# 7. Reconnect Suppression Lifecycle

If a session temporarily suppresses lower auto reconnect, maintain exact lifecycle.

Apply suppression when:

```text
session becomes STARTING/ACTIVE/DEGRADED/RECOVERING
AND
member belongs to session
AND
policy forbids Bluetooth reconnect
```

Remove/restore suppression when:

```text
session stops
session deleted
member removed
member disabled
active session switched
policy changes to ReconnectAndRestore
application shutdown
```

Do not leak suppression beyond the session’s lifetime.

---

# 8. Interaction With Device Preference

If `device.autoReconnectEnabled` is a user-level preference, preserve it.

Example:

```text
device autoReconnectEnabled = true

Session A:
    policy = None
    -> temporary session override blocks reconnect

Session A stops
    -> original device autoReconnectEnabled remains true
```

Likewise:

```text
device autoReconnectEnabled = false

Session B:
    policy = ReconnectAndRestore
```

Decide whether session policy may override a user-level disabled reconnect preference.

Recommended safety semantics:

```text
effectiveReconnectAllowed =
    device.autoReconnectEnabled
    AND sessionPolicyAllowsReconnect
```

unless existing product semantics explicitly define session policy as stronger than user preference.

Document the final rule.

---

# 9. Mandatory Production-Path Tests for Policy Authority

Use the real production chain as far as possible:

```text
FakeBlueZClient
    |
BluetoothManager
    |
DeviceLifecycleManager
    |
ReconnectPolicy
    |
DeviceRegistry
    |
SessionManager
```

Do not test only SessionManager with a fake registry.

## 9.1 Policy None Before Disconnect

Setup:

```text
device connected
device autoReconnectEnabled=true
session ACTIVE
policy=None
```

Emit:

```text
BlueZ Connected=false
```

Assert:

```text
ReconnectPolicy has no scheduled retry
attempt count unchanged
no reconnectDue
SessionManager does not request managed reconnect
session becomes DEGRADED
```

## 9.2 RestoreRoutesOnly Before Disconnect

Setup:

```text
session ACTIVE
policy=RestoreRoutesOnly
```

Disconnect device.

Assert:

```text
no lower Bluetooth reconnect schedule
```

Then externally simulate:

```text
device reconnects
endpoint returns
```

Assert:

```text
route restored
ACTIVE
```

## 9.3 ReconnectAndRestore

Setup:

```text
policy=ReconnectAndRestore
```

Disconnect.

Assert:

```text
one reconnect schedule
one authoritative attempt counter
```

---

# 10. FINAL DEFECT B — Terminal Reconnect Failure Is Not Propagated

## 10.1 Observed Problem

Retryable failures now reach reconnect exhaustion correctly.

However, a non-retryable/terminal error may follow:

```text
managed reconnect due
    |
BlueZ connect
    |
terminal error
    |
completeReconnectAttempt()
    |
cancelReconnect()
```

without notifying SessionManager.

Examples may include:

```text
AuthenticationFailed
NotAuthorized
NotSupported
Rejected
invalid device/service state
```

depending on current error mapping.

SessionManager may remain:

```text
managedReconnectRequested=true
recovering=true
state=RECOVERING
```

forever.

---

# 11. Required Generalized Managed-Reconnect Completion Event

Reconnect exhaustion is only one terminal completion.

Introduce or extend a generalized managed reconnect result event.

Conceptual:

```cpp
enum class ManagedReconnectTerminationReason {
    Exhausted,
    TerminalBluetoothError,
    OperationRejected,
    CancelledByPolicy
};
```

and/or:

```cpp
struct ManagedReconnectFailure {
    DeviceId deviceId;
    Error error;
    bool retryable;
    bool terminal;
    int attempts;
};
```

Possible signal:

```cpp
void managedReconnectTerminated(
    const DeviceId& deviceId,
    const ManagedReconnectFailure& failure);
```

Do not duplicate existing error types if repository already has structured Bluetooth errors.

---

# 12. Required Session Handling for Terminal Failure

When SessionManager receives a terminal managed reconnect failure:

```text
clear managedReconnectRequested
clear member recovering
record member/session error
emit sessionError
recompute state
```

State result:

## Healthy peer remains

```text
A healthy
B terminal reconnect failure

RECOVERING
    ->
DEGRADED
```

## No usable member remains

```text
all required members unavailable
no automatic recovery path

RECOVERING
    ->
FAILED
```

Do not remain indefinitely RECOVERING.

---

# 13. Mandatory Terminal Failure Tests

## 13.1 Two-Device Session

```text
A + B ACTIVE
B disconnects
ReconnectAndRestore
managed reconnect begins
BlueZ returns terminal non-retryable error
```

Assert:

```text
B recovering=false
managedReconnectRequested=false
member error set
sessionError emitted
A route remains active
session=DEGRADED
```

## 13.2 No Healthy Peer

Single-device session or all devices unavailable.

Terminal error.

Assert:

```text
session=FAILED
```

## 13.3 Explicit Retry After Terminal Failure

After terminal failure:

```text
retrySession()
```

must start a new operation generation and may attempt reconnect again if policy/user settings allow.

---

# 14. FINAL DEFECT C — beginOperation() Can Abandon a Due Reconnect

## 14.1 Observed Problem

Current lifecycle logic may contain:

```cpp
if (!beginOperation(
        devicePath,
        DeviceOperation::Reconnecting)) {
    reconnect_->completeReconnectAttempt(devicePath);
    return;
}
```

This creates a gap.

At the time the reconnect timer fires:

```text
ReconnectPolicy:
    inFlight=true
```

Then:

```text
beginOperation() fails
```

because another device operation is active.

After:

```text
completeReconnectAttempt()
```

the reconnect entry is no longer in-flight.

But if nothing reschedules it and no terminal signal is emitted, recovery stalls.

SessionManager may still believe recovery is active.

---

# 15. Define Contention Semantics

A transient failure to begin a reconnect operation is generally **not a terminal Bluetooth failure**.

Recommended behavior:

```text
reconnect due
    |
device busy with another operation
    |
do not consume terminal recovery state
    |
reschedule safely
```

Possible implementation:

```cpp
if (!beginOperation(...)) {
    reconnect_->completeReconnectAttempt(devicePath);
    reconnect_->rescheduleAfterContention(devicePath);
    return;
}
```

Use bounded behavior and avoid hot loops.

Alternative:

- emit a deferred/retry-later event;
- lifecycle scheduler retries after active operation completes.

Choose architecture consistent with existing operation serialization.

---

# 16. Do Not Double-Count Attempts on Contention

Clarify whether a reconnect timer firing counts as an attempt if no BlueZ connect operation was actually started.

Recommended:

```text
attempt count represents actual reconnect attempts
```

Therefore operation-contention rejection should preferably **not consume a reconnect attempt**.

If current ReconnectPolicy increments before due execution, refactor accounting if necessary.

Document exact semantics.

---

# 17. Operation-Completion Hook

A robust design may trigger deferred reconnect after another operation finishes.

Example:

```text
Reconnect due
    |
device currently Pairing
    |
mark reconnectPendingAfterOperation
    |
Pairing completes
    |
if still disconnected and recovery permitted:
    schedule/re-enter managed reconnect
```

Avoid polling if an operation-completion event already exists.

---

# 18. Mandatory Contention Tests

## 18.1 Busy Device When Reconnect Due

Setup:

```text
ReconnectAndRestore
device disconnected
reconnect scheduled
```

Before timer fires, mark device operation busy.

Fire due retry.

Assert:

```text
no BlueZ connect call yet
recovery does not stall
retry remains pending/deferred
attempt budget not incorrectly consumed
```

Then release operation.

Assert:

```text
managed reconnect proceeds
```

## 18.2 Stop While Deferred

```text
reconnect deferred due to busy operation
session stops
```

Then operation completes.

Assert:

```text
no reconnect
```

## 18.3 Policy Changes While Deferred

```text
reconnect deferred
policy -> None
```

Operation completes.

Assert:

```text
no reconnect
```

---

# 19. FINAL DEFECT D — ReconnectPolicy Timer Lifetime

## 19.1 Observed Problem

Reconnect timers may be allocated as:

```cpp
new QTimer(this)
```

and later cancelled with:

```cpp
timer->stop();
entries_.erase(...)
```

without deleting the timer.

Since the timer remains a QObject child, repeated reconnect episodes may accumulate stopped timers until application shutdown.

This is a long-running process hygiene issue.

---

# 20. Required Timer Cleanup

When a reconnect entry is permanently removed:

```text
cancel
success
terminal failure
exhaustion
device removal
shutdown
```

stop and destroy its timer safely.

Preferred:

```cpp
timer->stop();
timer->deleteLater();
```

or ownership through:

```text
std::unique_ptr<QTimer>
```

if consistent with QObject/thread ownership.

Be careful about deleting a timer from inside its own timeout callback.

`deleteLater()` is usually safer in Qt event-loop code.

---

# 21. Timer Cleanup Test

Add observability or a test-only helper if necessary.

Stress:

```text
for 1000 cycles:
    schedule reconnect
    cancel
```

Assert:

```text
ReconnectPolicy has zero live entries
QObject timer child count does not grow unbounded
```

Avoid fragile tests tied to unrelated QObject children.

A test-specific count of owned retry timers is preferable if available.

---

# 22. Recovery Policy Effective-Permission Function

To prevent future drift, centralize policy calculation.

Conceptual:

```cpp
bool SessionManager::bluetoothReconnectAllowed(
    const AuralisSession& session,
    const SessionDevice& member) const;
```

or lower-level equivalent.

Inputs may include:

```text
session recovery policy
session autoReconnect
member enabled
device generic autoReconnect preference
session state
active session identity
```

Use this single function for:

```text
requesting managed reconnect
applying reconnect suppression
handling policy changes
restoring suppression after session switch
```

---

# 23. Reconnect Authorization Matrix

Implement and test a matrix similar to:

| Device preference | Session autoReconnect | RecoveryPolicy | Effective Bluetooth reconnect |
|---|---:|---|---:|
| true | true | None | false |
| true | true | RestoreRoutesOnly | false |
| true | true | ReconnectAndRestore | true |
| true | false | ReconnectAndRestore | false |
| false | true | ReconnectAndRestore | false unless explicitly documented otherwise |
| false | true | None | false |

If existing semantics differ, document why.

---

# 24. Session Stop Must Restore Lower Reconnect Policy State

If SessionManager temporarily suppresses lower reconnect:

```text
Session ACTIVE, policy None
```

then stop.

After stop:

```text
session override removed
generic device autoReconnect preference restored
```

Test this.

Do not leave the Bluetooth device permanently suppressed.

---

# 25. Session Switch Must Transfer Override Cleanly

Example:

```text
Session A uses device X with policy None
Session B uses device X with ReconnectAndRestore
```

Switch A -> B.

Assert:

```text
A suppression removed
B effective policy applied
```

No momentary duplicate schedule should occur during switch.

---

# 26. Device Removal Must Clear Override

If a member is removed from a session:

```text
removeDevice(session, device)
```

clear any session-specific reconnect suppression/authorization state for that device.

Likewise for session deletion.

---

# 27. Shutdown

Before lower Bluetooth services are destroyed:

```text
SessionManager
    cancels recovery
    clears session-level reconnect overrides
    stops sessions
```

Then lower managers may shut down.

Avoid callbacks into destroyed session services.

---

# 28. Error Model

Use structured errors.

Add or reuse codes such as:

```text
RecoveryExhausted
ManagedReconnectTerminalFailure
ManagedReconnectDeferred
ManagedReconnectCancelled
```

Only terminal conditions should become persistent member errors.

Temporary contention/deferred retry should usually be logged rather than exposed as a terminal error.

---

# 29. Logging

Add concise structured logs.

Recommended events:

```text
SessionReconnectSuppressed
SessionReconnectSuppressionCleared
ManagedReconnectRequestIgnoredAlreadyScheduled
ManagedReconnectTerminalFailure
ManagedReconnectDeferredDeviceBusy
ManagedReconnectDeferredRescheduled
ManagedReconnectTimerDestroyed
```

Include:

```text
session
device
policy
attempt
reason
```

where relevant.

---

# 30. Do Not Regress Previously Fixed Behavior

Re-run and preserve all existing tests for:

- duplicate reconnect scheduling;
- reconnect exhaustion;
- `STOPPING` reentrancy;
- `RecoveryPolicy::None` route creation suppression;
- `RecoveryPolicy::None` route reactivation suppression;
- JSON persistence parse;
- `createSession()` persistence rollback;
- source loss/return;
- stable source restoration;
- generation safety;
- volume NaN/Inf;
- member-return endpoint ID replacement.

---

# 31. Required New Test Inventory

Add explicit tests equivalent to:

```text
tst_SessionRecoveryPolicyAuthorityIntegration
tst_ManagedReconnectTerminalFailure
tst_ManagedReconnectOperationContention
tst_ReconnectPolicyTimerCleanup
```

Names may be merged into current executables.

Coverage matters more than file count.

---

# 32. Production-Path Test Stack

At least one test must instantiate:

```text
FakeBlueZClient
BluetoothManager
DeviceLifecycleManager
ReconnectPolicy
DeviceRegistry
SessionManager
```

This is mandatory because the remaining defects exist specifically at the Session ↔ Bluetooth integration boundary.

Pure SessionManager fakes cannot prove these fixes.

---

# 33. Test — None Does Not Schedule Lower Auto-Reconnect

Exact scenario:

```text
device.autoReconnectEnabled=true

session:
    state ACTIVE
    policy None
    autoReconnect=true
```

Inject BlueZ:

```text
Connected=false
```

Assert:

```text
ReconnectPolicy::isScheduled(device) == false
attempt count unchanged
no reconnectDue signal
session DEGRADED
```

---

# 34. Test — RestoreRoutesOnly Does Not Schedule Lower Reconnect

Same production stack.

Assert:

```text
no lower reconnect schedule
```

Then simulate external reconnect and endpoint return.

Assert route recovery.

---

# 35. Test — ReconnectAndRestore Schedules Exactly Once

Inject one disconnect.

Assert:

```text
one schedule
```

Inject duplicate deviceUpdated/property events.

Assert:

```text
still one schedule
attempt counter not consumed twice
timer not restarted unnecessarily
```

---

# 36. Test — Terminal Bluetooth Failure

Configure fake BlueZ connect to return a known non-retryable terminal error.

Assert:

```text
ReconnectPolicy no longer scheduled
SessionManager notified
member recovering false
session leaves RECOVERING
error surfaced
```

---

# 37. Test — Busy Operation Deferred Retry

Force:

```text
beginOperation(Reconnecting) == false
```

due to another active operation.

Assert recovery remains live/deferred.

Then complete other operation.

Assert reconnect occurs.

---

# 38. Test — Deferred Retry Cancelled by Stop

Before deferred retry resumes:

```text
deactivateSession()
```

Then complete other operation.

Assert no reconnect.

---

# 39. Test — Timer Cleanup Stress

Example:

```cpp
for (int i = 0; i < 1000; ++i) {
    schedule
    cancel
}
```

Verify no growing retained timer population.

If QObject child inspection is used, isolate only retry timers.

---

# 40. Static Audit of ReconnectPolicy

Inspect:

```text
scheduleReconnect
completeReconnectAttempt
cancelReconnect
cancelAll
exhaustion
success reset
device removal
```

Confirm:

- no attempt double counting;
- no timer leak;
- no entry stuck `inFlight`;
- no entry silently abandoned;
- success resets state;
- terminal failure removes state.

---

# 41. Static Audit of DeviceLifecycleManager

Trace all returns from reconnect callbacks.

For every branch after:

```text
reconnect due
```

the operation must end in exactly one of:

```text
retry scheduled
success
terminal failure signaled
exhausted signaled
cancelled explicitly
deferred retry state
```

There must be no silent return that leaves upper layers believing recovery is active.

---

# 42. Static Audit of SessionManager

Trace every member recovery state assignment.

Search:

```bash
grep -R \
  "managedReconnectRequested\|recovering =\|RecoveryExhausted\|Terminal" \
  -n src/session include/auralis/session
```

For every path that sets:

```text
recovering=true
```

prove there is a path that clears it on:

```text
success
cancel
terminal failure
exhaustion
member removal
disable
stop
session delete
policy change
shutdown
```

---

# 43. Session State Invariant

After final correction:

```text
RECOVERING
```

must mean actual recovery work or a legitimate deferred recovery is still pending.

Never use RECOVERING for a dead-end state.

If no recovery work remains:

```text
healthy peer exists -> DEGRADED
no usable peer -> FAILED
```

---

# 44. Build and Targeted Test Cycle

During implementation:

```bash
cmake --build build
ctest --test-dir build \
  -R "Reconnect|Session|Bluetooth" \
  --output-on-failure
```

Run after each correction.

---

# 45. Full Clean Regression

Before completion:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Record exact totals.

---

# 46. Repeat Phase 6 Tests

Run:

```bash
for i in $(seq 1 100); do
  ctest --test-dir build \
    -R "Session|Reconnect|Bluetooth" \
    --output-on-failure || exit 1
done
```

If too slow, choose a still meaningful repetition count and document it.

The remaining issues are race/event-order related, so repetition matters.

---

# 47. Parallel Test

Run:

```bash
ctest --test-dir build \
  -j "$(nproc)" \
  --output-on-failure
```

Catch shared-state problems.

---

# 48. Sanitizer Pass

If supported, run at least focused:

```text
ASan
UBSan
```

especially:

```text
ReconnectPolicy timer lifetime
operation contention
stop
session switch
late callback
```

Do not claim sanitizer success unless actually executed.

---

# 49. Optional Live Hardware Validation

If two devices are available, test:

```text
policy None
disconnect one
verify Auralis does not initiate reconnect

policy RestoreRoutesOnly
disconnect one
verify Auralis does not initiate reconnect

policy ReconnectAndRestore
disconnect one
verify managed reconnect
```

Then verify route restoration.

If hardware is unavailable:

```text
LIVE HARDWARE VALIDATION: NOT RUN
```

---

# 50. Documentation Update

Update:

```text
docs/validation/phase-6-final-closure.md
```

or add a new final boundary-fix section.

Document:

- session-vs-device reconnect authority;
- terminal reconnect failure propagation;
- operation-contention recovery semantics;
- timer lifetime fix;
- tests added;
- exact results.

---

# 51. Required Final Report

End the implementation task with:

## A. Baseline

Exact pre-fix test result.

## B. Final Defect A

Explain how session recovery policy now suppresses lower Phase 3 auto reconnect when required.

## C. Final Defect B

Explain terminal reconnect failure propagation.

## D. Final Defect C

Explain operation-contention/deferred retry handling.

## E. Final Defect D

Explain timer cleanup/lifetime.

## F. Reconnect Ownership Matrix

Provide:

```text
device preference
session autoReconnect
session recovery policy
effective reconnect
```

## G. Tests Added

Exact test names and what each proves.

## H. Full CTest

Exact command and summary.

## I. Repetition / Parallel Results

Actual counts.

## J. Sanitizer Result

If run.

## K. Hardware Result

If run.

## L. Remaining Limitations

List any genuine limitations.

## M. Final Verdict

Use exactly one:

```text
PHASE 6 STATUS: COMPLETE
```

or:

```text
PHASE 6 STATUS: NOT COMPLETE
```

If not complete, list blockers.

---

# 52. Final Acceptance Gate

Do not mark Phase 6 complete unless all of these are true:

- [ ] `RecoveryPolicy::None` prevents lower Bluetooth auto reconnect before disconnect occurs
- [ ] `RestoreRoutesOnly` prevents lower Bluetooth auto reconnect before disconnect occurs
- [ ] `ReconnectAndRestore` schedules exactly one managed reconnect path
- [ ] device generic auto reconnect preference is not permanently corrupted by session overrides
- [ ] terminal/non-retryable reconnect failure reaches SessionManager
- [ ] SessionManager clears recovery flags on terminal failure
- [ ] session leaves RECOVERING when no recovery work remains
- [ ] beginOperation contention cannot silently abandon reconnect
- [ ] contention does not incorrectly consume retry budget
- [ ] deferred reconnect is cancelled on stop/policy change
- [ ] reconnect timers are destroyed when entries are removed
- [ ] no retained stopped-timer accumulation
- [ ] all previous Phase 6 fixes remain passing
- [ ] complete Phase 0–6 regression passes
- [ ] repeated focused tests show no flake
- [ ] documentation matches implementation

---

# 53. Hard Prohibitions

Do not:

- rewrite BluetoothManager;
- rewrite SessionManager;
- create another reconnect scheduler;
- permanently mutate user reconnect preferences for temporary session behavior;
- allow Phase 3 generic auto reconnect to bypass active session recovery policy;
- leave terminal reconnect failures invisible;
- silently abandon a reconnect due to operation contention;
- leak stopped retry timers;
- weaken existing tests;
- claim live hardware proof without hardware;
- move to Phase 7 before the final audit.

---

# 54. Core Completion Statement

Use this as the target:

> **Phase 6 is fully complete only when the session recovery policy is authoritative over the complete Bluetooth recovery path, reconnect timing has one lower-level owner, all reconnect termination paths are observable by the session layer, operation contention cannot strand recovery, reconnect timer lifetime is bounded and clean, and all Phase 0–6 automated regressions pass repeatedly without race or state inconsistency.**

---

# 55. Begin Now

Start with the baseline audit.

Then fix only these final recovery-boundary defects.

Preserve all working Phase 6 behavior.

Add production-path tests before declaring success.

Do not proceed into Phase 7 until the final independent audit confirms Phase 6 closure.
