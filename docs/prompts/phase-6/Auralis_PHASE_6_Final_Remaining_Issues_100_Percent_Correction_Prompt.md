# Auralis — Phase 6 Final Remaining Issues Correction Prompt
## Final Surgical Pass to Reach 100% Multi-Device Session Engine Completion

**Project:** Auralis  
**Target:** Current repository after the latest Phase 6 hardening pass  
**Status entering this task:** Phase 6 is substantially complete, but a small set of integration-level correctness issues remain  
**Goal:** Resolve every remaining Phase 6 blocker without rewriting the working architecture, add the missing regression tests, and leave the repository ready for an independent rigorous audit.

---

# 1. Role

You are the senior C++/Qt engineer responsible for the **final Phase 6 correction pass**.

The repository already contains working Phase 6 components, including:

- `SessionManager`
- `SessionStateMachine`
- `RoutingCoordinator`
- `VolumeCoordinator`
- session persistence
- managed Bluetooth reconnect integration
- Phase 6 unit tests
- Phase 6 integration tests
- Phase 6 documentation

Do **not** redesign Phase 6.

Do **not** create parallel managers or alternative routing/recovery subsystems.

Your job is to correct the small number of remaining defects identified by a source-level audit and then prove the corrections with focused automated tests.

---

# 2. Non-Negotiable Rule

Preserve the current architecture.

Do not replace:

```text
SessionManager
RoutingCoordinator
VolumeCoordinator
AudioRouter
BluetoothManager
DeviceLifecycleManager
ReconnectPolicy
SessionPersistence
```

unless a tiny API extension is required to close a specific correctness gap.

Prefer:

```text
small fix
+ focused regression test
+ full suite verification
```

over:

```text
large refactor
```

---

# 3. Mandatory Baseline Audit

Before editing:

```bash
git status --short
git branch --show-current
git log --oneline -n 20
```

Inspect:

```bash
grep -R \
  "requestManagedReconnect\|scheduleReconnect\|ReconnectPolicy\|SessionManager\|RoutingCoordinator\|SessionPersistence\|RecoveryPolicy\|operationGeneration\|routeStateChanged\|Stopping\|RecoveryExhausted" \
  -n include src tests docs 2>/dev/null
```

Then run:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If build state is stale:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record the exact baseline before changing code.

---

# 4. Remaining Blocker A — Duplicate Managed Reconnect Scheduling

## 4.1 Problem

A single BlueZ disconnect event may currently trigger reconnect scheduling through two paths:

```text
BlueZ Connected=false
        |
        +--> DeviceRegistry update
        |       |
        |       +--> SessionManager
        |               |
        |               +--> requestManagedReconnect()
        |                       |
        |                       +--> ReconnectPolicy::scheduleReconnect()
        |
        +--> DeviceLifecycleManager
                |
                +--> handleUnexpectedDisconnect()
                        |
                        +--> ReconnectPolicy::scheduleReconnect()
```

If `ReconnectPolicy::scheduleReconnect()` increments attempt count and restarts the timer on every call, one physical disconnect may consume multiple retry attempts.

This is not acceptable.

## 4.2 Required Invariant

For a single disconnect episode:

```text
one reconnect schedule state
one attempt counter progression
one authoritative backoff timer
```

regardless of how many upper-layer components request managed recovery.

## 4.3 Preferred Fix

Make managed reconnect scheduling **idempotent**.

Possible approach:

```cpp
bool ReconnectPolicy::isScheduled(const DeviceId&) const;
bool ReconnectPolicy::isReconnectInProgress(const DeviceId&) const;
```

Then:

```cpp
if (!reconnect_->isScheduled(device)
    && !reconnect_->isReconnectInProgress(device)) {
    reconnect_->scheduleReconnect(device);
}
```

Alternatively, change:

```cpp
scheduleReconnect(...)
```

itself so repeated calls for the same active disconnect episode do not increment attempts or restart the timer.

Whichever layer owns the guard must be clearly documented.

## 4.4 Better Ownership

If possible, prefer:

```text
DeviceLifecycleManager owns reconnect scheduling
SessionManager only expresses session recovery intent
```

SessionManager should not need to directly manipulate retry timing.

If `requestManagedReconnect()` exists, it should be a high-level idempotent request.

## 4.5 Mandatory Tests

Add a production-path integration test using the real:

```text
BluetoothManager
DeviceLifecycleManager
ReconnectPolicy
SessionManager
```

with fake BlueZ transport.

Test:

```text
Connected=true
then BlueZ emits Connected=false once
```

Assert:

```text
reconnect attempt counter increments once
only one timer is active
no duplicate reconnectDue emission
```

Also test duplicate identical disconnect signals:

```text
Connected=false
Connected=false
Connected=false
```

and ensure retry state does not reset/restart incorrectly.

---

# 5. Remaining Blocker B — Reconnect Exhaustion Must Reach Session State

## 5.1 Problem

The lower reconnect policy has a finite attempt budget, but SessionManager may never learn when that budget has been exhausted.

This can leave:

```text
session state = RECOVERING
member.recovering = true
managedReconnectRequested = true
```

forever.

## 5.2 Required New Lower-Level Signal

Expose an exhaustion notification.

Conceptual:

```cpp
void reconnectExhausted(
    DeviceId deviceId,
    int attempts,
    QString reason);
```

or repository-equivalent structured type.

If `ReconnectPolicy` already has an error/result path, extend that rather than inventing a parallel event system.

## 5.3 Required Propagation

Recommended flow:

```text
ReconnectPolicy
    |
    +--> reconnectExhausted
            |
            v
DeviceLifecycleManager / BluetoothManager
            |
            v
SessionManager
```

SessionManager then:

```text
clear managed reconnect request
clear member recovering flag
record RecoveryExhausted error
emit sessionError
recompute state
```

## 5.4 Required State Result

If another member remains healthy:

```text
RECOVERING
    |
attempts exhausted
    v
DEGRADED
```

If no usable member remains and no recovery path exists:

```text
RECOVERING
    |
attempts exhausted
    v
FAILED
```

Follow existing state semantics.

## 5.5 Mandatory Tests

Test:

```text
two-member ACTIVE session
member B disconnects
managed reconnect repeatedly fails
attempt limit reached
```

Assert:

```text
session does not remain RECOVERING forever
member recovery flag cleared
sessionError emitted with RecoveryExhausted
state becomes DEGRADED while A remains healthy
```

Also test all-members-unavailable case.

---

# 6. Remaining Blocker C — Stop/Deactivate Reentrancy

## 6.1 Problem

`AudioRouter::deactivateRoute()` may emit route state signals synchronously.

During:

```text
SessionManager::stopSession()
```

the route can emit:

```text
Deactivating
Inactive
Removed
```

while the outer stop operation is still executing.

If SessionManager responds by calling reconciliation or route teardown again, this may recursively manipulate the same route while the router still holds references to it.

This risks undefined behavior.

## 6.2 Required Invariant

While session state is:

```text
STOPPING
```

normal route callbacks must not initiate:

- route creation;
- route reactivation;
- route reconciliation;
- recursive stop;
- Bluetooth recovery.

The outer stop operation owns teardown.

## 6.3 Required Guard

Audit all session route-event callbacks.

Examples:

```text
handleRouteStateChanged
handleRouteRemoved
handleRouteFailed
```

Add explicit gating:

```cpp
if (session.state == SessionState::Stopping) {
    update only minimal runtime bookkeeping if necessary;
    return;
}
```

Likewise suppress route-producing behavior in:

```text
IDLE
FAILED
```

unless explicit retry/start has already changed state.

## 6.4 Mandatory Reentrancy Test

Create a fake or real test router where:

```cpp
deactivateRoute()
```

synchronously emits:

```text
Deactivating
Inactive
routeRemoved
```

Then:

```text
ACTIVE
deactivateSession()
```

must produce:

```text
STOPPING
route cleanup
IDLE
```

without:

- recursive stop;
- duplicate route removal;
- route recreation;
- crash;
- invalid iterator/reference;
- extra route count.

Run this test under ASan if project supports it.

---

# 7. Remaining Blocker D — RecoveryPolicy::None Route Reactivation Loophole

## 7.1 Problem

Policy `None` may correctly suppress *new* route creation after endpoint loss but still allow reactivation of an existing route object that transitions to:

```text
Inactive
Failed
Degraded
```

while source/destination still match.

This violates:

```text
RecoveryPolicy::None
```

because automatic route reactivation is still recovery.

## 7.2 Required Invariant

Outside initial activation:

```text
RecoveryPolicy::None
```

must suppress both:

```text
create missing route
reactivate existing inactive/failed route
```

unless the user explicitly calls:

```text
retrySession()
```

or restarts the session.

## 7.3 RoutingCoordinator Correction

Ensure `autoRestoreAllowed` or equivalent gates:

```text
route creation
AND
route activation/reactivation
```

when session is not in initial `STARTING`.

## 7.4 Mandatory Test

Scenario:

```text
session ACTIVE
policy = None
endpoint remains available
existing route becomes Inactive
```

Run reconciliation.

Assert:

```text
route remains inactive
no activateRoute() call
session DEGRADED or appropriate state
```

Then:

```text
retrySession()
```

and verify restoration occurs.

---

# 8. Remaining Issue E — JSON Parse Const-Cast Undefined Behavior

## 8.1 Problem

If code resembles:

```cpp
const QJsonParseError parseError;

QJsonDocument::fromJson(
    data,
    const_cast<QJsonParseError*>(&parseError));
```

this is undefined behavior because the originally-defined object is `const`.

## 8.2 Required Fix

Use:

```cpp
QJsonParseError parseError;

const QJsonDocument doc =
    QJsonDocument::fromJson(data, &parseError);
```

No `const_cast`.

## 8.3 Test

Existing malformed JSON tests should continue passing.

Add an assertion on parse error reporting/logging if useful.

---

# 9. Remaining Issue F — `createSession()` Persistence Failure Contract

## 9.1 Problem

Other mutation APIs may return:

```text
PersistenceFailure
```

but `createSession()` may return only an ID while persistence failure is delivered through `sessionError`.

This creates an inconsistent public command contract.

## 9.2 Preferred Correction

Use a structured create result.

Conceptual:

```cpp
struct CreateSessionResult {
    SessionCommandResult result;
    SessionId sessionId;
};
```

or project-equivalent generic result type.

Expected:

```text
success -> Accepted + valid ID
persistence failure -> PersistenceFailure + defined in-memory semantics
```

## 9.3 Backward Compatibility

If changing the public API would cause disproportionate churn, an acceptable alternative is:

```text
createSession(...)
```

returns empty/invalid ID on persistence failure and rolls back the in-memory session.

But do not silently report success.

## 9.4 Mandatory Test

Inject failing persistence.

Assert:

```text
create reports failure
sessionError emitted
in-memory state follows documented rollback/dirty-state semantics
```

---

# 10. Remaining Issue G — Mid-Session Source Loss/Return Test

The current tests may cover:

```text
source missing at activation
-> FAILED
-> source appears
-> still FAILED
-> retry
-> ACTIVE
```

That is good but insufficient.

Add a test for:

```text
ACTIVE
source disappears
```

Required semantics must be explicit.

Recommended:

```text
ACTIVE
source disappears
-> RECOVERING
routes become unavailable
source returns
routes reconcile
-> ACTIVE
```

If project semantics choose `FAILED`, then:

```text
source return alone must not restore routes
explicit retry required
```

The code, state machine, and tests must all match.

---

# 11. Re-audit Generation Safety

The prior pass added generation safety.

Verify every delayed/queued callback captures and validates the exact activation generation.

Search:

```bash
grep -R "operationGeneration\|generation" -n src/session include/auralis/session
```

For every asynchronous callback:

```text
timer
queued lambda
reconnect completion
route completion
endpoint return
source return
```

require:

```text
session exists
session still active
current generation == captured generation
member still exists
member enabled
policy still allows action
state allows action
```

Add any missing guard.

---

# 12. Re-audit Recovery Cancellation

Verify cancellation for:

```text
stop
delete
remove member
disable member
policy -> None
autoReconnect -> false
session switch
application shutdown
```

Do not rely only on `SessionManager` flags.

Where a lower managed reconnect timer already exists, make sure it is actually cancelled.

Add spies around:

```text
cancelManagedReconnect
ReconnectPolicy::cancelReconnect
```

or equivalent.

---

# 13. State Invariants to Re-Prove

After corrections, automated tests must establish:

```text
IDLE
-> no active session routes
-> no recovery intent
```

```text
STOPPING
-> no new route creation
-> no route activation
-> no reconnect
```

```text
FAILED
-> quiescent until explicit retry
```

```text
DEGRADED
-> healthy routes remain
```

```text
RECOVERING
-> at least one real recovery activity exists
```

```text
ACTIVE
-> all required enabled members healthy
```

---

# 14. Error Propagation

Add/verify error handling for:

```text
RecoveryExhausted
PersistenceFailure
RouteCreationFailed
RouteActivationFailed
SourceUnavailable
EndpointUnavailable
```

Errors should include context:

```text
session ID
device ID if relevant
route ID if relevant
human-readable reason
```

When recovery succeeds:

```text
clear stale member error
clear stale session-level recoverable error
```

---

# 15. Logging

Add structured events for the final fixes.

Examples:

```text
ManagedReconnectAlreadyScheduled
ManagedReconnectRequested
ManagedReconnectExhausted
SessionRecoveryExhausted
RouteRecoverySuppressedByPolicy
RouteCallbackIgnoredDuringStopping
StaleGenerationCallbackIgnored
SessionPersistenceCreateFailed
```

Avoid log spam.

---

# 16. Focused Regression Tests Required

At minimum, ensure the final test suite contains explicit coverage for:

1. one disconnect produces one managed reconnect schedule;
2. duplicate disconnect signals do not consume multiple attempts;
3. reconnect exhaustion transitions session out of RECOVERING;
4. stop/deactivate synchronous route callback reentrancy;
5. `RecoveryPolicy::None` suppresses existing-route reactivation;
6. persistence parse does not use const-cast;
7. createSession persistence failure is observable through the command API;
8. mid-session source loss and source return;
9. stale generation after stop/start remains ignored;
10. lower-level reconnect cancellation after policy change.

---

# 17. Integration Test — Real Production Path with Fake BlueZ

This is especially important.

Do not test only SessionManager against a fake DeviceRegistry.

Construct the production stack as far as practical:

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

Then simulate:

```text
Connected=true
Connected=false
```

Verify actual event ordering and retry scheduling.

This test should catch regressions that pure SessionManager fakes cannot.

---

# 18. Integration Test — Route Reentrancy

Construct:

```text
SessionManager
RoutingCoordinator
Fake/Real AudioRouter test double
```

where route teardown signals synchronously.

Assert:

```text
single stop
single teardown
zero recursion
zero reactivation
IDLE
```

If possible run under:

```text
ASan
UBSan
```

---

# 19. Full Regression Requirements

After targeted tests pass, run all existing tests.

```bash
ctest --test-dir build --output-on-failure
```

Do not weaken prior Phase 0–5 tests.

Verify:

- configuration;
- logging;
- Bluetooth discovery;
- pairing;
- device lifecycle;
- reconnect policy;
- PipeWire registry;
- endpoint resolver;
- AudioRouter;
- volume;
- session persistence;
- session routing;
- session recovery;
- desktop startup.

---

# 20. Clean Build

Final verification must use:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record exact totals.

---

# 21. Sanitizer Pass

If CMake exposes sanitizer options, run them.

If not, create a temporary local sanitizer build without permanently changing project policy.

Recommended:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

Focus on:

```text
stop/deactivate route reentrancy
session deletion
late callbacks
route container mutation
QObject lifetime
```

Do not claim sanitizer validation if the environment cannot support it.

---

# 22. Documentation

Update final Phase 6 docs only after validation.

Create/update:

```text
docs/validation/phase-6-final-closure.md
```

Include:

- every previously remaining issue;
- code correction;
- test covering it;
- exact test command;
- result;
- hardware validation status;
- final verdict.

---

# 23. Required Final Report

End the task with:

## A. Issues Fixed

For each remaining issue:

```text
issue
root cause
files changed
fix
test added
result
```

## B. Reconnect Ownership

Explain exactly why one disconnect cannot now schedule reconnect twice.

## C. Recovery Exhaustion

Explain the lower-to-upper signal path.

## D. Stop/Reentrancy Safety

Explain how synchronous router callbacks are prevented from recursive teardown.

## E. Policy None

Prove both missing-route creation and existing-route reactivation are suppressed.

## F. Persistence

Explain the parser fix and createSession failure semantics.

## G. Source Loss

Explain mid-session behavior.

## H. Test Results

Include:

```bash
ctest --test-dir build --output-on-failure
```

with exact output summary.

## I. Sanitizer Results

If run.

## J. Hardware Results

If run.

## K. Final Verdict

Use exactly one:

```text
PHASE 6 STATUS: COMPLETE
```

or:

```text
PHASE 6 STATUS: NOT COMPLETE
```

If not complete, enumerate blockers.

---

# 24. Final Completion Gate

Phase 6 can be marked complete only if:

- reconnect scheduling is single-owner/idempotent;
- reconnect exhaustion is observable and state-safe;
- stop teardown is reentrancy-safe;
- `RecoveryPolicy::None` blocks all automatic route recovery;
- persistence parser has no const-cast UB;
- create-session persistence failure has explicit command semantics;
- source loss/return is tested;
- all prior correction tests still pass;
- full Phase 0–6 regression passes;
- documentation matches reality.

Do not move to Phase 7 until all software gates pass.

Begin with the baseline audit, then make the smallest targeted changes necessary.
