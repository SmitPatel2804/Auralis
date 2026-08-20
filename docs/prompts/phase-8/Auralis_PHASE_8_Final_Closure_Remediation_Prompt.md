# AURALIS — PHASE 8 FINAL CLOSURE REMEDIATION PROMPT

**Project:** Auralis  
**Phase:** Phase 8 — Reliability, Testing and Production Hardening  
**Purpose:** Resolve the final remaining software-level blockers found during the latest independent audit and bring the Phase 8 **software exit gate** to a defensible PASS before live hardware validation.  
**Target environment:** Cursor AI IDE running inside the actual Auralis repository on the Linux development machine.

---

# 0. IMPORTANT CONTEXT

Phases 0–7 are already implemented.

Most of Phase 8 is also already implemented and should be preserved.

The latest repository has already fixed several previous blockers, including:

- suspend/resume early-return behavior that could leave Bluetooth reconnect paused;
- `autoRecoverServices` wiring into PipeWire automatic reconnect;
- PipeWire reconnect-attempt reset moving from transport `Connected` to `InitialSyncDone`;
- duplicate PipeWire retry ownership inside `RecoveryManager`;
- propagation of PipeWire retry attempts and exhaustion into recovery status;
- obvious duplicate BlueZ snapshot requests in `BluetoothManager`;
- application icon packaging;
- LICENSE installation;
- sanitizer CMake support;
- expanded Phase 8 test registration.

Do **not** reimplement those areas from scratch.

This prompt is only for the final remaining software-hardening issues.

Current independent verdict:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: REMEDIATION STILL REQUIRED
```

The purpose of this work is to reach:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

only if every software-level requirement below is actually implemented and tested.

---

# 1. OPERATING PRINCIPLES

Act as a principal C++/Qt/Linux reliability engineer.

Use the repository in front of you as the source of truth.

Do not:

- trust prior audit documents without checking code;
- weaken tests to obtain green results;
- add long arbitrary sleeps;
- create duplicate recovery ownership;
- bypass existing architecture;
- silently invent package metadata;
- mark skipped hardware tests as PASS;
- start Phase 9 work.

Preserve the established architectural rule:

> **Centralized orchestration, decentralized subsystem ownership.**

Meaning:

- `RecoveryManager` coordinates system-wide state and recovery sequencing.
- `BlueZDbusClient` owns BlueZ/system-bus connectivity mechanics.
- `BluetoothManager` owns Bluetooth model state.
- `ReconnectPolicy` / `DeviceLifecycleManager` own device reconnect semantics.
- `PipeWireManager` owns PipeWire retry scheduling.
- session/routing layers own session/route restoration.
- `SystemPowerMonitor` owns suspend/resume signal acquisition.
- configuration owns recovery preferences.
- packaging remains CMake/CPack owned.

---

# 2. REQUIRED FIRST STEP — RECONNAISSANCE

Before modifying code, run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
```

Then inspect the current implementation and confirm the exact state of the remaining issues.

At minimum inspect:

```text
src/bluetooth/BlueZDbusClient.cpp
include/auralis/bluetooth/BlueZDbusClient.h

src/bluetooth/BluetoothManager.cpp
include/auralis/bluetooth/BluetoothManager.h

src/audio/PipeWireManager.cpp
include/auralis/audio/PipeWireManager.h

src/audio/PipeWireConnection.cpp
include/auralis/audio/PipeWireConnection.h

src/recovery/RecoveryManager.cpp
include/auralis/recovery/RecoveryManager.h

src/recovery/SystemPowerMonitor.cpp
include/auralis/recovery/SystemPowerMonitor.h

src/core/ApplicationCore.cpp
src/core/ConfigurationManager.cpp

apps/desktop/DesktopApplication.cpp

tests/unit/recovery/tst_RecoveryManager.cpp
tests/unit/recovery/tst_SystemPowerMonitor.cpp
tests/unit/audio/tst_PipeWireManager.cpp
tests/unit/bluetooth/tst_BlueZDbusClientBusRecovery.cpp
tests/unit/bluetooth/tst_BluetoothManager.cpp
tests/integration/tst_ServiceRecoveryIntegration.cpp
tests/CMakeLists.txt

src/core/Logger.cpp
tests/unit/core/tst_Logger.cpp

src/session/SessionPersistence.cpp
tests/unit/session/tst_SessionPersistence.cpp

cmake/AuralisPackaging.cmake
apps/desktop/CMakeLists.txt
data/io.github.auralis.Auralis.desktop
data/io.github.auralis.Auralis.metainfo.xml
LICENSE

docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
README.md
```

Also search globally for:

```bash
rg -n "pollSystemBusHealth|busHealthTimer|busAttachGeneration|requestSnapshot|snapshotReceived"
rg -n "InitialSyncDone|reconnectAttempt_|reconnectExhausted|reconnectTimer"
rg -n "pendingReconcile|coalesceTimer|setAutoRecoverEnabled|runReconcile"
rg -n "PrepareForSleep|preparingForSleep|setSuspended|suspendEpoch"
rg -n "trySubscribeLogind|subscribed_|QDBus"
rg -n "auralis@localhost|CPACK_PACKAGE_CONTACT|CPACK_DEBIAN_PACKAGE_MAINTAINER"
```

---

# 3. BASELINE VALIDATION

Before editing, run a clean build:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Record:

```text
CTest registrations
Passed
Failed
Skipped
Duration
```

Also record whether live/hardware tests were skipped.

Do not count skipped tests as pass.

If the baseline fails, identify whether it is pre-existing before continuing.

---

# 4. BLOCKER A — FIX REAL RUNTIME SYSTEM D-BUS LOSS DETECTION

## 4.1 Existing problem

The current implementation reportedly has a timer similar to:

```cpp
busHealthTimer_.setInterval(...);
connect(
    &busHealthTimer_,
    &QTimer::timeout,
    this,
    &BlueZDbusClient::pollSystemBusHealth);
```

but the timer is stopped after a successful initial attachment:

```cpp
if (attachSystemBusInfrastructure()) {
    setSystemBusConnected(true);
    stopBusHealthTimer();
}
```

This means the implementation can recover from:

```text
Auralis starts while system bus is unavailable
→ polling remains active
→ bus later appears
```

but cannot reliably detect:

```text
Auralis starts healthy
→ system D-Bus disappears at runtime
```

because no health mechanism remains active.

This must be fixed.

---

## 4.2 Required behavior

The production BlueZ client must detect both:

```text
DOWN AT STARTUP → UP
```

and:

```text
UP AT STARTUP → DOWN AT RUNTIME → UP AGAIN
```

Required state transitions:

### Healthy

```text
systemBusConnected = true
BlueZ watcher attached
BlueZ signal subscriptions attached
BlueZ service state known
```

### Bus failure

When bus connectivity is lost:

```text
systemBusConnected = false
blueZAvailable = false
invalidate service watcher
invalidate/remove signal subscriptions
prevent new D-Bus calls from being treated as usable
notify upper layers once
start bounded/low-frequency recovery monitoring
```

### Bus return

When connectivity returns:

```text
acquire/revalidate system bus connection
recreate watcher
recreate ObjectManager subscription
recreate PropertiesChanged subscription
re-check org.bluez service registration
emit systemBusConnected(true) exactly once
if BlueZ exists:
    transition BlueZ available
    issue one effective snapshot refresh
```

No duplicate subscriptions.

No duplicate state-transition signals.

No busy loop.

---

# 5. CHOOSE A ROBUST D-BUS MONITORING STRATEGY

Use actual Qt 6 APIs available in the current project.

Do not invent nonexistent `QDBusConnection` signals.

Acceptable strategies include:

## Strategy A — low-frequency always-on health monitor

Keep a modest timer active while the client is initialized.

For example:

```text
every 1–3 seconds:
    check connection_.isConnected()
```

On state transition:

```text
connected -> disconnected:
    handle bus loss

disconnected -> connected:
    rebuild infrastructure
```

Advantages:

- simple;
- deterministic;
- easy to test.

Requirements:

- no logging every poll;
- log only transitions/failures;
- stop on shutdown;
- avoid repeatedly rebuilding infrastructure while already healthy.

---

## Strategy B — event-driven detection + fallback recovery timer

If Qt provides a reliable event that can detect the connection break:

```text
event detects loss
→ state becomes disconnected
→ start retry timer
```

Then use polling only while disconnected.

This is preferable if robustly supported.

---

## 5.1 Required invariant

At all times:

```text
if initialized == true
then there must exist some mechanism capable of noticing a future bus loss
```

The previous implementation violated this invariant.

---

# 6. D-BUS INFRASTRUCTURE LIFECYCLE

Refactor bus attachment into explicit lifecycle methods if helpful.

Conceptually:

```cpp
bool attachSystemBusInfrastructure();
void detachSystemBusInfrastructure();
void handleSystemBusLost();
void attemptSystemBusReattach();
```

Responsibilities should be clear.

`attachSystemBusInfrastructure()` should handle:

- obtaining connection;
- verifying connection;
- creating `QDBusServiceWatcher`;
- subscribing ObjectManager;
- subscribing PropertiesChanged;
- checking BlueZ service;
- advancing generation.

`detachSystemBusInfrastructure()` should:

- destroy/reset service watcher;
- disconnect signal subscriptions;
- invalidate in-flight assumptions;
- leave high-level application intent intact.

---

# 7. BLOCKER B — ADD REAL GENERATION FENCING FOR ASYNC BLUEZ CALLBACKS

## 7.1 Existing issue

The code reportedly contains:

```cpp
quint64 busAttachGeneration_ = 0;
```

but the generation is not actually used to reject stale callbacks.

That makes this possible:

```text
generation 7
→ GetManagedObjects issued

system bus dies

generation 8
→ new bus attaches
→ new GetManagedObjects issued

old generation-7 callback arrives late
→ stale object graph may be applied
```

This is unacceptable.

---

## 7.2 Required implementation

Every asynchronous operation tied to a specific bus attachment must capture:

```text
busAttachGeneration
```

at request creation time.

For example:

```cpp
const auto generation = busAttachGeneration_;
```

When completion arrives:

```cpp
if (!initialized_) {
    ignore;
}

if (generation != busAttachGeneration_) {
    ignore;
}
```

The exact implementation may use:

- a custom watcher wrapper;
- watcher properties;
- a small context object;
- lambda captures;
- a map keyed by watcher pointer.

Use existing Qt idioms.

Do not rely only on pointer identity.

---

## 7.3 Fence at least

- `GetManagedObjects`;
- any delayed service check;
- any async BlueZ method result that can outlive a bus generation;
- any queued callback capable of mutating current Bluetooth registry state.

If some methods are intentionally generation-independent, document why.

---

## 7.4 Shutdown safety

All async completions must also check lifecycle state:

```text
initialized / shuttingDown
```

After shutdown:

```text
no callback may repopulate devices
no callback may emit service availability
no callback may request recovery
```

---

# 8. D-BUS TESTING — TEST THE PRODUCTION PATH, NOT ONLY AN INJECTION FLAG

## 8.1 Current issue

The existing bus recovery test reportedly uses:

```cpp
injectSystemBusConnectedForTesting(false);
injectSystemBusConnectedForTesting(true);
```

but the test path does not execute the real:

```cpp
attachSystemBusInfrastructure()
```

sequence.

That is insufficient.

---

## 8.2 Required test seam

Introduce a small abstraction around system-bus acquisition/health if needed.

Examples:

```text
ISystemBusBackend
SystemBusProvider
BusConnectionProbe
BlueZBusTransport
```

Keep it narrow.

It should allow tests to simulate:

```text
healthy bus
bus lost
bus restored
BlueZ service present
BlueZ service absent
snapshot callback delayed
```

while still exercising the real production BlueZDbusClient state machine.

Avoid sprinkling `#ifdef TEST` through production code.

---

## 8.3 Required tests

Add tests equivalent to:

```text
runtimeBusDisconnectIsDetected
runtimeBusReconnectRebuildsInfrastructure
busReconnectRechecksBlueZService
busReconnectRequestsOneSnapshot
duplicateBusDownSignalIsIdempotent
duplicateBusUpSignalIsIdempotent
oldGenerationSnapshotIsIgnored
snapshotAfterShutdownIsIgnored
busHealthMonitoringContinuesWhileHealthy
```

At least one test must specifically reproduce the previous defect:

```text
client starts healthy
health mechanism active
bus later reports disconnected
client transitions to disconnected automatically
```

without calling a direct state-toggle helper.

---

# 9. BLOCKER C — PIPEWIRE INITIAL GRAPH SYNC TIMEOUT

## 9.1 Existing issue

PipeWire reconnect accounting is improved:

```text
Connected
    → do not reset attempt count

InitialSyncDone
    → reset attempt count
```

This is correct.

But a new failure mode remains:

```text
retry starts
→ PipeWire transport connects
→ retry timer stops
→ InitialSyncDone never arrives
→ no error occurs
→ no new attempt
→ no exhaustion
```

Recovery can stall forever.

---

# 10. DEFINE PIPEWIRE RECONNECT SUCCESS

A successful PipeWire recovery episode must mean:

```text
transport connected
AND
initial graph synchronization completed
```

not merely transport connectivity.

Therefore a bounded timeout must exist between:

```text
Connected
```

and:

```text
InitialSyncDone
```

during reconnect/recovery.

---

# 11. IMPLEMENT GRAPH-SYNC TIMEOUT

Add something conceptually like:

```cpp
QTimer initialSyncTimeoutTimer_;
```

It must be:

```text
single-shot
bounded
started only when needed
stopped on InitialSyncDone
stopped on error/disconnect
stopped on shutdown
stopped when auto-reconnect disabled
```

Timeout behavior:

```text
Connected
→ start graph sync timer

InitialSyncDone before timeout
→ stop timer
→ recovery success
→ reset attempt counter

timeout fires
→ treat current reconnect attempt as failed
→ disconnect/clean state as necessary
→ schedule next bounded retry
→ eventually emit reconnectExhausted
```

The timeout duration should be configurable in tests.

Avoid hardcoded long waits.

---

# 12. PIPEWIRE RETRY INVARIANTS

After this fix:

```text
retry attempts cannot reset before graph ready
graph-never-ready cannot stall forever
attempt count monotonically advances across failed episodes
exhaustion fires exactly once
manual retry after exhaustion starts a fresh episode
shutdown cancels graph-sync timeout
auto-recovery OFF cancels graph-sync timeout
```

---

# 13. REQUIRED PIPEWIRE TESTS

Add/extend tests:

```text
initialSyncTimeoutTriggersNextRetry
graphNeverReadyEventuallyExhausts
initialSyncDoneCancelsTimeout
shutdownCancelsInitialSyncTimeout
disableAutoReconnectCancelsInitialSyncTimeout
manualReconnectAfterGraphSyncExhaustion
lateInitialSyncFromOldGenerationIgnored
```

Use short injected test intervals.

Do not use multi-second sleeps.

---

# 14. BLOCKER D — CANCEL PENDING RECOVERYMANAGER WORK WHEN AUTO-RECOVERY IS DISABLED

## 14.1 Existing race

`RecoveryManager` reportedly schedules a delayed reconciliation via:

```text
coalesceTimer_
pendingReconcile_
```

and checks `autoRecoverEnabled` when scheduling.

However, if:

```text
service recovers
→ reconciliation scheduled
→ user disables auto recovery
→ timer later fires
```

then the pending reconciliation may still execute.

That violates user preference.

---

# 15. REQUIRED FIX

When `setAutoRecoverEnabled(false)` occurs:

```cpp
coalesceTimer_.stop();
pendingReconcile_ = false;
```

and any other automatic recovery timer owned by `RecoveryManager` must be cancelled.

Also add a defensive guard in the actual execution method:

```cpp
if (!status_.autoRecoverEnabled) {
    return;
}
```

This ensures even a queued callback cannot slip through.

Do not cancel user-invoked manual recovery.

---

# 16. RE-ENABLE SEMANTICS

Define what happens when the user turns auto recovery back ON while the system remains degraded.

Choose one behavior and test it.

Recommended:

```text
enable auto recovery
→ if dependencies are currently degraded/recoverable:
    begin one recovery episode
```

or:

```text
enable auto recovery
→ only future failures trigger recovery
```

Either is acceptable if consistent and documented.

Do not let behavior be accidental.

---

# 17. REQUIRED RECOVERYMANAGER TESTS

Add:

```text
disableAutoRecoverCancelsPendingReconcile
runReconcileDoubleChecksAutoRecoverPreference
disableAutoRecoverDuringCoalesceDoesNotRefreshSession
manualRecoveryStillWorksWhenAutoRecoverDisabled
reEnableAutoRecoverUsesDocumentedBehavior
```

---

# 18. BLOCKER E — SUSPEND/RESUME DUPLICATE EVENT IDEMPOTENCY

## 18.1 Existing issue

`SystemPowerMonitor` may correctly avoid changing its internal state twice but still emit:

```cpp
preparingForSleep(sleeping)
```

for duplicate identical input.

Therefore:

```text
true
true
```

can pause reconnect twice.

And:

```text
false
false
```

can:

- increment suspend epoch twice;
- bump recovery generation twice;
- resume reconnect twice;
- schedule duplicate recovery.

This must be fixed.

---

# 19. REQUIRED SYSTEMPOWEMONITOR BEHAVIOR

State transitions should emit only on actual change.

Conceptually:

```cpp
void SystemPowerMonitor::setSuspended(bool suspended)
{
    if (suspended_ == suspended) {
        return;
    }

    suspended_ = suspended;
    emit suspendedChanged(suspended_);
    emit preparingForSleep(suspended_);
}
```

Use existing signal semantics.

If `preparingForSleep` is intentionally a raw event stream rather than state-transition signal, then `RecoveryManager` must itself enforce idempotency.

At least one layer must guarantee no duplicate lifecycle work.

Prefer doing both safely:

```text
monitor emits transitions
RecoveryManager defensively ignores impossible duplicate state
```

---

# 20. RECOVERYMANAGER IDEMPOTENCY

Add guards:

```text
if sleeping && already suspended:
    return

if !sleeping && already resumed:
    return
```

Do not:

- bump generation;
- change epoch;
- pause/resume subsystem work;
- schedule reconciliation

for duplicate events.

---

# 21. REQUIRED TESTS

Add:

```text
duplicateSleepTrueIsIgnored
duplicateResumeFalseIsIgnored
duplicateSuspendDoesNotPauseReconnectTwice
duplicateResumeDoesNotIncrementEpochTwice
duplicateResumeDoesNotScheduleDuplicateReconcile
```

Also retain the valid repeated-cycle test:

```text
sleep
resume
sleep
resume
```

---

# 22. BLOCKER F — LATE LOGIND/SYSTEM-BUS SUBSCRIPTION RECOVERY

## 22.1 Existing issue

`SystemPowerMonitor` reportedly attempts logind subscription once:

```cpp
subscribed_ = trySubscribeLogind();
```

If unavailable:

```text
inject-only mode
```

and never retries.

Therefore:

```text
Auralis starts
→ system bus/logind temporarily unavailable
→ subscription fails
→ system later becomes healthy
→ suspend events never observed
```

That is not production-hardened.

---

# 23. REQUIRED FIX

Implement a low-frequency, bounded, or event-triggered resubscription strategy.

Possible approach:

```cpp
QTimer subscriptionRetryTimer_;
```

Behavior:

```text
initial subscription succeeds
→ no retry needed

initial subscription fails
→ start low-frequency retry

retry succeeds
→ stop retry timer
→ subscribed = true

subscription later breaks
→ subscribed = false
→ retry begins
```

If the system-bus recovery layer can notify `SystemPowerMonitor`, use that rather than redundant polling.

Avoid creating a second independent system-bus health implementation if a shared signal already exists.

---

# 24. REQUIRED POWER-MONITOR TESTS

Add tests equivalent to:

```text
initialSubscriptionFailureRetries
lateLogindAvailabilitySubscribes
successfulSubscriptionStopsRetry
shutdownStopsSubscriptionRetry
duplicateSubscriptionDoesNotCreateDuplicateCallbacks
```

If an abstraction is needed around logind subscription, keep it small.

---

# 25. BLOCKER G — BUILD A REAL-MANAGER PHASE 8 INTEGRATION HARNESS

## 25.1 Existing problem

The current `tst_ServiceRecoveryIntegration` mostly instantiates:

```text
RecoveryManager
```

plus hook lambdas.

This tests recovery orchestration but not enough of the real production stack.

Phase 8 needs at least one cross-layer integration harness with real managers and fake external boundaries.

---

# 26. REQUIRED HARNESS COMPOSITION

Instantiate as many real internal components as practical:

```text
ConfigurationManager
RecoveryManager
BluetoothManager
BlueZ client abstraction / fake transport
PipeWireManager
fake PipeWire connection/backend
endpoint/object model components
SessionManager
routing/session reconciliation components
```

The external Linux services may be fake.

The internal production state machines should be real.

Do not create a giant unrelated testing framework.

Build only the seams needed.

---

# 27. REQUIRED CROSS-LAYER SCENARIOS

## 27.1 Runtime system bus bounce

```text
start healthy
→ bus disappears
→ BlueZ invalidated
→ recovery state degraded
→ bus returns
→ BlueZ infrastructure rebuilt
→ service status rechecked
→ one snapshot
→ models repopulated
→ session reconcile once
```

---

## 27.2 BlueZ daemon bounce

```text
bus healthy
BlueZ disappears
→ model/service degraded
→ reconnect work pauses as appropriate
BlueZ returns
→ one snapshot
→ registry recovers
→ one reconcile
```

---

## 27.3 PipeWire graph bounce with ID changes

Simulate:

```text
endpoint stable ID "headset-A"
old graph node = 42

PipeWire restart

new graph node = 108
same stable endpoint identity
```

Assert:

```text
old node 42 not reused
new node 108 resolved
route recreated once
no stale generation event mutates new graph
```

---

## 27.4 Graph never ready

```text
transport connects
InitialSyncDone never happens
```

Assert:

```text
sync timeout
retry
eventual exhaustion
central RecoveryManager shows Exhausted
```

---

## 27.5 Combined BlueZ + PipeWire bounce

Test multiple orderings:

```text
BlueZ down first
PipeWire down first
BlueZ up first
PipeWire graph ready first
nearly simultaneous return
```

The final session reconcile count should remain bounded/coalesced.

---

## 27.6 Auto recovery disabled

```text
autoRecoverServices = false
service fails
service returns
```

Assert no forbidden automatic recovery/reconcile.

Then prove manual refresh/reconnect remains possible.

---

## 27.7 Suspend/resume

Both:

```text
restoreOnResume = true
restoreOnResume = false
```

and duplicate injected transitions.

---

## 27.8 Shutdown during recovery

At least:

```text
shutdown while bus reattach pending
shutdown while snapshot pending
shutdown while PipeWire sync timeout pending
shutdown while reconnect timer pending
shutdown while recovery coalesce pending
shutdown while suspended
```

No post-shutdown resurrection.

---

# 28. BLOCKER H — REAL STRESS TEST COVERAGE

The current stress label is not sufficient if it only loops `RecoveryManager` hook notifications.

Add repeated stress over actual internal managers.

---

# 29. BLUEZ/D-BUS CHURN STRESS

Run at least 100–500 deterministic cycles in normal stress mode:

```text
bus healthy
bus down
bus up
BlueZ absent
BlueZ present
snapshot
```

Assert:

```text
no duplicate service watchers
no duplicate signal subscriptions
no growing watcher count
no duplicate devices
no stale-generation snapshots applied
no timers left unexpectedly active
```

If instrumentation counters are needed, expose test-only introspection cleanly.

---

# 30. PIPEWIRE GRAPH CHURN STRESS

Run repeated cycles:

```text
graph ready
error
reconnect
Connected
InitialSyncDone
new generation
new graph IDs
```

Assert:

```text
generation advances
object store cleared
old nodes absent
new endpoints resolved
attempt count resets only after sync
no leaked retry timers
```

Also run periodic graph-never-ready failures.

---

# 31. SUSPEND/RESUME STRESS

Run 100+ transitions.

Include:

```text
valid sleep/resume cycles
duplicate true
duplicate false
restore enabled
restore disabled
```

Assert:

```text
pause count == expected
resume count == expected
suspend epoch == actual completed resumes
no duplicate reconciliation explosion
```

---

# 32. SESSION PERSISTENCE STRESS

Add a deterministic save/load loop, e.g. 250–1000 cycles with small state changes.

Assert:

```text
writes succeed
reload succeeds
schema remains valid
stable identities preserved
no corrupted file
atomic behavior maintained
file size bounded for equivalent data
```

Do not make test unnecessarily slow.

---

# 33. LOGGER ROTATION STRESS

The current simple rotation test is not enough.

Test:

```text
multiple rotations
retention count N
oldest file deleted
active log remains writable
retention 1 boundary
rotation failure does not crash
```

If retention zero is valid, test it.

Otherwise assert configuration rejects it.

---

# 34. OPTIONAL EXTENDED STRESS MODE

Support optional larger iteration counts.

For example:

```bash
AURALIS_RUN_STRESS=1
AURALIS_STRESS_ITERATIONS=10000
```

Default CI should remain reasonable.

Document the command.

---

# 35. BLOCKER I — REMOVE USER-FACING “ATTEMPT 0”

## 35.1 Existing issue

The central status can briefly be:

```text
pipeWire = Recovering
pipeWireAttempts = 0
```

and `userFacingStatus()` may render:

```text
Reconnecting audio service (attempt 0)
```

That is not meaningful.

---

# 36. REQUIRED FIX

Preferred behavior:

```text
if Recovering and attempts == 0:
    "Preparing to reconnect audio service"
```

or:

```text
"Reconnecting audio service"
```

Only show:

```text
attempt N
```

when `N > 0`.

Do not fake attempt `1` before the retry owner actually begins attempt 1.

---

# 37. REQUIRED TEST

Add:

```text
userFacingStatusNeverShowsAttemptZero
```

Also verify:

```text
attempt 1
attempt 2
...
```

render correctly after owner signals.

---

# 38. BLOCKER J — FINAL BLUEZ SNAPSHOT COALESCING

The obvious duplicate request in `BluetoothManager` is already fixed.

Now ensure stronger guarantees.

---

# 39. REQUIRED SNAPSHOT RULES

One logical recovery transition should produce:

```text
one effective GetManagedObjects request
```

unless:

- it fails;
- another newer refresh is requested while one is in flight.

If concurrent refresh requests are possible, implement coalescing.

Conceptual behavior:

```text
request #1 starts

request #2 arrives while #1 in flight
→ mark pendingRefresh = true

request #3 arrives
→ still only one pending refresh

#1 completes

if pendingRefresh:
    issue one fresh #2
```

This prevents parallel storms while preserving freshness.

---

# 40. SNAPSHOT GENERATION AND SHUTDOWN

A snapshot must be ignored if:

```text
bus generation changed
client shutting down
client no longer initialized
```

Required tests:

```text
duplicateBlueZRegisteredEventDoesNotIssueParallelSnapshots
snapshotRequestCoalesces
newGenerationInvalidatesOldSnapshot
shutdownInvalidatesSnapshot
failedSnapshotAllowsLaterRefresh
```

---

# 41. BLOCKER K — PACKAGE CONTACT/AUDIT TRUTHFULNESS

Current package may still contain:

```text
auralis@localhost
```

Do not invent a personal email address.

---

# 42. REQUIRED PACKAGING BEHAVIOR

Search repository for an authoritative project contact.

If one exists, use it.

If none exists:

- do not fabricate one;
- leave the local package build technically functional;
- clearly document:

```text
PUBLIC PACKAGE RELEASE METADATA: PENDING OWNER CONTACT
```

The `.deb` can pass local technical packaging tests.

But the audit must not claim publication readiness while placeholder metadata remains.

---

# 43. LICENSE STATUS

Do not choose a license.

If root `LICENSE` still states terms have not been selected:

```text
public distribution remains blocked by owner licensing decision
```

Keep AppStream truthful.

Do not reintroduce MIT or another SPDX license without authority.

---

# 44. REQUIRED PACKAGE VALIDATION

Run:

```bash
cpack --config build/CPackConfig.cmake
```

Then:

```bash
dpkg-deb -I <package>.deb
dpkg-deb -c <package>.deb
```

Verify presence of:

```text
/usr/bin/auralis-desktop
desktop file
AppStream metadata
icon
LICENSE
```

Extract:

```bash
rm -rf /tmp/auralis-package-root
mkdir -p /tmp/auralis-package-root

dpkg-deb -x <package>.deb /tmp/auralis-package-root
```

Smoke-launch from extracted prefix without relying on build-tree resources.

---

# 45. TEST LABELING

Use meaningful CTest labels.

Suggested:

```text
unit
integration
stress
recovery
bluetooth
pipewire
session
sanitizer-compatible
```

Do not label a simple hook-loop as comprehensive stress.

Ensure the new real-manager tests are discoverable.

---

# 46. SANITIZER VALIDATION

The sanitizer support already exists.

After remediation, run a clean sanitizer build.

Use the repository's existing sanitizer option if available.

Example:

```bash
rm -rf build-asan

cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAURALIS_ENABLE_SANITIZERS=ON

cmake --build build-asan

ctest --test-dir build-asan --output-on-failure
```

Record exact totals.

Any sanitizer failure is a blocker until understood.

---

# 47. COMPLETE HARDWARE-INDEPENDENT REGRESSION

After all changes:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Then run targeted categories:

```bash
ctest --test-dir build -R Recovery --output-on-failure -V
ctest --test-dir build -R PipeWire --output-on-failure -V
ctest --test-dir build -R BlueZ --output-on-failure -V
ctest --test-dir build -R Power --output-on-failure -V
ctest --test-dir build -R Session --output-on-failure -V
ctest --test-dir build -R Logger --output-on-failure -V
```

And stress label:

```bash
ctest --test-dir build -L stress --output-on-failure
```

Adjust names to actual registration.

---

# 48. MANDATORY TEST INVENTORY

Before software PASS, tests equivalent to all of the following must exist.

## D-Bus/BlueZ

```text
runtimeBusDisconnectIsDetected
runtimeBusReconnectRebuildsInfrastructure
busReconnectRechecksBlueZ
busReconnectRequestsOneSnapshot
oldGenerationSnapshotIgnored
snapshotAfterShutdownIgnored
duplicateBusStateIdempotent
snapshotCoalescingWorks
```

## PipeWire

```text
initialSyncTimeoutTriggersRetry
graphNeverReadyExhausts
initialSyncDoneCancelsTimeout
disableAutoReconnectCancelsSyncTimeout
shutdownCancelsSyncTimeout
manualRecoveryAfterExhaustion
```

## RecoveryManager

```text
disableAutoRecoverCancelsPendingReconcile
runReconcileChecksPreference
userFacingStatusNeverShowsAttemptZero
duplicateSuspendIgnored
duplicateResumeIgnored
```

## Power monitor

```text
lateLogindSubscriptionRecovers
subscriptionRetryStopsAfterSuccess
shutdownStopsSubscriptionRetry
```

## Real-manager integration

```text
runtimeBusBounceRecoversFullStack
blueZDaemonBounceRecoversFullStack
pipeWireBounceRebindsStableIdentity
combinedBlueZPipeWireBounceCoalesces
graphNeverReadyPropagatesExhaustion
autoRecoveryDisabledPreventsAutomaticRestore
suspendResumeEnabledCrossLayer
suspendResumeDisabledCrossLayer
shutdownDuringRecoveryCrossLayer
```

## Stress

```text
blueZBusChurnStress
pipeWireGraphChurnStress
suspendResumeStress
sessionPersistenceStress
loggerRotationStress
```

---

# 49. NO FAKE TEST COMPLETION

The following are not equivalent:

```text
test toggles bool false/true
```

vs:

```text
test drives real production recovery path
```

Likewise:

```text
RecoveryManager hook lambda increments a counter
```

is not equivalent to:

```text
real PipeWireManager retries
real BluetoothManager rebuilds state
real SessionManager reconciles
```

Hook-level unit tests remain useful.

But Phase 8 closure needs both:

```text
small unit tests
+
real-manager integration tests
```

---

# 50. LIVE/HARDWARE TESTS REMAIN SEPARATE

Even after this prompt succeeds, do not claim full Phase 8 complete unless actual machine tests are run.

Keep hardware checks separate:

```text
real BlueZ restart
real adapter off/on
real Bluetooth device reconnect
real PipeWire restart
real WirePlumber restart
two-device session recovery
laptop suspend/resume
long soak
```

If not run:

```text
PHASE 8 HARDWARE EXIT GATE: PENDING
```

---

# 51. REQUIRED AUDIT DOCUMENT UPDATE

Update:

```text
docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
```

with truthful categories:

```text
IMPLEMENTED
AUTOMATED PASS
ACTUAL-SERVICE PASS
ACTUAL-HARDWARE PASS
NOT RUN
PENDING OWNER INPUT
FAIL
```

Do not use plain PASS where actual live/hardware validation was not run.

---

# 52. FINAL SOFTWARE EXIT GATE

Only declare:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
```

when all are true:

- runtime system D-Bus loss is detectable while app is healthy;
- bus return rebuilds BlueZ infrastructure;
- stale BlueZ async callbacks are generation-fenced;
- snapshots are coalesced and shutdown-safe;
- PipeWire graph sync has a bounded timeout;
- graph-never-ready eventually exhausts retry budget;
- disabling auto recovery cancels pending reconciliation;
- suspend/resume duplicate events are idempotent;
- late logind availability can recover subscription;
- user-facing status never shows impossible attempt 0;
- real-manager Phase 8 integration tests exist;
- BlueZ/PipeWire/suspend/persistence/logger stress tests exist;
- normal test suite is green;
- sanitizer suite is green;
- package technical validation is green;
- audit clearly identifies owner-dependent release metadata;
- no Phase 0–7 regressions.

---

# 53. EXPECTED FINAL EXIT STATE

If all software requirements pass but live hardware has not been executed:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

If any software item remains:

```text
PHASE 8 SOFTWARE EXIT GATE: FAIL
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: REMEDIATION REQUIRED
```

Do not use "COMPLETE" while hardware validation is pending.

---

# 54. REQUIRED FINAL RESPONSE FROM CURSOR

Return a structured summary.

## 54.1 Files changed

Group into:

```text
D-Bus/BlueZ
PipeWire
Recovery
Power lifecycle
Integration tests
Stress tests
Packaging
Documentation
```

## 54.2 Remaining defects addressed

For each:

```text
Issue
Root cause
Code fix
Test proving fix
```

## 54.3 Normal test evidence

Include:

```text
registered
passed
failed
skipped
```

## 54.4 Stress evidence

Include actual iteration counts.

## 54.5 Sanitizer evidence

Include:

```text
ASan
UBSan
test totals
findings
```

## 54.6 Packaging evidence

Include:

```text
.deb path
dpkg-deb -I result summary
dpkg-deb -c result summary
extracted-prefix launch result
```

## 54.7 Owner-dependent metadata

Explicitly state:

```text
license decision pending?
maintainer/contact pending?
public release blocked?
```

## 54.8 Hardware validation

List:

```text
actually run
not run
pending
```

## 54.9 Final gate

End with exactly one truthful gate state.

---

# 55. IMPLEMENTATION ORDER

Recommended sequence:

```text
1. baseline build/test
2. runtime D-Bus monitor
3. D-Bus generation fencing
4. production-path BlueZ tests
5. PipeWire InitialSync timeout
6. RecoveryManager pending-work cancellation
7. suspend/resume idempotency
8. logind late subscription recovery
9. attempt-0 UI cleanup
10. snapshot coalescing hardening
11. real-manager integration harness
12. stress suites
13. full normal regression
14. sanitizer regression
15. package validation
16. audit update
```

Do not skip directly to documentation.

---

# 56. CODE QUALITY

All changes must:

- follow current naming conventions;
- use Qt ownership correctly;
- keep timers parented;
- avoid raw lifetime hazards;
- stop timers on shutdown;
- avoid duplicate subscriptions;
- avoid background busy loops;
- log state transitions, not every health poll;
- use deterministic tests;
- preserve stable endpoint identity;
- preserve user reconnect/session intent;
- keep recovery bounded.

---

# 57. KEY STATE-MACHINE INVARIANTS

## D-Bus

```text
initialized healthy state must still be capable of detecting future bus loss
one bus transition -> one state transition
one generation -> callbacks only mutate same generation
```

## BlueZ

```text
bus down => BlueZ cannot remain logically available
new bus generation => old BlueZ async results invalid
one recovery transition => bounded snapshot requests
```

## PipeWire

```text
Connected != recovery success
InitialSyncDone == usable graph boundary
graph-never-ready cannot wait forever
exhaustion ends automatic retry
```

## RecoveryManager

```text
disabled auto-recovery => no delayed automatic reconcile
manual recovery remains possible
duplicate lifecycle events are harmless
```

## Suspend

```text
true -> true = no-op
false -> false = no-op
true -> false = one real cycle
```

## Shutdown

```text
shutdown is terminal
no timer or callback may restart subsystem recovery afterwards
```

---

# 58. LOGGING EXPECTATIONS

Add useful logs for:

```text
system bus health transition
bus attachment generation
stale snapshot rejection
BlueZ infrastructure rebuild
PipeWire graph-sync timeout start/cancel/fire
automatic recovery cancellation
duplicate suspend event ignored
logind subscription retry/success
snapshot coalescing
```

Avoid noisy repeated polling logs.

---

# 59. SUCCESS CONDITION

The objective is to eliminate the last software-level recovery blind spots.

Auralis must now be able to handle:

```text
healthy D-Bus disappearing later
D-Bus returning with new BlueZ attachment generation
old async BlueZ callbacks arriving late
PipeWire transport connecting but graph never synchronizing
user disabling auto-recovery while work is queued
duplicate PrepareForSleep signals
logind unavailable at application startup
repeated BlueZ/PipeWire/power churn
shutdown during each recovery stage
```

with bounded, deterministic, testable behavior.

Once all of that is implemented and evidenced, the software side of Phase 8 is ready for the final actual-machine hardware validation pass.
