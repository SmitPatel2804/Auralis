# Auralis — Phase 6 Implementation Prompt
## Multi-Device Session Engine

**Project:** Auralis  
**Target platform:** Ubuntu Linux desktop  
**Implementation phase:** Phase 6 — Multi-Device Session Engine  
**Assumed completed baseline:** Phases 0, 1, 2, 3, 4, and 5 are implemented, tested, and working  
**Primary implementation language:** Modern C++  
**Desktop/application framework:** Qt 6 / Qt Quick / QML  
**Bluetooth integration:** BlueZ over D-Bus  
**Audio integration:** PipeWire native integration where practical  
**Build system:** CMake + Ninja  
**Purpose of this document:** Give an AI coding agent running inside the Auralis repository a precise, implementation-oriented contract for completing Phase 6 without breaking the already-working Phase 0–5 architecture.

---

# 1. Role and Mission

You are the senior software engineer responsible for implementing **Phase 6 — Multi-Device Session Engine** in the existing Auralis repository.

The repository already contains working implementations from earlier phases. Treat those implementations as the authoritative codebase. Do **not** replace proven subsystems merely because another design looks cleaner in isolation.

Your task is to extend the current architecture so Auralis can treat multiple physical Bluetooth/audio devices as a single logical, persistent, controllable session.

The central product outcome is:

```text
ONE LOGICAL AURALIS SESSION
        |
        +-- one selected audio source
        |
        +-- Device A
        +-- Device B
        +-- Device C ...
        |
        +-- coordinated routing
        +-- group controls
        +-- per-device controls
        +-- session state
        +-- degraded operation
        +-- reconnect/recovery
        +-- persistence/restoration
```

Auralis must no longer behave only like:

```text
choose source -> create route -> output endpoint
```

It must also support:

```text
create session
    -> assign source
    -> assign multiple logical devices
    -> resolve their current audio endpoints
    -> activate all viable routes
    -> expose one coherent session state
    -> survive partial device loss
    -> recover returning devices
    -> stop cleanly
    -> save/restore configuration
```

Phase 6 is complete only when this behavior is implemented, automated tests pass, existing Phase 0–5 behavior remains functional, and the Phase 6 acceptance gates in this document are satisfied.

---

# 2. Authoritative Project Context

Auralis is a Linux desktop application intended to discover, connect, manage, group, and route audio to multiple Bluetooth/hearing devices.

The established architecture separates:

- Bluetooth adapter/discovery
- Bluetooth device lifecycle
- audio endpoint discovery
- Bluetooth-to-PipeWire endpoint mapping
- audio routing
- device-domain state
- session-domain state
- persistence
- configuration
- logging
- user interface

The Phase 6 roadmap defines the session layer conceptually as:

```text
SessionManager
|
+-- Session
+-- SessionDevice
+-- SessionStateMachine
+-- SessionPersistence
+-- RecoveryManager
+-- VolumeCoordinator
+-- RoutingCoordinator
```

The expected session data includes:

```text
AuralisSession
|
+-- sessionId
+-- name
+-- source
+-- devices[]
+-- routes[]
+-- state
+-- groupVolume
+-- autoReconnect
+-- recoveryPolicy
+-- createdAt
+-- lastUsedAt
```

Expected high-level states:

```text
IDLE
STARTING
ACTIVE
DEGRADED
RECOVERING
STOPPING
FAILED
```

Expected Phase 6 behaviors include:

- create a session
- add multiple devices
- select a source
- activate the session
- coordinate devices as a group
- preserve useful per-device control
- handle one device disconnecting
- keep the session alive where possible
- enter degraded mode when appropriate
- reconnect/recover a returning device
- preserve route consistency
- save sessions
- restore saved sessions

The architecture must remain transport-independent above the audio endpoint abstraction. Do not make the SessionManager depend on A2DP-specific details. Session code should work with higher-level device/endpoint capabilities so that a future LE Audio transport can reuse the same session layer.

---

# 3. Phase 6 Scope Boundary

## 3.1 In Scope

Implement the backend/domain capabilities required for production-quality logical multi-device sessions:

1. Session domain model.
2. Session repository/registry.
3. Session create/read/update/delete operations.
4. Device membership management.
5. Optional left/right/general role metadata.
6. Audio source assignment.
7. Session state machine.
8. Session activation.
9. Session deactivation.
10. Routing coordination across multiple endpoints.
11. Per-device route tracking.
12. Group volume coordination.
13. Per-device volume control integration.
14. Mute semantics if existing Phase 5 APIs support it.
15. Degraded mode.
16. Reconnection/recovery orchestration.
17. Session persistence.
18. Restoration of saved sessions after application restart.
19. Automatic restoration policy where safely supported.
20. Robust reaction to asynchronous device/endpoint/route events.
21. Unit tests.
22. Integration tests using fakes/mocks where hardware is unnecessary.
23. Optional real-hardware integration tests gated by environment variables.
24. Logging and diagnostics hooks.
25. Minimal backend exposure needed for the existing UI/test shell.

## 3.2 Not In Scope

Do not turn Phase 6 into Phase 7 or Phase 8.

Do **not** perform a wholesale GUI redesign. Phase 7 is reserved for the complete Auralis GUI.

Do **not** redesign or replace the working Bluetooth stack from Phases 2–3.

Do **not** replace the working PipeWire registry/endpoint mapping from Phase 4.

Do **not** replace the Phase 5 AudioRouter unless a narrowly-scoped extension is demonstrably necessary.

Do **not** make Auralis depend on shell commands such as:

```bash
bluetoothctl
wpctl
pactl
pw-link
pw-cli
```

They may be used only as developer diagnostics or manual verification tools. Production implementation must continue to use BlueZ D-Bus and the native/current PipeWire integration.

Do **not** implement LE Audio, Auracast, LC3, BAP, PACS, ASCS, Android, packaging, or full production hardening as part of this phase.

Do **not** prematurely add a complex distributed synchronization engine if one does not already exist in the roadmap/current implementation. Phase 6 should preserve route/session consistency and recovery. Advanced latency/drift compensation can remain a later specialized track unless the current repository already contains abstractions for it.

---

# 4. Non-Negotiable Engineering Rules

## 4.1 Preserve Working Phase 0–5 Behavior

Before modifying code:

1. inspect the repository;
2. configure it;
3. build it;
4. run the existing test suite;
5. record the baseline result.

Do not start a major refactor if the existing architecture already exposes an appropriate extension point.

Any previously passing Phase 0–5 automated test that fails after the Phase 6 work must be treated as a regression unless the test itself is proven obsolete and updated with a clearly documented reason.

## 4.2 Audit Before Editing

Do not assume the repository still matches an earlier planned file layout.

Inspect the actual code.

Determine:

- current targets
- namespaces
- coding style
- error/result conventions
- logging conventions
- QObject usage
- thread model
- persistence implementation
- configuration implementation
- AudioRouter API
- AudioRoute representation
- VolumeController API
- AudioEndpoint registry API
- EndpointResolver API
- Bluetooth device registry API
- device lifecycle signal/event APIs
- ApplicationCore service ownership
- SessionManager skeleton, if present
- existing tests and test helpers

Then extend those real interfaces.

## 4.3 No Duplicate Subsystems

If Phase 5 already has an `AudioRouter`, do not create `SessionAudioRouter` that separately manipulates PipeWire.

Instead introduce a higher-level coordinator that calls the existing router.

If device connectivity is already owned by `BluetoothManager`, do not create a second Bluetooth connection manager inside the session module.

If endpoint resolution is already owned by `EndpointResolver`, SessionManager should consume it, not replicate its mapping logic.

## 4.4 Transport Independence

The session layer may know that a member has:

- a logical device ID
- an available playback endpoint
- endpoint capabilities
- connection/availability state

It should not encode session logic such as:

```cpp
if (transport == "a2dp") { ... }
```

unless the current API absolutely requires transport-specific compatibility checks.

Prefer capability-based checks.

## 4.5 Asynchronous Truth Comes From Services

A user/API request to connect, route, start, stop, or recover is an intent.

Actual state must be derived from the authoritative Bluetooth/PipeWire/router events.

Do not mark a session ACTIVE merely because activation calls were issued.

ACTIVE means the activation criteria have actually been met.

## 4.6 Deterministic and Testable Domain Logic

Hardware-independent session state logic must be unit-testable without:

- a Bluetooth adapter
- real hearing devices
- PipeWire hardware
- a graphical display

Inject or abstract external dependencies where necessary.

---

# 5. Mandatory First Task — Repository Baseline Audit

Before implementation, produce an internal Phase 6 audit note or terminal summary covering the current repository.

At minimum inspect:

```bash
git status --short
git branch --show-current
git log --oneline -n 15

find . -maxdepth 3 -type f | sort

grep -R "class SessionManager\|struct AuralisSession\|AudioRouter\|AudioRoute\|VolumeController\|AudioEndpoint\|EndpointResolver\|DeviceRegistry\|PersistenceManager" \
    -n include src apps tests 2>/dev/null
```

Use repository-appropriate equivalents if folders differ.

Then run the established project commands. If the repository uses the previous baseline, this is expected to resemble:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Also verify that the desktop target still launches if the environment permits:

```bash
./build/apps/desktop/auralis-desktop
```

If a clean build is the project convention, also verify from a clean build directory before final completion.

Create a mental/current-code dependency map similar to:

```text
ApplicationCore
|
+-- BluetoothManager
|   +-- DeviceRegistry
|
+-- PipeWireManager
|   +-- AudioEndpointRegistry
|   +-- EndpointResolver
|
+-- AudioRouter
|   +-- route state
|   +-- volume
|
+-- SessionManager   <-- Phase 6
```

Do not make architectural decisions until this baseline has been inspected.

---

# 6. Target Phase 6 Architecture

The preferred architecture is:

```text
                       ApplicationCore
                             |
                             v
                      SessionManager
                             |
          +------------------+-------------------+
          |                  |                   |
          v                  v                   v
 SessionStateMachine   SessionPersistence   SessionRegistry
          |
          +------------------+-------------------+
          |                                      |
          v                                      v
 RoutingCoordinator                       VolumeCoordinator
          |                                      |
          v                                      v
     AudioRouter                         Phase 5 volume APIs
          |
          v
  AudioEndpointRegistry
          ^
          |
   EndpointResolver
          ^
          |
   Bluetooth/Device state
```

Recovery event flow:

```text
Device/Endpoint/Route event
        |
        v
 SessionManager
        |
        v
 SessionStateMachine
        |
        +--> state update
        |
        +--> RecoveryCoordinator
                  |
                  +--> request reconnect if policy allows
                  +--> wait for endpoint availability
                  +--> restore missing route
                  +--> restore member volume
                  +--> recompute session state
```

The exact class names may be adapted to the existing repository. The responsibilities must remain clear even if the actual type names differ.

---

# 7. Domain Model Requirements

## 7.1 Stable Session Identifier

A session requires a stable application-level ID that survives application restarts.

Use the repository’s existing ID convention.

Suitable approaches include:

- UUID/QUuid
- stable opaque string
- existing typed ID wrapper

Do not use:

- vector index
- pointer address
- PipeWire object ID
- transient BlueZ D-Bus path

as the persistent session identity.

## 7.2 `AuralisSession`

Define or complete the session domain object using repository conventions.

Conceptually:

```cpp
struct AuralisSession {
    SessionId id;
    QString name;

    AudioSourceReference source;
    std::vector<SessionDevice> devices;
    std::vector<RouteReference> routes;

    SessionState state;

    float groupVolume;
    bool muted;

    bool autoReconnect;
    RecoveryPolicy recoveryPolicy;

    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime lastUsedAt;
};
```

This is conceptual, not mandatory syntax.

If the project avoids Qt types in pure-domain objects, preserve that convention.

### Required persisted fields

Persist stable configuration, not transient runtime handles.

Persist:

- session ID
- session name
- source selection using a stable source reference where possible
- member logical device IDs
- member role metadata
- configured group volume
- member relative/per-device volume data if supported
- autoReconnect
- recovery policy
- createdAt
- lastUsedAt
- optional “was active at clean shutdown” or restore-intent flag

Do not persist as authoritative runtime state:

- raw PipeWire node IDs
- raw PipeWire link IDs
- BlueZ transient object paths if stable device ID/address exists
- `ACTIVE` simply because the app exited during playback

Runtime objects must be re-resolved after restart.

## 7.3 `SessionDevice`

Each physical/logical member should carry stable membership metadata.

Conceptually:

```cpp
struct SessionDevice {
    DeviceId deviceId;
    SessionDeviceRole role;

    bool enabled;
    float volumeOffsetOrIndividualVolume;
    bool muted;

    SessionMemberRuntimeState runtimeState;
};
```

Recommended role model:

```text
Unspecified
Left
Right
Center
Auxiliary
```

Do not hard-code the entire session engine around exactly two devices. Left/right support is important, but sessions must support N devices.

## 7.4 Runtime State Separation

Prefer separating persisted configuration from runtime state.

Example:

```text
Persisted:
  deviceId=ha-left
  role=Left
  enabled=true
  individualVolume=0.85

Runtime:
  bluetoothConnected=true
  endpointAvailable=true
  endpointId=...
  routeId=...
  routeActive=true
  recoveryAttempt=2
  lastError=...
```

This prevents persistence from becoming coupled to transient PipeWire/BlueZ identifiers.

## 7.5 Session State Enum

Implement all required logical states unless the existing project already has a compatible superset:

```cpp
enum class SessionState {
    Idle,
    Starting,
    Active,
    Degraded,
    Recovering,
    Stopping,
    Failed
};
```

If naming conventions use `IDLE`, `Idle`, etc., follow the repository.

Provide stable conversion helpers for:

- logging
- serialization if needed
- UI exposure
- tests

Unknown serialized values must not crash the application.

---

# 8. Session State Machine

The state machine is a core Phase 6 deliverable.

Do not scatter state assignments arbitrarily across callbacks.

Create one coherent transition policy.

## 8.1 State Definitions

### IDLE

The session exists but is not actively routing audio.

Expected characteristics:

- no session-owned active routes
- membership configuration retained
- source retained
- group volume retained
- no active recovery loop

### STARTING

Activation has been requested and Auralis is resolving/connecting/creating required routes.

### ACTIVE

All currently required enabled members have reached the defined healthy state.

For a typical playback session:

- source is resolved/available
- each required member has an available output endpoint
- each required member route is active
- no unresolved required member failure exists

### DEGRADED

The session remains useful but one or more intended members are unavailable or unrouted.

Example:

```text
expected members: left + right
left: healthy
right: disconnected
session: DEGRADED
audio continues on left
```

### RECOVERING

The session is actively attempting to restore missing members/routes according to its recovery policy.

Recovery may alternate logically with DEGRADED depending on implementation style.

Recommended meaning:

- DEGRADED = stable partial operation / no immediate active retry in progress
- RECOVERING = at least one allowed recovery action is currently in progress

### STOPPING

A stop/deactivate request is being processed. Session-owned routes are being torn down and recovery is suppressed.

### FAILED

The session cannot provide meaningful operation or has encountered a terminal activation/recovery error according to policy.

A failure of one member must **not automatically** imply FAILED if remaining members can operate and degraded mode is allowed.

## 8.2 Suggested Transition Matrix

Implement and test equivalent behavior:

| Current | Event / Condition | Next |
|---|---|---|
| IDLE | activate requested | STARTING |
| STARTING | all required members routed | ACTIVE |
| STARTING | some members routed, some unavailable, degraded allowed | DEGRADED or RECOVERING |
| STARTING | no viable output and terminal condition | FAILED |
| ACTIVE | one required member lost | DEGRADED or RECOVERING |
| DEGRADED | recovery attempt starts | RECOVERING |
| RECOVERING | all members healthy | ACTIVE |
| RECOVERING | partial recovery remains possible | DEGRADED / RECOVERING |
| RECOVERING | terminal failure but remaining output viable | DEGRADED |
| any active state | stop requested | STOPPING |
| STOPPING | session routes removed | IDLE |
| FAILED | stop/reset requested | STOPPING or IDLE |
| FAILED | explicit retry | STARTING |

Do not allow late asynchronous callbacks from a previous activation generation to reactivate a session after it has entered STOPPING/IDLE.

Use an operation token/generation ID/cancellation flag if needed.

---

# 9. SessionManager Responsibilities

`SessionManager` is the authoritative owner of logical sessions.

It should own or coordinate:

- session registry
- lifecycle commands
- membership updates
- source assignment
- activation/deactivation
- state transitions
- routing coordination
- recovery coordination
- volume coordination
- persistence scheduling
- session-domain events/signals

It should **not** own:

- low-level BlueZ calls
- raw PipeWire graph manipulation
- discovery scanning logic
- endpoint matching algorithms already owned by EndpointResolver
- GUI widgets

## 9.1 Minimum API Surface

Adapt to project conventions, but support equivalent operations:

```text
createSession(name)
deleteSession(sessionId)
renameSession(sessionId, name)

session(sessionId)
sessions()

addDevice(sessionId, deviceId, role)
removeDevice(sessionId, deviceId)
setDeviceEnabled(sessionId, deviceId, enabled)
setDeviceRole(sessionId, deviceId, role)

setSource(sessionId, sourceRef)

activateSession(sessionId)
deactivateSession(sessionId)
retrySession(sessionId)

setGroupVolume(sessionId, value)
setSessionMuted(sessionId, bool)
setDeviceVolume(sessionId, deviceId, value)
setDeviceMuted(sessionId, deviceId, bool)

setAutoReconnect(sessionId, bool)
setRecoveryPolicy(sessionId, policy)

loadSessions()
saveSession(sessionId)
restoreLastSession() or equivalent policy-driven restore
```

Use synchronous return values only for validation/request acceptance.

Do not pretend an asynchronous operation completed synchronously.

For example:

```text
activateSession()
```

may return:

```text
Accepted
InvalidSession
NoSourceConfigured
NoMembersConfigured
AlreadyActive
```

The eventual state should arrive asynchronously through state/events.

---

# 10. Session CRUD Semantics

## 10.1 Create

Creating a session should:

- allocate stable ID
- validate/default name
- initialize state to IDLE
- initialize group volume to a sensible value
- initialize default recovery policy
- initialize timestamps
- persist it

Do not automatically activate unless explicitly requested by an existing product requirement.

## 10.2 Rename

Validate:

- non-empty after trimming
- reasonable length
- valid UTF-8/QString behavior
- duplicate names may be allowed because IDs are authoritative unless existing project rules forbid them

## 10.3 Delete

Deleting a session must be safe.

If active:

1. deactivate/stop;
2. clean session-owned routes;
3. cancel recovery;
4. remove persistence record;
5. emit removal event.

Do not leave orphaned session routes.

## 10.4 Membership

Prevent duplicate membership entries for the same stable device ID unless the domain explicitly supports multiple logical roles for one physical device.

Removing a device from an active session should reconcile routing immediately:

```text
member removed
    -> cancel recovery for that member
    -> remove session-owned route for member
    -> update session model
    -> recompute state
    -> persist
```

Adding a device while active may either:

A. dynamically bring it into the active session, or  
B. require an explicit restart.

Prefer A if the existing router/services make it safe and testable. If implementing A, it must use the same recovery/route coordination path as activation.

---

# 11. Source Assignment

The session source must use a stable application-level source reference if Phase 5 provides one.

Do not persist transient PipeWire object IDs as the sole source identity if the router already has a stable source abstraction.

When the source disappears:

- do not crash;
- update session health;
- decide whether the session becomes RECOVERING or FAILED based on recovery capability;
- remove or mark routes appropriately;
- attempt source re-resolution if the source identity is restorable.

If the source changes while a session is active:

1. validate the new source;
2. avoid leaving half-old/half-new routes indefinitely;
3. use a controlled route reconciliation operation;
4. if replacement fails, either rollback to the old source or enter a well-defined degraded/failed state.

Choose the simplest atomic/rollback behavior supported by the current Phase 5 router.

---

# 12. RoutingCoordinator

The session layer must coordinate Phase 5 routes; it must not manually duplicate routing internals.

## 12.1 Responsibilities

`RoutingCoordinator` should conceptually:

- map each enabled session member to its current output endpoint
- generate the desired route set
- compare desired vs actual session-owned routes
- create missing routes using AudioRouter
- deactivate/remove obsolete routes
- track route ownership by session/member
- react to route failure/removal
- reconcile after endpoint changes
- avoid duplicate routes
- prevent stale route handles from surviving endpoint re-creation

## 12.2 Desired-State Reconciliation

Prefer a desired-state model:

```text
desired session routes
        |
        v
compare with current session routes
        |
        +-- missing -> create
        +-- obsolete -> remove
        +-- unhealthy -> recreate/recover
        +-- healthy -> keep
```

This is more robust than one-off imperative callback chains.

## 12.3 Route Ownership

Every route created on behalf of a session needs enough metadata/mapping to answer:

```text
Which session owns this route?
Which session member does this route serve?
Which source does it use?
Which endpoint does it target?
```

This can be kept in SessionManager if the core AudioRoute type should remain generic.

## 12.4 Multi-Output Route Compatibility

If Phase 5 represents one route with many destinations, use that model if it already provides reliable per-destination health and recovery.

If Phase 5 uses one route per destination, Phase 6 should coordinate a set of routes.

Do not force a route model rewrite merely to match this prompt.

## 12.5 Transaction-Like Activation

Activation should avoid silently claiming success after only some setup calls.

Recommended flow:

```text
validate session
    |
resolve source
    |
resolve enabled member devices
    |
resolve available output endpoints
    |
determine viable members
    |
create/activate route(s)
    |
observe actual route state
    |
recompute session state
```

If some routes fail:

- keep successful routes if degraded mode is allowed;
- clean up all routes if policy requires all-or-nothing;
- default Phase 6 behavior should favor useful degraded operation for multi-hearing-device scenarios.

---

# 13. Endpoint and Device Resolution

Session membership must use stable logical device IDs.

At runtime:

```text
SessionDevice.deviceId
       |
       v
DeviceRegistry / DeviceManager
       |
       v
Bluetooth device state
       |
       v
EndpointResolver
       |
       v
AudioEndpointRegistry
       |
       v
current playback endpoint
```

Do not persist the current PipeWire node ID as the member identity.

When a Bluetooth reconnect causes PipeWire to create a new node ID, the session must re-resolve the device and recover routing.

---

# 14. Degraded Mode Requirements

Degraded mode is mandatory.

Example:

```text
Session "Living Room"
Expected:
  HA-L
  HA-R

Runtime:
  HA-L connected + routed
  HA-R disconnected

Result:
  session remains operational
  HA-L continues receiving audio
  session state = DEGRADED or RECOVERING
  HA-R marked unavailable/recovering
```

## 14.1 Do Not Tear Down Healthy Members

One member disappearing must not automatically destroy routes to healthy members.

## 14.2 Clear Member Health

Track enough runtime state to distinguish:

```text
configured but disabled
configured but disconnected
connected but endpoint unavailable
endpoint available but route inactive
route active
recovering
failed
```

## 14.3 Recovery Success

When the lost device returns:

```text
BlueZ connected
    |
PipeWire endpoint appears
    |
EndpointResolver maps endpoint
    |
session recovery detects viable member
    |
route recreated
    |
member volume/mute restored
    |
session state recomputed
    |
ACTIVE if all required members healthy
```

---

# 15. Recovery Policy

Implement a narrowly-scoped session recovery policy suitable for Phase 6.

Do not implement the entire future Phase 8 global reliability framework unless it already exists.

Recommended policy model:

```cpp
enum class RecoveryPolicy {
    None,
    RestoreRoutesOnly,
    ReconnectAndRestore
};
```

or equivalent richer flags:

```text
autoReconnectDevice
autoRestoreEndpoint
autoRestoreRoute
maxAttempts
backoff
```

Follow existing project configuration conventions.

## 15.1 Recovery Rules

### If Bluetooth device disconnects

If `autoReconnect` is enabled and the current Bluetooth subsystem already exposes an async reconnect operation:

- request reconnection through BluetoothManager;
- do not call BlueZ directly from SessionManager;
- mark member recovering;
- wait for authoritative connection change.

If reconnect is disabled:

- remain degraded;
- do not loop.

### If endpoint disappears while Bluetooth remains connected

- wait for endpoint re-resolution;
- re-create the route when the endpoint returns;
- do not unnecessarily disconnect/reconnect Bluetooth.

### If route breaks

- use AudioRouter recovery/recreate semantics if available;
- otherwise deactivate the stale route and create a fresh one.

### If session is stopping

Suppress automatic recovery.

### If member is removed

Cancel pending recovery for it.

## 15.2 Retry / Backoff

If the Bluetooth layer already owns reconnect backoff, reuse it.

Do not create competing retry loops.

If Phase 6 must schedule route-level retries, use a bounded/configurable approach.

Tests must use deterministic fake timers or short controlled intervals.

---

# 16. VolumeCoordinator

Group volume is a Phase 6 requirement.

Do not bypass Phase 5 volume APIs.

## 16.1 Volume Model

Choose a coherent volume model based on existing Phase 5 capabilities.

Preferred logical model:

```text
session group volume
        x
member relative volume / trim
        =
effective destination volume
```

For example:

```text
group = 0.70
left trim = 1.00
right trim = 0.90

left effective = 0.70
right effective = 0.63
```

If Phase 5 supports direct independent endpoint volume only, implement an equivalent predictable mapping.

## 16.2 Range

Use one canonical internal range:

- preferably normalized `[0.0, 1.0]`, or
- existing repository convention

Clamp external values.

Reject NaN/invalid values.

## 16.3 Group Volume Change

Changing group volume should apply consistently to every currently active member.

Unavailable members should retain the intended state so it is applied when they recover.

## 16.4 Individual Volume

Support per-device volume/trim without losing the group-volume concept.

A group volume change must not accidentally erase intentional left/right balance unless product semantics explicitly say it should.

## 16.5 Mute

If existing Phase 5 APIs support mute:

- session mute should mute all active members;
- individual mute should affect only that member;
- recovered routes must reapply intended mute state.

Do not implement a second unrelated software audio mixer if endpoint/router volume already exists.

---

# 17. Persistence

Persistence is mandatory.

Use the repository’s existing `PersistenceManager`/configuration storage if one exists.

Do not add an unrelated database dependency unless the project already uses one.

## 17.1 Versioned Schema

Persist sessions with a version field.

Example conceptual JSON:

```json
{
  "schemaVersion": 1,
  "sessions": [
    {
      "id": "3e...",
      "name": "Living Room",
      "source": {
        "stableId": "system-playback"
      },
      "devices": [
        {
          "deviceId": "ha-left",
          "role": "left",
          "enabled": true,
          "volume": 1.0,
          "muted": false
        },
        {
          "deviceId": "ha-right",
          "role": "right",
          "enabled": true,
          "volume": 0.95,
          "muted": false
        }
      ],
      "groupVolume": 0.70,
      "muted": false,
      "autoReconnect": true,
      "recoveryPolicy": "reconnect-and-restore",
      "createdAt": "...",
      "lastUsedAt": "...",
      "restoreIntent": false
    }
  ]
}
```

Use the project’s actual persistence format and naming conventions.

## 17.2 Atomic Save

Avoid corrupting all session data if the process stops during write.

If existing PersistenceManager provides atomic writing, use it.

With Qt, `QSaveFile` is one possible implementation if consistent with the codebase.

## 17.3 Invalid Data

Persistence loader must safely handle:

- missing file
- empty file
- unknown schema version
- missing optional fields
- duplicate session IDs
- duplicate device IDs within a session
- invalid group volume
- invalid enum string
- missing referenced device
- missing referenced source
- malformed record

Prefer:

- load what is valid
- log diagnostics
- skip/quarantine invalid entries

Do not crash application startup.

## 17.4 Restore Behavior

On application startup:

1. load persisted session definitions;
2. put them in IDLE initially;
3. re-resolve stable devices/sources against runtime registries;
4. do not trust persisted PipeWire IDs;
5. optionally restore last active session only if existing configuration/policy says so.

Automatic activation at startup must be deliberate, not an accidental consequence of persisting `ACTIVE`.

---

# 18. Last-Used and Auto-Restore Semantics

Maintain `lastUsedAt` when a session is successfully activated or otherwise meaningfully used.

If implementing “restore last session”:

- determine the last-used eligible session;
- validate it before activation;
- if no devices are available, do not crash;
- enter an appropriate recoverable/degraded/failed state or remain idle based on policy;
- log what happened.

A clean application shutdown should deactivate routes cleanly unless the existing architecture deliberately leaves PipeWire routes unmanaged.

Persist the *intent* to restore separately from transient runtime state.

---

# 19. Concurrency and Event Ordering

Bluetooth, PipeWire, route, and Qt events may arrive asynchronously.

Phase 6 must be resilient to event ordering.

Examples:

```text
Bluetooth connected
PipeWire endpoint not yet present
```

```text
PipeWire endpoint removed
Bluetooth disconnect signal arrives later
```

```text
stop requested
old route-activation completion callback arrives
```

```text
device reconnects with a different PipeWire node ID
```

## 19.1 Rules

- Session model mutation should occur on one well-defined thread/event loop.
- If services emit from worker threads, marshal events safely.
- Do not hold locks while invoking external service callbacks if avoidable.
- Avoid recursive state transitions.
- Avoid raw-pointer lifetime coupling between session entries and registry objects.
- Use stable IDs and safe observers/signals.
- Cancel/ignore stale async completions.

## 19.2 Operation Generation

For activation/recovery races, consider:

```cpp
quint64 activationGeneration_;
```

or an operation token.

Each start/stop cycle can invalidate callbacks belonging to a previous generation.

Use this only if needed by the current async design.

---

# 20. Errors and Result Types

Follow the repository’s existing result/error style.

Do not introduce exceptions into a codebase that consistently uses result objects, or vice versa.

Phase 6 should distinguish at least:

```text
SessionNotFound
InvalidSessionName
NoSourceConfigured
NoMembersConfigured
DuplicateMember
MemberNotFound
SourceUnavailable
DeviceUnavailable
EndpointUnavailable
RouteCreationFailed
RouteActivationFailed
PersistenceFailure
OperationAlreadyInProgress
InvalidState
RecoveryExhausted
```

Errors should carry context such as:

- session ID
- device ID
- endpoint ID
- route ID
- low-level error
- human-readable message

Do not expose raw implementation details as the only user-facing diagnostic.

---

# 21. Signals / Events / Observability

Expose Phase 6 state changes through the same mechanism already used elsewhere in Auralis.

If Qt signals are the project convention, likely events include equivalents of:

```text
sessionAdded(sessionId)
sessionRemoved(sessionId)
sessionUpdated(sessionId)

sessionStateChanged(sessionId, oldState, newState)

sessionMemberAdded(sessionId, deviceId)
sessionMemberRemoved(sessionId, deviceId)
sessionMemberStateChanged(sessionId, deviceId, state)

sessionRouteChanged(sessionId, deviceId, routeId, routeState)

sessionVolumeChanged(sessionId, volume)
sessionError(sessionId, error)
```

Do not emit duplicate/no-op changes if avoidable.

Ensure model/event consumers can update without polling.

---

# 22. Logging Requirements

Use the existing Logger.

Log structured lifecycle events.

Minimum useful events:

```text
SessionCreated
SessionDeleted
SessionRenamed
SessionMemberAdded
SessionMemberRemoved
SessionSourceChanged

SessionStartRequested
SessionStarting
SessionActivated
SessionDegraded
SessionRecovering
SessionRecoverySucceeded
SessionRecoveryFailed
SessionStopRequested
SessionStopped
SessionFailed

SessionRouteCreateRequested
SessionRouteActivated
SessionRouteRemoved
SessionRouteFailed

SessionDeviceDisconnected
SessionDeviceReconnected
SessionEndpointUnavailable
SessionEndpointRestored

SessionGroupVolumeChanged
SessionMemberVolumeChanged

SessionPersisted
SessionLoaded
SessionRestoreRequested
SessionRestoreSucceeded
SessionRestoreFailed
```

Each relevant log should include:

```text
session=<id>
device=<id>       when applicable
route=<id>        when applicable
state=<state>
reason=<reason>   when applicable
```

Avoid logging on every high-frequency audio callback.

---

# 23. ApplicationCore Integration

Inspect current `ApplicationCore`.

Phase 6 must fit its existing ownership/lifetime strategy.

Expected dependency ordering is conceptually:

```text
Configuration/Logger
    |
Bluetooth + PipeWire services
    |
Device/Endpoint registries
    |
AudioRouter
    |
SessionManager
    |
UI exposure
```

SessionManager must not start using dependent services before they are ready.

Shutdown ordering should prevent session callbacks from touching already-destroyed router/Bluetooth/PipeWire services.

Suggested shutdown sequence:

```text
stop active sessions
cancel recovery
flush session persistence
destroy SessionManager
then tear down AudioRouter / endpoint services
then lower-level services
```

Adapt to actual current ownership.

---

# 24. Minimal UI / QML Exposure

Phase 6 is backend-first.

Do not build the full Sessions screen planned for Phase 7.

However, if the existing desktop shell already exposes backend models, provide enough integration to prove the backend can be driven.

Acceptable minimal additions:

- expose session count
- expose current/selected session
- expose session state
- expose group volume
- expose basic create/start/stop test controls only if consistent with current shell

Prefer automated tests over building premature UI.

Do not spend Phase 6 effort on visual styling.

---

# 25. Suggested File Organization

**Do not blindly create these files.** First inspect the current repository and merge with its conventions.

A reasonable structure could be:

```text
include/auralis/session/
    AuralisSession.h
    SessionDevice.h
    SessionTypes.h
    SessionManager.h
    SessionStateMachine.h
    SessionPersistence.h
    RoutingCoordinator.h
    VolumeCoordinator.h
    SessionRecoveryCoordinator.h

src/session/
    AuralisSession.cpp
    SessionManager.cpp
    SessionStateMachine.cpp
    SessionPersistence.cpp
    RoutingCoordinator.cpp
    VolumeCoordinator.cpp
    SessionRecoveryCoordinator.cpp

tests/unit/session/
    tst_SessionStateMachine.cpp
    tst_SessionManager.cpp
    tst_SessionPersistence.cpp
    tst_RoutingCoordinator.cpp
    tst_VolumeCoordinator.cpp
    tst_SessionRecovery.cpp

tests/integration/session/
    tst_SessionLifecycleIntegration.cpp
    tst_SessionRouteRecoveryIntegration.cpp
    tst_SessionPersistenceIntegration.cpp
```

If the project uses fewer files/classes, keep the design simpler.

Avoid needless class explosion.

The responsibilities are mandatory; the exact file count is not.

---

# 26. CMake Requirements

Integrate new Phase 6 sources into existing CMake targets.

Requirements:

- no global include path hacks
- no ad-hoc linker flags
- no unused dependencies
- use target-based CMake
- keep test targets isolated
- reuse existing Qt test setup
- preserve warnings/compiler settings
- keep C++ standard consistent with the repository

All new tests must be registered with CTest.

A clean build must succeed.

---

# 27. Testing Strategy

Testing is a mandatory implementation deliverable, not a follow-up.

Separate:

1. pure unit tests
2. service-level tests with fakes
3. integration tests
4. optional real-hardware tests

---

# 28. Unit Tests — Session State Machine

Create exhaustive state-machine tests.

At minimum:

## 28.1 Activation

- IDLE -> STARTING after activate
- STARTING -> ACTIVE when all members become healthy
- STARTING -> DEGRADED when at least one viable route exists but a required member is missing
- STARTING -> FAILED when no viable route exists and activation is terminal

## 28.2 Device Loss

- ACTIVE + one member lost -> DEGRADED/RECOVERING
- healthy member routes remain active
- recovery is scheduled only when policy allows

## 28.3 Recovery

- DEGRADED -> RECOVERING
- RECOVERING -> ACTIVE after missing member returns and route is restored
- failed recovery may return to DEGRADED
- recovery exhaustion never destroys unrelated healthy routes

## 28.4 Stop

- ACTIVE -> STOPPING -> IDLE
- DEGRADED -> STOPPING -> IDLE
- RECOVERING -> STOPPING cancels recovery
- stale recovery callback after IDLE has no effect

## 28.5 Invalid Operations

- activating already active session
- stopping already idle session
- deleting during activation
- changing membership during stopping
- retry from invalid state

Implement defined behavior for each.

---

# 29. Unit Tests — Session CRUD

Test:

- create session produces unique stable ID
- default state IDLE
- rename
- invalid empty name
- delete idle session
- delete active session safely stops first
- add member
- reject duplicate member
- remove member
- remove unknown member
- assign role
- change role
- N-device membership works, not only two devices
- set source
- source replacement
- timestamps update appropriately

---

# 30. Unit Tests — RoutingCoordinator

Use a fake/mock AudioRouter.

Test:

- one member -> one desired destination
- two members -> two destinations/routes
- no duplicate route creation on repeated reconcile
- unavailable member is skipped/degraded
- returning member creates missing route
- obsolete route removed after member removal
- endpoint ID change triggers route recreation
- route failure is surfaced
- stop removes all session-owned routes
- route belonging to another session is untouched
- cancellation prevents late activation from reappearing after stop

---

# 31. Unit Tests — VolumeCoordinator

Test:

- group volume applies to all active members
- volume clamped to legal range
- per-device trim/volume preserved across group changes
- unavailable member retains intended volume
- recovered member receives current intended volume
- session mute applies to all active members
- per-device mute does not affect peers
- NaN/invalid input rejected
- repeated same value does not cause unnecessary updates if project convention avoids them

---

# 32. Unit Tests — Persistence

Test:

- save one session
- save multiple sessions
- load session IDs
- load members
- load roles
- load source reference
- load volume
- load recovery policy
- timestamps preserved as intended
- runtime state is not incorrectly persisted
- missing file produces empty/default state
- malformed entry handled without crash
- duplicate IDs handled
- unknown enum handled
- unknown schema version handled safely
- missing device reference does not crash
- save is atomic if existing persistence supports it

Use temporary directories/files.

Tests must not touch the developer’s real application config.

---

# 33. Integration Tests — Session Lifecycle

Build fakes around actual interfaces rather than using real Bluetooth hardware.

Recommended fake services:

```text
FakeDeviceRegistry / FakeBluetoothManager
FakeAudioEndpointRegistry / FakeEndpointResolver
FakeAudioRouter
FakePersistence
```

Test complete flows:

## 33.1 Two-device happy path

```text
create session
add left
add right
set source
make both endpoints available
activate
routes become active
session ACTIVE
change group volume
stop
session IDLE
```

## 33.2 One device unavailable at start

```text
left available
right unavailable
activate
left route active
session DEGRADED / RECOVERING
right appears
right route created
session ACTIVE
```

## 33.3 Disconnect during playback

```text
session ACTIVE
right disconnects
right endpoint removed
right route removed/failed
left continues
session DEGRADED/RECOVERING
right reconnects
new endpoint ID appears
route restored
session ACTIVE
```

## 33.4 Stop while recovering

```text
right missing
recovery running
user stops session
all routes removed
recovery cancelled
right reconnect event arrives late
no route recreated
state remains IDLE
```

## 33.5 Restart/persistence

```text
create session
save
destroy managers
recreate managers
load session
membership restored
state IDLE
runtime endpoint/route IDs not reused
activate after registries are ready
routes created from freshly resolved endpoints
```

---

# 34. Optional Real-Hardware Integration Tests

Do not make ordinary `ctest` fail on machines without Bluetooth devices.

Follow the existing project’s opt-in integration-test pattern.

For example:

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="AA:BB:...;CC:DD:..." \
ctest --test-dir build \
-R tst_SessionLiveIntegration \
--output-on-failure
```

Adapt names to existing conventions.

Important: document placeholders clearly.

Do not write shell examples such as:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

in a way that Bash interprets `<...>` as redirection.

Use quoted obvious placeholders:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

or document that the value must be replaced.

Potential live tests:

- two connected audio endpoints
- activate session to two devices
- group volume change
- disconnect one device
- verify session remains alive
- reconnect
- verify route recovery
- stop cleanly

If only one hardware device is available, keep the multi-device live test SKIPPED with a clear message rather than falsely passing.

---

# 35. Regression Tests for Phase 0–5

Run the entire test suite, not only Phase 6 tests.

Specifically verify that Phase 6 changes do not break:

- configuration/logging initialization
- Bluetooth discovery
- device pairing/connection state logic
- endpoint registry
- endpoint resolver
- AudioRouter
- route creation/deactivation
- Phase 5 volume behavior
- desktop application startup

If Phase 6 requires extending AudioRouter APIs, add direct regression tests for old behavior.

---

# 36. Failure Scenarios to Explicitly Handle

Implement/test graceful behavior for:

1. Session has zero members.
2. Session has no source.
3. Source unavailable.
4. One device missing.
5. All devices missing.
6. Device connected but playback endpoint not yet created.
7. Endpoint removed and recreated with new ID.
8. Device disconnect during STARTING.
9. Device disconnect during ACTIVE.
10. Route fails to create.
11. Route becomes inactive unexpectedly.
12. One member route fails while peers remain healthy.
13. Bluetooth reconnect fails.
14. Recovery is disabled.
15. Recovery attempts are exhausted if bounded.
16. User stops during recovery.
17. User removes recovering member.
18. User deletes active session.
19. Persistence save fails.
20. Persistence load contains malformed session.
21. Application loads a session referencing a forgotten Bluetooth device.
22. Multiple sessions reference the same physical device.
23. Two active sessions attempt to route to the same endpoint.
24. Group volume changes while one member is absent.
25. Member returns after group volume has changed.
26. Source changes while session active.
27. Rapid start-stop-start.
28. Duplicate asynchronous events.
29. Service emits removal then re-add rapidly.
30. Application shutdown while session active.

For #22 and #23, define a policy rather than leaving behavior accidental.

---

# 37. Multi-Session Resource Policy

The project may eventually support multiple saved sessions and possibly multiple active sessions.

For Phase 6, choose an explicit rule consistent with existing architecture.

Recommended safe V1 policy:

```text
many sessions may be saved
only one session may be ACTIVE/STARTING/RECOVERING at a time
```

unless Phase 5 clearly supports independent concurrent session routes.

If enforcing single-active-session:

- activation of another session should either reject with a clear result or cleanly stop/switch from the current session;
- do not silently overlap routes.

A `switchActiveSession(newId)` helper may be useful but is not mandatory.

Document the selected policy.

---

# 38. Device Sharing Policy

If two saved sessions contain the same physical device, that is acceptable.

If concurrent active sessions are disallowed, no conflict exists at runtime.

If concurrent sessions are allowed, define ownership/conflict behavior for:

- endpoint volume
- mute
- routes
- reconnect policy

Do not leave this ambiguous.

---

# 39. Session Invariants

Enforce invariants centrally.

Examples:

1. Session IDs are unique.
2. Device IDs within one session are unique.
3. Group volume is always valid.
4. Session state has one authoritative owner.
5. IDLE session has no active session-owned routes.
6. STOPPING session cannot start new recovery.
7. Removed members cannot own active routes.
8. Session route records reference current members.
9. Route ownership mappings cannot outlive deleted sessions.
10. Runtime endpoint IDs are not treated as persistent identities.
11. Session state is recomputed after every material member/route availability change.
12. A healthy peer is not torn down merely because another peer failed unless all-or-nothing policy was explicitly selected.

Write tests for the most important invariants.

---

# 40. Recommended Internal Health Model

Consider an internal per-member health structure:

```cpp
struct SessionMemberRuntime {
    bool devicePresent;
    bool connected;
    bool endpointAvailable;
    bool routeRequested;
    bool routeActive;
    bool recovering;

    std::optional<EndpointId> endpointId;
    std::optional<RouteId> routeId;

    SessionMemberError lastError;
};
```

Again, adapt to current style.

Use this to derive the overall session state.

Example:

```text
all enabled members routeActive
    -> ACTIVE

at least one enabled member routeActive
and at least one missing/unhealthy
    -> DEGRADED or RECOVERING

none routeActive
but recovery in progress
    -> RECOVERING

none routeActive
and no viable recovery
    -> FAILED
```

If the source is unavailable, treat it as a session-wide dependency and derive state accordingly.

---

# 41. State Recalculation Function

Prefer one function that derives overall state from operation intent and runtime health.

Conceptually:

```cpp
void SessionManager::recomputeState(SessionId id);
```

This function should consider:

- stop in progress
- start in progress
- source viability
- number of enabled members
- healthy members
- recovering members
- terminal errors
- policy

Avoid dozens of direct state assignments in unrelated callbacks.

---

# 42. Route Reconciliation Trigger Events

Reconcile the session when any relevant event occurs:

- session activation requested
- source changes
- member added
- member removed
- member enabled/disabled
- Bluetooth connectivity changes
- audio endpoint added
- audio endpoint removed
- endpoint mapping changes
- route state changes
- recovery completes
- session stop requested

Debounce only if necessary.

Correctness is more important than micro-optimization.

Reconciliation should be idempotent.

---

# 43. Recovery and Route Reconciliation Must Be Idempotent

Repeated identical events should not create duplicate routes or duplicate reconnect requests.

Examples:

```text
endpointAdded emitted twice
```

must not produce two routes.

```text
deviceConnected true repeated
```

must not restart a healthy route.

Use stable desired-state checks before issuing operations.

---

# 44. Session Persistence Timing

Persist configuration after meaningful mutations:

- create
- rename
- member add/remove
- role change
- source change
- volume/mute change
- recovery setting change
- successful lastUsed update

Avoid writing to disk for every transient route-state event.

If writes are debounced by an existing persistence layer, use it.

Ensure changes are flushed on clean shutdown.

---

# 45. API Design for QML / Future Phase 7

Even though the full GUI is Phase 7, design the session service so Phase 7 can consume it cleanly.

Avoid APIs that require QML to understand:

- PipeWire node IDs
- BlueZ object paths
- low-level route links

Expose domain-level concepts:

```text
session name
session state
source name/reference
member list
member role
member availability
member connection state
member route state
group volume
member volume
warnings
recovery state
```

If the project uses model classes, a `SessionListModel` or later `SessionDeviceModel` may be appropriate, but implement only what is needed to preserve architecture and tests.

---

# 46. Documentation to Add/Update in Repository

Update documentation after implementation.

At minimum add or update:

```text
docs/phase-6-session-engine.md
```

or repository-equivalent.

Document:

- architecture
- session state machine
- persistence schema
- recovery semantics
- degraded mode
- volume semantics
- single-active-session or concurrent-session policy
- build/test commands
- live hardware test procedure
- known limitations

Update README only where needed.

Do not rewrite unrelated documentation.

---

# 47. Suggested Implementation Sequence

Implement in small, testable slices.

## Slice 1 — Audit + Domain Types

- inspect existing Phase 0–5
- confirm baseline tests pass
- implement/complete SessionState
- implement SessionDevice
- implement AuralisSession
- add serialization helpers only if needed
- add pure domain tests

**Gate:** clean compile + domain tests pass.

## Slice 2 — Session Registry / CRUD

- implement create/delete/rename
- membership operations
- source assignment
- policy/volume config
- signals/events
- persistence hooks

**Gate:** CRUD tests pass; no regression.

## Slice 3 — State Machine

- centralize transitions
- add state recomputation
- add invalid-operation checks
- unit-test transition matrix

**Gate:** state-machine tests pass.

## Slice 4 — Persistence

- load/save
- schema version
- validation
- startup restore into IDLE
- temp-file tests

**Gate:** persistence tests pass.

## Slice 5 — RoutingCoordinator

- integrate actual Phase 5 AudioRouter
- desired route reconciliation
- route ownership tracking
- member endpoint resolution
- stop cleanup
- fake router tests

**Gate:** two-device simulated activation succeeds.

## Slice 6 — VolumeCoordinator

- group volume
- per-device volume/trim
- mute if available
- recovery reapply
- tests

**Gate:** volume tests pass.

## Slice 7 — Degraded Mode

- member loss event path
- keep healthy route alive
- state DEGRADED
- tests

**Gate:** disconnect-one-member simulation passes.

## Slice 8 — Recovery

- integrate reconnect via existing BluetoothManager
- wait for endpoint reappearance
- recreate route
- cancel recovery on stop/remove
- tests

**Gate:** full disconnect/reconnect simulation returns to ACTIVE.

## Slice 9 — ApplicationCore / Minimal Exposure

- register SessionManager
- startup load
- shutdown cleanup
- minimal UI/backend exposure if needed

**Gate:** desktop launches; no lifecycle crash.

## Slice 10 — Integration / Regression / Docs

- full ctest
- clean build
- optional hardware tests
- docs
- final audit

**Gate:** Phase 6 definition of done satisfied.

Commit incrementally if permitted by the environment, using meaningful messages.

---

# 48. Suggested Git Commit Structure

If creating commits is allowed, prefer coherent commits such as:

```text
phase-6: add session domain model and state machine
phase-6: add persistent session registry
phase-6: coordinate multi-device audio routes
phase-6: add group volume coordination
phase-6: add degraded session operation
phase-6: add session reconnect and route recovery
phase-6: add session integration tests and documentation
```

Do not create a single enormous opaque commit if avoidable.

Do not commit build outputs.

---

# 49. Build and Verification Commands

Use the repository’s actual commands.

At final verification, prefer a clean build:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Then launch:

```bash
./build/apps/desktop/auralis-desktop
```

If the application requires graphical/session services unavailable in the AI environment, document that launch could not be performed there, but do not skip compile/test validation.

Run targeted Phase 6 tests during development, for example:

```bash
ctest --test-dir build -R "Session|session" --output-on-failure
```

Exact test names should follow repository naming conventions.

---

# 50. Manual Functional Verification

Where hardware is available, manually verify:

## Scenario A — Two devices

1. Pair/connect Device A.
2. Pair/connect Device B.
3. Confirm both expose playback endpoints.
4. Create a session.
5. Add both devices.
6. Select laptop/system audio source.
7. Activate.
8. Confirm both devices receive routed audio.
9. Adjust group volume.
10. Confirm both react.
11. Stop session.
12. Confirm routes are removed cleanly.

## Scenario B — One disconnects

1. Start with both devices active.
2. Power off / disconnect Device B.
3. Confirm Device A continues.
4. Confirm session becomes DEGRADED/RECOVERING.
5. Reconnect Device B.
6. Confirm endpoint is re-resolved.
7. Confirm route is restored.
8. Confirm session returns ACTIVE.

## Scenario C — Restart

1. Create and save session.
2. Exit Auralis cleanly.
3. Restart.
4. Confirm session definition reappears.
5. Confirm IDs/membership/volume/policy preserved.
6. Activate.
7. Confirm runtime endpoints are freshly resolved.

---

# 51. Phase 6 Acceptance Criteria

Phase 6 is **not complete** until all mandatory criteria below are satisfied.

## 51.1 Build

- [ ] clean CMake configure succeeds
- [ ] clean Ninja build succeeds
- [ ] existing Phase 0–5 targets still build
- [ ] desktop target still links and launches when environment permits

## 51.2 Session Model

- [ ] stable session ID
- [ ] name
- [ ] source reference
- [ ] multiple member devices
- [ ] roles
- [ ] route/runtime tracking
- [ ] state
- [ ] group volume
- [ ] auto reconnect/recovery policy
- [ ] timestamps

## 51.3 Lifecycle

- [ ] create
- [ ] rename
- [ ] delete
- [ ] add/remove devices
- [ ] select source
- [ ] activate
- [ ] deactivate
- [ ] retry/recover

## 51.4 Multi-Device Routing

- [ ] one source can serve multiple session members
- [ ] route creation uses existing AudioRouter
- [ ] duplicate routes are prevented
- [ ] route ownership is tracked
- [ ] stop removes session-owned routes
- [ ] endpoint recreation can be reconciled

## 51.5 State Machine

- [ ] IDLE
- [ ] STARTING
- [ ] ACTIVE
- [ ] DEGRADED
- [ ] RECOVERING
- [ ] STOPPING
- [ ] FAILED
- [ ] transitions covered by tests

## 51.6 Degraded Mode

- [ ] one member may disappear without destroying healthy peer routes
- [ ] remaining audio continues where technically possible
- [ ] member state is observable
- [ ] session state reflects partial failure

## 51.7 Recovery

- [ ] reconnect requests go through existing BluetoothManager
- [ ] endpoint return is re-resolved
- [ ] missing route restored
- [ ] volume/mute state reapplied
- [ ] session can return to ACTIVE
- [ ] recovery is cancelled on stop/remove

## 51.8 Volume

- [ ] group volume
- [ ] per-device volume/trim
- [ ] mute if supported
- [ ] intended volume retained for temporarily unavailable member
- [ ] recovered member receives current intended volume

## 51.9 Persistence

- [ ] sessions saved
- [ ] sessions loaded
- [ ] schema versioned
- [ ] invalid data handled safely
- [ ] stable identities persisted
- [ ] transient PipeWire route/node IDs not trusted after restart
- [ ] restored sessions start from safe runtime state

## 51.10 Automated Tests

- [ ] state machine tests
- [ ] CRUD tests
- [ ] routing coordination tests
- [ ] volume tests
- [ ] persistence tests
- [ ] degraded-mode test
- [ ] recovery test
- [ ] stop-during-recovery race test
- [ ] restart/restore test
- [ ] full `ctest` passes

## 51.11 Documentation

- [ ] Phase 6 architecture documented
- [ ] state machine documented
- [ ] recovery policy documented
- [ ] persistence documented
- [ ] hardware test instructions documented
- [ ] known limitations documented

---

# 52. Hard Prohibitions

Do not mark Phase 6 complete if any of the following are true:

- SessionManager is only a stub.
- Session state is only stored in the UI.
- Only one device is supported.
- “Multi-device” is implemented by hard-coding left and right only.
- A disconnected member destroys all healthy routes unconditionally.
- Recovery requires restarting Auralis.
- Session persistence stores transient PipeWire node IDs as permanent identities.
- Production routing shells out to `wpctl`, `pactl`, `pw-link`, or similar.
- Session code calls BlueZ directly instead of going through the existing Bluetooth service.
- Existing Phase 5 AudioRouter is bypassed by a parallel route engine.
- Tests require real hardware by default.
- Failures are hidden by tests that always PASS/SKIP incorrectly.
- Existing Phase 0–5 tests regress.
- Full GUI work distracts from backend correctness.
- The implementation introduces transport-specific A2DP logic throughout the session domain.

---

# 53. Code Quality Requirements

All Phase 6 code should:

- follow existing namespace conventions
- follow current formatting
- use RAII
- avoid raw owning pointers
- use const-correctness
- use `[[nodiscard]]` where consistent/useful
- avoid blocking the UI/main event loop
- avoid unnecessary copying of large collections
- use stable IDs across subsystem boundaries
- validate public inputs
- keep state transitions explicit
- use structured logging
- keep test seams clean
- document non-obvious concurrency assumptions

Do not add abstractions only for theoretical elegance.

Prefer the smallest architecture that fully satisfies Phase 6 and cleanly supports Phase 7.

---

# 54. Static Analysis / Compiler Warnings

Respect existing warning policy.

New Phase 6 code must not introduce warnings.

If the project supports:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
```

or equivalent, keep new code clean under configured warnings.

If sanitizers are already configured, run relevant tests under them.

Do not change global warning policy solely to silence new warnings.

---

# 55. Memory / Lifetime Safety

Pay special attention to:

- callbacks after SessionManager destruction
- callbacks after session deletion
- recovery timers after member removal
- stale route references after endpoint removal
- application shutdown ordering
- QObject parent ownership if used
- queued signal delivery

A deleted session must never be resurrected by a delayed callback.

---

# 56. Performance Expectations

Session management is not a high-frequency audio-processing path.

Prioritize correctness and clarity.

Still avoid:

- O(N²) scans on every irrelevant event when simple maps/indexes can be used
- unnecessary persistence writes on transient state
- busy polling
- repeated route recreation when desired state already matches actual state
- reconnect storms

For expected V1 session sizes of roughly 2–4 devices, straightforward data structures are sufficient.

Design should remain valid for more devices later.

---

# 57. Future-Proofing Without Overengineering

Phase 6 should make later work easier by preserving boundaries.

Good future-ready decisions:

- capability-based endpoint selection
- N-device sessions
- stable IDs
- session/member health model
- versioned persistence
- recovery coordinator interface
- source abstraction
- route reconciliation

Do **not** implement speculative future systems such as:

- Auracast broadcast groups
- coordinated LE Audio set APIs
- Android backend
- network-distributed sessions
- cloud synchronization
- elaborate plugin framework

unless these already exist and Phase 6 merely integrates them.

---

# 58. Expected Final Repository Behavior

After implementation, this conceptual scenario must work:

```text
Auralis starts
    |
loads saved sessions
    |
"Living Room" exists in IDLE
    |
user activates
    |
source resolves
    |
left device resolves -> endpoint L
right device resolves -> endpoint R
    |
routes created
    |
both healthy
    |
ACTIVE

right device disconnects
    |
right endpoint disappears
    |
right route becomes invalid/removed
    |
left route remains
    |
DEGRADED / RECOVERING

right device reconnects
    |
new PipeWire endpoint appears
    |
resolver maps it to right device
    |
right route recreated
    |
current group/member volume restored
    |
ACTIVE

user stops
    |
all session routes removed
    |
recovery cancelled
    |
IDLE

Auralis exits
    |
session configuration remains persisted
```

That is the Phase 6 product proof.

---

# 59. Required Final AI-IDE Report

When implementation is finished, do not merely say “done.”

Produce a structured final report containing:

## 59.1 Summary

What Phase 6 functionality was implemented.

## 59.2 Repository Changes

List all important created/modified files and their purpose.

## 59.3 Architecture

Explain:

- SessionManager
- state machine
- routing coordination
- volume coordination
- recovery flow
- persistence

## 59.4 Phase 5 Integration

Explain exactly how Phase 6 calls/reuses:

- AudioRouter
- AudioEndpointRegistry
- EndpointResolver
- BluetoothManager/device registry
- persistence/config/logging

## 59.5 State Machine

List supported states and key transitions.

## 59.6 Tests

Provide exact test names and results.

Include the final command:

```bash
ctest --test-dir build --output-on-failure
```

and its summary.

## 59.7 Manual / Hardware Verification

State what was physically verified and what remains opt-in because hardware was unavailable.

Never claim hardware verification if no real hardware test was run.

## 59.8 Known Limitations

List genuine current limitations.

## 59.9 Phase 6 Gate

End with exactly one of:

```text
PHASE 6 STATUS: COMPLETE
```

or:

```text
PHASE 6 STATUS: NOT COMPLETE
```

If not complete, list the blocking items.

---

# 60. Completion Standard

Treat the following as the definitive completion statement:

> **Phase 6 is complete when Auralis can persistently define a logical session containing multiple devices and one source, activate the necessary Phase 5 routes, represent the combined runtime state through a tested session state machine, maintain useful operation when one member disappears, recover that member and its route when it returns, coordinate group/per-device volume, stop cleanly, restore saved session definitions after restart, and pass the complete regression test suite without breaking Phases 0–5.**

Do not proceed into Phase 7 implementation until this gate is met.

---

# 61. Start Now

Begin by auditing the current repository.

Do not ask the user to restate architecture already present in the codebase.

Do not assume planned names if the repository has different real interfaces.

Inspect, build, test, then implement Phase 6 incrementally.

Prioritize:

```text
correct integration
> deterministic state
> degraded operation
> recovery
> persistence
> tests
> documentation
> cosmetic UI
```

The existing Phase 0–5 code is the foundation.

Extend it carefully and leave the repository in a clean, tested, Phase-6-complete state.
