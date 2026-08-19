# Auralis — Phase 6 Correction, Hardening, Validation & Final Closure Prompt
## Bring the Multi-Device Session Engine from “Substantially Implemented” to “100% Phase-6 Complete”

**Project:** Auralis  
**Target:** Existing Linux desktop repository after Phase 6 implementation  
**Current baseline:** Phases 0–5 completed; Phase 6 substantially implemented but not yet accepted as complete  
**Primary language/framework:** Modern C++ / Qt 6 / CMake / Ninja  
**Core integrations:** BlueZ over D-Bus, PipeWire, existing AudioRouter, existing Bluetooth recovery machinery  
**Purpose:** Correct all currently known Phase 6 defects, strengthen behavioral guarantees, close test gaps, validate regressions, update documentation, and produce a defensible final Phase 6 completion verdict.

---

# 0. Critical Instruction

You are **not implementing Phase 6 from scratch**.

The repository already contains a real Phase 6 implementation with components such as:

- `SessionManager`
- `SessionStateMachine`
- `RoutingCoordinator`
- `VolumeCoordinator`
- session persistence
- Session-domain types
- Phase 6 unit tests
- Phase 6 integration tests
- Phase 6 documentation
- ApplicationCore integration

Preserve this architecture unless a specific defect requires a narrowly-scoped change.

Do **not** create parallel replacements such as:

```text
SessionManager2
NewSessionEngine
SessionAudioRouter
AlternateRecoveryManager
NewPersistenceSystem
```

Do not bypass working Phase 0–5 components.

The goal is:

```text
existing Phase 6 implementation
          +
known corrective fixes
          +
missing tests
          +
regression verification
          +
documentation cleanup
          =
PHASE 6 STATUS: COMPLETE
```

---

# 1. Primary Mission

Bring the existing Phase 6 implementation to a state where all of the following are true:

1. recovery policy is authoritative;
2. reconnect scheduling does not compete with the lower Bluetooth recovery subsystem;
3. a `FAILED` session can never secretly create active routes while remaining `FAILED`;
4. persistence works reliably on first run and reports failures correctly;
5. stale recovery callbacks cannot affect later activation generations;
6. recovery work is cancelled immediately when user/session policy changes make it invalid;
7. member return actually restores routes and returns sessions to `ACTIVE`;
8. source loss and restoration follow explicit, tested semantics;
9. volume/mute intent is correctly retained and reapplied;
10. invalid volume values are rejected rather than silently converted to misleading valid values;
11. session-domain errors are observable and cleared when appropriate;
12. `lastUsedAt` means successful/meaningful use rather than “activation was merely requested”;
13. stable source restoration semantics are adequate for application restart;
14. all Phase 0–6 automated tests pass;
15. optional hardware tests are honest and opt-in;
16. documentation accurately states Phase 6 status;
17. the final implementation contains no known Phase 6 blocker.

Do not proceed into Phase 7.

---

# 2. Mandatory Baseline Audit Before Editing

Before modifying code, inspect the exact current repository.

At minimum run repository-appropriate equivalents of:

```bash
git status --short
git branch --show-current
git log --oneline -n 20

find include src apps tests docs -maxdepth 4 -type f 2>/dev/null | sort

grep -R \
  "SessionManager\|SessionStateMachine\|RoutingCoordinator\|VolumeCoordinator\|SessionPersistence\|RecoveryPolicy\|ReconnectPolicy\|reconnectDevice\|operationGeneration\|lastUsedAt\|sessionError" \
  -n include src apps tests docs 2>/dev/null
```

Then configure/build/test the existing baseline before correction:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If the existing build directory is stale or generated for another environment, use a clean build:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record:

- number of tests;
- passing/failing/skipped;
- current Phase 6 test names;
- any environment limitations;
- current warnings.

Do not treat repository self-audit documents as proof. They are useful context only.

---

# 3. Known Defects That Must Be Addressed

The following issues were identified during source-level Phase 6 review.

Treat each one as a mandatory correction item unless inspection proves the implementation has already changed.

---

# 4. DEFECT A — Recovery Policy Is Not Authoritative

## 4.1 Observed Problem

The existing session reconciliation path can perform route reconciliation before or regardless of the configured `RecoveryPolicy`.

This makes it possible for a session configured with:

```text
RecoveryPolicy::None
autoReconnect = false
```

to still have a missing route restored when its endpoint later becomes available.

That violates the meaning of recovery policy.

## 4.2 Required Semantics

Define exact behavior for each policy.

Recommended semantics:

### `RecoveryPolicy::None`

When a member route or endpoint is lost after activation:

```text
do not reconnect device
do not automatically recreate missing route
do not automatically restore member
keep healthy peers operating if possible
session becomes DEGRADED
```

The only actions that may restore the missing member are:

- explicit `retrySession()`;
- stop then start;
- explicit user action that logically requests reconciliation.

### `RecoveryPolicy::RestoreRoutesOnly`

When a member disappears:

```text
do not initiate Bluetooth reconnect
wait for device/endpoint to return externally
when endpoint becomes available again:
    recreate missing route
    reapply volume/mute
    recompute state
```

### `RecoveryPolicy::ReconnectAndRestore`

When a member disappears:

```text
delegate reconnect intent to the existing Bluetooth recovery/lifecycle layer
wait for authoritative connection event
wait for endpoint reappearance
restore route
reapply volume/mute
recompute session state
```

## 4.3 Implementation Rule

Do not let `RoutingCoordinator::reconcile()` alone decide recovery.

Separate:

```text
normal desired-state reconciliation
```

from:

```text
policy-authorized recovery reconciliation
```

Possible designs:

- pass an explicit reconciliation mode;
- pass allowed-member set;
- SessionManager computes desired routes based on policy;
- maintain `recoveryEligible` state per member.

Choose the smallest change consistent with the current architecture.

## 4.4 Mandatory Tests

Add tests proving:

```text
RecoveryPolicy::None
    endpoint returns
    route is NOT automatically recreated
```

```text
RecoveryPolicy::RestoreRoutesOnly
    Bluetooth reconnect is NOT requested
    endpoint returns externally
    route IS recreated
```

```text
RecoveryPolicy::ReconnectAndRestore
    reconnect intent is delegated
    route restored after endpoint returns
```

---

# 5. DEFECT B — Competing Reconnect Loops

## 5.1 Observed Problem

Phase 3 already owns Bluetooth reconnect behavior through a lifecycle/reconnect policy layer with:

- max attempts;
- initial delay;
- backoff multiplier;
- max delay;
- reconnect cancellation.

Phase 6 currently has or had its own timer-based reconnect scheduling, e.g. short fixed-delay timers plus periodic recovery sweeps.

This risks:

- duplicate reconnect attempts;
- cancellation/reset of Phase 3 backoff;
- reconnect storms;
- confusing ownership;
- nondeterministic tests.

## 5.2 Required Architecture

There must be one owner of low-level Bluetooth reconnect timing.

Preferred ownership:

```text
SessionManager
    |
    | expresses reconnect intent
    v
BluetoothManager / DeviceLifecycleManager
    |
    v
ReconnectPolicy
    |
    v
actual retries/backoff
```

SessionManager may own:

- whether this session/member desires recovery;
- whether reconnect is allowed by session policy;
- whether route restoration should occur after connectivity returns.

SessionManager must **not** run a second competing exponential/fixed retry loop for Bluetooth connection itself.

## 5.3 Required Change

Remove or neutralize session-layer timers that repeatedly call `reconnectDevice()`.

If the Bluetooth API currently exposes only “manual reconnect now,” inspect whether a lower-layer API can be extended cleanly to express:

```text
requestManagedReconnect(deviceId)
cancelManagedReconnect(deviceId)
```

or equivalent.

Do not reach directly into BlueZ from SessionManager.

If a small API extension to `BluetoothManager` is required, add it with regression tests.

## 5.4 Route-Level Retry Is Different

It is acceptable for Phase 6 to re-evaluate route restoration when:

- endpoint appears;
- route fails;
- registry changes;
- periodic health reconciliation occurs.

But it must not duplicate Bluetooth retry/backoff ownership.

## 5.5 Mandatory Tests

Use fakes/spies to prove:

- one session recovery event does not produce repeated immediate reconnect requests;
- Bluetooth recovery backoff remains authoritative;
- stopping session cancels session recovery intent;
- disabling auto reconnect cancels session recovery intent;
- policy `RestoreRoutesOnly` never requests Bluetooth reconnect.

---

# 6. DEFECT C — `FAILED` Session Can Recreate Active Routes

## 6.1 Observed Problem

The state machine intentionally keeps a session in `FAILED` until explicit retry.

However, reconciliation may continue while the failed session remains the active session.

This can produce:

```text
state = FAILED
route = ACTIVE
audio = flowing
```

which is invalid.

## 6.2 Required Invariant

A session in:

```text
FAILED
IDLE
STOPPING
```

must not create new session-owned routes unless an explicit operation transitions it back to a route-producing state.

Recommended route-producing states:

```text
STARTING
ACTIVE
DEGRADED
RECOVERING
```

and only if operation intent/policy allows it.

## 6.3 Required Behavior

If session activation fails because of a source-wide terminal condition:

```text
session -> FAILED
```

then route creation is suppressed until:

```text
retrySession()
```

or:

```text
deactivate + activate
```

depending on current API semantics.

If the source becomes available while session remains `FAILED`, nothing should automatically reactivate unless explicitly designed and documented.

## 6.4 Alternative

If the intended product semantics are “failed session auto-recovers when source returns,” then `FAILED` is the wrong state.

In that case the session should remain:

```text
RECOVERING
```

and the state machine/tests/documentation must be updated consistently.

Do not preserve a contradictory mixture.

## 6.5 Mandatory Tests

Add:

```text
activate with unavailable source
-> FAILED

source later appears

assert:
    no active session route is created
    state remains FAILED

explicit retry

assert:
    STARTING -> ACTIVE
```

Also test source loss from ACTIVE.

---

# 7. DEFECT D — Persistence Parent Directory

## 7.1 Observed Problem

The default session persistence path can be:

```text
QStandardPaths::AppDataLocation/sessions.json
```

without ensuring the parent directory exists.

Unit tests using an already-existing temporary directory do not prove first-run production behavior.

## 7.2 Required Fix

Before save:

1. obtain parent path;
2. create directory if absent;
3. verify creation succeeded;
4. then perform atomic save.

Example concept:

```cpp
QFileInfo info(filePath_);
QDir dir(info.absolutePath());

if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    return persistence error;
}
```

Use repository style.

## 7.3 Mandatory Tests

Use a nested nonexistent directory:

```text
tmp/
  does-not-exist/
    nested/
      sessions.json
```

Call save.

Assert:

- parent directories created;
- file created;
- reload succeeds.

Also test directory creation failure if feasible.

---

# 8. DEFECT E — Persistence Failures Are Ignored

## 8.1 Problem

Public mutation operations may currently succeed in memory while `persistAll()` fails silently.

Examples:

```text
createSession -> Accepted
but save failed
```

This violates persistent-session guarantees.

## 8.2 Required Semantics

Choose and document one of these models:

### Model A — Transactional mutation

```text
apply tentative mutation
persist
if persist fails:
    rollback mutation
    return failure
```

Best for CRUD/config changes.

### Model B — In-memory success with explicit dirty/error state

```text
apply mutation
persist fails
operation returns PersistFailure or warning result
session remains dirty
retry persistence later
emit sessionError
```

Either is acceptable if consistent.

Do not silently return unconditional success.

## 8.3 Minimum Operations That Must Surface Save Failure

- create session
- delete session
- rename session
- add/remove member
- source change
- role change
- group volume persistence
- individual volume/mute persistence
- recovery policy change
- autoReconnect change

If high-frequency volume persistence is debounced, errors still need observable reporting.

## 8.4 Mandatory Tests

Inject fake failing persistence.

Verify:

- correct operation result;
- `sessionError` emitted;
- in-memory/rollback semantics match documentation;
- no crash.

---

# 9. DEFECT F — Recovery Work Survives Policy Changes

The following operations must immediately invalidate pending recovery work where appropriate:

```text
setAutoReconnect(false)
setRecoveryPolicy(None)
setDeviceEnabled(false)
removeDevice(...)
deactivateSession(...)
deleteSession(...)
switch active session
application shutdown
```

## 9.1 Required Cancellation Function

Centralize cancellation where practical.

Conceptual:

```cpp
cancelRecovery(sessionId, deviceId)
cancelAllRecovery(sessionId)
```

This should:

- cancel session-layer pending route recovery tokens/timers;
- cancel managed Bluetooth reconnect intent if owned for this member;
- clear member `recovering`;
- invalidate relevant callbacks/generation;
- recompute session state.

## 9.2 Mandatory Tests

Example:

```text
device lost
recovery pending
setAutoReconnect(false)

advance fake time

assert:
    reconnect not requested
```

```text
device lost
recovery pending
disable member

endpoint later returns

assert:
    route not restored
```

---

# 10. DEFECT G — Stale Recovery Callbacks Need Generation Safety

## 10.1 Problem

Recovery callbacks may currently check only whether a session is still “active” or has a nonzero generation.

That does not prove they belong to the current activation generation.

## 10.2 Required Pattern

Capture the generation at scheduling time:

```cpp
const auto generation = session.operationGeneration;
```

When callback runs:

```cpp
if (!sessionExists(sessionId)) {
    return;
}

if (currentSession.operationGeneration != generation) {
    return;
}

if (generations_.value(sessionId) != generation) {
    return;
}
```

Then re-check:

- member still exists;
- member enabled;
- session still allows recovery;
- session state permits recovery;
- active session is still this session.

## 10.3 Mandatory Race Test

```text
generation 10: start
member lost
recovery callback scheduled

stop
generation increments

start again
generation 12

old callback from generation 10 executes

assert:
    no reconnect request
    no route creation
    no state mutation
```

---

# 11. DEFECT H — Member Return Recovery Test Missing

This is a mandatory Phase 6 product proof.

Add deterministic integration coverage:

```text
create session
add Device A + Device B
source available
endpoints A1 + B1 available

activate
-> ACTIVE

remove/disconnect B
remove endpoint B1
-> DEGRADED or RECOVERING

keep A route healthy

reconnect B
add endpoint B2 with a NEW runtime endpoint ID

assert:
    endpoint resolver maps B2 to Device B
    missing B route recreated
    A route was not unnecessarily recreated
    current B volume applied
    current B mute applied
    session -> ACTIVE
```

This must work without real hardware by using fakes.

---

# 12. DEFECT I — Source Loss / Source Return Semantics

Define and test source-wide failure behavior.

## 12.1 Active Session Source Loss

Example:

```text
ACTIVE
source endpoint disappears
```

Choose explicit behavior.

Recommended:

```text
all routes become unusable
session -> RECOVERING
if source can be re-resolved automatically
```

or:

```text
session -> FAILED
if source loss is considered terminal
```

The choice must match route behavior.

## 12.2 Source Return

If state was `RECOVERING`:

```text
source returns
routes restored
ACTIVE
```

If state was `FAILED`:

```text
source return alone does not recreate routes
explicit retry required
```

unless you intentionally redefine `FAILED`.

## 12.3 Mandatory Tests

- source unavailable at initial start;
- source lost during ACTIVE;
- source returns;
- explicit retry path;
- no secret active-route/FAILED mismatch.

---

# 13. DEFECT J — `lastUsedAt` Semantics

## 13.1 Problem

`lastUsedAt` should not be updated merely because activation was requested.

A failed activation should not necessarily become the “most recently used successfully” session.

## 13.2 Required Semantics

Recommended:

Update `lastUsedAt` on:

```text
transition to ACTIVE
```

Optionally also on:

```text
meaningful successful DEGRADED activation
```

if the product considers degraded playback a successful session use.

Do not update on:

```text
activate request only
validation failure
immediate terminal failure
```

## 13.3 Tests

- failed activation does not update lastUsed;
- successful activation does;
- successful degraded activation follows documented rule.

---

# 14. DEFECT K — Stable Source Restoration

## 14.1 Problem

The persisted source reference may be too tightly tied to a runtime PipeWire identifier.

Node IDs and some serials may change across restart or graph recreation.

## 14.2 Required Audit

Inspect the existing `AudioSource` abstraction.

Determine whether its persisted `id` is truly stable enough.

If not, extend the persisted source reference to include stable matching metadata such as:

```text
stable application source ID
node name
media/application identity
device identity
source kind
human-readable label
other existing stable properties
```

Do not persist only a transient numeric PipeWire object ID.

## 14.3 Restore Algorithm

On load:

```text
persisted source descriptor
    |
    v
current source registry
    |
match by strongest stable identity
    |
fallback if allowed
    |
resolved runtime source
```

If unresolved:

```text
session remains safe
source marked unavailable
no crash
```

## 14.4 Tests

Simulate:

```text
run 1 source runtime ID = 41
save

restart

same logical source runtime ID = 93

load
resolve
activate successfully
```

---

# 15. DEFECT L — `sessionError` Must Be Meaningful

## 15.1 Problem

A signal may exist but not actually be emitted consistently.

## 15.2 Required Behavior

Emit session-domain errors for material failures such as:

- persistence failure;
- source unavailable;
- route creation failure;
- member route failure;
- reconnect exhaustion;
- invalid operation;
- unrecoverable runtime failure.

Avoid noisy duplicate emissions.

## 15.3 Clear Stale Errors

When a previously failed member recovers:

```text
member lastError
```

should be cleared or replaced with a healthy state.

Session-wide errors should also be reconciled so the UI does not show a stale failure after recovery.

## 15.4 Tests

- error emitted on route failure;
- member error populated;
- member recovers;
- stale member error cleared;
- session error state no longer claims failure.

---

# 16. DEFECT M — Invalid Volume Must Be Rejected

## 16.1 Required Validation

Reject:

```text
NaN
+infinity
-infinity
```

Do not silently turn them into `0.0`.

For finite out-of-range values, choose consistent behavior:

- clamp;
- or reject.

Recommended:

```text
NaN/inf -> reject
finite below/above range -> clamp
```

if Phase 5 already clamps finite values.

## 16.2 Tests

```cpp
setGroupVolume(NaN) -> InvalidArgument
setGroupVolume(+inf) -> InvalidArgument
setDeviceVolume(-inf) -> InvalidArgument
```

and:

```text
1.4 -> 1.0
-0.2 -> 0.0
```

only if clamping is the chosen existing convention.

---

# 17. DEFECT N — README / Documentation Status

Search for stale statements such as:

```text
Phase 6+: Not implemented
```

Update documentation to reflect actual project status **only after all final gates pass**.

Do not prematurely state Phase 6 complete.

At minimum update:

- README phase status;
- Phase 6 implementation doc;
- validation/audit doc;
- hardware-test instructions;
- known limitations.

---

# 18. State Machine Hardening

After applying the fixes, audit the entire state machine against these invariants.

## 18.1 Allowed State Behavior

### `IDLE`

- no active session-owned routes;
- no reconnect intent;
- no route recovery intent.

### `STARTING`

- route creation allowed;
- device connection intent allowed if policy permits.

### `ACTIVE`

- all required enabled members healthy;
- active route set matches desired state.

### `DEGRADED`

- at least one useful healthy route remains;
- one or more intended members unavailable;
- no false claim of complete health.

### `RECOVERING`

- active recovery work exists;
- or source/member restoration is actively expected.

### `STOPPING`

- no new routes;
- no new reconnect attempts;
- cleanup only.

### `FAILED`

- no new routes;
- no reconnect attempts unless explicit retry transitions state;
- remains quiescent.

## 18.2 Mandatory Invariant Tests

Assert:

```text
IDLE -> zero session-owned active routes
```

```text
FAILED -> zero newly-created routes after entry
```

```text
STOPPING -> no route creation
```

```text
removed member -> no route
```

```text
disabled member -> no route
```

```text
healthy peer remains active during another member failure
```

---

# 19. Routing Reconciliation Hardening

`RoutingCoordinator` must remain idempotent.

Test repeated calls with identical desired state.

Expected:

```text
reconcile
reconcile
reconcile
```

must not produce:

```text
duplicate route A
duplicate route A
duplicate route A
```

Ensure stale route replacement works when endpoint runtime ID changes.

Example:

```text
Device B -> endpoint 52
route RB52

endpoint 52 removed
endpoint 87 appears for same Device B

old RB52 invalid
new RB87 created
```

No old stale route metadata should remain authoritative.

---

# 20. Recovery Ownership Model

After correction, document ownership clearly:

```text
SessionManager
    owns:
      session-level recovery intent
      member desired presence
      route restoration policy
      recovery state

BluetoothManager / lifecycle layer
    owns:
      Bluetooth reconnect execution
      retry timing
      exponential backoff
      attempt counting
      lower-level failure

RoutingCoordinator
    owns:
      desired vs actual route reconciliation

VolumeCoordinator
    owns:
      reapplying intended member volume/mute after route restoration
```

There must be no ambiguous retry ownership.

---

# 21. Recovery Cancellation Matrix

Implement and test this matrix:

| Event | Cancel route recovery | Cancel Bluetooth reconnect intent |
|---|---:|---:|
| stop session | yes | yes |
| delete session | yes | yes |
| remove member | yes | yes |
| disable member | yes | yes |
| `autoReconnect=false` | maybe route restore depends on policy | yes |
| policy `None` | yes | yes |
| policy `RestoreRoutesOnly` | no route recovery | yes Bluetooth reconnect |
| policy `ReconnectAndRestore` | no | no |
| switch active session | yes | yes |
| shutdown | yes | yes |

Clarify the “route recovery” column according to the final policy semantics.

---

# 22. Multi-Session Safety

Retest the chosen policy.

If current design allows only one active session:

```text
Session A ACTIVE
activate Session B
```

must have explicit behavior:

- reject;
- or clean switch.

No accidental overlapping recovery from Session A may survive after Session B becomes active.

Add a stale recovery regression test across session switching.

---

# 23. Persistence Schema Validation

Do not change schema version unnecessarily.

If new persisted source fields or semantics require a format change:

- bump schema version;
- provide migration from current Phase 6 schema;
- test old schema loading;
- preserve session IDs, names, devices, volume, policy.

Unknown future schema versions should fail safely without crash.

---

# 24. Persistence Error Model

Use a structured error result.

Conceptual:

```cpp
enum class SessionPersistenceError {
    None,
    DirectoryCreationFailed,
    OpenFailed,
    WriteFailed,
    CommitFailed,
    ParseFailed,
    UnsupportedSchema,
    InvalidRecord
};
```

Follow existing project style.

Log:

```text
SessionPersistenceFailed
session=<id if known>
path=<path>
reason=<...>
```

Do not log sensitive unrelated data.

---

# 25. Application Startup Restore

Verify:

```text
ApplicationCore starts
    |
load saved session definitions
    |
all runtime states initially safe
    |
device/source resolution occurs only after registries ready
```

Do not let persistence load prematurely trigger routing before AudioRouter/PipeWire/DeviceRegistry is ready.

If there is an auto-restore intent feature:

- gate activation until required services are ready;
- apply current recovery policy;
- use fresh runtime endpoint resolution.

---

# 26. Shutdown Safety

Audit shutdown in real ownership order.

Required:

```text
SessionManager:
    stop recovery
    cancel callbacks
    deactivate session-owned routes
    flush persistence
```

before:

```text
AudioRouter destroyed
PipeWire services destroyed
Bluetooth services destroyed
```

Test or reason carefully about synchronous Qt signal reentrancy.

If route removal emits signals synchronously during session stop, ensure:

- STOPPING state suppresses route recreation;
- reconciliation guard prevents recursive creation;
- no use-after-destroy.

Add a focused test if fake router can emit synchronously from `removeRoute()`.

---

# 27. Reentrancy Regression Test

Create a fake AudioRouter that emits route-state/removal signals synchronously during:

```cpp
removeRoute(...)
```

Then:

```text
ACTIVE
deactivateSession
```

must:

```text
enter STOPPING
remove routes
receive synchronous callbacks
NOT recreate routes
finish IDLE
```

This test directly protects shutdown and stop behavior.

---

# 28. Test Coverage Expansion — Required Cases

The final Phase 6 suite should include all of these.

## 28.1 Policy Tests

- None
- RestoreRoutesOnly
- ReconnectAndRestore

## 28.2 Recovery

- member lost
- member returns with same endpoint ID
- member returns with new endpoint ID
- route failure only
- Bluetooth disconnect
- endpoint disappears while Bluetooth remains connected
- recovery cancellation
- recovery exhaustion if lower layer exposes it

## 28.3 State

- FAILED remains quiescent
- explicit retry leaves FAILED
- stop during RECOVERING
- rapid start-stop-start
- stale callback generation ignored

## 28.4 Persistence

- first-run missing parent directory
- save failure propagation
- malformed file
- schema migration if needed
- source runtime ID changes across restart

## 28.5 Volume

- current group volume restored to returning member
- current trim restored
- mute restored
- invalid non-finite input rejected

## 28.6 Error Observability

- sessionError emission
- member error set
- member error cleared after recovery

## 28.7 Multi-session

- no stale recovery from old active session
- no duplicate endpoint ownership conflict if single-active policy

---

# 29. Suggested Test Names

Follow existing naming conventions, but consider cases equivalent to:

```text
tst_SessionRecoveryPolicy
tst_SessionManagedReconnect
tst_SessionFailedStateInvariant
tst_SessionPersistenceFirstRun
tst_SessionPersistenceFailure
tst_SessionRecoveryGeneration
tst_SessionMemberReturnIntegration
tst_SessionSourceRecoveryIntegration
tst_SessionStopReentrancy
tst_SessionVolumeValidation
tst_SessionErrorPropagation
```

Do not create redundant test executables if these cases fit existing files.

---

# 30. Optional Live Hardware Test Upgrade

The current live Phase 6 test must not merely prove initialization.

If two compatible devices are available, optional live verification should test:

```text
connect both
create session
route to both
ACTIVE
change group volume
disconnect one
remaining device continues
DEGRADED / RECOVERING
reconnect
route returns
ACTIVE
stop
clean routes
```

Gate it behind environment variables.

Never make default `ctest` depend on hardware.

If hardware is unavailable:

```text
SKIPPED — hardware not configured
```

is acceptable.

Do not claim live validation happened when it did not.

---

# 31. Example Environment Variable Documentation

Use shell-safe examples.

Good:

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="AA:BB:CC:DD:EE:FF;11:22:33:44:55:66" \
ctest --test-dir build \
-R tst_SessionLiveIntegration \
--output-on-failure
```

Do not publish:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

because shell interprets angle brackets.

---

# 32. Build Warnings and Static Quality

New correction code must not add warnings.

If configured:

```text
-Wall
-Wextra
-Wpedantic
```

keep it clean.

Run sanitizers if project supports them.

Especially look for:

- dangling timer captures;
- use-after-session-delete;
- duplicate QObject connections;
- queued callback after shutdown;
- iterator invalidation during member removal;
- route callback reentrancy.

---

# 33. No Regression of Phase 0–5

The correction pass may require touching:

- BluetoothManager;
- DeviceLifecycleManager;
- ReconnectPolicy;
- AudioRouter;
- persistence.

If so, add direct regression tests.

The following must continue to work:

- Bluetooth discovery;
- pairing;
- connection lifecycle;
- lower-level reconnect policy;
- endpoint registry;
- endpoint resolver;
- AudioRouter route lifecycle;
- Phase 5 volume;
- desktop startup.

Never weaken old tests to make Phase 6 pass.

---

# 34. Recommended Implementation Sequence

Use this order.

## Step 1 — Reproduce/Audit

- inspect current code;
- run baseline tests;
- map current recovery flow;
- identify exact files.

## Step 2 — Fix Recovery Ownership

- remove competing SessionManager Bluetooth retry loop;
- integrate with managed Bluetooth reconnect path;
- add cancellation.

## Step 3 — Make Recovery Policy Authoritative

- implement exact `None`;
- `RestoreRoutesOnly`;
- `ReconnectAndRestore`;
- add policy tests.

## Step 4 — Fix FAILED-State Invariant

- suppress reconciliation route creation in invalid states;
- define source recovery semantics;
- add tests.

## Step 5 — Fix Generation Safety

- capture exact operation generation;
- revalidate all callbacks;
- add rapid start-stop-start test.

## Step 6 — Fix Persistence Reliability

- create parent directory;
- surface failures;
- migration/source reference if needed;
- add tests.

## Step 7 — Fix Settings Cancellation

- autoReconnect;
- policy change;
- member disabled;
- removal;
- switch;
- shutdown.

## Step 8 — Error/Volume/lastUsed Cleanup

- sessionError;
- clear stale errors;
- reject non-finite volume;
- correct lastUsed timestamp.

## Step 9 — Integration Recovery Tests

- member disconnect/return/new endpoint ID;
- source loss/return;
- reentrancy.

## Step 10 — Full Regression

- clean build;
- full CTest;
- desktop startup;
- optional hardware tests.

## Step 11 — Documentation

- update Phase 6 docs;
- README;
- final audit.

---

# 35. Detailed Definition of “100% Phase 6”

The Phase 6 code is considered fully complete only when all mandatory software gates below are met.

## 35.1 Recovery Policy

- [ ] `None` never automatically restores lost routes
- [ ] `RestoreRoutesOnly` never initiates Bluetooth reconnect
- [ ] `RestoreRoutesOnly` restores route after externally returning endpoint
- [ ] `ReconnectAndRestore` delegates Bluetooth reconnect to lower lifecycle subsystem
- [ ] recovered member route is restored
- [ ] healthy peers are preserved

## 35.2 Retry Ownership

- [ ] no duplicate Bluetooth retry loop in SessionManager
- [ ] Phase 3 backoff remains authoritative
- [ ] reconnect can be cancelled from session intent changes

## 35.3 State Integrity

- [ ] FAILED cannot silently create active routes
- [ ] IDLE has no active session-owned routes
- [ ] STOPPING creates no routes
- [ ] explicit retry behavior tested
- [ ] source loss semantics tested

## 35.4 Callback Safety

- [ ] exact generation captured
- [ ] stale generation ignored
- [ ] session deletion safe
- [ ] member deletion safe
- [ ] stop/start race safe
- [ ] switch-session race safe

## 35.5 Persistence

- [ ] parent directory created
- [ ] atomic save
- [ ] failure surfaced
- [ ] schema validated
- [ ] runtime PipeWire IDs not treated as stable identity
- [ ] source restores across runtime ID change
- [ ] safe startup load

## 35.6 Recovery Cancellation

- [ ] stop cancels
- [ ] delete cancels
- [ ] remove member cancels
- [ ] disable member cancels
- [ ] policy None cancels
- [ ] autoReconnect false cancels Bluetooth reconnect intent
- [ ] shutdown cancels

## 35.7 Recovery Integration

- [ ] 2-device ACTIVE
- [ ] one member lost
- [ ] peer continues
- [ ] session DEGRADED/RECOVERING
- [ ] member returns with new endpoint
- [ ] route restored
- [ ] volume restored
- [ ] mute restored
- [ ] state ACTIVE

## 35.8 Volume/Error Semantics

- [ ] NaN rejected
- [ ] infinity rejected
- [ ] finite bounds handled consistently
- [ ] errors emitted
- [ ] stale member error cleared after recovery

## 35.9 Timestamps

- [ ] failed activation does not falsely update lastUsed
- [ ] successful session use does

## 35.10 Regression

- [ ] Phase 0 tests pass
- [ ] Phase 1 tests pass
- [ ] Phase 2 tests pass
- [ ] Phase 3 tests pass
- [ ] Phase 4 tests pass
- [ ] Phase 5 tests pass
- [ ] Phase 6 tests pass
- [ ] desktop target builds
- [ ] desktop starts when environment supports it

---

# 36. “100%” and Hardware Validation

Software completion and hardware validation must be reported separately.

If all software gates pass but two-device live hardware is unavailable:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 LIVE HARDWARE VALIDATION: NOT RUN — HARDWARE UNAVAILABLE
```

Do not downgrade a fully deterministic software implementation solely because the CI machine lacks two physical devices.

However, if the project’s own official roadmap explicitly defines live two-device hardware proof as a mandatory Phase 6 exit gate, then the final overall status must remain:

```text
PHASE 6 STATUS: NOT COMPLETE
```

until that physical validation is performed.

Follow the repository roadmap exactly.

Never fake hardware proof.

---

# 37. Final Validation Commands

Run a clean final build:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Then run Phase 6-targeted tests:

```bash
ctest --test-dir build \
-R "Session|session" \
--output-on-failure
```

If available, launch desktop:

```bash
./build/apps/desktop/auralis-desktop
```

If sanitizers are supported:

```bash
ctest --test-dir build --output-on-failure
```

under the sanitizer-enabled build profile.

Record exact output summary.

---

# 38. Final Code Audit Checklist

Before declaring completion, search for dangerous leftovers.

## 38.1 Competing reconnect calls

```bash
grep -R "reconnectDevice" -n src include tests
```

Verify SessionManager does not repeatedly force direct reconnect calls against the lower managed policy.

## 38.2 Fixed recovery timers

```bash
grep -R "start(500\|500)" -n src/session include/auralis/session
```

Investigate any session-layer fixed reconnect timers.

## 38.3 Silent persistence

```bash
grep -R "persistAll" -n src include
```

Verify failures are not ignored.

## 38.4 Failed-state reconciliation

Search every caller of:

```text
reconcileActiveSession
RoutingCoordinator::reconcile
```

and verify state gating.

## 38.5 Stale README

```bash
grep -R "Phase 6.*Not implemented\|Phase 6+" -n README* docs
```

Update stale claims only after validation.

---

# 39. Required Documentation Update

Update/create a final Phase 6 validation document such as:

```text
docs/validation/phase-6-final-closure.md
```

It must contain:

1. baseline;
2. identified defects;
3. changes made;
4. state/recovery semantics;
5. recovery ownership;
6. persistence changes;
7. tests added;
8. exact test commands;
9. exact pass counts;
10. hardware test status;
11. remaining limitations;
12. final verdict.

Do not merely overwrite history.

Keep earlier audit if useful, and add a final closure document.

---

# 40. Required Final AI-IDE Report

At the end of the coding task, output a detailed report with the following exact structure.

## A. Executive Summary

State whether every known Phase 6 defect was corrected.

## B. Files Changed

For each important file:

```text
path
purpose
what changed
```

## C. Recovery Policy Semantics

State exact behavior of:

```text
None
RestoreRoutesOnly
ReconnectAndRestore
```

## D. Reconnect Ownership

Explain which layer owns:

- reconnect timing;
- backoff;
- reconnect cancellation;
- route restoration.

## E. State-Machine Corrections

Explain how `FAILED`, `STOPPING`, `IDLE`, and `RECOVERING` are protected.

## F. Persistence Corrections

Explain:

- parent directory;
- save errors;
- atomic write;
- source restoration;
- schema version/migration if changed.

## G. Race/Generation Safety

Describe stale callback protection.

## H. Test Additions

List exact tests.

## I. Full Test Results

Include exact command and summary:

```bash
ctest --test-dir build --output-on-failure
```

Example reporting format:

```text
Total tests: 47
Passed: 47
Failed: 0
Skipped: 2
```

Use actual values.

## J. Live Hardware Validation

State exactly what was or was not run.

## K. Remaining Limitations

List only real remaining limitations.

## L. Final Verdict

End with exactly one of:

```text
PHASE 6 STATUS: COMPLETE
```

or:

```text
PHASE 6 STATUS: NOT COMPLETE
```

If not complete, list every blocker immediately after.

---

# 41. Hard Prohibitions

Do not:

- rewrite Phase 6 from scratch;
- bypass AudioRouter;
- manipulate PipeWire directly from SessionManager;
- call BlueZ directly from SessionManager;
- create a second Bluetooth reconnect engine;
- ignore persistence failures;
- silently convert NaN to zero;
- allow `FAILED` state with newly-active routes;
- allow disabled/removed members to recover;
- let stale callbacks survive generation changes;
- weaken Phase 0–5 tests;
- mark hardware tests PASS if hardware was not used;
- state Phase 6 complete based only on file presence;
- jump to Phase 7.

---

# 42. Preferred Engineering Style

Make narrowly-scoped, reviewable changes.

Prefer:

```text
fix invariant
add regression test
run test
```

over:

```text
large architectural rewrite
```

Reuse existing:

- result types;
- ID types;
- logging;
- QObject ownership;
- fake backends;
- test helpers;
- persistence layer;
- Bluetooth lifecycle APIs.

Add abstractions only where they eliminate a real correctness problem.

---

# 43. Suggested Commit Breakdown

If commits are allowed:

```text
phase-6: make recovery policies authoritative
phase-6: delegate reconnect scheduling to Bluetooth lifecycle
phase-6: enforce failed-state route invariants
phase-6: harden session persistence and error propagation
phase-6: cancel stale recovery and validate operation generations
phase-6: strengthen volume and session error semantics
phase-6: add full member-return and source recovery tests
phase-6: finalize Phase 6 validation documentation
```

---

# 44. Core Regression Scenarios

The following should all pass in the final implementation.

---

## Scenario 1 — Happy Two-Device Session

```text
A + B available
source available

activate

A route active
B route active
ACTIVE
```

---

## Scenario 2 — One Member Lost, No Recovery

```text
policy = None

ACTIVE
B disappears

A remains active
B absent
DEGRADED

B endpoint returns externally

B route does NOT return automatically
```

---

## Scenario 3 — Route-Only Recovery

```text
policy = RestoreRoutesOnly

ACTIVE
B disappears

no Bluetooth reconnect requested

B returns externally

B route restored
ACTIVE
```

---

## Scenario 4 — Managed Reconnect and Restore

```text
policy = ReconnectAndRestore

ACTIVE
B disconnects

SessionManager requests managed recovery intent
Bluetooth lifecycle owns retries/backoff

B reconnects
new endpoint appears
route restored
volume/mute restored
ACTIVE
```

---

## Scenario 5 — Stop During Recovery

```text
B recovering

stop

STOPPING
all routes removed
all recovery intent cancelled
IDLE

late reconnect/endpoint event arrives

no route recreated
```

---

## Scenario 6 — Disable Member During Recovery

```text
B recovering

setDeviceEnabled(B, false)

pending recovery cancelled
B later reconnects

no B route
session health based only on enabled members
```

---

## Scenario 7 — Policy Changed to None

```text
B recovering

setRecoveryPolicy(None)

recovery cancelled

endpoint returns

no automatic route restoration
```

---

## Scenario 8 — Failed Source

```text
source unavailable

activate

FAILED
no routes

source appears

still FAILED
no routes

retrySession

STARTING
routes created
ACTIVE
```

---

## Scenario 9 — Source Lost While Active

Use the final documented semantics and verify them completely.

---

## Scenario 10 — Stale Generation

```text
start generation 100
schedule recovery
stop
start generation 102

old generation 100 callback executes

ignored
```

---

## Scenario 11 — First-Run Persistence

```text
app-data parent does not exist

create session

parent created
sessions file committed
reload succeeds
```

---

## Scenario 12 — Persistence Failure

```text
save backend fails

public mutation surfaces error
sessionError emitted
behavior matches rollback/dirty-state policy
```

---

## Scenario 13 — Runtime Source ID Changes

```text
save logical source
restart
runtime PipeWire ID changed
logical source matched
activation succeeds
```

---

## Scenario 14 — Synchronous Route Removal Signals

```text
stop active session
router emits callbacks synchronously
no recursive route recreation
IDLE
```

---

# 45. Final Quality Bar

A Phase 6 implementation is not “100%” because all files compile.

It is complete only when it is behaviorally consistent under:

```text
success
partial failure
recovery
policy changes
stop
delete
restart
stale callbacks
source changes
persistence failure
```

and those behaviors are automated and repeatable.

---

# 46. Final Completion Statement

Use this as the definitive target:

> **Phase 6 is complete when the existing Auralis Multi-Device Session Engine has one authoritative recovery policy, one authoritative Bluetooth reconnect owner, consistent state/route invariants, durable and error-aware persistence, generation-safe asynchronous recovery, correct degraded operation, verified member return with endpoint re-resolution, restored volume/mute state, stable source restoration, deterministic error reporting, and a fully passing Phase 0–6 regression suite.**

---

# 47. Begin

Start with the repository audit and baseline tests.

Then fix the defects in the order given.

Do not ask the user to re-explain Phase 6.

Do not move to Phase 7.

Do not declare completion until the final validation report proves every applicable closure gate.
