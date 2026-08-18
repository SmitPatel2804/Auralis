# Auralis — Phase 5 Audio Routing Engine
# Correction, Hardening, Regression & Final Closure Prompt

> **Purpose:** This prompt is for Cursor AI IDE (or another capable coding agent) running inside the Auralis Linux development machine.
>
> **Repository state assumed:** Phases 0, 1, 2, 3, and 4 are complete. Phase 5 is substantially implemented, but a focused audit found several correctness, ownership, lifecycle, thread-safety, recovery, and test-coverage gaps.
>
> **Goal:** Correct the existing Phase 5 implementation **without rewriting the architecture**, preserve all working Phase 0–4 behavior, close every blocker listed in this document, add rigorous regression tests, run the full build/test matrix, and produce a final Phase 5 closure audit.

---

# 1. Your Role

Act as a **senior C++/Qt 6/PipeWire Linux audio systems engineer** working on the Auralis desktop application.

You must reason about:

- PipeWire native client APIs and object/proxy lifecycle
- PipeWire thread-loop synchronization
- asynchronous global-ID assignment
- graph object volatility
- route ownership and additive routing
- Qt object lifetime and signal/property semantics
- deterministic route state machines
- transactional rollback
- graph churn/rebind behavior
- Bluetooth endpoint disappearance/recreation
- multi-route concurrency
- integration-test correctness
- regression safety across Phases 0–4

Do not treat this as a code-generation exercise where compilation alone is success.

The implementation is complete only when the required invariants are demonstrably true.

---

# 2. Mandatory First Step — Inspect Before Editing

Before changing anything:

1. Read the repository root `README.md`.
2. Read:
   - `docs/architecture.md`
   - `docs/phase-5-validation.md`
3. Inspect all existing Phase 5 code, especially:
   - `include/auralis/audio/AudioRoute.h`
   - `include/auralis/audio/AudioRouter.h`
   - `include/auralis/audio/IPipeWireLinkBackend.h`
   - `include/auralis/audio/LinkManager.h`
   - `include/auralis/audio/PipeWireConnection.h`
   - `include/auralis/audio/PipeWireTypes.h`
   - `src/audio/AudioRoute.cpp`
   - `src/audio/AudioRouter.cpp`
   - `src/audio/LinkManager.cpp`
   - `src/audio/PipeWireConnection.cpp`
   - `src/audio/PipeWireTypes.cpp`
   - `src/audio/RoutePlanner.cpp`
   - `src/audio/VolumeController.cpp`
   - `ui/components/RoutePanel.qml`
   - relevant `CMakeLists.txt`
4. Inspect all Phase 5 tests:
   - `tests/unit/audio/tst_AudioRoute.cpp`
   - `tests/unit/audio/tst_AudioSources.cpp`
   - `tests/unit/audio/tst_RoutePlanner.cpp`
   - `tests/unit/audio/tst_AudioRouter.cpp`
   - `tests/unit/audio/tst_VolumeController.cpp`
   - `tests/integration/tst_AudioRoutingLiveIntegration.cpp`
5. Inspect the existing PipeWire fake/mock backend used by tests.
6. Search the full repository for:
   - `createLink`
   - `destroyOwnedLink`
   - `globalId`
   - `OwnedLink`
   - `auralis.route.id`
   - `activationTimer_`
   - `volumeSupported`
   - `pw_thread_loop_lock`
   - `routeStateChanged`
   - `routeError`
   - `linksOperational`
   - `bindPendingLinkIds`
7. Understand how `PipeWireManager` forwards PipeWire graph and connection-state changes into `AudioRouter`.

Do not begin by inventing new architecture.

Modify the smallest coherent set of interfaces required to make ownership and lifetime correct.

---

# 3. Existing Phase 5 Architecture Must Be Preserved

The current Phase 5 architecture is fundamentally correct and must remain recognizable.

Existing concepts that should be retained unless a change is strictly necessary:

- `AudioRoute`
- `AudioRouter`
- `RoutePlanner`
- `LinkManager`
- `VolumeController`
- `IPipeWireLinkBackend`
- `PipeWireConnection`
- `PipeWireObjectStore`
- `AudioEndpointRegistry`
- `AudioSource`
- `AudioSourceListModel`
- native PipeWire `link-factory`
- additive routing
- multi-destination fan-out
- route recovery through graph replacement/replanning
- QML route control surface
- unit tests plus opt-in live PipeWire integration test

This task is **Phase 5 correction/hardening**, not Phase 5 replacement.

---

# 4. Hard Scope Boundary

## 4.1 Do NOT regress Phases 0–4

Do not break:

- project/bootstrap architecture
- Bluetooth discovery/connection
- BlueZ integration
- PipeWire connection lifecycle
- PipeWire graph observation
- audio endpoint discovery
- Bluetooth-device-to-PipeWire endpoint mapping
- stable logical endpoint identity
- existing build/test workflows

All existing non-Phase-5 tests must continue to pass.

---

## 4.2 Do NOT implement Phase 6

Do not add a synchronization/session engine.

Specifically, do not introduce Phase 6 features such as:

- `SessionManager`
- persistent multi-device session groups
- cross-device clock synchronization
- latency alignment
- sample-delay compensation
- per-device drift correction
- DSP synchronization pipelines
- session persistence
- recovery orchestration beyond Phase 5 route rebind behavior

Phase 5 supports one logical route with one source and one-or-more selected destinations.

It does not become the future synchronized session engine.

---

## 4.3 No shell routing

Production code must not use:

- `pw-link`
- `wpctl`
- `pactl`
- `pacmd`
- shell scripts
- `QProcess` for audio routing

Continue using the native PipeWire API.

---

# 5. Current Audit Result

Phase 5 is substantially implemented but **must not yet be marked complete**.

The following blockers were confirmed in the current repository and must all be addressed.

---

# 6. Blocker 1 — PipeWire Disconnect Leaves Route Incorrectly `Active`

Current behavior in `AudioRouter::handleConnectionState()` clears runtime IDs and records `PipeWireDisconnected`, but an enabled route can remain:

```text
RouteState::Active
```

while PipeWire is:

```text
Error / Stopping / Stopped
```

This is semantically false.

An `Active` route means the required Auralis-owned runtime links are currently valid and operational.

If the PipeWire core is gone, that statement cannot be true.

The existing unit test currently reinforces this incorrect behavior by asserting that the route remains `Active` after connection error.

## Required behavior

For an enabled route:

```text
Active
  -> PipeWire Error/Stopping/Stopped
  -> Degraded
```

Requirements:

- `route.enabled` remains `true` so logical intent is retained.
- runtime PipeWire link ownership/bindings are invalidated.
- stale global IDs are not retained.
- route error becomes `RouteError::PipeWireDisconnected`.
- route state is no longer `Active`.
- automatic replan must not happen until:
  - connection state is `Connected`, AND
  - initial graph synchronization is complete.
- after fresh graph sync:
  - stable source identity is resolved again,
  - stable destination identity is resolved again,
  - new node/port IDs are planned,
  - fresh Auralis-owned links are created,
  - route returns to `Active` only when those new owned links are operational.

## Required test correction

Rewrite the disconnect unit test so it verifies:

1. route starts `Active`;
2. connection enters `Error`;
3. route remains logically enabled;
4. route state becomes `Degraded`;
5. runtime link ownership is invalidated/cleared;
6. error is `PipeWireDisconnected`;
7. `Connected` with `initialSyncComplete == false` does **not** reactivate;
8. `Connected` with `initialSyncComplete == true` triggers replan;
9. route eventually becomes `Active`;
10. new runtime link identities differ from the stale pre-disconnect identities.

Do not keep a test that expects `Active` during PipeWire failure.

---

# 7. Blocker 2 — Link Ownership Depends Too Heavily on PipeWire Global ID

The current `IPipeWireLinkBackend::createLink()` returns:

```cpp
std::optional<quint32>
```

where the returned value is treated as the PipeWire global link ID.

However, native `pw_core_create_object()` is asynchronous.

The newly-created `pw_proxy` can exist successfully while:

```text
pw_proxy_get_bound_id(proxy)
```

has not yet produced a valid global ID.

The current implementation represents that successful-but-not-yet-bound case as:

```text
globalId == 0
```

This creates a serious ownership hole.

## Failure example

```text
Create route with two stereo pairs.

Pair 1:
  pw_core_create_object succeeds
  proxy exists
  global ID not assigned yet
  createLink returns 0

Pair 2:
  creation fails

LinkManager rollback:
  sees Pair 1 globalId == 0
  destroyOwnedLink is not called
  Pair 1 proxy survives

Later:
  PipeWire binds Pair 1
  real graph link appears
  AudioRouter no longer owns/tracks it
```

That violates transactional activation and the Phase 5 no-stale-link invariant.

---

# 8. Required Ownership Redesign — Immediate Opaque Ownership Handle

A successful `pw_core_create_object()` must return an ownership identity **immediately**, even if the PipeWire global ID does not yet exist.

Implement an opaque Auralis-side ownership handle/token.

The exact names may differ, but the design must have these properties.

## 8.1 Suggested data shape

For example:

```cpp
struct PipeWireOwnedLinkHandle {
    quint64 token = 0;      // Auralis-local ownership identity; valid immediately
    quint32 globalId = 0;   // 0 until PipeWire assigns it
};
```

or equivalent.

`OwnedLink` should retain an opaque ownership token independent of the volatile PipeWire global ID.

For example:

```cpp
struct OwnedLink {
    QString routeId;
    QString destinationId;

    quint32 outputNodeId = 0;
    quint32 outputPortId = 0;
    quint32 inputNodeId = 0;
    quint32 inputPortId = 0;

    quint64 ownershipToken = 0;
    quint32 globalId = 0;

    QString channel;
};
```

Do not expose raw `pw_proxy*` pointers to `AudioRouter` or `LinkManager`.

Use an opaque scalar/value handle or another RAII-safe backend abstraction.

---

## 8.2 Backend lifecycle requirements

The PipeWire backend must maintain authoritative ownership mapping such as:

```text
ownershipToken -> created BoundProxy
ownershipToken -> current globalId (possibly 0)
globalId -> BoundProxy
```

A unique ownership token must be allocated before or immediately after successful `pw_core_create_object()`.

Once `createLink()` reports success:

- rollback must be able to destroy that created proxy by ownership token;
- deactivation must be able to destroy it by ownership token;
- route removal must be able to destroy it by ownership token;
- shutdown must be able to destroy it by ownership token;
- this must work whether global ID is:
  - already assigned,
  - still pending,
  - later removed,
  - already invalidated.

Destroy operations must be idempotent.

---

## 8.3 API direction

A coherent API could look conceptually like:

```cpp
struct LinkCreateResult {
    quint64 ownershipToken = 0;
    quint32 globalId = 0;
};

virtual std::optional<LinkCreateResult> createLink(...);

virtual bool destroyOwnedLink(quint64 ownershipToken);

virtual quint32 ownedLinkGlobalId(quint64 ownershipToken) const;
```

or another design with equivalent semantics.

The exact interface is not mandatory.

The invariant is mandatory:

> **Auralis ownership must never depend on waiting for a PipeWire global ID.**

---

# 9. Blocker 3 — Never Infer Ownership From Matching Ports

The current implementation contains topology matching equivalent to:

```cpp
if (link.outputPort == owned.outputPortId &&
    link.inputPort == owned.inputPortId) {
    owned.globalId = link.globalId;
}
```

This occurs in the pending-link binding path.

The current `linksOperational()` path also falls back to finding any link with the same input/output ports and may treat that link as satisfying the Auralis route.

This is unsafe.

## Why

A foreign link created by:

- WirePlumber
- another application
- another PipeWire client
- a pre-existing session manager

may connect the exact same ports.

Auralis must not conclude:

```text
same ports => my link
```

That can cause:

- incorrect ownership assignment;
- false activation success;
- wrong destruction behavior;
- corruption of additive routing semantics.

---

# 10. Required Ownership Resolution

Delete or replace topology-based ownership inference in:

- `AudioRouter::bindPendingLinkIds()`
- `AudioRouter::linksOperational()`
- any equivalent code elsewhere.

Use the Auralis ownership token/backend mapping as authoritative.

The backend that created the proxy knows which proxy belongs to which token.

When the proxy receives a real global ID through the PipeWire bound event, update the token-to-global-ID mapping.

The router may then synchronize:

```text
OwnedLink.globalId <- backend global ID for OwnedLink.ownershipToken
```

without scanning arbitrary graph links.

The object store remains useful for:

- observing the global link state;
- checking `Active`, `Paused`, `Error`;
- diagnostics;
- integration validation.

It must not become the authority for ownership merely because topology matches.

---

# 11. Preserve Auralis Ownership Metadata

Continue attaching explicit properties when creating links, including:

```text
application.name = Auralis
auralis.route.id = <route-id>
```

If helpful, add another non-persistent diagnostic property such as:

```text
auralis.link.token = <token>
```

provided it is safe and does not become the sole lifecycle mechanism.

The authoritative ownership still belongs to the creating backend/proxy.

Properties are excellent for:

- diagnostics;
- integration-test verification;
- graph inspection;
- stale-link detection.

---

# 12. Blocker 4 — Source/Destination Disappearance Must Clean Runtime Ownership

Current graph-change behavior transitions a route to `Degraded` when its source or destination disappears, but runtime ownership is not comprehensively cleared at that point.

For a route with recovery enabled, logical intent should survive, but stale runtime bindings should not.

## Required source-loss behavior

When the stable logical source can no longer resolve:

```text
enabled = true
state = Degraded
error = SourceRemoved
runtime links = destroyed/forgotten safely
```

Do not retain stale node/port/global IDs as if still valid.

## Required destination-loss behavior

When any required destination can no longer resolve:

```text
enabled = true
state = Degraded
error = DestinationRemoved
runtime links = destroyed/forgotten safely
```

The implementation must destroy every Auralis-owned proxy that still exists.

If PipeWire has already removed a proxy, cleanup must remain idempotent.

## Recovery

When source/destination identities reappear with new PipeWire IDs:

- wait for a coherent graph;
- re-run `RoutePlanner`;
- create new links;
- reach `Active` only after new owned links are operational.

---

# 13. Blocker 5 — PipeWire Thread-Loop Synchronization

The current PipeWire backend has public operations that inspect loop-owned data before acquiring the PipeWire thread-loop lock.

Examples include patterns equivalent to:

```cpp
BoundProxy* bound = proxies.value(nodeId);
...
pw_thread_loop_lock(loop);
pw_node_set_param(... bound->proxy ...);
```

and reading `createdLinkIds` or `proxies` from public methods without consistently holding the loop lock.

This creates a race:

```text
Application thread:
  obtains BoundProxy*

PipeWire thread:
  callback removes/destroys proxy

Application thread:
  later locks and dereferences stale pointer
```

This must be corrected.

---

# 14. Required Thread-Safety Model

Establish and document a clear locking contract.

At minimum, all access to PipeWire-loop-owned mutable state must be serialized appropriately, including:

- `proxies`
- `pendingCreated`
- `createdLinkIds`
- ownership-token maps
- `BoundProxy*`
- `pw_proxy*`
- `pw_node*`
- current global ID stored on a created proxy
- writable parameter capability state stored in `BoundProxy`

## Recommended pattern

Use internal helper functions with explicit lock assumptions.

Example concept:

```cpp
// Public method: acquires lock.
bool PipeWireConnection::setNodeVolume(...)
{
    pw_thread_loop_lock(loop);
    const bool result = impl_->setNodeVolumeLocked(...);
    pw_thread_loop_unlock(loop);
    return result;
}

// Internal callback/helper:
// called only while PipeWire loop already owns serialization.
bool Impl::setNodeVolumeLocked(...);
```

or a small RAII guard around `pw_thread_loop_lock/unlock`.

Avoid double-locking from PipeWire callbacks if the API would deadlock.

Make the contract obvious in names/comments.

---

# 15. `applyNodeProps()` Must Lock Before Proxy Lookup

Do not do:

```text
lookup raw BoundProxy
then acquire loop lock
```

Instead:

```text
validate impl/loop
acquire loop lock
lookup proxy
validate type/proxy
construct/apply or safely copy needed state
release loop lock
```

The proxy pointer must not be allowed to become stale between lookup and use.

---

# 16. `destroyOwnedLink()` Must Lock Before Ownership Lookup

Do not inspect:

```text
createdLinkIds
token maps
proxies
pendingCreated
```

outside the synchronization boundary if callbacks can mutate them.

The ownership check and destruction must be one atomic serialized operation from Auralis's perspective.

---

# 17. `volumeSupported()` Must Be Thread-Safe

`volumeSupported()` currently performs a proxy lookup without appropriate synchronization.

Correct this.

Because it is a `const` method, do not use `const` as an excuse to read mutable cross-thread structures unsafely.

Options include:

- lock the PipeWire loop in the query;
- maintain a safely synchronized capability cache;
- expose capability through the already marshalled object-store snapshot layer.

Choose the simplest correct architecture consistent with the current code.

---

# 18. Blocker 6 — Real Volume Capability Detection Is Too Broad

Current real-backend behavior effectively treats:

```text
any bound PipeWire Node
```

as:

```text
volume capable
```

That is not sufficient.

The test fake supports an explicit capability distinction, but production capability detection must reflect actual PipeWire node parameter support.

---

# 19. Required Volume Capability Detection

Inspect `pw_node_info` parameter metadata.

A destination should be considered volume-control capable only if the node exposes writable property parameters appropriate for setting node properties.

At minimum inspect the node's `spa_param_info` entries for:

```text
SPA_PARAM_Props
```

with writable capability such as the appropriate `SPA_PARAM_INFO_WRITE` flag.

Store that capability in the backend's node proxy state or in a safely synchronized representation.

Do not advertise support merely because:

```text
kind == Node
```

If needed, enumerate/query `SPA_PARAM_Props` to tighten capability detection further.

## Behavior requirements

Unsupported:

```text
VolumeControlUnsupported
```

Actual set operation fails despite advertised capability:

```text
VolumeControlFailed
```

Volume failure must not automatically tear down an otherwise healthy route.

Normalize/clamp route volume exactly as Phase 5 already specifies.

---

# 20. Blocker 7 — Activation Timeout Must Be Per Route / Per Attempt

The current router has one global:

```cpp
QTimer activationTimer_;
```

for all route activation attempts.

That is incorrect in a system that can hold multiple `AudioRoute` objects.

Example:

```text
Route A starts activation -> timer starts
Route B starts activation -> same timer restarts
Route A completes -> shared timer stops
Route B remains Activating forever
```

A route must not be able to cancel another route's timeout.

---

# 21. Required Activation Attempt Model

Make activation timeout ownership route-specific and generation-aware.

Acceptable patterns include:

### Option A — one timer per active route attempt

Store a per-route timer/attempt object.

### Option B — deadline map + one scheduler timer

Store:

```text
routeId
generation
deadline
```

and have a common scheduler evaluate expired attempts without allowing one route to cancel another.

### Option C — `QTimer::singleShot` captured generation

Use a safe context-bound single-shot callback that checks:

```text
route still exists
route still Activating
generation still matches
```

before rollback.

Whichever design you choose:

- activating A must not interfere with activating B;
- completing A must not cancel B timeout;
- deactivating/removing A must invalidate A's timeout;
- stale timer callbacks must be harmless;
- reconnect/replan generation changes must invalidate stale attempts.

Add explicit unit tests for this.

---

# 22. Blocker 8 — Live Bluetooth Address Selection Must Be Strict

The current live integration test reads:

```text
AURALIS_EXPECT_DEVICE_ADDRESS
```

but if the requested device is not found, it falls back to another available playback endpoint.

That makes the environment variable unreliable.

If the user asks to test a specific Bluetooth device, the test must actually test that device.

---

# 23. Required Live-Test Address Behavior

When:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22"
```

is non-empty:

- normalize address formatting/case;
- search for that exact logical Bluetooth endpoint;
- require endpoint availability;
- require playback/duplex direction;
- if not found, **fail the test with strong diagnostics**;
- do not silently choose laptop speakers or another sink.

When the environment variable is absent:

- choosing the first suitable available playback endpoint remains acceptable.

Diagnostics on failure should include:

- expected address;
- endpoint IDs;
- endpoint names/descriptions;
- endpoint Bluetooth addresses;
- availability;
- current PipeWire connection state;
- object counts.

---

# 24. Blocker 9 — Live Deactivation Must Verify the Actual PipeWire Graph

Checking only:

```text
router->ownedLinkCount() == 0
```

proves only that the router's bookkeeping is empty.

It does not prove that a pending created PipeWire proxy did not later become a graph link.

The Phase 5 completion gate requires:

> no stale Auralis-created links remain after deactivation/shutdown.

---

# 25. Required Graph-Level Stale-Link Verification

Use the observed PipeWire graph (`PipeWireObjectStore`) after deactivation.

Auralis-created links already carry:

```text
application.name = Auralis
auralis.route.id = <route-id>
```

The live integration test should wait until no graph link remains whose properties identify it as belonging to the deactivated route.

Conceptually:

```text
for each PipeWireLinkInfo:
  if properties["auralis.route.id"] == routeId:
      stale Auralis link exists -> FAIL
```

Also ensure links belonging to other clients are ignored.

The test should verify both:

```text
router bookkeeping == zero owned links
AND
observed PipeWire graph == zero Auralis links for route
```

This is mandatory.

---

# 26. Blocker 10 — Q_PROPERTY Notification Semantics

`AudioRouter` currently reuses domain event signals such as:

```text
routeChanged(routeId)
routeStateChanged(routeId, state)
routeError(routeId, error, detail)
```

as Q_PROPERTY NOTIFY signals.

This is unnecessarily fragile for QML property bindings.

QML-facing properties should have dedicated notification signals whose semantics match the property.

---

# 27. Required QML Property Signal Cleanup

Keep domain signals for event consumers:

```cpp
void routeAdded(const QString& routeId);
void routeChanged(const QString& routeId);
void routeRemoved(const QString& routeId);
void routeStateChanged(const QString& routeId, RouteState state);
void routeError(const QString& routeId, RouteError error, const QString& detail);
```

But add proper property notification signals, for example:

```cpp
void currentRouteIdChanged();
void routeStateTextChanged();
void lastErrorTextChanged();
void routeEnabledChanged();
void routeVolumeChanged();
void routeMutedChanged();
void volumeCapableChanged();
void ownedLinkCountChanged();
```

Exact names may differ.

Update `Q_PROPERTY` declarations accordingly.

Emit property notifications only when their observable value may have changed.

Do not create infinite update loops.

Add unit/QML-adjacent signal tests where practical.

---

# 28. Additional Ownership Invariant — `linksOperational()` Must Check Owned Link Identity

After the ownership-handle redesign, `linksOperational()` must behave approximately as:

```text
for each OwnedLink:
    resolve current global ID from ownership token
    if global ID not assigned -> not operational yet
    find that exact global link in object store
    require state Active or Paused
```

Do not fall back to:

```text
find any link on same ports
```

If the Auralis-created link has disappeared:

- route is not operational;
- recovery logic should replan/recreate as appropriate.

---

# 29. Link State Handling

Preserve correct handling of native PipeWire link states.

A created link is not automatically successful just because `pw_core_create_object()` returned a proxy.

An activation must remain `Activating` until every expected Auralis-owned link is observed with an acceptable operational state.

Treat states according to the project's existing model.

Expected success:

```text
Active
Paused   // if the existing Phase 5 semantics intentionally consider it operational
```

Failure:

```text
Error
```

Not yet ready:

```text
Init
Negotiating
Allocating
Unknown
```

Do not prematurely mark a route `Active`.

---

# 30. Atomic Activation

A route activation must be transactional.

Given an N-link plan:

```text
create link 1
create link 2
...
create link N
```

If any creation fails:

- destroy every Auralis-created proxy already created in that attempt;
- this includes proxies still waiting for global IDs;
- clear route runtime ownership;
- report structured activation failure;
- do not leave partial Auralis graph links.

Add a test where:

- first link creation succeeds but remains pending (`globalId == 0`);
- second creation fails;
- rollback destroys the first link by ownership token;
- no created ownership handle remains.

---

# 31. Deactivation Must Be Idempotent

Calling deactivation:

- on an active route,
- on an activating route,
- on a degraded route,
- twice,
- after a graph object disappeared,
- after PipeWire removed a link,
- during reconnect,

must not:

- crash;
- double-free a proxy;
- destroy foreign links;
- leave Auralis ownership records behind.

After successful deactivation:

```text
enabled = false
state = Inactive
owned runtime links = empty
no Auralis route links in graph
```

---

# 32. Shutdown Must Be Clean

`AudioRouter::shutdown()` and PipeWire shutdown must cooperate.

Required invariants:

- all enabled routes become disabled;
- all Auralis-owned link handles are released;
- all pending created proxies are destroyed;
- all bound created proxies are destroyed;
- router collections are cleared;
- no stale callback can reactivate a route after shutdown;
- thread-loop teardown does not race with public backend access.

The native PipeWire backend may also clean all proxies during connection stop, but router/link-manager ownership must remain internally consistent.

---

# 33. Graph Churn / Rebind Requirements

PipeWire global IDs are volatile.

Never persist them as stable identity.

For a route using:

```text
source logical identity S
destination logical identity D
```

if PipeWire recreates:

```text
source node 41 -> 107
destination node 52 -> 203
ports also replaced
```

Auralis must:

1. detect old runtime objects disappeared;
2. move route out of `Active`;
3. clear/destroy stale owned runtime links;
4. retain logical `sourceId` and destination IDs;
5. wait for coherent graph and connection state;
6. resolve new node/port IDs;
7. re-run planner;
8. create fresh owned links;
9. become `Active` only after those exact links are operational.

Existing rebind behavior should be preserved and hardened, not removed.

---

# 34. Foreign Link Safety

Create a unit-test scenario where a foreign link already exists with the exact same:

```text
outputPort
inputPort
```

as a planned Auralis link.

Then create/activate the Auralis route.

The test must prove:

- Auralis does not bind its `OwnedLink` to the foreign global ID;
- foreign link does not satisfy `linksOperational()`;
- Auralis still waits for its own created link;
- deactivation destroys only the Auralis-owned handle;
- foreign graph link remains untouched.

This test is essential.

---

# 35. Backend Ownership Tests

Add focused tests for the ownership abstraction.

At minimum cover:

1. create returns immediate non-zero ownership token even when global ID is zero;
2. token later maps to assigned global ID;
3. destroying pending token destroys pending proxy;
4. destroying bound token destroys bound proxy;
5. destroy unknown token is safe;
6. double destroy is safe;
7. global removal clears mappings;
8. proxy error clears mappings;
9. stopping PipeWire clears pending + bound created link maps;
10. a foreign graph link never appears in the created ownership map.

If direct native PipeWire unit testing is impractical, cover contract behavior through the fake backend and keep native-specific behavior in the live integration test.

---

# 36. Route State Tests

Strengthen `tst_AudioRouter.cpp`.

Required cases include:

## Basic success

```text
Inactive
Planning
Ready
Activating
Active
Deactivating
Inactive
```

## Partial activation failure

- first owned handle created;
- later link creation fails;
- every created handle rolled back;
- state `Failed`;
- route disabled if that remains the selected Phase 5 semantic.

## Link enters error

- owned link reaches error;
- route becomes `Degraded` or failure state according to existing policy;
- error category `LinkEnteredErrorState`;
- no false `Active`.

## Source disappears

- enabled stays true;
- state `Degraded`;
- `SourceRemoved`;
- runtime owned links cleaned;
- reappearance can recover.

## Destination disappears

- enabled stays true;
- state `Degraded`;
- `DestinationRemoved`;
- runtime owned links cleaned;
- reappearance can recover.

## PipeWire disconnect

As specified in Blocker 1.

## Reconnect before graph sync

`Connected, initialSyncComplete=false` must not create links.

## Reconnect after graph sync

Fresh replan and successful activation.

## In-flight deactivate

Deactivate during `Activating`:

- invalidates generation;
- releases every created token including pending ones;
- later graph callbacks cannot restore `Active`.

## In-flight remove route

Same safety requirements.

## Multiple simultaneous routes

- start route A activation;
- start route B activation;
- complete A;
- B retains independent timeout;
- timeout or success for B is independent.

---

# 37. Activation Timeout Tests

Do not rely on a five-second real wait for every unit test.

Make timeout duration injectable/configurable for tests if needed, or structure the timer callback so `QTest` can exercise it deterministically.

Tests must prove:

- A's success does not cancel B timeout;
- A's failure does not cancel B timeout;
- B's success does not cancel A timeout;
- stale timeout callback with old generation does nothing;
- route removal invalidates pending timeout;
- shutdown invalidates all timeouts.

---

# 38. Volume Tests

Keep existing `VolumeController` tests and add backend capability behavior where needed.

Cases:

- volume clamped to `[0.0, 1.0]`;
- mute propagated;
- supported destination succeeds;
- unsupported destination reports `VolumeControlUnsupported`;
- node exists but lacks writable `SPA_PARAM_Props` => unsupported;
- advertised capability but `pw_node_set_param` fails => `VolumeControlFailed`;
- route volume fan-out with one unsupported/failing destination reports partial failure;
- route itself remains routed when volume control fails.

---

# 39. Live Integration Test — Required Flow

The opt-in live test should validate the real native pipeline.

Environment:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1
```

Optional exact Bluetooth endpoint:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22"
```

Required workflow:

1. initialize Bluetooth manager;
2. initialize PipeWire manager;
3. require PipeWire `Connected`;
4. require initial graph sync;
5. require playback endpoints;
6. if expected Bluetooth address is supplied:
   - resolve that exact address or fail;
7. create the test-only low-volume `pw_stream` source;
8. wait until it appears as an `AudioSource`;
9. create route;
10. activate route;
11. wait for `Active`;
12. require expected Auralis-owned link count;
13. require actual graph links with:
    - `application.name=Auralis` and/or
    - matching `auralis.route.id`;
14. ensure observed links correspond to the route's owned identities;
15. deactivate route;
16. wait for router-owned count to reach zero;
17. wait for graph to contain **zero** links for `auralis.route.id=<route>`;
18. ensure unrelated/foreign links were not destroyed;
19. cleanly stop test stream;
20. cleanly destroy managers.

Failure diagnostics must be excellent.

---

# 40. Multi-Destination Live/Unit Semantics

Phase 5 supports:

```text
one source -> one-or-more destinations
```

For stereo source to two stereo sinks, planner should yield four channel pairs if all channels are present.

Preserve deterministic channel matching.

Do not introduce DSP mixing or synchronization.

If a destination cannot accept the additive route:

- report structured route degradation/failure;
- never delete WirePlumber's existing link;
- never hijack foreign ownership.

---

# 41. QML Behavior Must Remain Functional

After property-notify cleanup:

- source list still updates;
- selected route state updates;
- error text updates;
- route enabled status updates;
- volume/mute controls update;
- owned link count updates;
- QML must not require polling.

Run the desktop app manually after the tests where practical.

No QML runtime warnings should be introduced.

---

# 42. CMake / Build Discipline

Do not weaken warning or test configuration to get a green build.

Do not:

- disable tests;
- comment out failing assertions;
- hide warnings with broad suppression;
- remove `-Werror`-style strictness if present;
- skip integration code from compilation;
- conditionally exclude broken files.

Fix the implementation.

---

# 43. Logging Requirements

Keep logs useful and structured.

Important lifecycle events should identify:

- route ID;
- ownership token where useful;
- current PipeWire global ID when assigned;
- source/destination;
- channel;
- state transition;
- activation generation;
- error category.

Useful examples:

```text
LinkCreateRequested route=<id> token=<token> global=<0|id> ...
LinkBound route=<id> token=<token> global=<id>
LinkDestroyed route=<id> token=<token> global=<id>
RouteState id=<id> Activating -> Active
RouteDegraded id=<id> reason=PipeWireDisconnected
RouteRebind id=<id> generation=<n>
```

Do not flood logs every UI frame.

---

# 44. Avoid Use-After-Free During Proxy Callbacks

Pay special attention to:

- `pw_proxy_events::bound`
- `pw_proxy_events::removed`
- `pw_proxy_events::error`
- node info callbacks
- link info callbacks

When destroying a `BoundProxy`:

- remove it from every ownership/global/pending container exactly once;
- remove SPA hooks safely;
- null internal pointers as appropriate;
- do not touch user data after `pw_proxy_destroy()` if lifetime is owned by PipeWire;
- prevent callbacks from finding stale container entries.

Audit every callback path for consistent cleanup.

---

# 45. Suggested PipeWire Backend Internal Model

This is a suggested shape, not a mandatory exact implementation.

```cpp
struct BoundProxy {
    Impl* impl = nullptr;

    quint64 ownershipToken = 0;  // non-zero only for Auralis-created links
    quint32 globalId = 0;

    PipeWireObjectKind kind = PipeWireObjectKind::Unknown;
    pw_proxy* proxy = nullptr;

    bool created = false;
    bool writableProps = false;

    spa_hook objectListener{};
    spa_hook proxyListener{};
};

QHash<quint32, BoundProxy*> proxiesByGlobalId;
QHash<quint64, BoundProxy*> createdLinksByToken;
QSet<BoundProxy*> pendingCreatedLinks;
```

On successful creation:

```text
allocate token
initialize BoundProxy
insert token -> BoundProxy
insert into pending created set
return token immediately
```

On bound:

```text
set globalId
remove from pending set
insert global -> BoundProxy
retain token -> BoundProxy
```

On destroy by token:

```text
lookup exact created BoundProxy
if global ID present:
    remove global mapping
else:
    remove pending mapping
remove token mapping
remove listeners
destroy proxy
```

On PipeWire removed/error:

```text
erase all mappings
destroy/clear safely
```

This model eliminates topology guessing.

Adapt naming/types to the repository's style.

---

# 46. Decide Clearly What `OwnedLink.globalId == 0` Means

After the redesign, `globalId == 0` may legitimately mean:

```text
Auralis owns a created proxy, but PipeWire has not assigned global ID yet.
```

That is safe only because ownership is represented by a non-zero token.

Define invariants such as:

```text
ownershipToken != 0  => Auralis owns a created link attempt
globalId == 0        => link not bound/observable by global ID yet
globalId != 0        => exact Auralis-created link has been bound
```

Do not allow an `OwnedLink` with:

```text
ownershipToken == 0
```

to represent a successfully-created Auralis link.

---

# 47. Planner Must Stay Pure

Do not push proxy ownership into `RoutePlanner`.

`RoutePlanner` should remain responsible for deterministic graph resolution:

```text
logical source + logical destinations
    ->
specific output/input node-port pairs
```

It should not:

- create proxies;
- own links;
- mutate graph;
- hold timers;
- perform PipeWire threading.

Keep separation of concerns.

---

# 48. `LinkManager` Responsibilities After Fix

`LinkManager` should remain the transactional owner/coordinator above the backend.

Responsibilities:

- create every pair in a resolved plan;
- collect exact Auralis ownership handles;
- rollback every already-created handle if a later creation fails;
- destroy a route's exact owned handles;
- clear runtime link records;
- never destroy by topology;
- never destroy a global ID unless backend ownership proves it belongs to Auralis.

---

# 49. Route Error Semantics

Keep errors structured.

At minimum preserve:

- `SourceNotFound`
- `SourceNotRoutable`
- `DestinationNotFound`
- `DestinationUnavailable`
- `NoCompatiblePorts`
- `UnsupportedDirection`
- `FormatNegotiationFailed`
- `LinkCreationFailed`
- `LinkEnteredErrorState`
- `PipeWireDisconnected`
- `PermissionDenied`
- `DestinationSuspended`
- `SourceRemoved`
- `DestinationRemoved`
- `PartialActivationFailed`
- `VolumeControlUnsupported`
- `VolumeControlFailed`
- `InternalError`

Do not replace structured errors with only free-form strings.

---

# 50. Recovery Semantics

For `RouteRecoveryPolicy::RebindOnGraphReplacement`:

- logical route intent may stay enabled while runtime graph is unavailable;
- route must be `Degraded`, not falsely `Active`;
- recovery waits for graph readiness;
- fresh runtime IDs are resolved;
- old IDs are never reused simply because numerical values happen to exist;
- new links get new ownership tokens.

For `NoAutomaticRecovery`:

- respect the existing intended behavior;
- do not silently introduce automatic retries contrary to the enum.

Add tests if current behavior is ambiguous.

---

# 51. Generation Semantics

Continue using activation generations or an equivalent anti-stale mechanism.

Every asynchronous activation/replan attempt should have a generation identity.

Before applying an asynchronous result:

```text
if route no longer exists -> ignore/cleanup
if generation no longer matches -> cleanup this attempt and ignore
if route no longer enabled -> cleanup and ignore
```

A stale result must never resurrect a deactivated/removed route.

Ownership-token cleanup is mandatory on stale generations.

---

# 52. Current Route / Multi-Route UI Compatibility

The current QML-facing API appears to expose properties based on the first/current route.

Do not unnecessarily redesign the whole UI during this correction pass.

Fix correctness and property notification semantics while preserving current UI behavior.

If an obvious bug exists around current route selection, document it separately unless it blocks Phase 5 completion.

---

# 53. Tests Must Detect the Previous Bugs

A corrected codebase is not enough.

Add tests that would fail on the pre-correction implementation.

The final test suite must make it difficult to reintroduce these bugs.

Specifically, tests must fail if someone later restores:

- `Active` after PipeWire disconnect;
- port-pair ownership guessing;
- inability to destroy a pending link with no global ID;
- one shared activation timeout;
- live Bluetooth fallback to wrong device;
- router-only stale-link verification;
- unlocked backend proxy lookup.

---

# 54. Static Search Gate

Before declaring completion, run searches to ensure forbidden patterns are gone.

Examples:

```bash
grep -RIn "pw-link\|wpctl\|pactl\|pacmd" src include apps ui
```

Production result must contain no routing subprocess usage.

Search for topology ownership inference:

```bash
grep -RIn "outputPort.*inputPort\|inputPort.*outputPort" src/audio include/auralis/audio
```

Manually inspect any matches to ensure they are:

- planner comparisons;
- diagnostics;

and not:

```text
same ports => this is our link
```

Search for remaining raw accesses to PipeWire-owned containers outside locking discipline.

---

# 55. Clean Build Procedure

From repository root:

```bash
rm -rf build

cmake -S . -B build -G Ninja

cmake --build build
```

Do not rely only on an incremental build.

---

# 56. Full Default Test Suite

Run:

```bash
ctest --test-dir build --output-on-failure
```

All default tests from Phases 0–5 must pass.

If any fail:

- diagnose;
- fix root cause;
- rerun until green.

Do not merely report known failures unless they require unavailable physical hardware and are correctly opt-in tests.

---

# 57. Focused Phase 5 Tests

Also run focused filters such as:

```bash
ctest --test-dir build -R "AudioRoute|AudioSources|RoutePlanner|AudioRouter|VolumeController" --output-on-failure
```

Use the exact CTest target names present in the repository.

If names differ, discover them with:

```bash
ctest --test-dir build -N
```

---

# 58. Live Integration Test

With working PipeWire environment:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
ctest --test-dir build \
-R tst_AudioRoutingLiveIntegration \
--output-on-failure
```

For the intended Bluetooth device:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" \
ctest --test-dir build \
-R tst_AudioRoutingLiveIntegration \
--output-on-failure
```

If the exact device is absent, the test must fail rather than silently selecting another sink.

---

# 59. Optional Runtime Graph Inspection

During manual validation, use PipeWire diagnostic utilities only for observation, not production control.

For example:

```bash
pw-cli ls Link
```

or other read-only graph inspection available on the machine.

It is acceptable to use tools manually to confirm behavior.

It is not acceptable to build production routing around them.

---

# 60. Manual Desktop Smoke Test

Run:

```bash
./build/apps/desktop/auralis-desktop
```

Validate:

1. app launches;
2. Bluetooth/device functionality from previous phases still works;
3. PipeWire graph connects;
4. source list appears;
5. destination endpoints appear;
6. route can be created;
7. route activates;
8. volume/mute UI reflects capability correctly;
9. deactivation removes Auralis links;
10. disconnect/reconnect does not leave route falsely Active;
11. no crash on app exit.

Capture significant warnings/errors.

---

# 61. Do Not Hide Environmental Failures

If a live test cannot run because:

- PipeWire daemon is unavailable;
- Bluetooth hardware is absent;
- requested Bluetooth device is disconnected;
- permissions are unavailable;

report that precisely.

But the code and default tests must still be correct.

Do not change the live test to pass by silently picking a different device when a specific device was requested.

---

# 62. Required Documentation Update

Update `docs/phase-5-validation.md` after implementation.

Document:

- ownership-token model;
- meaning of token versus global ID;
- disconnect state transition;
- graph rebind behavior;
- volume capability detection;
- per-route activation timeout;
- exact Bluetooth address test semantics;
- graph-level stale-link validation;
- commands used for validation.

Keep it concise but technically precise.

---

# 63. Code Quality Requirements

Use modern, idiomatic C++ consistent with the repository.

Requirements:

- clear ownership;
- no raw cross-thread lifetime assumptions;
- no unnecessary shared state;
- no duplicated routing logic;
- no giant god methods;
- no magic numbers when a named constant is appropriate;
- explicit state transitions;
- useful assertions only where valid;
- no exceptions if repository policy avoids them;
- no unsafe casts beyond unavoidable PipeWire API boundaries;
- RAII for locks where practical;
- deterministic tests.

---

# 64. Do Not Over-Engineer

Although the ownership issue requires an interface improvement, do not turn this correction into a large framework.

Prefer:

```text
small opaque handle type
+
clear backend maps
+
correct LinkManager rollback
+
correct router synchronization
```

over:

- a generic resource-management framework;
- a new event bus;
- a new session abstraction;
- a large concurrency library;
- a complete rewrite of PipeWireConnection.

---

# 65. Expected Files Likely to Change

At minimum inspect, and likely modify, some subset of:

```text
include/auralis/audio/AudioRoute.h
include/auralis/audio/AudioRouter.h
include/auralis/audio/IPipeWireLinkBackend.h
include/auralis/audio/LinkManager.h
include/auralis/audio/PipeWireConnection.h

src/audio/AudioRouter.cpp
src/audio/LinkManager.cpp
src/audio/PipeWireConnection.cpp

tests/unit/audio/tst_AudioRouter.cpp
tests/unit/audio/tst_VolumeController.cpp
tests/integration/tst_AudioRoutingLiveIntegration.cpp

docs/phase-5-validation.md
```

Depending on the chosen design, also update:

```text
PipeWireTypes.*
test fake backends
CMake test sources
QML tests
```

Do not edit unrelated files without a concrete reason.

---

# 66. Required Test Matrix

The final audit must contain a table like:

| Scenario | Expected result | Test | Status |
|---|---|---|---|
| Single stereo route | Active | unit/live | PASS |
| Stereo -> 2 sinks | 4 owned links | unit | PASS |
| Partial create failure | full rollback | unit | PASS |
| First link pending, second fails | pending proxy destroyed | unit | PASS |
| Foreign identical port link | never treated as owned | unit | PASS |
| Source disappears | Degraded + cleanup | unit | PASS |
| Destination disappears | Degraded + cleanup | unit | PASS |
| PipeWire disconnect | enabled + Degraded | unit | PASS |
| Reconnect before sync | no reactivation | unit | PASS |
| Reconnect after sync | fresh rebind | unit | PASS |
| Deactivate while activating | no resurrection | unit | PASS |
| Two simultaneous route activations | independent timeouts | unit | PASS |
| Unsupported volume | structured unsupported | unit | PASS |
| Requested BT address absent | live test fails | integration | PASS |
| Deactivate | no Auralis graph links remain | integration | PASS |
| Full regression | all default tests | ctest | PASS |

Expand this table as needed.

---

# 67. Phase 5 Completion Gate

Do not write:

```text
Phase 5 complete
```

until **all** of the following are true.

## Architecture

- [ ] Existing Phase 5 architecture preserved.
- [ ] No Phase 6 session/sync engine introduced.
- [ ] No production shell-based routing.

## Ownership

- [ ] Every successful native link creation gets immediate Auralis ownership identity.
- [ ] Pending link with no global ID can be destroyed.
- [ ] Rollback can destroy pending + bound links.
- [ ] Deactivation can destroy pending + bound links.
- [ ] Shutdown clears pending + bound ownership.
- [ ] Foreign links are never considered owned.
- [ ] Ownership is never inferred only from matching ports.

## State machine

- [ ] Route cannot remain `Active` when PipeWire is disconnected.
- [ ] Enabled route becomes `Degraded` on runtime graph loss.
- [ ] Reconnect waits for initial graph sync.
- [ ] Recovered route uses fresh runtime IDs.
- [ ] Stale generations cannot resurrect routes.

## Thread safety

- [ ] Public backend methods do not use stale proxy pointers.
- [ ] Loop-owned maps are accessed under defined synchronization.
- [ ] Node volume/mute path is safe.
- [ ] Owned-link destroy path is safe.
- [ ] Capability query path is safe.

## Volume

- [ ] Production volume capability reflects writable PipeWire properties.
- [ ] Unsupported capability is distinct from runtime failure.
- [ ] Volume errors do not tear down healthy routing.

## Timers

- [ ] Activation timeout is per-route/per-attempt.
- [ ] One route cannot stop another route's timeout.
- [ ] stale timeout callbacks are harmless.

## Tests

- [ ] Old disconnect expectation corrected.
- [ ] pending rollback test added.
- [ ] foreign identical-port-link test added.
- [ ] source-loss cleanup test added.
- [ ] destination-loss cleanup test added.
- [ ] multi-route timeout test added.
- [ ] exact Bluetooth address behavior tested.
- [ ] graph-level stale-link validation present.
- [ ] all existing tests pass.
- [ ] clean build passes.
- [ ] live test passes when hardware/environment is available.

## UI

- [ ] Q_PROPERTY notification signals are QML-safe.
- [ ] no new QML runtime warnings.
- [ ] route controls remain functional.

Only after these gates pass may Phase 5 be declared complete.

---

# 68. Final Deliverable — Mandatory Audit Document

After implementing and testing, create:

```text
docs/PHASE_5_FINAL_CLOSURE_AUDIT.md
```

The audit must contain:

## A. Executive result

Exactly one:

```text
PHASE 5: COMPLETE
```

or:

```text
PHASE 5: NOT COMPLETE
```

Do not claim complete if any mandatory gate is unresolved.

## B. Files changed

For every modified file:

- why it changed;
- key implementation change.

## C. Ownership design

Explain:

- ownership token;
- global ID;
- pending link;
- bound link;
- destruction path;
- proxy callback cleanup.

## D. State-machine changes

Document:

```text
disconnect
source loss
destination loss
reconnect
deactivate
rollback
```

## E. Thread-safety model

Explicitly state which thread/lock owns:

- proxy maps;
- created link maps;
- pending links;
- volume operations.

## F. Tests added/changed

List every test and what regression it prevents.

## G. Commands executed

Include exact commands and exit results.

## H. Test results

Include:

- configure;
- build;
- default ctest;
- focused Phase 5 tests;
- live routing test if run;
- Bluetooth-address live test if run.

## I. Known environmental limitations

If hardware tests could not run, say exactly why.

## J. Remaining issues

Must be:

```text
None
```

for a true Phase 5 closure, except clearly documented non-blocking future Phase 6 scope.

---

# 69. Required Final Console Summary

At the end of your work, print a compact summary in the IDE/chat:

```text
AURALIS PHASE 5 CORRECTION PASS

Build: PASS/FAIL
Default tests: PASS/FAIL
Phase 5 unit tests: PASS/FAIL
Live PipeWire routing: PASS/FAIL/NOT RUN
Exact Bluetooth routing: PASS/FAIL/NOT RUN

Ownership handle fix: DONE/NOT DONE
Pending-link rollback: DONE/NOT DONE
Foreign-link isolation: DONE/NOT DONE
Disconnect state fix: DONE/NOT DONE
Source/destination cleanup: DONE/NOT DONE
PipeWire thread safety: DONE/NOT DONE
Volume capability detection: DONE/NOT DONE
Per-route timeout: DONE/NOT DONE
QML property notifications: DONE/NOT DONE
Graph stale-link verification: DONE/NOT DONE

Final verdict:
PHASE 5: COMPLETE / NOT COMPLETE

Audit:
docs/PHASE_5_FINAL_CLOSURE_AUDIT.md
```

---

# 70. Implementation Priority Order

Use this order unless code dependencies require a small adjustment:

1. introduce immediate Auralis ownership handle/token;
2. update backend create/destroy/query contract;
3. fix PipeWire proxy maps and callback cleanup;
4. fix locking/thread-loop discipline;
5. update `OwnedLink` and `LinkManager`;
6. remove topology-based ownership inference;
7. fix router operational checks;
8. fix disconnect transition;
9. fix source/destination cleanup;
10. implement independent activation timeouts;
11. implement production volume capability detection;
12. clean Q_PROPERTY notifications;
13. update unit tests;
14. strengthen live integration test;
15. update validation documentation;
16. clean rebuild;
17. run all tests;
18. run live test where possible;
19. manual desktop smoke test;
20. write final closure audit.

---

# 71. Important Reasoning Rules

While implementing:

- Assume PipeWire callbacks can occur at inconvenient times.
- Assume global IDs can disappear and be reused.
- Assume a created proxy may not have a global ID immediately.
- Assume foreign clients may create links on identical ports.
- Assume multiple routes may activate simultaneously.
- Assume users may deactivate/remove a route during activation.
- Assume Bluetooth graph objects may disappear and return with new IDs.
- Assume UI code may query properties while PipeWire events are arriving.
- Make every destructive operation ownership-safe and idempotent.

---

# 72. Definition of Correct Phase 5

The final implementation should satisfy this conceptual contract:

```text
User selects stable logical source
        +
one-or-more stable logical playback destinations
        |
        v
RoutePlanner resolves CURRENT PipeWire nodes/ports
        |
        v
LinkManager asks PipeWire backend to create links
        |
        v
Each successful create immediately returns an
AURALIS OWNERSHIP TOKEN
        |
        v
PipeWire may assign global IDs asynchronously
        |
        v
Token -> exact created proxy -> exact global ID
        |
        v
ObjectStore observes exact links becoming operational
        |
        v
Route becomes Active
```

On failure:

```text
ANY partial failure
        |
        v
destroy every created ownership token
including still-pending proxies
        |
        v
zero stale Auralis graph links
```

On graph loss:

```text
runtime graph disappears
        |
        v
route leaves Active
logical route intent may remain enabled
runtime ownership cleared
        |
        v
fresh graph sync
        |
        v
resolve fresh IDs
create fresh owned links
        |
        v
Active again
```

On deactivation:

```text
destroy only Auralis-owned handles
        |
        v
never destroy foreign links
        |
        v
router bookkeeping empty
AND
PipeWire graph contains zero links for this Auralis route
```

That is the Phase 5 standard.

---

# 73. Final Instruction

Do not stop after making the first failing test green.

Complete the entire correction pass.

Do not declare Phase 5 complete merely because:

- code compiles;
- existing tests pass;
- route works once on one sink;
- router's owned-link vector becomes empty.

Phase 5 is complete only when:

- ownership is exact;
- pending proxies are destroyable;
- rollback is atomic;
- foreign links are safe;
- disconnect/recovery state is truthful;
- thread-loop access is safe;
- timeouts are route-independent;
- volume capability is real;
- live endpoint selection is strict;
- graph-level cleanup is proven;
- regressions are covered by tests;
- the full clean build/test suite passes.

When done, produce `docs/PHASE_5_FINAL_CLOSURE_AUDIT.md` and report the evidence.
