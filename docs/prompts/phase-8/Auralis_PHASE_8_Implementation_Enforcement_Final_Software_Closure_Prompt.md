# AURALIS — PHASE 8 IMPLEMENTATION-ENFORCEMENT / FINAL SOFTWARE-CLOSURE PROMPT

**Project:** Auralis  
**Target:** Current repository only  
**Purpose:** Actually implement the remaining Phase 8 software-closure corrections.  
**Critical instruction:** Do not merely copy this prompt into `docs/`, summarize it, or update audit text. Modify production code and tests.

---

# 0. REQUIRED OUTCOME

The repository currently contains a Phase 8 remediation prompt but the last requested corrections were not actually applied.

Treat the current state as:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: REMEDIATION REQUIRED
```

Your task is to implement the remaining corrections and produce code/test evidence.

Do not start Phase 9.

Do not mark software PASS until every mandatory item below is resolved.

---

# 1. REPOSITORY BASELINE

Before changes:

```bash
git status --short
git branch --show-current
git rev-parse HEAD

rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record:

- commit;
- test count;
- passed;
- failed;
- skipped;
- hardware/live tests not run.

Preserve unrelated user changes.

---

# 2. BLOCKER — SYSTEM POWER MONITOR MUST SURVIVE A RUNTIME SYSTEM D-BUS RESTART

## Current failure

`SystemPowerMonitor` can retry after an initial subscription failure, but it does not invalidate an already-successful logind subscription when the system D-Bus disappears.

The production connection currently handles the positive bus transition but not the negative one.

Required state sequence:

```text
application starts
→ logind subscribed
→ system D-Bus goes DOWN
→ subscription becomes invalid
→ SystemPowerMonitor records subscribed = false
→ system D-Bus comes UP
→ fresh logind subscription created
→ PrepareForSleep monitoring restored
```

## Required API

Add a production method equivalent to:

```cpp
void notifySystemBusUnavailable();
void notifySystemBusAvailable();
```

Exact names may differ.

### On unavailable

- mark old logind subscription invalid;
- set `subscribed_ = false`;
- stop assuming callbacks will arrive;
- do not hammer the unavailable bus;
- leave monitor initialized.

### On available

- attempt fresh subscription;
- if successful, mark subscribed;
- if not, use existing bounded/low-frequency retry;
- avoid duplicate subscriptions.

## Production wiring

Wherever the application observes system bus state:

```cpp
systemBusStateChanged(bool connected)
```

wire both paths:

```text
connected == false → SystemPowerMonitor bus unavailable
connected == true  → SystemPowerMonitor bus available
```

## Tests

Add deterministic tests:

```text
runtimeBusLossInvalidatesExistingLogindSubscription
runtimeBusReturnResubscribesLogind
repeatedBusLossIsIdempotent
repeatedBusReturnDoesNotDuplicateSubscription
shutdownWhileResubscriptionPendingIsSafe
```

---

# 3. BLOCKER — GENERATION-FENCE ALL BLUEZ ASYNC CALLS

Snapshot fencing alone is insufficient.

Audit all asynchronous BlueZ operations, including:

```text
StartDiscovery
StopDiscovery
Pair
CancelPairing
Connect
Disconnect
Trusted property update
RemoveDevice
RegisterAgent
RequestDefaultAgent
UnregisterAgent
```

Every callback tied to a bus attachment must capture the current:

```cpp
busAttachGeneration_
```

At completion:

```cpp
if (!initialized_ || generation != busAttachGeneration_) {
    // stale result
    // dispose watcher
    // DO NOT emit success/failure into current lifecycle
    return;
}
```

Prefer a shared helper rather than repeating fragile code.

Examples of acceptable abstractions:

```text
watchCallForCurrentGeneration(...)
completeIfCurrentGeneration(...)
GenerationBoundPendingCall
```

The implementation must make future unfenced async calls difficult to add accidentally.

## Tests

At minimum cover representative operation classes:

```text
oldGenerationConnectCompletionIgnored
oldGenerationPairCompletionIgnored
oldGenerationTrustedCompletionIgnored
oldGenerationDiscoveryCompletionIgnored
oldGenerationAgentCompletionIgnored
asyncCompletionAfterShutdownIgnored
```

---

# 4. BLOCKER — GENERATION-FENCE BLUEZ SIGNAL SUBSCRIPTIONS

Audit:

```cpp
onInterfacesAdded(...)
onInterfacesRemoved(...)
onPropertiesChanged(...)
```

A queued signal from an obsolete system-bus attachment must not mutate the new Bluetooth model.

Use one clean design:

### Preferred design

Create a per-attachment subscription context carrying:

```text
generation
```

Destroy it when detaching.

Callbacks check/capture that generation before forwarding to `BlueZDbusClient`.

Alternative generation-aware signal adapter designs are acceptable.

Required invariant:

```text
old bus generation
→ queued InterfacesAdded/Removed/PropertiesChanged
→ ignored after a new generation is active
```

Tests:

```text
oldGenerationPropertiesSignalIgnored
oldGenerationInterfacesAddedIgnored
oldGenerationInterfacesRemovedIgnored
signalAfterShutdownIgnored
```

---

# 5. BLOCKER — STALE SNAPSHOT MUST NOT CLEAR CURRENT SNAPSHOT STATE

Current stale completion behavior must not do:

```cpp
snapshotInFlight_ = false;
```

when:

```cpp
generation != busAttachGeneration_
```

because a new-generation snapshot may already be running.

Required invariant:

```text
generation N snapshot completion
cannot modify generation N+1 snapshot bookkeeping
```

Prefer tracking both:

```text
snapshot generation
snapshot request token/id
```

if needed.

Only the exact active request may clear:

```text
snapshotInFlight_
snapshotInFlightGeneration_
```

Tests must exercise the real production completion path:

```text
staleSnapshotCompletionDoesNotClearCurrentSnapshot
currentSnapshotCompletionClearsCurrentSnapshot
staleCompletionCannotCreateParallelCurrentSnapshots
onePendingRefreshRunsAfterCurrentCompletion
snapshotCompletionAfterShutdownIsIgnored
```

---

# 6. BLOCKER — SESSION RECONCILIATION MUST BE DEPENDENCY-GATED

`RecoveryManager::runReconcile()` must not call:

```cpp
refreshActiveSession()
```

before dependencies are usable.

Required ordering:

```text
not shutting down
not suspended
auto recovery enabled
system bus connected
BlueZ available
PipeWire connected
PipeWire initial graph ready
↓
session reconciliation
```

If the active-session policy also requires another existing readiness flag, include it.

If dependencies are incomplete:

- do not call session refresh;
- remain in a truthful waiting/recovering state;
- allow the final dependency-ready transition to schedule one coalesced reconcile.

Tests:

```text
reconcileWaitsForSystemBus
reconcileWaitsForBlueZ
reconcileWaitsForPipeWireConnection
reconcileWaitsForPipeWireGraph
finalDependencyArrivalRunsExactlyOneReconcile
dependencyBurstCoalescesToExactlyOneReconcile
```

Correct any old test that expected session refresh while PipeWire was unavailable.

---

# 7. BLOCKER — PHASE 8 INTEGRATION HARNESS MUST EXERCISE SESSION + ROUTING

The current harness is still too shallow.

Build a reusable test fixture containing real internal managers:

```text
ConfigurationManager
RecoveryManager
BluetoothManager
PipeWireManager
PipeWireObjectStore
EndpointResolver
SessionManager
RoutingCoordinator
AudioRouter / current production route implementation
```

Fake only external boundaries:

```text
BlueZ / D-Bus
PipeWire daemon events
physical Bluetooth hardware
```

Do not fake the internal recovery logic being tested.

---

# 8. MANDATORY STABLE-ENDPOINT REBIND TEST

Construct:

```text
stable endpoint = headset-A
old PipeWire node ID = 42
active route/session uses headset-A
```

Simulate:

```text
PipeWire generation destroyed
old node 42 removed
new PipeWire generation appears
same stable endpoint = headset-A
new node ID = 108
InitialSyncDone
```

Assert exactly:

```text
old node 42 not reused
stable endpoint resolves to node 108
route uses node 108
route restoration occurs exactly once
late old-generation events cannot overwrite node 108
```

Do not merely call an existing low-level `rebind()` unit test; the Phase 8 harness must drive recovery through real managers.

---

# 9. MANDATORY MULTI-DEVICE MEMBER CHURN TEST

Create a session with:

```text
device A
device B
```

Then:

```text
B disappears
A remains valid
```

Assert behavior matches the existing Phase 6 recovery policy:

- A is preserved where policy permits;
- B becomes unavailable;
- session is not unnecessarily destroyed;
- B returning re-resolves stable identity correctly;
- no duplicate route/session entry appears.

---

# 10. REMOVE ALL TEST BYPASSES

The current harness must contain no unconditional logic such as:

```cpp
pipeWire.connected() || true
```

Search:

```bash
rg -n "\|\| true" tests
```

Remove all such false-pass bypasses.

Do not replace with another constant.

Use actual fake-backend/manager state.

---

# 11. EXACT COALESCING ASSERTIONS

Tests named “coalesces to one” must assert one.

Replace weak logic such as:

```cpp
QVERIFY(sessionRefresh >= 1);
QVERIFY(sessionRefresh <= 3);
```

with:

```cpp
QCOMPARE(sessionRefresh, 1);
```

where exactly one is the design contract.

Combined service bounce must test multiple event orderings:

```text
BlueZ down → PipeWire down → BlueZ up → PW graph ready
PW down → BlueZ down → PW connected → BlueZ up → PW graph ready
nearly simultaneous return
```

For every ordering:

```text
session reconciliation count == 1
```

after the final dependency is usable.

---

# 12. BLUEZ RETURN MUST PROVE EXACTLY ONE SNAPSHOT

A test like:

```cpp
QVERIFY(snapshotRequests >= snapshotsBefore);
```

is invalid because zero new snapshots passes.

The integration test must prove:

```cpp
QCOMPARE(snapshotRequests, snapshotsBefore + 1);
```

or the exact count defined by production semantics.

The fake must model the actual production service-registration transition, not merely emit an availability Boolean.

Prefer testing through the real `BlueZDbusClient` state machine with a fake external bus backend.

---

# 13. HARDWARE-INDEPENDENT D-BUS TESTS MUST NOT TOUCH HOST BLUEZ

Current tests must not accidentally create real:

```text
QDBusServiceWatcher
ObjectManager subscriptions
PropertiesChanged subscriptions
```

against the developer/CI host.

Introduce/finish a narrow boundary such as:

```text
ISystemBusBackend
IBlueZBusBackend
BlueZDbusTransport
```

Production implementation:

```text
Qt/real system bus
```

Test implementation:

```text
fully fake
```

The fake must control:

```text
bus connected/disconnected
BlueZ service present/absent
watcher attach/detach counters
signal-subscription attach/detach counters
snapshot request/completion
delayed callback
queued interface/property signal
```

Required test guarantee:

```text
tests pass identically whether host BlueZ is running or not
```

No root privileges.

No dependence on physical Bluetooth hardware.

---

# 14. STRESS ENVIRONMENT CONTRACT

Fix stress controls.

Do not interpret:

```bash
AURALIS_RUN_STRESS=1
```

as “run one iteration.”

Required semantics:

```text
no variables
→ normal deterministic default, e.g. 100

AURALIS_RUN_STRESS=1
→ extended deterministic default, e.g. 1000

AURALIS_RUN_STRESS=1
AURALIS_STRESS_ITERATIONS=10000
→ exactly 10000
```

Add:

```text
AURALIS_STRESS_ITERATIONS
```

Clamp/validate invalid or unreasonable values safely.

Print actual iteration count once.

---

# 15. STRESS TESTS

Keep existing stress tests and strengthen them.

Required categories:

```text
D-Bus/BlueZ churn
PipeWire graph-generation churn
suspend/resume churn
session persistence save/load
logger rotation/retention
```

Normal CI should remain reasonable.

Extended stress:

```bash
AURALIS_RUN_STRESS=1 \
AURALIS_STRESS_ITERATIONS=1000 \
ctest --test-dir build -L stress --output-on-failure
```

Optional deep run:

```bash
AURALIS_RUN_STRESS=1 \
AURALIS_STRESS_ITERATIONS=10000 \
ctest --test-dir build -L stress --output-on-failure
```

Only report runs actually executed.

---

# 16. LOGGER RETENTION STRESS

The current assertion:

```text
active file exists OR .1 exists
```

is not enough.

Configure a small deterministic max size and:

```text
retention = 3
```

Force at least five rotations.

Assert:

```text
active log exists
.1 exists
.2 exists
.3 exists
.4 does NOT exist
```

Then write additional content and prove active log remains writable.

Where practical add unique content markers so rotation ordering can be checked.

Also test:

```text
retention = 1
```

and assert no `.2`.

If retention zero is legal, test it.
If not, verify normalization/rejection.

---

# 17. SHUTDOWN / STALE WORK REGRESSION

After changes test:

```text
shutdown with old BlueZ async operation pending
shutdown with snapshot pending
shutdown during D-Bus reattach
shutdown during logind resubscription
shutdown with recovery coalesce pending
shutdown with PipeWire graph-sync timeout pending
```

After shutdown:

```text
no timer restarts
no session reconcile
no Bluetooth model mutation
no operation-completed signals from obsolete generations
```

---

# 18. REQUIRED TEST INVENTORY

Before PASS, the suite must contain tests equivalent to:

## Power

```text
runtimeBusLossInvalidatesLogindSubscription
runtimeBusReturnResubscribesLogind
duplicateBusTransitionsIdempotent
```

## BlueZ async

```text
oldAsyncOperationCompletionIgnored
oldQueuedPropertiesSignalIgnored
oldQueuedInterfacesSignalIgnored
asyncCompletionAfterShutdownIgnored
```

## Snapshot

```text
staleSnapshotCannotClearCurrentInFlightState
snapshotRequestsCoalesce
oldSnapshotIgnored
snapshotAfterShutdownIgnored
```

## Recovery dependency ordering

```text
sessionReconcileWaitsForBlueZ
sessionReconcileWaitsForPipeWireConnection
sessionReconcileWaitsForPipeWireGraph
finalDependencyRunsOneReconcile
```

## Cross-layer

```text
pipeWireNodeIdChangeRebindsStableEndpoint
routeRestoredExactlyOnce
combinedBlueZPipeWireBounceReconcilesExactlyOnce
blueZReturnRequestsExactlyOneFreshSnapshot
multiDeviceMemberChurnPreservesValidPeer
```

## Stress

```text
blueZDbusChurn
pipeWireGraphChurn
suspendResumeChurn
sessionPersistenceLoop
loggerRetentionRotation
```

---

# 19. TEST QUALITY GATE

Search:

```bash
rg -n "\|\| true" tests
rg -n "snapshotRequests.*>=" tests
rg -n "sessionRefresh.*>=" tests
```

Review all weak assertions.

A test must fail if the named behavior never happened.

---

# 20. NORMAL VALIDATION

Run:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Then targeted:

```bash
ctest --test-dir build -R BlueZ --output-on-failure -V
ctest --test-dir build -R Recovery --output-on-failure -V
ctest --test-dir build -R Power --output-on-failure -V
ctest --test-dir build -R PipeWire --output-on-failure -V
ctest --test-dir build -R Phase8 --output-on-failure -V
ctest --test-dir build -R Session --output-on-failure -V
ctest --test-dir build -R Logger --output-on-failure -V
ctest --test-dir build -L stress --output-on-failure
```

---

# 21. SANITIZERS

Use the existing sanitizer option.

```bash
rm -rf build-asan

cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAURALIS_ENABLE_SANITIZERS=ON

cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

Run critical Phase 8 integration/stress tests under sanitizers as well.

Any unresolved ASan/UBSan defect blocks software PASS.

---

# 22. PACKAGE REVALIDATION

Do not redesign packaging.

Run:

```bash
cpack --config build/CPackConfig.cmake
dpkg-deb -I <generated-package>.deb
dpkg-deb -c <generated-package>.deb
```

Verify:

```text
binary
desktop file
AppStream metadata
icon
LICENSE
```

The repository may truthfully remain:

```text
PUBLIC PACKAGE CONTACT: PENDING OWNER INPUT
PUBLIC DISTRIBUTION: BLOCKED UNTIL OWNER SELECTS LICENSING TERMS
```

Do not invent an email or choose a license.

These owner decisions do not block the software engineering gate if documented honestly.

---

# 23. UPDATE AUDITS LAST

Only after implementation and validation, update:

```text
docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
README.md
```

The final audit must explicitly reference this implementation-enforcement prompt and the final software-closure remediation contract.

Use evidence categories:

```text
IMPLEMENTED
AUTOMATED PASS
STRESS PASS
SANITIZER PASS
ACTUAL SERVICE TEST NOT RUN
ACTUAL HARDWARE TEST NOT RUN
OWNER INPUT PENDING
```

Do not claim hardware PASS from mocks.

---

# 24. HARD ACCEPTANCE CHECKLIST

Before saying software PASS:

- [ ] runtime bus loss invalidates logind subscription;
- [ ] runtime bus return re-subscribes logind;
- [ ] all relevant BlueZ async completions are generation-fenced;
- [ ] old queued BlueZ property/interface callbacks are generation-fenced;
- [ ] stale snapshot cannot clear current snapshot state;
- [ ] session reconciliation occurs only after dependencies are ready;
- [ ] Phase 8 harness instantiates real session/routing components;
- [ ] stable endpoint old node `42` → new node `108` rebind is tested;
- [ ] route restoration count is exactly one;
- [ ] no `|| true` bypass exists;
- [ ] combined recovery reconciliation is exactly one;
- [ ] BlueZ return snapshot increment is exactly one;
- [ ] D-Bus unit/integration tests do not touch host BlueZ;
- [ ] `AURALIS_STRESS_ITERATIONS` exists and works;
- [ ] logger retention proves `.1/.2/.3` and absence of `.4`;
- [ ] normal CTest is green;
- [ ] stress is green;
- [ ] sanitizer build/tests are green;
- [ ] Phase 0–7 regressions remain green;
- [ ] audit is truthful.

---

# 25. FINAL EXIT STATE

If all software items pass and hardware has not yet been run:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

If any required software item remains:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: REMEDIATION REQUIRED
```

Never say `PHASE 8 COMPLETE` until real hardware validation has passed.

---

# 26. REQUIRED FINAL RESPONSE FROM CURSOR

Return:

## Files changed

Group by subsystem.

## Every blocker

For each:

```text
root cause
production fix
test proving it
result
```

## Exact integration evidence

Report:

```text
old PipeWire node ID
new PipeWire node ID
route restoration count
combined recovery reconcile count
BlueZ snapshot increment
```

## Test totals

```text
normal
targeted
stress
extended stress
sanitizer
```

## Package evidence

Report generated package and installed entries.

## Pending external items

List:

```text
owner contact/license
hardware validation
```

## Final gate

End with one truthful gate state.

---

# 27. FINAL COMMAND

Implement the code now.

Do not merely copy this document into the repository.

Do not stop at analysis.

Do not update PASS text before production code and tests prove PASS.
