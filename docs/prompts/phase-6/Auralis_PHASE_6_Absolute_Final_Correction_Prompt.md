# Auralis — Phase 6 Absolute Final Correction Prompt
## Close the Last 2–3%: Reconnect Timer Lifetime, Deferred-Reconnect Proof, Member Suppression Lifecycle, and Device-vs-Session Reconnect Semantics

**Project:** Auralis  
**Phase:** Phase 6 — Multi-Device Session Engine  
**Target:** Current repository after the latest recovery-boundary hardening pass  
**Execution environment:** AI coding IDE running inside the current Auralis repository on the Linux development machine  
**Objective:** Resolve the final known Phase 6 software gaps without redesigning working Phase 0–6 architecture, add production-path regression coverage, and prepare the repository for a final independent actual-machine rigorous audit.

---

# 1. Mission

The current Phase 6 implementation is already mature.

The following major areas are already implemented and must be preserved:

- SessionManager
- SessionStateMachine
- RoutingCoordinator
- VolumeCoordinator
- persistent sessions
- multi-device routing
- degraded operation
- RecoveryPolicy::None
- RecoveryPolicy::RestoreRoutesOnly
- RecoveryPolicy::ReconnectAndRestore
- lower-level managed reconnect scheduling
- duplicate reconnect de-duplication
- retry exhaustion propagation
- terminal reconnect failure propagation
- stop/reentrancy guards
- source loss/recovery
- operation generation safety
- persistence error handling
- stable source restoration
- volume/mute restoration

Do **not** rewrite these systems.

Your job is only to close the remaining defects identified by the latest independent source audit.

---

# 2. Known Remaining Issues

The latest audit identified these final items:

1. cancelled/successfully-completed reconnect QTimers can remain allocated until ReconnectPolicy destruction;
2. DeviceLifecycleManager contention/deferred-reconnect logic is implemented but not proven with a real lifecycle-level integration test;
3. session reconnect suppression is not fully removed/reapplied when a member is disabled/enabled during an active session;
4. semantics between device-level `autoReconnectEnabled` and session-level `ReconnectAndRestore` are not explicitly defined/tested;
5. final closure documentation currently overstates timer cleanup;
6. final actual-machine evidence must include raw logs.

Treat all six as mandatory closure items.

---

# 3. First Step — Baseline Audit

Before changing code:

```bash
git status --short
git branch --show-current
git log --oneline -n 20

grep -R \
  "cancelReconnect\|cancelAll\|deleteLater\|retryAfterContention\|beginOperation\|autoReconnectSuppressed\|suppressAutoReconnect\|unsuppressAutoReconnect\|autoReconnectEnabled\|setDeviceEnabled" \
  -n include src tests docs 2>/dev/null
```

Run:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If baseline is stale:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record:

- total tests;
- passed;
- failed;
- skipped;
- existing Session/Reconnect/Bluetooth tests.

Do not begin editing until the baseline is understood.

---

# 4. FINAL ISSUE A — Reconnect QTimer Lifetime

## 4.1 Observed Problem

Reconnect timers are QObject children, typically created as:

```cpp
new QTimer(this)
```

Some terminal paths correctly call:

```cpp
timer->stop();
timer->deleteLater();
```

However common cancellation paths may currently do only:

```cpp
timer->stop();
entries_.erase(it);
```

or:

```cpp
timer->stop();
entries_.clear();
```

The QTimer object therefore remains alive as a QObject child until the entire ReconnectPolicy object is destroyed.

Common affected paths include:

```text
successful reconnect
session stop
policy cancellation
device removal
cancelManagedReconnect
cancelAll
shutdown
```

Repeated disconnect/reconnect cycles can accumulate stopped timer objects.

## 4.2 Required Invariant

For every reconnect entry that is permanently removed:

```text
entry removed
=> retry timer is no longer retained
```

After the Qt event loop processes deferred deletes:

```text
live timer count should converge to zero
```

when no reconnect entries remain.

## 4.3 Required Correction

Audit every path that removes an entry:

```text
cancelReconnect
cancelAll
successful reconnect/onConnected
terminal failure
exhaustion
device forgotten/removed
shutdown
```

Use one centralized helper if possible.

Conceptual:

```cpp
void ReconnectPolicy::destroyEntryTimer(Entry& entry)
{
    if (entry.timer == nullptr) {
        return;
    }

    entry.timer->stop();
    entry.timer->deleteLater();
    entry.timer = nullptr;
}
```

Then all permanent entry removal goes through the helper.

Do not manually duplicate cleanup across many branches if avoidable.

## 4.4 Delete Safety

Be careful if cancellation occurs from inside:

```text
QTimer::timeout
```

Use:

```cpp
deleteLater()
```

rather than immediate delete where QObject reentrancy is possible.

## 4.5 `cancelAll()`

Before clearing:

```cpp
for each entry:
    stop
    deleteLater
    clear pointer
```

Then clear the container.

## 4.6 Successful Reconnect

If successful connection currently calls:

```text
cancelReconnect(device)
```

that path must also free the timer.

---

# 5. Mandatory Timer Lifetime Tests

Add a deterministic test.

Suggested name:

```text
tst_ReconnectPolicy::cancelDestroysRetryTimer
```

and a stress test:

```text
tst_ReconnectPolicy::repeatedScheduleCancelDoesNotAccumulateTimers
```

## 5.1 Single Cycle

```text
schedule reconnect
assert one entry/timer
cancel
process event loop
assert zero entries
assert zero retained retry timers
```

## 5.2 Stress

Run:

```text
1000 cycles
```

of:

```text
schedule
cancel
process deferred deletion periodically
```

Assert retained retry timers do not grow with cycle count.

Avoid counting unrelated QObject children.

Prefer a test-only accessor:

```cpp
int liveRetryTimerCountForTesting() const;
```

compiled only in test builds if appropriate.

Alternatively assign a unique QObject name to retry timers and count only those.

---

# 6. FINAL ISSUE B — Production-Level Operation Contention Proof

## 6.1 Current Design

The current reconnect flow may correctly support:

```text
reconnect timer due
    |
beginOperation(Reconnecting) fails because device busy
    |
retryAfterContention(...)
```

with attempt rollback.

This is promising, but pure ReconnectPolicy testing is insufficient.

The bug exists at:

```text
DeviceLifecycleManager
↔
ReconnectPolicy
```

therefore the test must exercise this integration boundary.

---

# 7. Required Contention Integration Test

Instantiate actual:

```text
ReconnectPolicy
DeviceLifecycleManager
```

and preferably:

```text
BluetoothManager
FakeBlueZClient
```

if the current test architecture supports it.

## Scenario

1. device starts disconnected;
2. reconnect gets scheduled;
3. before reconnect timer fires, place device lifecycle into another operation such as:
   - pairing;
   - disconnecting;
   - connecting;
   - test-only busy operation;
4. fire reconnect due;
5. verify `beginOperation(Reconnecting)` fails;
6. verify reconnect is deferred/requeued safely;
7. verify retry attempt budget is not incorrectly consumed;
8. complete the blocking operation;
9. verify reconnect becomes eligible and eventually invokes BlueZ connect exactly once.

## Assertions

```text
BlueZ connect call count before contention clears = 0
recovery remains pending
attempt budget unchanged or restored
no terminal failure emitted
after contention clears:
    one BlueZ connect
```

---

# 8. Contention Cancellation Test — Stop

Scenario:

```text
reconnect pending
device busy
retry deferred
session stop/cancel reconnect
blocking operation completes
```

Assert:

```text
no BlueZ connect
no retry resurrection
no scheduled reconnect entry
```

If this requires SessionManager + lifecycle integration, use the production path.

---

# 9. Contention Cancellation Test — Policy Change

Scenario:

```text
ReconnectAndRestore
reconnect deferred due to device busy
policy changes to None
blocking operation finishes
```

Assert:

```text
no reconnect
suppression active if applicable
no stale deferred reconnect
```

Also test change to:

```text
RestoreRoutesOnly
```

---

# 10. FINAL ISSUE C — Disabled Member Suppression Lifecycle

## 10.1 Problem

While a session is active, SessionManager may install a lower Bluetooth auto-reconnect suppression for session members.

Example:

```text
Session active
Device B member
policy None
```

Lower generic reconnect is suppressed.

But if user executes:

```text
setDeviceEnabled(B, false)
```

B is no longer a required participant.

The session must not continue changing generic reconnect behavior for a disabled member.

## 10.2 Required Semantics

When member becomes disabled:

```text
cancel member session recovery
remove session-level reconnect suppression for that member
remove member route
recompute session state
```

When member becomes enabled while session is active:

```text
apply current session reconnect suppression/authorization policy
then reconcile member
```

---

# 11. Mandatory Disable/Enable Tests

## 11.1 Disable

```text
session ACTIVE
policy None
member B enabled
suppression active
```

Call:

```text
setDeviceEnabled(B, false)
```

Assert:

```text
suppression removed
B route removed
B no longer affects session health
generic device reconnect behavior restored
```

## 11.2 Re-enable

```text
setDeviceEnabled(B, true)
```

Assert:

```text
current session policy reapplied
if policy None:
    generic reconnect suppression reinstalled
if ReconnectAndRestore:
    no suppressive override remains
```

Then verify route/member reconciliation.

---

# 12. Member Removal / Session Delete / Stop

Re-audit and test suppression cleanup for:

```text
removeDevice
deleteSession
stopSession
switchActiveSession
shutdown
```

After cleanup:

```text
generic device reconnect behavior must return to its pre-session state
```

Do not leave suppression tokens behind.

---

# 13. FINAL ISSUE D — Device Preference vs Session Reconnect Policy

## 13.1 Problem

There are two levels of intent:

```text
device.autoReconnectEnabled
```

and:

```text
session.autoReconnect
session.recoveryPolicy
```

The effective rule must be explicit.

Currently a session configured as:

```text
ReconnectAndRestore
```

may request a reconnect even if:

```text
device.autoReconnectEnabled == false
```

If this is accidental, fix it.

If it is intentional, document it and test it.

---

# 14. Recommended Effective Rule

Use the conservative rule:

```text
effectiveBluetoothReconnectAllowed =
    device.autoReconnectEnabled
    AND session.autoReconnect
    AND session.recoveryPolicy == ReconnectAndRestore
    AND member.enabled
    AND session state allows recovery
```

This respects both the user's/device preference and session policy.

If product design explicitly requires session policy to override the generic device setting, document that exception and add tests.

Do not leave behavior implicit.

---

# 15. Centralize Effective Reconnect Decision

Avoid checking different combinations in different files.

Provide one logical policy function.

Conceptual:

```cpp
bool SessionManager::effectiveReconnectAllowed(
    const AuralisSession& session,
    const SessionDevice& member,
    const BluetoothDevice& device) const;
```

or an equivalent lower-level policy service.

Use it for:

```text
requestManagedReconnect
suppression installation
suppression removal
policy changes
member enable/disable
session activation
session switching
```

---

# 16. Reconnect Permission Matrix

Add test coverage for:

| Device autoReconnect | Session autoReconnect | RecoveryPolicy | Expected Auralis Bluetooth reconnect |
|---|---:|---|---:|
| true | true | None | false |
| true | true | RestoreRoutesOnly | false |
| true | true | ReconnectAndRestore | true |
| true | false | ReconnectAndRestore | false |
| false | true | ReconnectAndRestore | false |
| false | false | ReconnectAndRestore | false |

If chosen semantics differ, update the expected matrix and documentation deliberately.

---

# 17. Device Preference Must Survive Session Lifecycle

Test:

```text
device.autoReconnectEnabled = true
session policy None
session activates
temporary suppression applied
session stops
```

Assert:

```text
device.autoReconnectEnabled remains true
generic auto reconnect available again
```

Test with `false` as well.

No session operation may silently change the user's stored device preference.

---

# 18. ReconnectAndRestore + Device AutoReconnect False

Under recommended semantics:

```text
device.autoReconnectEnabled=false
session ReconnectAndRestore
device disconnects
```

Expected:

```text
no managed reconnect request
session becomes DEGRADED or FAILED as appropriate
route may restore only if device returns externally and policy semantics allow
```

Document this explicitly.

---

# 19. Re-audit Terminal Failure Handling

The previous pass added:

```text
ReconnectPolicy terminal failure
    ->
BluetoothManager
    ->
SessionManager
```

Retest it after the policy changes.

Ensure no regression.

Test both:

```text
healthy peer remains -> DEGRADED
no healthy peer -> FAILED
```

---

# 20. Re-audit Retry Exhaustion

Ensure attempt exhaustion still:

```text
clears recovery state
emits error
removes reconnect timer
leaves RECOVERING
```

The timer cleanup correction must apply here too.

---

# 21. Re-audit Session Stop

With suppression cleanup changes:

```text
ACTIVE/DEGRADED/RECOVERING
stop
```

must:

```text
STOPPING
cancel managed reconnect
clear suppression
remove routes
IDLE
```

Late events must not resurrect session recovery.

---

# 22. Re-audit Session Switch

If one active session is allowed:

```text
Session A active
Session B activated
```

Ensure:

```text
A recovery cancelled
A suppressions cleared
B suppressions installed according to B policy
B becomes active
```

No gap should allow stale A reconnect work to survive.

---

# 23. Re-audit Shutdown

Before destroying Bluetooth services:

```text
SessionManager
    stop sessions
    cancel recovery
    clear session reconnect suppressions
```

Then lower Bluetooth lifecycle may stop.

Avoid restoring generic reconnect behavior in a way that triggers new reconnect scheduling during shutdown.

If necessary, use a shutdown guard:

```text
applicationShuttingDown
```

so clearing suppression does not cause reevaluation/reconnect.

---

# 24. Shutdown-Suppression Test

Simulate:

```text
session active with policy None
device disconnected
suppression active
application shutdown begins
```

Assert:

```text
suppression cleanup does not schedule a reconnect during shutdown
```

This is important if `unsuppressAutoReconnect()` reevaluates reconnect candidates immediately.

---

# 25. Timer Cleanup + Shutdown Interaction

`cancelAll()` during shutdown must:

```text
stop all retry timers
deleteLater all timers
clear entries
```

Because event loop may soon terminate, consider whether `deleteLater()` will execute.

If QObject parent destruction immediately follows, this is still safe.

For testability, process events before destruction.

Do not use unsafe immediate deletion inside timeout callbacks.

---

# 26. Test Naming Suggestions

Use current test organization where possible.

Suggested cases:

```text
tst_ReconnectPolicy::cancelDestroysRetryTimer
tst_ReconnectPolicy::repeatedScheduleCancelDoesNotAccumulateTimers

tst_DeviceLifecycleReconnect::busyOperationDefersReconnect
tst_DeviceLifecycleReconnect::deferredReconnectCancelled
tst_DeviceLifecycleReconnect::deferredReconnectPolicyChange

tst_SessionReconnectSuppression::disableRemovesSuppression
tst_SessionReconnectSuppression::enableReappliesSuppression
tst_SessionReconnectSuppression::stopRestoresDeviceBehavior
tst_SessionReconnectSuppression::shutdownDoesNotTriggerReconnect

tst_SessionReconnectPolicy::devicePreferenceMatrix
```

Do not create unnecessary executables if existing test targets can host these cases.

---

# 27. Production-Path Requirement

At least the following tests must use the real chain:

```text
FakeBlueZClient
BluetoothManager
DeviceLifecycleManager
ReconnectPolicy
DeviceRegistry
SessionManager
```

Required production-path cases:

- policy None before disconnect;
- RestoreRoutesOnly before disconnect;
- ReconnectAndRestore before disconnect;
- member disable suppression cleanup;
- device autoReconnectEnabled false;
- contention + reconnect deferral if feasible.

---

# 28. Avoid Weak Tests

Do not accept:

```text
QVERIFY(true)
signal count >= 1
no crash
```

when exact assertions are possible.

Prefer:

```text
reconnect schedule count == 0/1
attempt == expected
route count == expected
suppression state == expected
session state == expected
BlueZ connect calls == expected
timer count == expected
```

---

# 29. Repeat Race Tests

After implementation:

```bash
for i in $(seq 1 100); do
  ctest --test-dir build \
    -R "Session|Reconnect|Bluetooth" \
    --output-on-failure || exit 1
done
```

If the suite is large, repeat the exact new test binaries 500 times where practical.

---

# 30. Full Regression

Run:

```bash
ctest --test-dir build --output-on-failure
```

All prior Phase 0–6 tests must pass.

Do not change or weaken earlier tests to make new behavior pass.

---

# 31. Clean Final Build

Mandatory:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

---

# 32. Sanitizers

If supported, run focused ASan/UBSan.

Especially test:

```text
1000 reconnect timer schedule/cancel cycles
device contention
session disable/enable
session stop
shutdown
late callbacks
```

---

# 33. Static Audit

Before completion:

```bash
grep -R "new QTimer" -n src/bluetooth include/auralis/bluetooth
grep -R "cancelReconnect\|cancelAll" -n src/bluetooth include/auralis/bluetooth
grep -R "suppressAutoReconnect\|unsuppressAutoReconnect" -n src/session src/bluetooth
grep -R "autoReconnectEnabled" -n src include tests
```

Manually verify every relevant branch.

---

# 34. Documentation Correction

Update:

```text
docs/validation/phase-6-final-closure.md
```

Do not claim timer cleanup fixed until:

```text
cancelReconnect
cancelAll
success
terminal
exhaustion
```

all release their timers correctly.

Add the final reconnect permission matrix.

---

# 35. Raw Evidence

Create:

```text
audit-phase6-final-absolute/
```

Store:

```text
baseline-ctest.log
configure.log
build.log
ctest-full.log
ctest-session-repeat.log
ctest-parallel.log
asan.log
ubsan.log
new-tests.log
git-diff-stat.txt
```

These raw logs are important for the final external audit.

Do not only write a Markdown summary.

---

# 36. Final Implementation Report

At completion produce:

## A. Baseline

```text
tests before changes
```

## B. Timer Lifetime Fix

Explain every cleanup path.

## C. Contention Proof

Explain production lifecycle behavior and tests.

## D. Suppression Lifecycle

Explain:

```text
activate
disable
enable
remove
stop
switch
shutdown
```

## E. Reconnect Permission Matrix

State exact policy.

## F. Tests Added

Exact names.

## G. Full CTest

Exact command and actual totals.

## H. Repeat Tests

Exact iterations and results.

## I. Sanitizers

Actual result.

## J. Raw Logs

List files created under:

```text
audit-phase6-final-absolute/
```

## K. Hardware Test

If not run, say:

```text
NOT RUN
```

## L. Final Verdict

Use exactly:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
```

or:

```text
PHASE 6 SOFTWARE STATUS: NOT COMPLETE
```

---

# 37. Software Completion Gate

Do not say COMPLETE unless:

- [ ] cancelled reconnect timers are deleted
- [ ] cancelAll deletes timers
- [ ] successful reconnect deletes timer
- [ ] exhaustion deletes timer
- [ ] terminal failure deletes timer
- [ ] 1000-cycle timer stress test passes
- [ ] real DeviceLifecycle contention test passes
- [ ] deferred retry cancellation passes
- [ ] disabling member removes session suppression
- [ ] re-enabling reapplies current policy
- [ ] member removal clears suppression
- [ ] stop clears suppression
- [ ] session switch transfers suppression safely
- [ ] shutdown does not trigger reconnect while clearing suppression
- [ ] device `autoReconnectEnabled` semantics are explicitly defined
- [ ] permission matrix is tested
- [ ] all prior Phase 6 recovery tests still pass
- [ ] clean full Phase 0–6 CTest passes
- [ ] repeated race-focused tests pass
- [ ] raw audit logs exist

---

# 38. Hard Prohibitions

Do not:

- rewrite Phase 6;
- create a new reconnect engine;
- replace DeviceLifecycleManager;
- change user reconnect preferences merely to simulate session suppression;
- leave stopped QTimers retained;
- use sleeps instead of deterministic tests when avoidable;
- weaken earlier tests;
- claim hardware validation without hardware;
- proceed to Phase 7 before the independent final audit.

---

# 39. Final Target

> Phase 6 software is ready to close only when reconnect resource lifetime is bounded, session reconnect suppression follows member/session lifecycle exactly, device-level and session-level reconnect intent have one explicit tested rule, deferred reconnect cannot strand or resurrect recovery, and the complete Phase 0–6 regression suite passes repeatedly with raw evidence.

Begin with the baseline audit and make only the smallest changes necessary.
