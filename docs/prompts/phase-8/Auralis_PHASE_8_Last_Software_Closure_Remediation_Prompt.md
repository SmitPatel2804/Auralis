# AURALIS — PHASE 8 LAST SOFTWARE-CLOSURE REMEDIATION PROMPT

**Project:** Auralis  
**Phase:** Phase 8 — Reliability, Testing and Production Hardening  
**Purpose:** Resolve the final software-level blockers found in the latest independent audit so that the Phase 8 **software exit gate** can be legitimately marked PASS before actual-machine/hardware validation.  
**Target environment:** Cursor AI IDE running against the current Auralis repository on the Linux development machine.

---

# 0. IMPORTANT STATUS

Phases 0–7 are already implemented.

Almost all Phase 8 functionality is now implemented as well.

The current codebase already contains and should preserve:

- `RecoveryManager`
- runtime system D-Bus health polling
- BlueZ service watching
- snapshot generation tagging
- snapshot coalescing
- PipeWire reconnect retry ownership
- PipeWire `InitialSyncDone` timeout
- bounded retry exhaustion
- central retry/exhaustion diagnostics
- `autoRecoverServices` wiring
- suspend/resume recovery
- duplicate suspend/resume suppression
- logind retry after initial startup failure
- persistence stress tests
- BlueZ/PipeWire/suspend stress foundations
- ASan/UBSan build support
- `.deb` packaging
- icon installation
- LICENSE installation
- truthful “contact pending” package metadata

Do not rewrite those systems from scratch.

The current independent verdict is still:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: FINAL SOFTWARE REMEDIATION REQUIRED
```

The remaining software blockers are now narrow and specific.

---

# 1. REMAINING BLOCKERS TO FIX

The current repository still has the following software-level issues:

1. `SystemPowerMonitor` does not invalidate/re-subscribe its logind signal subscription after a **runtime** system D-Bus restart.
2. BlueZ generation fencing only protects snapshot completion, not all asynchronous D-Bus operations.
3. queued BlueZ D-Bus signal callbacks are not adequately protected against obsolete bus generations.
4. stale snapshot completion can incorrectly clear `snapshotInFlight_` for a newer/current generation.
5. `RecoveryManager::runReconcile()` invokes session refresh before verifying all dependencies are usable.
6. the Phase 8 “real-manager” integration harness is still missing real `SessionManager`/routing/stable-endpoint rebind coverage.
7. one combined recovery test contains an unconditional truth bypass similar to `pipeWire.connected() || true`.
8. several integration assertions prove “something happened” instead of proving exact invariants.
9. BlueZ daemon-return integration coverage does not prove exactly one new snapshot.
10. the stress iteration environment-variable contract is incorrect.
11. logger rotation stress assertions are too weak to prove retention behavior.
12. hardware-independent D-Bus tests may still interact with the host system bus/BlueZ.
13. the final audit must remain truthful about public-release metadata and hardware validation.

This prompt addresses only these remaining items.

---

# 2. FIRST STEP — REPOSITORY RECONNAISSANCE

Before changing code, run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
```

Do not discard unrelated changes.

Then inspect at minimum:

```text
src/recovery/SystemPowerMonitor.cpp
include/auralis/recovery/SystemPowerMonitor.h

src/recovery/RecoveryManager.cpp
include/auralis/recovery/RecoveryManager.h

src/bluetooth/BlueZDbusClient.cpp
include/auralis/bluetooth/BlueZDbusClient.h

src/bluetooth/BluetoothManager.cpp
include/auralis/bluetooth/BluetoothManager.h

src/audio/PipeWireManager.cpp
include/auralis/audio/PipeWireManager.h

src/audio/PipeWireConnection.cpp
include/auralis/audio/PipeWireConnection.h

src/session/SessionManager.cpp
include/auralis/session/SessionManager.h

src/session/SessionPersistence.cpp

src/audio/AudioRouter.cpp
src/audio/EndpointResolver.cpp
src/audio/PipeWireObjectStore.cpp

src/core/ApplicationCore.cpp
apps/desktop/DesktopApplication.cpp

tests/unit/recovery/tst_SystemPowerMonitor.cpp
tests/unit/recovery/tst_RecoveryManager.cpp

tests/unit/bluetooth/tst_BlueZDbusClientBusRecovery.cpp
tests/unit/bluetooth/tst_BluetoothManager.cpp

tests/integration/tst_Phase8RecoveryHarness.cpp
tests/integration/tst_ServiceRecoveryIntegration.cpp

tests/stress/tst_Phase8Stress.cpp
tests/unit/core/tst_Logger.cpp

tests/CMakeLists.txt

docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
README.md
```

Search globally:

```bash
rg -n "notifySystemBusAvailable|notifySystemBusUnavailable|subscribed_|trySubscribeLogind"
rg -n "busAttachGeneration|QDBusPendingCallWatcher|watchCall|requestSnapshot"
rg -n "onInterfacesAdded|onInterfacesRemoved|onPropertiesChanged"
rg -n "snapshotInFlight_|snapshotInFlightGeneration|pendingSnapshot"
rg -n "runReconcile|refreshActiveSession|pipeWireGraphReady|blueZAvailable"
rg -n "\|\| true"
rg -n "AURALIS_RUN_STRESS|AURALIS_STRESS_ITERATIONS"
rg -n "rotat|retention|\\.1|\\.2|\\.3" tests src
```

---

# 3. BASELINE

Before editing:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Record:

```text
registered
passed
failed
skipped
```

Also note any live/hardware tests skipped.

Do not count a skipped live test as PASS.

---

# 4. BLOCKER A — RUNTIME LOGIND RE-SUBSCRIPTION AFTER SYSTEM D-BUS RESTART

## 4.1 Current defect

`SystemPowerMonitor` can recover if logind/system D-Bus is unavailable at startup.

However, if the monitor previously subscribed successfully:

```text
Auralis starts
→ logind subscription succeeds
→ subscribed_ = true

system D-Bus restarts
→ D-Bus signal subscription is destroyed externally

SystemPowerMonitor still believes:
subscribed_ = true
```

Then when the bus comes back, the existing method likely does:

```cpp
if (!initialized_ || subscribed_) {
    return;
}
```

and therefore never re-subscribes.

This is a production bug.

---

# 5. REQUIRED POWER-MONITOR STATE MODEL

`SystemPowerMonitor` must distinguish:

```text
initialized
system bus available
logind subscribed
shutting down
```

Do not equate:

```text
subscribed_ == true
```

with “subscription is guaranteed to still exist after a D-Bus restart.”

---

# 6. ADD BUS-LOSS NOTIFICATION

Add an explicit API such as:

```cpp
void notifySystemBusUnavailable();
void notifySystemBusAvailable();
```

Names may differ, but semantics must be explicit.

On bus loss:

```text
mark logind subscription invalid
subscribed_ = false
stop relying on old D-Bus connection
prepare for retry/re-subscription
```

Do not repeatedly spam retries while the bus is known unavailable.

On bus return:

```text
attempt fresh logind subscription
if success:
    subscribed_ = true
    stop retry timer
else:
    start low-frequency retry
```

---

# 7. WIRE BOTH BUS TRANSITIONS

Where the production application currently observes:

```text
BlueZDbusClient::systemBusStateChanged(bool)
```

or equivalent, wire:

```text
false → power monitor bus-unavailable
true  → power monitor bus-available
```

Do not notify only the `true` transition.

---

# 8. REQUIRED POWER-MONITOR TESTS

Add deterministic tests equivalent to:

```text
runtimeBusLossInvalidatesExistingLogindSubscription
runtimeBusReturnResubscribesLogind
runtimeBusReturnDoesNotDuplicateSubscription
repeatedBusDownIsIdempotent
repeatedBusUpIsIdempotent
shutdownDuringSubscriptionRetryIsSafe
```

Important scenario:

```text
subscription succeeds
→ simulate bus loss
→ subscribed becomes false
→ simulate bus return
→ subscription attempt occurs exactly once
→ success
```

This must not merely test initial-startup failure.

---

# 9. BLOCKER B — GENERATION-FENCE ALL ASYNCHRONOUS BLUEZ OPERATIONS

## 9.1 Current state

`GetManagedObjects` snapshots already carry a bus generation.

That is good.

But other async operations still reportedly use raw `QDBusPendingCallWatcher` callbacks without verifying that the callback belongs to the current system-bus attachment.

Examples may include:

```text
startDiscovery
stopDiscovery
pairDevice
cancelPairing
connectDevice
disconnectDevice
setDeviceTrusted
removeDevice
registerAgent
requestDefaultAgent
unregisterAgent
```

Every async operation that can outlive a D-Bus restart must be fenced.

---

# 10. REQUIRED ASYNC GENERATION CONTRACT

For every async request tied to the current D-Bus attachment:

```cpp
const quint64 generation = busAttachGeneration_;
```

Capture that generation with the request.

When the callback runs:

```cpp
if (!initialized_) {
    ignore callback;
    dispose watcher;
    return;
}

if (generation != busAttachGeneration_) {
    log stale callback rejection;
    dispose watcher;
    return;
}
```

Only current-generation callbacks may:

- emit success/failure signals;
- mutate discovery state;
- mutate device lifecycle state;
- update trust/pair state;
- update agent state;
- trigger upper-layer recovery.

---

# 11. CENTRALIZE THE PATTERN

Do not duplicate 15 subtly different generation checks.

Prefer a helper abstraction.

Possible patterns:

```text
watchCallWithGeneration(...)
isCurrentBusGeneration(...)
attachGenerationToWatcher(...)
```

or a small context object.

The final architecture should make it difficult to accidentally add an unfenced async D-Bus call later.

---

# 12. QUEUED D-BUS SIGNAL CALLBACKS

Audit:

```cpp
onInterfacesAdded(...)
onInterfacesRemoved(...)
onPropertiesChanged(...)
```

The system bus may restart while Qt still has queued events from the old connection.

The current bus generation should protect model mutation from stale queued events.

Possible safe designs:

## Option A

Route each subscription through an object bound to a particular generation.

When bus generation changes:

```text
destroy old subscription context
```

Old queued callbacks target destroyed context and cannot mutate the client.

## Option B

Capture generation when connecting subscriptions and compare in the callback.

## Option C

Use a generation-aware adapter object that emits only if current.

Choose a clean Qt-safe implementation.

---

# 13. REQUIRED BLUEZ ASYNC TESTS

Add tests equivalent to:

```text
oldGenerationConnectCompletionIgnored
oldGenerationPairCompletionIgnored
oldGenerationTrustCompletionIgnored
oldGenerationDiscoveryCompletionIgnored
oldGenerationAgentCompletionIgnored
oldGenerationPropertiesSignalIgnored
oldGenerationInterfacesAddedIgnored
oldGenerationInterfacesRemovedIgnored
asyncCompletionAfterShutdownIgnored
```

You do not need one test for every trivial method if a common helper is proven comprehensively, but ensure all categories are covered.

---

# 14. BLOCKER C — FIX STALE SNAPSHOT COMPLETION BOOKKEEPING

## 14.1 Current bug

The stale snapshot branch reportedly does:

```cpp
if (!initialized_ || generation != busAttachGeneration_) {
    snapshotInFlight_ = false;
    return;
}
```

This is unsafe.

Example:

```text
generation 20 snapshot A in flight
bus restarts
generation 21 snapshot B starts
snapshotInFlight_ = true for B
old snapshot A completes
generation mismatch
stale callback sets snapshotInFlight_ = false
snapshot B is still active
```

Then another refresh can launch snapshot C concurrently.

---

# 15. REQUIRED FIX

The stale callback must not modify current-generation snapshot bookkeeping.

Conceptually:

```cpp
if (!initialized_) {
    return;
}

if (generation != busAttachGeneration_) {
    return;
}
```

Only if the completing watcher belongs to the **current active snapshot generation/request** may it clear:

```text
snapshotInFlight_
snapshotInFlightGeneration_
```

If necessary, track a request token as well as generation.

For example:

```text
generation + monotonically increasing snapshot request id
```

This is stronger than a single Boolean.

---

# 16. REQUIRED SNAPSHOT TESTS

Add tests:

```text
staleSnapshotCompletionDoesNotClearCurrentInFlightState
currentSnapshotCompletionClearsInFlightState
staleSnapshotThenRefreshDoesNotCreateParallelCurrentSnapshots
pendingRefreshRunsOnceAfterCurrentSnapshotCompletes
shutdownMakesAllSnapshotCompletionsNoOp
```

The stale-completion test must exercise the same production completion path, not bypass it through a simplified helper that cannot reproduce the bug.

---

# 17. BLOCKER D — DO NOT RECONCILE SESSION BEFORE DEPENDENCIES ARE READY

## 17.1 Current defect

`RecoveryManager::runReconcile()` reportedly performs:

```cpp
if (hooks_.refreshActiveSession) {
    hooks_.refreshActiveSession();
}
```

before checking:

```text
BlueZ available?
PipeWire connected?
PipeWire graph ready?
```

This violates dependency ordering.

The session should not be restored against an incomplete graph.

---

# 18. REQUIRED DEPENDENCY GATE

Before invoking session refresh/reconciliation, check all dependencies required by the existing session/routing architecture.

At minimum:

```text
system bus connected
BlueZ available
PipeWire connected
PipeWire graph ready
not suspended
not shutting down
auto recovery enabled
```

If adapter readiness is also required for the active session, include the appropriate existing condition.

---

# 19. WAITING BEHAVIOR

If dependencies are not ready:

```text
do not refresh session
remain Recovering / Waiting / Degraded as semantically appropriate
```

Then when the final dependency becomes ready:

```text
schedule one coalesced reconciliation
```

Do not repeatedly call session refresh while waiting.

---

# 20. REQUIRED RECOVERYMANAGER TESTS

Correct any tests that currently expect refresh while PipeWire is unavailable.

Add:

```text
reconcileDoesNotRunBeforePipeWireConnected
reconcileDoesNotRunBeforePipeWireGraphReady
reconcileDoesNotRunBeforeBlueZAvailable
reconcileRunsExactlyOnceWhenFinalDependencyBecomesReady
combinedDependencyBurstCoalescesToOneSessionRefresh
```

Assertions should prefer:

```cpp
QCOMPARE(sessionRefresh, 1);
```

where exact-one behavior is the contract.

Avoid:

```cpp
QVERIFY(sessionRefresh >= 1);
QVERIFY(sessionRefresh <= 3);
```

unless multiple executions are genuinely allowed by design.

---

# 21. BLOCKER E — REBUILD THE PHASE 8 INTEGRATION HARNESS INTO A TRUE CROSS-LAYER TEST

## 21.1 Current state

The current harness now uses more production managers than before, which is good.

But it still does not sufficiently exercise:

```text
SessionManager
EndpointResolver
RoutingCoordinator
AudioRouter
stable endpoint identity rebind
route recreation after PipeWire graph replacement
```

That is still a major Phase 8 requirement.

---

# 22. REQUIRED REAL-MANAGER HARNESS

Build one reusable integration fixture containing real internal components, for example:

```text
ConfigurationManager
RecoveryManager
BluetoothManager
fake BlueZ transport/client
PipeWireManager
fake PipeWire connection/backend
PipeWireObjectStore
EndpointResolver
SessionManager
RoutingCoordinator
AudioRouter or its current production equivalent
```

Do not fake internal recovery logic that should be tested.

Only fake external boundaries:

```text
D-Bus/BlueZ external service
PipeWire external daemon/graph events
actual Bluetooth hardware
```

---

# 23. STABLE ENDPOINT IDENTITY REBIND TEST

This test is mandatory.

Construct initial state:

```text
stable endpoint identity = headset-A
old PipeWire node id = 42
active session/route references headset-A
```

Then simulate a full PipeWire rebuild:

```text
old graph disappears
generation advances
new graph appears
same endpoint stable identity = headset-A
new node id = 108
InitialSyncDone
```

Assert:

```text
node 42 is not reused
stable identity resolves to node 108
route/session is recreated against node 108
route recreated exactly once
old-generation events cannot overwrite new route
```

---

# 24. SESSION MEMBER CHURN TEST

For a multi-device session:

```text
A + B active
B disappears
A remains valid
```

Assert according to existing Phase 6 session policy:

```text
A remains valid where policy permits
B is treated as unavailable
session is not unnecessarily destroyed
```

Then B returns and stable identity resolves correctly.

---

# 25. COMBINED SERVICE BOUNCE TEST

Fix any bypass such as:

```cpp
pipeWire.connected() || true
```

Remove all unconditional-truth logic.

Use the actual fake PipeWire manager/backend state.

Run multiple orderings:

```text
BlueZ down → PipeWire down → BlueZ up → PW graph ready
PipeWire down → BlueZ down → PW transport up → BlueZ up → graph ready
```

Assert:

```text
session refresh == exactly 1
```

after all required dependencies become usable.

---

# 26. BLUEZ RETURN TEST MUST PROVE EXACTLY ONE FRESH SNAPSHOT

If the test records:

```cpp
int snapshotsBefore = fake->snapshotRequests();
```

then after BlueZ returns, assert:

```cpp
QCOMPARE(fake->snapshotRequests(), snapshotsBefore + 1);
```

or the exact correct count.

Do not use:

```cpp
>= snapshotsBefore
```

because that can pass even if zero new snapshots occurred.

The fake must model production semantics accurately enough that:

```text
BlueZ registered
→ one authoritative snapshot
```

is exercised.

---

# 27. TEST THE ACTUAL PRODUCTION BLUEZ RETURN PATH

If `FakeBlueZClient::setBlueZAvailable(true)` does not trigger the same snapshot behavior as production, improve the test seam.

Possible approaches:

- expose a production-level `simulateServiceRegistered()` seam;
- use a fake bus backend underneath real `BlueZDbusClient`;
- factor registration transition handling into a testable method.

Do not duplicate production semantics manually in every test.

---

# 28. BLOCKER F — REMOVE WEAK ASSERTIONS AND FALSE-POSITIVE TESTS

Audit the entire new Phase 8 harness for:

```text
|| true
>= oldValue
<= 3
QVERIFY(counter >= 0)
```

or equivalent assertions that can pass without the intended event occurring.

Every test name should prove its named behavior.

Examples:

```text
"coalescesToOne"
→ assert exactly 1

"requestsSnapshot"
→ assert exactly +1

"exhausts"
→ assert Exhausted exactly once

"rebindsStableIdentity"
→ assert old node != current node and new node is selected
```

---

# 29. BLOCKER G — FIX STRESS ITERATION CONFIGURATION

## 29.1 Current defect

The stress helper reportedly interprets:

```cpp
AURALIS_RUN_STRESS
```

as the number of iterations.

Therefore:

```bash
AURALIS_RUN_STRESS=1
```

runs only one iteration.

That is backwards.

---

# 30. REQUIRED STRESS ENV CONTRACT

Use:

```bash
AURALIS_RUN_STRESS=1
```

as a Boolean enable flag.

Use:

```bash
AURALIS_STRESS_ITERATIONS=10000
```

for iteration count.

Recommended behavior:

```text
normal test run:
100 iterations

AURALIS_RUN_STRESS=1:
1000 or documented extended default

AURALIS_RUN_STRESS=1
AURALIS_STRESS_ITERATIONS=10000:
10000 iterations
```

Clamp unreasonable values if necessary.

Log the actual iteration count once.

---

# 31. REQUIRED STRESS-CONFIG TEST

If the helper can be unit-tested, verify:

```text
no env → default
RUN_STRESS=1 → extended default
STRESS_ITERATIONS=N → N
invalid iteration value → safe fallback
```

Otherwise cover via the stress executable itself.

---

# 32. BLOCKER H — STRENGTHEN LOGGER ROTATION STRESS

## 32.1 Current weakness

Current logger stress reportedly only verifies:

```text
active file exists OR .1 exists
```

after many writes.

That does not prove retention.

---

# 33. REQUIRED LOGGER RETENTION TEST

Configure a small maximum log size and retention count, for example:

```text
maxBytes = small deterministic value
retentionCount = 3
```

Force enough writes for 5+ rotations.

Assert:

```text
active log exists
.1 exists
.2 exists
.3 exists
.4 does NOT exist
```

Then write again and confirm the active log remains writable.

Where practical, verify file modification order or content markers:

```text
newest rotated data in .1
older data in .2
oldest retained in .3
```

---

# 34. LOGGER BOUNDARY CASES

Test at least:

```text
retention = 1
multiple rotations
active file remains available
no .2 retained
```

If retention zero is supported, test it.

If not supported, test configuration normalization/rejection.

Also test:

```text
rotation rename/delete failure
```

if current logger exposes a testable seam.

The application must not crash.

---

# 35. BLOCKER I — MAKE HARDWARE-INDEPENDENT D-BUS TESTS ACTUALLY HARDWARE-INDEPENDENT

## 35.1 Current issue

Even with a bus-health override, the real `BlueZDbusClient` may still attach a real:

```text
QDBusServiceWatcher
ObjectManager subscription
PropertiesChanged subscription
```

when the host system bus happens to be available.

That makes CI behavior dependent on the host.

---

# 36. REQUIRED EXTERNAL-BUS SEAM

Create or complete a narrow testable abstraction around external D-Bus behavior.

The production class should be able to use:

```text
RealSystemBusBackend
```

while tests use:

```text
FakeSystemBusBackend
```

The fake should support:

```text
connected true/false
BlueZ service present/absent
subscription attach count
subscription detach count
snapshot request
delayed completion
queued properties/interfaces events
```

Do not abstract all of Qt D-Bus if unnecessary.

Keep the seam as small as possible.

---

# 37. REQUIRED DETERMINISM ASSERTIONS

In test mode:

```text
no request should reach the real host system bus
no dependence on whether host BlueZ is running
no dependency on root privileges
```

Add counters to prove:

```text
watcher created once per generation
subscriptions attached once
subscriptions removed once
```

---

# 38. SHUTDOWN SAFETY RECHECK

After all generation and bus work changes, rerun shutdown tests.

At minimum:

```text
shutdown while old async BlueZ method pending
shutdown while snapshot pending
shutdown while bus reattach pending
shutdown while logind re-subscription pending
shutdown while RecoveryManager reconcile pending
shutdown while PipeWire InitialSync timeout pending
```

After shutdown:

```text
no callback may mutate state
no retry timer may restart
no session refresh
no BlueZ model repopulation
```

---

# 39. NORMAL TEST MATRIX

After corrections:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Then run targeted tests:

```bash
ctest --test-dir build -R BlueZ --output-on-failure -V
ctest --test-dir build -R Recovery --output-on-failure -V
ctest --test-dir build -R Power --output-on-failure -V
ctest --test-dir build -R PipeWire --output-on-failure -V
ctest --test-dir build -R Phase8 --output-on-failure -V
ctest --test-dir build -R Session --output-on-failure -V
ctest --test-dir build -R Logger --output-on-failure -V
```

Adjust regexes to actual names.

---

# 40. STRESS VALIDATION

Run normal stress:

```bash
ctest --test-dir build -L stress --output-on-failure
```

Then extended stress:

```bash
AURALIS_RUN_STRESS=1 \
AURALIS_STRESS_ITERATIONS=1000 \
ctest --test-dir build -L stress --output-on-failure
```

If practical on the actual development machine, also run:

```bash
AURALIS_RUN_STRESS=1 \
AURALIS_STRESS_ITERATIONS=10000 \
ctest --test-dir build -L stress --output-on-failure
```

Do not claim the 10,000-cycle run if it was not actually executed.

---

# 41. SANITIZER VALIDATION

Use the existing sanitizer option.

For example:

```bash
rm -rf build-asan

cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAURALIS_ENABLE_SANITIZERS=ON

cmake --build build-asan

ctest --test-dir build-asan --output-on-failure
```

Then run the critical Phase 8 stress/integration subset under sanitizers as well.

Record:

```text
ASan errors
UBSan errors
leaks if detected
test totals
```

Any unresolved sanitizer error is a software blocker.

---

# 42. PACKAGE VALIDATION

Do not unnecessarily modify packaging if already correct.

Re-run:

```bash
cpack --config build/CPackConfig.cmake
```

Then:

```bash
dpkg-deb -I <package>.deb
dpkg-deb -c <package>.deb
```

Verify:

```text
binary
.desktop
AppStream metadata
icon
LICENSE
```

The current truthful state may remain:

```text
PUBLIC PACKAGE RELEASE METADATA: PENDING OWNER CONTACT
PUBLIC DISTRIBUTION: BLOCKED UNTIL LICENSE TERMS ARE SELECTED
```

Do not choose a license or invent an email.

This owner-input limitation does not by itself block the Phase 8 **software** exit gate if local package engineering is correct and the audit is truthful.

---

# 43. REQUIRED FINAL INTEGRATION TEST INVENTORY

Before software PASS, tests equivalent to all of these must exist.

## D-Bus / logind

```text
runtimeBusLossInvalidatesLogindSubscription
runtimeBusReturnResubscribesLogind
duplicateBusTransitionsAreIdempotent
```

## BlueZ async generation

```text
oldAsyncMethodCompletionIgnored
oldQueuedPropertySignalIgnored
oldQueuedInterfaceSignalIgnored
asyncCompletionAfterShutdownIgnored
```

## Snapshot lifecycle

```text
staleSnapshotCannotClearCurrentInFlightState
snapshotCoalescesToOnePendingRefresh
oldGenerationSnapshotIgnored
snapshotAfterShutdownIgnored
```

## Recovery dependency ordering

```text
sessionReconcileWaitsForBlueZ
sessionReconcileWaitsForPipeWireConnection
sessionReconcileWaitsForPipeWireGraphReady
finalDependencyArrivalTriggersExactlyOneReconcile
```

## Cross-layer

```text
pipeWireGraphIdChangeRebindsStableEndpoint
routeRestoredExactlyOnceAfterRebind
combinedBlueZPipeWireBounceReconcilesExactlyOnce
BlueZReturnRequestsExactlyOneFreshSnapshot
multiDeviceMemberChurnPreservesValidPeer
shutdownDuringRecoveryDoesNotResurrectState
```

## Stress

```text
BlueZ/D-Bus churn
PipeWire graph churn
suspend/resume churn
persistence loop
logger multiple-rotation retention
```

---

# 44. EXACT INVARIANTS TO PROVE

## D-Bus

```text
one runtime bus restart
→ one attachment-generation advance
→ one set of new subscriptions
→ no stale old-generation mutation
```

## Snapshot

```text
old completion cannot alter current-generation in-flight bookkeeping
```

## BlueZ

```text
BlueZ registered
→ exactly one effective fresh snapshot
```

## PipeWire / routing

```text
old node 42
→ graph rebuild
→ new node 108
→ stable identity resolves to 108
→ route restored exactly once
```

## RecoveryManager

```text
refreshActiveSession() is never called before dependencies are usable
```

## Combined recovery

```text
multiple service-ready signals
→ one final session reconciliation
```

## Power

```text
bus restart cannot permanently remove PrepareForSleep monitoring
```

## Stress

```text
extended stress flag means more stress, never fewer iterations
```

---

# 45. REMOVE ALL FALSE-PASS TEST LOGIC

Before declaring completion, search:

```bash
rg -n "\|\| true" tests
rg -n ">= .*Before" tests
rg -n "<= 3" tests
```

Review every weak assertion.

Not every `>=` is wrong, but no Phase 8 recovery test should pass when its named event never happened.

---

# 46. UPDATE AUDITS ONLY AFTER TESTS PASS

Update:

```text
docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
README.md
```

Use explicit evidence categories:

```text
IMPLEMENTED
AUTOMATED PASS
SANITIZER PASS
STRESS PASS
ACTUAL SERVICE TEST NOT RUN
ACTUAL HARDWARE TEST NOT RUN
OWNER INPUT PENDING
```

Do not conflate these.

---

# 47. SOFTWARE EXIT GATE

You may declare:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
```

only if:

- runtime logind subscription survives a system-bus loss/return through re-subscription;
- all relevant BlueZ asynchronous operations are generation-fenced;
- queued BlueZ signal callbacks cannot mutate state after bus-generation replacement;
- stale snapshot completion cannot clear current snapshot state;
- session reconciliation is dependency-gated;
- real-manager integration includes session/routing/stable-endpoint rebind;
- no unconditional test bypass remains;
- combined recovery proves exactly-one reconciliation;
- BlueZ return proves exactly-one fresh snapshot;
- extended stress configuration is correct;
- logger rotation stress proves actual retention;
- hardware-independent tests do not depend on the host D-Bus/BlueZ service;
- normal tests pass;
- sanitizer tests pass;
- stress tests pass;
- Phase 0–7 regressions remain green;
- audit is truthful.

---

# 48. EXPECTED FINAL STATE

If all software requirements pass but hardware validation has not yet been performed:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

Do not declare:

```text
PHASE 8 COMPLETE
```

until actual-machine/hardware validation is run.

If any software blocker remains:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 OVERALL: REMEDIATION REQUIRED
```

and state the exact blocker.

---

# 49. REQUIRED FINAL RESPONSE FROM CURSOR

Return:

## Files changed

Group by:

```text
SystemPowerMonitor
BlueZ/D-Bus
RecoveryManager
Session/routing integration
Stress tests
Logger
Packaging/docs
```

## Defect-by-defect closure

For each item:

```text
problem
root cause
fix
test name
result
```

## Test evidence

```text
normal tests
targeted Phase 8 tests
stress tests
extended stress iterations
ASan/UBSan
```

## Integration evidence

Explicitly report:

```text
stable endpoint old node ID
stable endpoint new node ID
route recreation count
combined recovery reconciliation count
BlueZ snapshot request count
```

## Owner-dependent release items

State:

```text
contact pending?
license pending?
public distribution blocked?
```

## Hardware validation

List what was actually run versus pending.

## Final gate

End with exactly one truthful gate state.

---

# 50. FINAL PRINCIPLE

This last remediation pass is not about adding more recovery code for its own sake.

It is about proving that the existing recovery architecture behaves correctly at the hardest lifecycle boundaries:

```text
system D-Bus restarts after logind was already subscribed
old D-Bus callbacks arrive after a new generation exists
a stale snapshot finishes while a new snapshot is still running
BlueZ returns before PipeWire is ready
PipeWire rebuilds with new ephemeral node IDs
multiple recovery signals arrive almost simultaneously
tests claim recovery without actually exercising it
stress configuration accidentally reduces coverage
```

The Phase 8 software exit gate is ready only when these cases are deterministic, bounded, generation-safe, dependency-aware, and proven by real-manager tests.

Implement these corrections now.

Do not start Phase 9.

Do not stop after the first green CTest run.

Finish code, integration tests, stress tests, sanitizers, package verification, and evidence-based audit updates.
