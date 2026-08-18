# AURALIS — PHASE 5 IMPLEMENTATION PROMPT
## Audio Routing Engine

> **Use this document as the authoritative implementation prompt inside the AI IDE for the existing Auralis repository.**
>
> **Project state:** PHASE 0, PHASE 1, PHASE 2, PHASE 3, and PHASE 4 are already implemented and verified. Do **not** re-implement them. The task is to inspect the current repository, preserve the completed architecture, and implement **PHASE 5 — Audio Routing Engine** as the next incremental phase.

---

# 0. YOUR ROLE

You are acting as a **senior Linux audio systems engineer, C++/Qt architect, PipeWire integration engineer, and test engineer** working inside the existing Auralis codebase.

You are not creating a prototype from scratch.

You are extending a real, already-working codebase whose earlier phases have established:

- the build system,
- Qt/QML application shell,
- application/core architecture,
- logging and configuration infrastructure,
- Bluetooth discovery,
- BlueZ D-Bus integration,
- device lifecycle management,
- pairing/trust/connect/disconnect/forget/reconnect flows,
- native PipeWire connection/registry monitoring,
- PipeWire node/device/port/link awareness,
- audio endpoint discovery,
- Bluetooth-device-to-audio-endpoint mapping.

Your implementation must be **incremental, conservative, production-oriented, testable, and compatible with the existing architecture**.

Do not replace working subsystems simply because you would have designed them differently.

Do not create a parallel architecture beside the existing one.

Do not silently rename public APIs or move large directory trees unless an actual architectural defect makes it necessary.

If the repository already has classes whose responsibilities overlap names in this prompt, **extend/reuse those classes instead of duplicating them**.

---

# 1. AUTHORITATIVE PHASE 5 OBJECTIVE

Implement **PHASE 5 — Audio Routing Engine**.

The objective is:

> **Allow Auralis to select a routable PipeWire audio source and route that audio to one or more available output endpoints, monitor the route, control volume/mute where supported, survive ordinary graph changes, and deactivate the route cleanly.**

The conceptual route is:

```text
Audio Source
     |
     v
Auralis Audio Router
     |
     +--------------------+
     |                    |
     v                    v
Output Endpoint A    Output Endpoint B
```

The initial single-destination milestone is:

```text
Laptop / PipeWire Audio Source
              |
              v
       Auralis AudioRouter
              |
              v
      Bluetooth Audio Sink
```

Then Phase 5 must support one source routed to more than one selected destination:

```text
                         +--> Destination A
Selected Audio Source ---+
                         +--> Destination B
```

This is still **routing**, not a Phase 6 logical session.

---

# 2. PHASE 5 EXIT GATE

Phase 5 is complete only when Auralis can perform the complete workflow from its own application/service API:

1. Enumerate/select a valid audio source.
2. Enumerate/select one or more available output endpoints.
3. Validate that a route is possible.
4. Activate the route through native PipeWire APIs.
5. Observe the created PipeWire links and their states.
6. Confirm the route reaches an operational state.
7. Reflect route state changes back into the Auralis model/UI.
8. Detect breakage caused by graph changes.
9. Restore/replan the route where Phase 5 policy permits.
10. Control destination/route volume and mute where supported.
11. Deactivate the route.
12. Remove only links/resources owned by Auralis.
13. Leave the PipeWire graph clean after deactivation/application shutdown.
14. Pass the Phase 5 automated test suite.
15. Pass live PipeWire routing integration tests when explicitly enabled.

**Code existing is not enough. The exit gate must be demonstrated.**

---

# 3. LOCKED PROJECT BASELINE

Treat these assumptions as locked unless the current repository proves otherwise.

## 3.1 Platform

The primary development platform is Linux/Ubuntu with:

- modern GCC/G++,
- CMake,
- Ninja,
- Qt 6,
- BlueZ,
- D-Bus,
- PipeWire,
- WirePlumber.

The known development environment has PipeWire in the 1.6.x family. Compile against the **installed system headers** and do not require APIs newer than the target machine provides.

## 3.2 Production integration rules

Production Auralis code must **not** depend on launching and parsing shell commands such as:

```text
bluetoothctl
wpctl
pw-cli
pw-link
pactl
```

Those commands are allowed for:

- developer diagnostics,
- manual validation,
- troubleshooting instructions,
- comparison during integration tests.

They must not be the implementation of AudioRouter.

Bluetooth control remains through BlueZ/D-Bus.

PipeWire graph control remains through the native PipeWire client API and the architecture already established in Phase 4.

## 3.3 Architectural separation

Preserve separation among:

```text
QML / UI
   |
Application/Core Services
   |
AudioRouter / Routing Domain
   |
PipeWire integration
   |
PipeWire server / WirePlumber
```

Do not put PipeWire C API calls directly into QML-facing view code.

Do not put QML concerns inside the low-level PipeWire backend.

---

# 4. STRICT PHASE BOUNDARIES

## 4.1 IN SCOPE FOR PHASE 5

Implement or complete the following responsibilities:

- audio source discovery/selection for routable sources already visible in PipeWire,
- destination endpoint selection,
- route definition/model,
- route validation,
- route planning,
- PipeWire port resolution,
- PipeWire link creation,
- PipeWire link destruction,
- link ownership tracking,
- route activation/deactivation,
- route/link state monitoring,
- handling source disappearance,
- handling destination disappearance,
- handling PipeWire node/port ID replacement,
- handling stream recreation,
- handling destination suspension,
- handling link negotiation failure,
- handling incompatible source/destination topology,
- minimal recovery/replanning for the same logical route,
- volume and mute control where supported,
- minimal Phase 5 UI/API integration necessary to exercise routing,
- unit tests,
- fake-backend/component tests,
- live PipeWire integration tests behind an explicit environment gate,
- documentation for Phase 5 behavior and test commands.

## 4.2 OUT OF SCOPE FOR PHASE 5

Do **not** implement Phase 6, Phase 7, or Phase 8 under the guise of Phase 5.

Specifically, do not build:

- a persistent multi-device `SessionManager`,
- named user sessions such as “Living Room”,
- session persistence,
- session membership persistence,
- sophisticated multi-device synchronization,
- per-device buffering engines,
- latency compensation DSP,
- drift correction,
- long-term clock synchronization,
- multi-device calibration,
- full reconnection orchestration across Bluetooth + audio + session layers,
- final production GUI redesign,
- packaging/installers,
- system-wide reliability hardening unrelated to routing,
- LE Audio/Auracast architecture replacement,
- Android implementation.

Phase 5 can expose latency/status information **if already available from PipeWire**, but it must not become the future synchronization engine.

Phase 5 may apply a route-level volume to multiple destinations, but that does not make it a Phase 6 logical session abstraction.

---

# 5. FIRST ACTION: REPOSITORY AUDIT BEFORE EDITING

Before changing code, inspect the entire repository relevant to Phases 0–4.

Do not start coding until you understand what already exists.

## 5.1 Inspect at minimum

Inspect:

- root `CMakeLists.txt`,
- all nested CMake files,
- `src/`, `lib/`, `modules/`, `include/`, or equivalent project directories,
- audio/PipeWire classes,
- endpoint models,
- endpoint registries,
- PipeWire node/port/link models,
- BlueZ-to-PipeWire endpoint resolver,
- application/core service wiring,
- QML registrations/context properties,
- existing UI panels,
- existing test directory structure,
- test helpers/fakes/mocks,
- logging macros/categories,
- configuration classes,
- thread/event-loop architecture,
- existing error/result types.

## 5.2 Identify the actual Phase 4 implementation

Find the exact classes responsible for concepts equivalent to:

```text
PipeWireManager
CoreConnection
RegistryMonitor
DeviceMonitor
NodeMonitor
PortMonitor
LinkMonitor
AudioEndpointRegistry
EndpointResolver
AudioEndpoint
```

Names may differ.

Record the actual names before implementing Phase 5.

## 5.3 Determine what PipeWire data is already retained

Determine whether Phase 4 currently retains:

- global object ID,
- object serial,
- node ID,
- node name,
- node description,
- media class,
- media type/category/role,
- device ID,
- device API,
- Bluetooth address or mapping identity,
- port ID,
- port direction,
- port name/alias,
- channel position,
- monitor-port flag,
- node state,
- link state,
- route/profile information.

If Phase 5 requires a small extension to a Phase 4 internal model, add it carefully and add regression tests.

Do not rewrite Phase 4 wholesale.

## 5.4 Produce an internal implementation map

Before coding, form an internal mapping like:

```text
Prompt Concept              Existing Auralis Type
------------------------------------------------------------
AudioEndpoint               <actual type>
AudioEndpointRegistry       <actual type>
PipeWireManager             <actual type>
Port record                 <actual type>
Link record                 <actual type>
AudioRouter                 <existing or new>
Application service owner   <actual type>
QML exposure                <actual type/model>
```

Use this map to integrate Phase 5 naturally.

---

# 6. REQUIRED PHASE 5 ARCHITECTURE

The roadmap concept is:

```text
AudioRouter
|
+-- SourceSelector
+-- EndpointSelector
+-- RoutePlanner
+-- LinkManager
+-- StreamMonitor
+-- VolumeController
+-- RouteState
```

Do not mechanically create seven classes if the existing codebase would be cleaner with fewer objects.

However, the responsibilities must remain distinct and testable.

A recommended architecture is:

```text
+---------------------------------------------------------+
|                   Application / QML                     |
+-----------------------------+---------------------------+
                              |
                              v
+---------------------------------------------------------+
|                       AudioRouter                       |
|                                                         |
| createRoute / activate / deactivate / update selection |
| state aggregation / error reporting                    |
+--------+-------------------+----------------------------+
         |                   |
         v                   v
+------------------+   +---------------------+
|   RoutePlanner   |   |  VolumeController   |
| source/dest/port |   | volume/mute control |
| compatibility    |   +---------------------+
+--------+---------+
         |
         v
+---------------------------------------------------------+
|                      LinkManager                        |
| native PipeWire link create/destroy/state ownership    |
+-----------------------------+---------------------------+
                              |
                              v
+---------------------------------------------------------+
| Existing Phase 4 PipeWireManager / Registry / Models   |
+-----------------------------+---------------------------+
                              |
                              v
+---------------------------------------------------------+
|                  PipeWire / WirePlumber                 |
+---------------------------------------------------------+
```

`StreamMonitor` can be a distinct class or an aspect of existing Phase 4 graph monitoring if the latter is already authoritative.

The key rule is: **there must be one source of truth for graph objects.**

Do not create a second independent PipeWire registry monitor unless technically necessary.

---

# 7. DOMAIN MODEL

Use existing project naming conventions. The following model describes required semantics, not mandatory spelling.

## 7.1 AudioRoute

Introduce or complete an `AudioRoute` domain object.

Recommended logical fields:

```text
AudioRoute
|
+-- id
+-- sourceRef
+-- destinationRefs[]
+-- active
+-- state
+-- error
+-- createdAt
+-- activatedAt
+-- ownedLinkIds[]
+-- volumePolicy
+-- recoveryPolicy
+-- latencyInfo (optional / observational only)
```

Avoid storing unstable PipeWire global IDs as the *only* persistent identity inside the high-level route.

High-level route references should prefer stable logical Auralis identifiers.

PipeWire node/port IDs belong in the resolved plan/runtime binding.

## 7.2 Route identity

`routeId` must be unique within the process.

Use the project's existing ID strategy.

If none exists, use a deterministic/robust value such as a UUID represented using the project’s standard string type.

Do not use a raw PipeWire node ID as a route ID.

## 7.3 RouteState

Implement an explicit state model.

Recommended:

```text
Inactive
Planning
Ready
Activating
Active
Degraded
Deactivating
Failed
```

If the existing project uses a different enum style, adapt it.

Suggested semantics:

### `Inactive`

The route exists but owns no active links.

### `Planning`

Auralis is resolving the logical source/destinations into current PipeWire nodes/ports.

### `Ready`

A complete valid plan exists but links are not yet active.

### `Activating`

Links have been requested and are negotiating/allocating.

### `Active`

All required links for all selected destinations are operational according to the Phase 5 success rule.

### `Degraded`

At least one required part of an active route disappeared or failed while another portion may remain operational, or the route is waiting for a logical endpoint/stream to reappear.

### `Deactivating`

Auralis is removing the links/resources it owns.

### `Failed`

The route cannot currently be activated or recovered without a new user action or a meaningful graph change.

## 7.4 Route error model

Use structured errors, not only free-form strings.

Recommended error categories:

```text
None
SourceNotFound
SourceNotRoutable
DestinationNotFound
DestinationUnavailable
NoCompatiblePorts
UnsupportedDirection
FormatNegotiationFailed
LinkCreationFailed
LinkEnteredErrorState
PipeWireDisconnected
PermissionDenied
DestinationSuspended
SourceRemoved
DestinationRemoved
PartialActivationFailed
VolumeControlUnsupported
VolumeControlFailed
InternalError
```

Carry a human-readable detail string alongside the enum/category.

Do not expose raw negative errno values directly as the only UI message.

---

# 8. SOURCE MODEL AND SOURCE SELECTION

Phase 5 must clearly distinguish **audio producers** from **audio consumers**.

PipeWire media classes commonly relevant to routing include concepts equivalent to:

```text
Audio/Source          -> a source device such as a microphone
Audio/Sink            -> an output sink such as speakers / Bluetooth playback
Stream/Output/Audio   -> an application playback stream
Stream/Input/Audio    -> an application capture stream
```

For Phase 5 routing to output devices, candidate source types should normally include:

- application playback streams (`Stream/Output/Audio`),
- physical/virtual audio source nodes (`Audio/Source`) when meaningful,
- sink monitor output ports where the current graph exposes them and the product wants “system/laptop playback mix” routing.

Do **not** treat every PipeWire node as a routable source.

## 8.1 Recommended AudioSource model

If an equivalent does not already exist, introduce a lightweight source descriptor:

```text
AudioSource
|
+-- id                    // stable Auralis logical ID
+-- pipeWireNodeId        // current runtime binding
+-- objectSerial          // when available
+-- nodeName
+-- description
+-- mediaClass
+-- sourceType
+-- available
+-- active
+-- applicationName       // if stream
+-- processId             // optional metadata only
+-- monitorSource         // true if based on monitor ports
+-- portRefs[]
```

Possible source type enum:

```text
ApplicationPlaybackStream
PhysicalAudioSource
VirtualAudioSource
SinkMonitor
Unknown
```

## 8.2 Stable source identity

Application playback streams are ephemeral.

Do not assume a PipeWire node ID remains stable when:

- a browser tab restarts playback,
- an application recreates its stream,
- PipeWire restarts,
- the stream format/profile changes.

Where practical, preserve enough semantic identity for stream re-detection, for example:

- application/process binary/name,
- media name/title,
- node name,
- client ID/metadata,
- object serial for current instance,
- other existing Phase 4 properties.

Do not over-engineer persistent application rules in Phase 5.

The minimum requirement is to detect recreation and make the route state truthful.

## 8.3 “Laptop audio” interpretation

Do not invent an invasive system-wide audio-capture mechanism simply to satisfy a label.

Implement the best source modes supported naturally by the existing PipeWire graph:

1. selected application playback stream, and/or
2. selected physical/virtual source, and/or
3. monitor output of a selected sink when monitor ports are available.

If the existing Phase 4 graph exposes monitor ports, support them through the same routing engine rather than special shell commands.

If a true aggregate “all system audio” virtual sink is not already in architecture, document that as a future enhancement rather than silently loading system modules or changing the system default sink.

---

# 9. DESTINATION MODEL AND ENDPOINT SELECTION

Reuse the Phase 4 `AudioEndpoint` / endpoint registry.

A routable Phase 5 output destination must represent an available playback endpoint backed by a current PipeWire sink/node.

## 9.1 Destination validation

Before activation, validate at least:

- endpoint exists logically,
- endpoint is marked available,
- endpoint direction is output/playback,
- current PipeWire node binding exists,
- compatible input ports can be resolved,
- it is not a duplicate destination selection.

Do not require the destination to be Bluetooth-specific.

Phase 5 should work with:

- built-in audio output,
- USB audio output,
- Bluetooth A2DP output,
- another future endpoint type,

as long as it appears as a supported `AudioEndpoint`.

This preserves transport independence.

## 9.2 Bluetooth mapping

For Bluetooth destinations, use the Phase 4 endpoint mapping to associate:

```text
BluetoothDevice
    -> AudioEndpoint
    -> current PipeWire node
```

Do not query BlueZ again from AudioRouter merely to rediscover an audio node mapping that Phase 4 already owns.

---

# 10. ROUTE PLANNING

Create a deterministic `RoutePlanner` responsibility.

Given:

```text
logical source
+ logical destination(s)
+ current PipeWire graph snapshot
```

it should produce a resolved plan:

```text
ResolvedRoutePlan
|
+-- routeId
+-- sourceNodeId
+-- destinations[]
    |
    +-- logicalEndpointId
    +-- destinationNodeId
    +-- portPairs[]
         |
         +-- outputPortId
         +-- inputPortId
         +-- channel
```

The planner must not create PipeWire links itself.

It should be as close to a pure/testable transformation as practical.

## 10.1 Planning rules

The planner must:

1. verify source existence,
2. verify source direction/capability,
3. verify every destination,
4. locate current source output ports,
5. locate destination input ports,
6. filter out control/MIDI/non-audio ports,
7. prefer audio-channel-compatible pairs,
8. avoid duplicate links,
9. produce an explicit failure if no valid plan exists,
10. never partially mutate the live PipeWire graph.

## 10.2 Port matching

Prefer deterministic channel-aware mapping.

Typical stereo example:

```text
Source out_FL -> Sink A in_FL
Source out_FR -> Sink A in_FR

Source out_FL -> Sink B in_FL
Source out_FR -> Sink B in_FR
```

Use existing PipeWire properties where available, such as:

- port direction,
- port name/alias,
- audio channel position,
- monitor-port marker,
- node ownership.

Do not match ports only by array index if meaningful channel metadata is available.

## 10.3 Fallback behavior

If channel metadata is absent:

- use a deterministic stable ordering only when source and destination topology is unambiguous,
- log that fallback matching was used,
- reject obviously incompatible layouts.

Do not silently connect left to right or audio to control ports.

## 10.4 Format compatibility

PipeWire links negotiate media format/buffers.

Do not build an unnecessary PCM conversion/re-encoding engine in Phase 5.

Let PipeWire/audio adapters negotiate supported formats when possible.

If link negotiation fails, capture that as a route/link failure and surface a meaningful error.

---

# 11. PIPEWIRE LINK CREATION — REQUIRED NATIVE APPROACH

Use the native PipeWire API already integrated in Phase 4.

The intended mechanism is the server-side **link factory**.

Conceptually:

```cpp
pw_core_create_object(
    core,
    "link-factory",
    PW_TYPE_INTERFACE_Link,
    PW_VERSION_LINK,
    &properties->dict,
    0);
```

Create links using properties equivalent to:

```text
link.output.node
link.output.port
link.input.node
link.input.port
```

Use the actual `PW_KEY_*` constants from the installed PipeWire headers rather than hardcoding strings where practical.

## 11.1 Explicit port links

For deterministic multi-channel routing, prefer explicit node + port bindings in the resolved route plan.

Each PipeWire `Link` represents a connection between an output port and an input port.

Create the required link object for each required port pair.

## 11.2 Link ownership

Auralis must know exactly which links it owns.

Maintain a runtime ownership record such as:

```text
OwnedLink
|
+-- routeId
+-- destinationId
+-- outputNodeId
+-- outputPortId
+-- inputNodeId
+-- inputPortId
+-- proxy
+-- globalId
+-- state
+-- error
```

Never destroy an arbitrary PipeWire link simply because it connects the same nodes.

Only remove links that were created/claimed by the current Auralis route lifecycle.

## 11.3 Object lifetime

For Phase 5 temporary routes, do **not** deliberately make links linger after Auralis exits unless the existing architecture explicitly requires persistent PipeWire objects.

The safe default is:

> Auralis-created route links live for the Auralis route and are destroyed when the route is deactivated or when the owning PipeWire connection is torn down.

Avoid leaving stale links in the graph.

## 11.4 Destroying links

Use the native PipeWire object/resource destruction path appropriate to how the current backend creates proxies, such as the existing wrapper around `pw_core_destroy()`/proxy destruction.

Do not invoke `pw-link -d`.

Destroy link listeners/hooks and clear ownership records safely.

## 11.5 Partial creation rollback

Activation must be transactional at the route level.

Example failure:

```text
Destination A left link created
Destination A right link created
Destination B left link created
Destination B right link fails
```

Do not report the route Active.

Either:

- roll back all newly created links for this activation attempt, or
- deliberately enter `Degraded` only if the project’s Phase 5 behavior explicitly allows partial activation and the state/UI makes this obvious.

Preferred initial implementation: **atomic activation with rollback on required-link failure**.

This is easier to reason about and test.

---

# 12. LINK STATE MONITORING

Do not assume `pw_core_create_object()` means audio is flowing.

Monitor the resulting link state.

PipeWire link states include concepts equivalent to:

```text
ERROR
UNLINKED
INIT
NEGOTIATING
ALLOCATING
PAUSED
ACTIVE
```

Use native link info/listener events and/or the existing Phase 4 LinkMonitor.

## 12.1 Route activation success

A route becomes `Active` only after all required owned links reach an acceptable operational state.

For the initial implementation, prefer:

```text
all required links == ACTIVE
```

If real devices commonly remain `PAUSED` until media begins to flow, distinguish “correctly linked but idle” from failure. If necessary, model an internal `Ready/Linked` status while keeping route semantics truthful.

Do not misclassify a suspended/idle but correctly connected sink as a hard error without verifying PipeWire behavior.

## 12.2 Link error propagation

When a link enters an error state:

- capture the PipeWire-provided error detail,
- identify the route and destination,
- update `OwnedLink`,
- update the aggregate `AudioRoute` state,
- log structured diagnostics,
- attempt limited replan/recovery if the graph changed,
- otherwise transition to `Failed` or `Degraded`.

---

# 13. STREAM MONITORING

Phase 5 must detect relevant source-stream lifecycle changes.

At minimum:

- new application playback stream appears,
- selected stream disappears,
- selected stream is recreated,
- stream node/ports change,
- source loses routable output ports.

Reuse the existing Phase 4 registry monitor where possible.

## 13.1 No polling-first architecture

Prefer PipeWire registry/events over periodic shell/polling logic.

A low-frequency safety reconciliation timer may exist if the current codebase uses one, but event-driven graph updates remain authoritative.

## 13.2 Stream recreation

When a selected application stream is destroyed and another semantically equivalent stream appears:

- do not continue using stale node/port IDs,
- invalidate the resolved route plan,
- preserve the high-level logical selection when possible,
- re-resolve against the new graph object,
- recreate owned links if recovery policy permits.

Do not promise perfect application identity persistence in Phase 5.

---

# 14. GRAPH CHANGE AND RECOVERY BEHAVIOR

Phase 5 must be robust against the normal dynamic nature of PipeWire.

## 14.1 Source disappears

If the selected source disappears:

1. mark the route `Degraded` or `Failed` according to recoverability,
2. remove stale runtime bindings,
3. do not destroy unrelated graph links,
4. listen for a matching source reappearance if the route uses automatic rebind,
5. replan before recreating links.

## 14.2 Destination disappears

If a destination endpoint/node disappears:

1. reflect endpoint unavailability from Phase 4,
2. mark the route `Degraded`,
3. clear/destroy any remaining owned-link proxies safely,
4. retain the logical destination ID if recovery policy is enabled,
5. when the same logical endpoint reappears with a new PipeWire node ID, replan against the new node/ports,
6. recreate links.

AudioRouter must not initiate Bluetooth pairing/reconnection itself.

Bluetooth lifecycle remains owned by the Phase 3 service.

## 14.3 Node ID changes

Treat PipeWire global/node/port IDs as runtime identifiers.

Never assume the ID for a Bluetooth sink remains unchanged after reconnect/profile change.

Resolve through the logical endpoint registry each time a route is planned/recovered.

## 14.4 Profile change

A Bluetooth reconnect or profile change may replace the available sink node.

When that happens:

- allow Phase 4 to update endpoint mapping,
- invalidate the route’s resolved plan,
- select only a playback-capable endpoint/profile,
- replan ports,
- reactivate if appropriate.

Do not hardcode A2DP-specific names as the only matching strategy.

## 14.5 PipeWire core disconnect

If the PipeWire core connection fails:

- transition active routes out of `Active`,
- invalidate all runtime proxies/global IDs,
- let the existing Phase 4 PipeWire connection recovery mechanism reconnect,
- rebuild/reconcile graph state,
- re-resolve routes only after the registry is synchronized again.

Do not dereference stale PipeWire proxies after disconnect.

---

# 15. ROUTE RECOVERY POLICY

Keep Phase 5 recovery deliberately small and explicit.

Recommended enum:

```text
NoAutomaticRecovery
RebindOnGraphReplacement
```

Optionally include a third mode only if the existing architecture already supports it.

`RebindOnGraphReplacement` means:

- if the same logical source/destination becomes available again,
- and the route is still logically enabled,
- then replan using current PipeWire node/port IDs and recreate the Auralis-owned links.

It does **not** mean:

- pair a Bluetooth device,
- force trust,
- reconnect BlueZ,
- create a Phase 6 session,
- perform audio synchronization/calibration.

---

# 16. VOLUME AND MUTE CONTROL

Phase 5 roadmap requires support for volume concepts including:

- source volume,
- destination volume,
- per-device volume,
- route/group volume,
- mute,
- optional normalization.

Implement this conservatively.

## 16.1 Priority order

At minimum implement:

1. destination volume where supported,
2. destination mute where supported,
3. route-level helper that applies a requested value to all selected destinations.

Source volume can be implemented where the selected source node exposes writable volume controls.

Optional normalization must remain optional and should not introduce a new DSP pipeline in Phase 5.

## 16.2 Native PipeWire controls

Where appropriate, bind to the relevant PipeWire node and use native node parameters.

PipeWire SPA property controls include concepts equivalent to:

```text
SPA_PARAM_Props
SPA_PROP_volume
SPA_PROP_channelVolumes
SPA_PROP_mute
```

Use installed headers and the current node capability/parameter information.

Do not assume every endpoint exposes writable software/hardware volume.

## 16.3 Capability-aware control

The `AudioEndpoint` or a routing-side capability view should expose whether volume/mute control is supported.

If unsupported:

- do not fail route activation,
- return a structured `Unsupported` result for that control,
- disable the relevant UI control,
- keep routing functional.

## 16.4 Volume value semantics

Expose a normalized application-level value, recommended:

```text
0.0 .. 1.0
```

Map it to PipeWire’s relevant control semantics.

Clamp safely.

Do not permit NaN/infinite/out-of-range values through the public API.

## 16.5 Route/group volume

For Phase 5, route/group volume means:

> apply one normalized request to each selected destination using each destination’s supported control path.

Do not build Phase 6’s persistent group/session volume coordinator.

If one destination cannot apply the value, return a partial-result structure rather than silently pretending all succeeded.

---

# 17. THREADING AND EVENT-LOOP SAFETY

This section is critical.

Do not make random PipeWire API calls from arbitrary Qt/QML threads.

Reuse the thread/loop architecture established in Phase 4.

## 17.1 PipeWire ownership

Determine which thread owns:

- `pw_context`,
- `pw_core`,
- registry,
- node proxies,
- link proxies,
- PipeWire listeners/hooks.

All graph mutation must follow the existing synchronization strategy.

If Phase 4 uses:

- a dedicated PipeWire thread loop,
- `pw_thread_loop`,
- a `pw_main_loop` integrated into Qt,
- queued invocations,

continue that strategy consistently.

## 17.2 UI interaction

Public AudioRouter calls invoked from QML/UI should be asynchronous or safely marshalled when the underlying PipeWire operation is asynchronous.

Do not block the GUI waiting indefinitely for link state.

Prefer signals/results such as:

```text
routeAdded
routeChanged
routeStateChanged
routeActivationFailed
routeRemoved
sourcesChanged
```

following the project’s conventions.

## 17.3 Callback lifetime

Ensure no callback can reference a deleted `AudioRoute`, `OwnedLink`, or service object.

Be disciplined with:

- `spa_hook` lifetime,
- proxy destruction ordering,
- queued Qt lambdas,
- QObject ownership,
- shutdown sequencing.

Add tests for destruction during pending activation if practical.

---

# 18. PUBLIC AUDIOROUTER API

Adapt to the existing project style.

A possible C++ API shape is:

```cpp
class AudioRouter : public QObject
{
    Q_OBJECT
public:
    // queries
    QList<AudioSource> sources() const;
    QList<AudioRoute> routes() const;
    std::optional<AudioRoute> routeById(const QString &id) const;

    // route lifecycle
    QString createRoute(const QString &sourceId,
                        const QStringList &destinationEndpointIds);
    void removeRoute(const QString &routeId);
    void activateRoute(const QString &routeId);
    void deactivateRoute(const QString &routeId);

    // selection changes while inactive or via controlled replan
    void setRouteSource(const QString &routeId, const QString &sourceId);
    void setRouteDestinations(const QString &routeId,
                              const QStringList &destinationEndpointIds);

    // volume
    void setDestinationVolume(const QString &endpointId, double value);
    void setDestinationMuted(const QString &endpointId, bool muted);
    void setRouteVolume(const QString &routeId, double value);
    void setRouteMuted(const QString &routeId, bool muted);

signals:
    void sourcesChanged();
    void routeAdded(const QString &routeId);
    void routeChanged(const QString &routeId);
    void routeRemoved(const QString &routeId);
    void routeStateChanged(const QString &routeId, RouteState state);
    void routeError(const QString &routeId,
                    RouteError error,
                    const QString &detail);
};
```

This is illustrative.

Do not force this exact API if the repository already has a command/result architecture.

## 18.1 Validation rules

Reject or normalize:

- empty route ID,
- unknown source ID,
- unknown destination IDs,
- empty destination list,
- duplicate destination IDs,
- source selected as destination,
- non-output endpoint as destination,
- activation of already-active route,
- deletion while activation callback is still unsafe.

Make operations idempotent where it improves robustness.

Examples:

- deactivating an already-inactive route should be harmless,
- repeated graph removal events must not double-free proxies.

---

# 19. ROUTE ACTIVATION ALGORITHM

Implement a clear activation sequence.

Recommended flow:

```text
activateRoute(routeId)
    |
    v
Validate high-level route
    |
    v
state = Planning
    |
    v
Snapshot current graph / resolve logical refs
    |
    v
RoutePlanner builds ResolvedRoutePlan
    |
    +--> fail -> state = Failed + structured error
    |
    v
state = Ready
    |
    v
state = Activating
    |
    v
Create required links through LinkManager
    |
    v
Monitor each link state
    |
    +--> required link fails -> rollback -> Failed
    |
    v
All required links operational
    |
    v
state = Active
```

## 19.1 Do not trust stale plans

Immediately before graph mutation, ensure source/destination/ports still exist.

If the graph changed between planning and activation:

- discard the stale plan,
- replan once or according to a bounded retry strategy,
- never loop forever.

## 19.2 Bounded activation

Do not allow a route to remain indefinitely in `Activating` with no explanation.

Use the project’s asynchronous timeout mechanism if appropriate.

A timeout should:

- not crash,
- not leak links,
- collect current link states,
- rollback owned links created by the attempt,
- transition to Failed with diagnostics.

Make timeout configurable for tests if necessary.

---

# 20. ROUTE DEACTIVATION ALGORITHM

Recommended flow:

```text
deactivateRoute(routeId)
    |
    v
state = Deactivating
    |
    v
Stop automatic rebind for current attempt
    |
    v
Destroy Auralis-owned link resources
    |
    v
Wait/reconcile removal events if necessary
    |
    v
Clear runtime bindings
    |
    v
state = Inactive
```

Do not disconnect the Bluetooth device merely because a route is deactivated.

Do not change system default audio devices unless a separate existing feature explicitly owns that behavior.

Do not destroy the underlying endpoint node.

---

# 21. PIPEWIRE PORT / GRAPH DATA EXTENSIONS

If Phase 4 does not retain sufficient port metadata for deterministic routing, extend the internal graph model.

Recommended port record fields:

```text
PipeWirePortInfo
|
+-- id
+-- objectSerial (if available)
+-- nodeId
+-- direction
+-- name
+-- alias
+-- audioChannel
+-- mediaClass / format hints if available
+-- monitor
+-- physical
+-- terminal
+-- control
```

Do not make these QML-exposed unless the UI needs them.

Keep low-level graph details inside the audio backend/domain.

---

# 22. PIPEWIRE REGISTRY RECONCILIATION

Use the registry as the dynamic graph source of truth.

The registry emits additions/removals as objects are hot-plugged or reconfigured.

Phase 5 must subscribe to/reuse those events and keep routing state consistent.

## 22.1 Global removal handling

When a global is removed:

- determine whether it belongs to a selected source,
- destination,
- port used by a plan,
- Auralis-owned link,
- underlying node.

Invalidate only the affected runtime state.

Do not clear the entire endpoint registry for one link removal.

## 22.2 Initial graph synchronization

Do not allow route planning before the existing PipeWire manager considers its initial registry sync ready.

If Phase 4 exposes a ready/synchronized signal/state, depend on it.

---

# 23. MINIMAL PHASE 5 UI DELIVERABLE

Phase 7 owns the complete GUI.

Phase 5 only needs enough UI to exercise and prove the routing engine.

Integrate into the existing desktop UI without redesigning the whole application.

Recommended minimal panel:

```text
Audio Routing
-------------------------------------------------
Source:
[ Selected Application / Source              v ]

Outputs:
[ ] Built-in Audio
[x] Hearing Device A
[x] Hearing Device B

Route state: Active

[ Activate Route ]   [ Deactivate Route ]

Route Volume: [---------o----] 70%
Mute: [ ]

Destination A: available / active
Destination B: available / active

Last error: none
```

## 23.1 UI requirements

The UI must:

- list current routable sources,
- list current output endpoints from Phase 4,
- permit one or more destination selections,
- create/update one simple current route or expose route records according to current UX architecture,
- activate/deactivate,
- show route state,
- show meaningful error text,
- expose volume/mute only where supported,
- update asynchronously when graph objects disappear/reappear.

Do not put low-level PipeWire IDs in the normal user UI.

They may appear in a diagnostics section.

---

# 24. LOGGING AND DIAGNOSTICS

Phase 5 must be highly diagnosable.

Add structured logs using the existing Auralis logging system.

At minimum log:

## Route lifecycle

```text
route created
route planning started
route plan completed
route activation started
route activated
route degraded
route replan started
route recovered
route deactivation started
route deactivated
route failed
```

## Link lifecycle

```text
link create requested
link proxy created
link bound/global ID known
link state changed
link negotiation error
link destroy requested
link removed
```

## Graph identity

For diagnostic/debug log levels include:

- route ID,
- logical source ID,
- source node ID,
- destination logical endpoint ID,
- destination node ID,
- output port ID,
- input port ID,
- channel/port names,
- PipeWire error string.

## Safety

Do not spam info-level logs for every unrelated PipeWire object event.

Use debug/trace appropriately.

---

# 25. TESTABILITY REQUIREMENT

The implementation must not be designed such that every test requires physical Bluetooth hardware.

Separate route-domain logic from the native PipeWire mutation layer.

Recommended interfaces/fakes:

```text
IPipeWireGraphView
IPipeWireLinkBackend
IAudioVolumeBackend
```

or equivalent using existing project abstraction conventions.

Do not add interfaces merely for ceremony if the codebase has another established mocking pattern.

The important requirement is that:

- RoutePlanner can be tested with synthetic graph data,
- AudioRouter state transitions can be tested with a fake LinkManager/backend,
- live native PipeWire tests are separate and opt-in.

---

# 26. REQUIRED UNIT TESTS

Add rigorous automated tests.

Names can follow existing conventions.

## 26.1 Route model tests

Test:

- unique route ID,
- default state,
- invalid empty destination list,
- duplicate destination removal/rejection,
- state transition validity,
- structured errors.

## 26.2 Source classification tests

Test classification of representative nodes:

```text
Stream/Output/Audio -> application playback source
Audio/Source        -> physical/virtual source
Audio/Sink          -> not an ordinary source except monitor ports
Stream/Input/Audio  -> not a playback-routing source
```

Test monitor-port detection if supported.

## 26.3 Destination validation tests

Test:

- playback endpoint accepted,
- capture-only endpoint rejected,
- unavailable endpoint rejected,
- duplicate endpoint rejected,
- missing current node binding handled.

## 26.4 RoutePlanner tests

Construct synthetic graphs.

At minimum:

### Mono

```text
source out_MONO -> sink in_MONO
```

### Stereo

```text
out_FL -> in_FL
out_FR -> in_FR
```

### One source to two destinations

Expected plan creates four stereo link pairs.

### Reversed registry ordering

Planner must still map channels correctly.

### Missing channel metadata

Exercise deterministic fallback.

### Incompatible ports

Planner returns `NoCompatiblePorts`.

### Control ports present

Planner ignores them.

### Destination disappears during planning snapshot

Planner fails cleanly.

## 26.5 LinkManager fake tests

Test:

- each requested port pair causes one native-link request,
- ownership record created,
- link state update routed to correct route,
- link error propagated,
- destroy only owned links,
- repeated destroy is safe,
- partial creation rollback.

## 26.6 AudioRouter state-machine tests

Test at minimum:

```text
Inactive -> Planning -> Ready -> Activating -> Active
Active -> Deactivating -> Inactive
Planning -> Failed
Activating -> Failed + rollback
Active -> Degraded on destination removal
Degraded -> Planning -> Activating -> Active on logical endpoint reappearance
Active -> Degraded on source loss
```

## 26.7 PipeWire disconnect tests

Using fake backend:

- active routes leave Active,
- runtime proxies invalidated,
- no use-after-free,
- reconnect/re-sync permits replan.

## 26.8 Volume tests

Test:

- 0.0,
- 1.0,
- midpoint,
- out-of-range clamping/rejection according to API contract,
- mute true/false,
- unsupported destination,
- one failure in route-level volume produces a partial result,
- no route state corruption after volume failure.

---

# 27. COMPONENT / INTEGRATION TESTS WITHOUT BLUETOOTH HARDWARE

Where a user PipeWire daemon is available, add opt-in tests that exercise the real PipeWire backend using safe temporary test nodes/streams or the existing audio graph.

Do not make ordinary `ctest` fail on headless CI merely because there is no PipeWire server.

Tests requiring a live PipeWire server should:

- detect prerequisites,
- be skipped by default or explicitly gated,
- have deterministic cleanup,
- never alter persistent user audio configuration.

Possible test strategy:

1. create or discover a controlled test source,
2. create/discover a controlled test sink if the existing test framework supports it,
3. route source -> sink,
4. observe created link(s),
5. assert link state progression,
6. deactivate,
7. assert owned links disappear.

If creating temporary native test nodes would over-expand scope, keep this test limited and rely on the hardware/live test below.

---

# 28. LIVE PHASE 5 ROUTING INTEGRATION TEST

Create a dedicated live test target following the conventions used by prior live Bluetooth/PipeWire integration tests.

Recommended name:

```text
tst_AudioRoutingLiveIntegration
```

or the closest project-consistent name.

## 28.1 Gate it explicitly

The test must not run against user audio hardware accidentally during ordinary `ctest`.

Recommended gate:

```text
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1
```

If the test requires a specific destination, support a stable selector such as one of:

```text
AURALIS_EXPECT_AUDIO_ENDPOINT_ID
AURALIS_EXPECT_BT_DEVICE_ADDRESS
AURALIS_EXPECT_PIPEWIRE_NODE_NAME
```

Prefer the project’s stable logical endpoint identity or Bluetooth address over a volatile numeric PipeWire node ID.

Do not require a literal placeholder like `<TEST_DEVICE_ADDRESS>` to be typed into a shell command.

Document how to substitute a real value or how the test can enumerate candidates.

## 28.2 Live test workflow

The live test should verify as much as practical:

1. Phase 4 PipeWire connection ready.
2. Target output endpoint exists.
3. A controlled/selectable source exists.
4. Route created.
5. Route planning succeeds.
6. Native links are created.
7. Link state is observed.
8. Route reaches operational state.
9. Optional short test tone/audio is produced if the test architecture supports it safely.
10. Route deactivates.
11. Auralis-owned links disappear.
12. No stale link remains.

Human confirmation of audible output may remain a manual validation step unless the environment provides a loopback measurement path.

---

# 29. MANUAL HARDWARE ACCEPTANCE TEST

Document a manual Phase 5 acceptance test.

Example workflow:

## Preparation

1. Launch Auralis.
2. Connect/pair a Bluetooth audio device using completed Phase 3 functionality.
3. Confirm Phase 4 shows the mapped Bluetooth audio endpoint as available.
4. Start a known audio source, for example a browser/music/test application.

## Routing

5. Open the Audio Routing section.
6. Select the application playback stream or supported laptop-audio source.
7. Select the Bluetooth output endpoint.
8. Activate route.
9. Confirm UI state becomes active/operational.
10. Confirm audio is audible at the selected endpoint.

## Volume

11. Change destination volume.
12. Toggle mute.
13. Confirm UI remains responsive and route remains valid.

## Clean deactivation

14. Deactivate the route.
15. Confirm audio is no longer routed by Auralis.
16. Confirm endpoint remains connected.
17. Confirm no stale Auralis links remain.

## Dynamic graph

18. Reactivate route.
19. Disconnect or power off the Bluetooth device.
20. Confirm route state changes away from Active without crash.
21. Reconnect the device.
22. Confirm Phase 4 maps the new PipeWire endpoint.
23. If automatic Phase 5 rebind is enabled, confirm route recovers; otherwise confirm a clean reactivation succeeds.

---

# 30. MULTI-DESTINATION PHASE 5 TEST

Phase 5 must exercise one source to at least two outputs where hardware/environment permits.

Example:

```text
Application playback stream
          |
          +--> Built-in Audio
          |
          +--> Bluetooth Headphones
```

or:

```text
Application playback stream
          |
          +--> Bluetooth Device A
          |
          +--> Bluetooth Device B
```

Do not claim synchronized playback as a Phase 5 success metric.

The success metric is:

- required links exist,
- both destinations receive the selected source where supported by the graph/hardware,
- route state reflects failures truthfully,
- cleanup is correct.

Synchronization/latency compensation belongs to later work.

---

# 31. CMAKE / BUILD INTEGRATION

Follow the existing build structure.

Likely work may include:

- adding new audio routing source/header files,
- linking against the existing PipeWire target already used by Phase 4,
- registering new Qt metatypes/enums if required,
- adding test targets,
- adding integration test labels/options.

Do not add duplicate `pkg-config` discovery if Phase 4 already defines an imported PipeWire target.

Do not globally add compiler flags that affect unrelated phases.

All builds must continue to support the repository’s established configuration.

---

# 32. COMPILER QUALITY GATE

Build with the project’s normal warnings.

Do not introduce new warnings.

Pay special attention to:

- C callback casts,
- lifetime of listener data,
- signed/unsigned PipeWire IDs,
- `SPA_ID_INVALID`,
- raw pointer ownership,
- integer/string conversion of PipeWire property IDs,
- Qt queued signal argument registration,
- missing enum switch cases,
- narrowing conversion of volumes/channel counts.

Use RAII wrappers where consistent with the existing codebase.

Do not create elaborate wrappers that obscure the native PipeWire API without benefit.

---

# 33. MEMORY / RESOURCE SAFETY

Phase 5 introduces graph objects and callbacks, so resource handling must be explicit.

Audit:

- `pw_proxy*` ownership,
- `pw_link*` proxies,
- `spa_hook` removal,
- property object ownership (`pw_properties*`),
- QObject lifetime,
- queued callbacks after destruction,
- route deletion while active,
- application shutdown with active routes,
- core disconnect during activation.

On shutdown:

1. stop route recovery,
2. deactivate/destroy owned links if the core is valid,
3. unregister listeners/hooks safely,
4. tear down routing services before the underlying PipeWire owner disappears, following existing dependency order.

No use-after-free.

No double destruction.

No intentionally leaked links.

---

# 34. ERROR HANDLING REQUIREMENTS

Never crash because:

- a selected endpoint vanishes,
- a port is removed between plan and create,
- PipeWire refuses link creation,
- a link enters error,
- a node becomes suspended,
- the core disconnects,
- volume control is unsupported,
- the UI issues repeated activate/deactivate commands.

Every external operation must have a defined failure path.

Prefer project-standard `Result<T, Error>` / signal error architecture if one exists.

Do not use exceptions across C callbacks unless the project explicitly and safely does so.

Never allow a C++ exception to escape through a PipeWire C callback boundary.

---

# 35. SECURITY / PERMISSION BEHAVIOR

If the PipeWire server denies graph mutation or node control:

- report a structured permission failure,
- log enough information to debug,
- keep the application alive,
- do not attempt privilege escalation,
- do not invoke `sudo`,
- do not modify PipeWire/WirePlumber system configuration automatically.

Phase 5 is a user-session audio feature.

---

# 36. PERFORMANCE EXPECTATIONS

Phase 5 routing should be lightweight.

Do not copy PCM data through the Qt event loop merely to route existing PipeWire nodes.

Prefer graph links so PipeWire handles the audio path.

Do not implement a per-sample C++ forwarding loop unless a specific source mode technically requires an Auralis-created stream.

The normal path should be graph orchestration, not audio-buffer ownership.

Avoid:

- high-frequency polling,
- rebuilding all routes for unrelated graph events,
- repeated full endpoint scans for every link state callback,
- blocking waits on the GUI thread.

---

# 37. OBSERVABILITY / OPTIONAL ROUTE METRICS

If existing PipeWire data makes this inexpensive, expose diagnostic fields such as:

```text
requiredLinkCount
activeLinkCount
failedLinkCount
sourceNodeId
currentDestinationNodeIds
lastTransitionTimestamp
lastError
```

Latency may be recorded as observational metadata if already known.

Do not implement active latency measurement DSP in Phase 5.

---

# 38. API STABILITY AND PHASE 6 PREPARATION

Design Phase 5 so Phase 6 can compose it rather than replace it.

Phase 6 should eventually be able to call something equivalent to:

```text
SessionManager
    |
    v
AudioRouter
    |
    v
AudioRoute(s)
```

Therefore:

- `AudioRouter` must be usable without QML,
- route creation/activation must be programmatic,
- destination IDs must use stable Auralis endpoint identity,
- route state must be queryable/observable,
- volume APIs should be reusable from a future coordinator,
- recovery behavior should not be hardcoded into UI widgets.

Do not implement `SessionManager` now.

---

# 39. IMPLEMENTATION ORDER

Use small verified increments.

Recommended sequence:

## Step 1 — Audit and design integration

- map existing Phase 4 classes,
- identify missing graph metadata,
- decide exact file/class placement,
- confirm PipeWire thread ownership.

## Step 2 — Domain model

- `AudioRoute`,
- route state/error types,
- optional source model,
- unit tests.

## Step 3 — Source selector/model

- classify existing nodes/streams,
- expose routable sources,
- track dynamic source lifecycle,
- tests.

## Step 4 — RoutePlanner

- logical validation,
- node resolution,
- port matching,
- resolved plans,
- extensive synthetic graph tests.

## Step 5 — LinkManager

- native PipeWire link creation,
- ownership,
- listeners/state,
- destruction,
- rollback,
- fake/component tests.

## Step 6 — AudioRouter orchestration

- route lifecycle,
- activation/deactivation,
- state aggregation,
- graph-change reaction,
- bounded recovery.

## Step 7 — VolumeController

- capability detection,
- destination volume/mute,
- route-level fan-out,
- tests.

## Step 8 — Minimal UI

- source selection,
- multi-output selection,
- activate/deactivate,
- state/error,
- volume/mute.

## Step 9 — Live integration test

- opt-in PipeWire test,
- hardware test documentation.

## Step 10 — Full regression

- clean configure/build,
- all ctests,
- desktop launch,
- no Phases 0–4 regression.

Do not make one giant unverified commit.

---

# 40. REQUIRED REGRESSION TESTS FOR PHASES 0–4

After Phase 5 changes, ensure all previous tests continue to pass.

At minimum run the repository’s established equivalent of:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If the project uses a clean build for phase verification, also perform:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Do not weaken previous tests to make Phase 5 pass.

Do not disable a test because your changes broke it.

Fix the regression.

---

# 41. LIVE TEST COMMAND DOCUMENTATION

At the end of implementation, provide exact commands based on the targets actually created.

For example, if implemented as recommended:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
AURALIS_EXPECT_BT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" \
ctest --test-dir build \
  -R tst_AudioRoutingLiveIntegration \
  --output-on-failure
```

If the target selector differs, document the actual variable.

Important:

- do not write shell examples with an unquoted `<PLACEHOLDER>` token that Bash interprets as redirection,
- show realistic quoted example values,
- explain how to discover/select the actual test endpoint,
- keep test-only shell tools out of production implementation.

---

# 42. TEST TONE SUPPORT — OPTIONAL BUT USEFUL

A small **test-only** native PipeWire tone source can make live routing verification deterministic.

If the repository already has a safe test audio generator or if adding one is modest, consider a test utility that:

- creates a `pw_stream` playback/output source,
- emits a low-volume sine tone for a short bounded duration,
- has a clearly recognizable node/media name,
- is compiled only into tests/tools,
- never starts automatically in the production app.

This utility can provide a stable source for:

```text
TestToneSource -> AudioRouter -> selected endpoint
```

Keep the volume conservative.

Do not make this a dependency for normal Auralis routing.

---

# 43. ROUTE OWNERSHIP TAGGING

Where supported, give Auralis-created links meaningful properties for diagnostics.

For example, properties may identify:

```text
application.name / client identity
route id
human-readable purpose
```

Use standard PipeWire properties where appropriate and project-specific names only when safe.

Do not rely exclusively on custom tags for ownership—retain the actual created proxy/object record in process memory.

---

# 44. DO NOT FIGHT WIREPLUMBER UNNECESSARILY

WirePlumber is the active session manager.

Auralis should intentionally create the links it owns, but it should not broadly disable WirePlumber policy, edit system policy files, or change default-device policy as a side effect of Phase 5.

If WirePlumber independently creates default links for the same application stream, Auralis must avoid accidentally deleting those links.

This reinforces the owned-link rule.

If duplicate audio to the system default + selected endpoints is desired, leave existing default links untouched and add Auralis-owned additional links when technically supported.

If “move exclusively to Auralis destination” is later needed, make that an explicit routing policy rather than silently destroying session-manager links.

---

# 45. ROUTING POLICY

Define a simple Phase 5 routing policy rather than ambiguous behavior.

Recommended initial policy:

```text
AdditiveRouting
```

Meaning:

> Auralis adds its own links from the selected source to selected destination(s) and does not remove links created by WirePlumber or other clients.

Optionally define:

```text
ExclusiveRouting
```

only if the existing product requirements and implementation can safely identify/remove/move only appropriate links.

If not required, do not implement ExclusiveRouting in Phase 5.

Document the actual behavior.

---

# 46. MULTI-DESTINATION GRAPH SEMANTICS

One source feeding multiple destinations may involve multiple links from the same source output ports.

Do not assume every node/port configuration supports unlimited fan-out.

If PipeWire or a particular node rejects the topology:

- surface the failure accurately,
- do not emulate the feature through shell commands,
- do not silently start a CPU-heavy user-space copy engine,
- document the observed limitation.

If a dedicated Auralis fan-out/filter node becomes technically necessary, first demonstrate why direct graph links are insufficient and implement the smallest native PipeWire solution consistent with Phase 5.

Do not prematurely implement the future synchronization/buffering engine.

---

# 47. DESTINATION SUSPENDED / IDLE HANDLING

PipeWire nodes can legitimately be suspended or idle when no audio is flowing.

Do not equate every suspended state with a permanent error.

Differentiate:

- endpoint unavailable/removed,
- link negotiation error,
- sink idle/suspended waiting for audio,
- active audio flow.

Use existing node/link state information.

The route UI should avoid frightening false failures when a valid route is simply idle.

---

# 48. SHUTDOWN TEST

Add or manually verify:

1. activate a route,
2. close Auralis normally,
3. restart Auralis,
4. inspect graph,
5. verify no stale Auralis-created temporary links remain from the previous process,
6. verify Phase 4 graph monitoring starts cleanly.

Also test application destruction while a route is `Activating` if practical.

---

# 49. REPEATED CYCLE TEST

Automate or manually run:

```text
activate
wait operational
deactivate
repeat 20–100 times
```

Verify:

- no accumulated links,
- no proxy growth,
- no listener leak,
- no crash,
- no duplicate route ownership entries,
- stable UI state.

Use a lower cycle count in normal CI and a larger count in stress/integration mode if needed.

---

# 50. GRAPH CHURN TEST

Using fakes and, if possible, a live integration environment, test graph churn:

```text
source exists
route active
source removed
source recreated with new node/port IDs
route replans
```

and:

```text
destination endpoint A mapped to node 100
route active
node 100 removed
the same logical endpoint reappears as node 147
route must never reuse node 100
route replans using 147
```

This is a core Phase 5 requirement.

---

# 51. DIAGNOSTIC SNAPSHOT

If consistent with the existing diagnostics architecture, add a method/log snapshot such as:

```text
AudioRouter Diagnostics
-----------------------
PipeWire ready: yes
Sources: 3
Routes: 1

Route: <id>
State: Active
Source: Firefox / node 92
Destinations:
  Hearing Device A / node 110
Owned links:
  201: 92:45 -> 110:31 ACTIVE
  202: 92:46 -> 110:32 ACTIVE
Last error: none
```

This can be invaluable for future Phase 6 work.

Do not expose internal raw pointers.

---

# 52. DOCUMENTATION TO UPDATE

At completion update the repository documentation appropriately.

At minimum document:

- Phase 5 architecture,
- route model,
- source types supported,
- destination behavior,
- additive routing policy if used,
- volume/mute support limitations,
- live test instructions,
- known hardware/PipeWire limitations,
- Phase 5 exit-gate results.

If the project tracks phase status in a roadmap/readme, mark Phase 5 complete **only after tests pass**.

Do not edit the original historical roadmap in a way that destroys its intended record unless the project already treats it as live status documentation.

---

# 53. EXPECTED FILES — GUIDANCE ONLY

Do not blindly create these paths. Adapt to the repository.

A possible arrangement is:

```text
src/audio/
  AudioRouter.hpp
  AudioRouter.cpp
  AudioRoute.hpp
  AudioRoute.cpp
  AudioSource.hpp
  RoutePlanner.hpp
  RoutePlanner.cpp
  PipeWireLinkManager.hpp
  PipeWireLinkManager.cpp
  VolumeController.hpp
  VolumeController.cpp

src/audio/pipewire/
  ...existing Phase 4 files...

 tests/
  tst_AudioRoute.cpp
  tst_RoutePlanner.cpp
  tst_AudioRouter.cpp
  tst_PipeWireLinkManager.cpp
  tst_AudioRoutingLiveIntegration.cpp
```

If current repository uses `modules/audio`, `lib/auralis/audio`, or another convention, follow it.

---

# 54. EXPECTED SIGNAL FLOW

A typical Phase 5 runtime should look like:

```text
PipeWire registry event
      |
      v
Existing Phase 4 graph model
      |
      +----> AudioEndpointRegistry
      |
      +----> source/stream view
                  |
                  v
              AudioRouter
                  |
          RoutePlanner resolves
                  |
                  v
              LinkManager
                  |
                  v
        pw_core_create_object(...)
                  |
                  v
          PipeWire link events
                  |
                  v
            route state update
                  |
                  v
              Qt/QML UI
```

No shell parser belongs in this flow.

---

# 55. ROUTE STATE INVARIANTS

Enforce invariants.

Examples:

## Inactive

```text
owned active link count == 0
```

## Active

```text
logical source valid
>= 1 logical destination
resolved plan valid
all required owned links accounted for
no fatal route error
```

## Failed

```text
lastError != None
```

## Deactivating

New automatic recovery must not race with manual deactivation.

## Degraded

The route must explain *why* it is degraded.

Add debug assertions where appropriate, but do not crash production on asynchronous external graph changes.

---

# 56. RACE CONDITIONS TO EXPLICITLY HANDLE

Review and test these races:

1. User activates route while destination disappears.
2. User deactivates while link creation callbacks are arriving.
3. User deletes route while destination removal callback is queued.
4. PipeWire disconnects during activation.
5. Core reconnects while old proxy-removal events are pending.
6. Source stream dies between plan and link creation.
7. Destination node stays but ports are recreated.
8. User repeatedly clicks Activate.
9. Automatic rebind triggers at same time as manual deactivate.
10. Application exits while recovery is scheduled.

Use generation tokens/operation IDs if necessary so stale callbacks cannot complete a newer activation attempt.

A recommended pattern is an activation generation counter:

```text
route.operationGeneration++
```

Every asynchronous callback verifies it still belongs to the current generation before mutating route state.

Use this only if it fits the existing architecture.

---

# 57. ROUTE PLAN GENERATION / VERSIONING

Because the graph changes dynamically, a resolved plan should be treated as ephemeral.

Optionally track:

```text
planGeneration
sourceGraphIdentity
endpointGraphIdentities
```

Any relevant graph replacement invalidates the plan.

Do not serialize resolved PipeWire port IDs to disk.

---

# 58. VOLUME BACKEND DETAIL

When implementing native node volume/mute:

1. inspect whether the node advertises/supports the required parameter,
2. bind through the existing PipeWire registry/core,
3. build the required SPA POD safely,
4. call the relevant native node parameter setter,
5. observe parameter updates when practical,
6. update Auralis state only after request/observation semantics consistent with the existing codebase.

Do not hardcode stereo channel count for all endpoints.

For per-channel volumes, build arrays matching the actual channel count when needed.

For a simple overall volume, prefer the node's supported scalar/channel controls rather than assuming hardware behavior.

---

# 59. OPTIONAL NORMALIZATION

The roadmap lists optional normalization.

Do not make normalization mandatory for the Phase 5 gate.

If implemented, use existing PipeWire/channelmix capabilities or a clearly isolated option.

Do not introduce a custom limiter/compressor/DSP stack solely for Phase 5.

Mark it experimental/optional and test disabled-by-default behavior.

---

# 60. DO NOT CHANGE AUDIO CODECS

Phase 5 controls graph routing.

It should not choose Bluetooth SBC/AAC/aptX/LC3 codecs unless that is already a separate completed Phase 4/profile-management capability.

Auralis should route to the current audio endpoint/profile exposed by PipeWire.

Codec/profile policy is separate from the routing engine.

---

# 61. DO NOT RECONNECT DEVICES FROM AUDIOROUTER

If the route target disappears because Bluetooth disconnected:

```text
AudioRouter -> mark destination unavailable / degraded
BluetoothManager -> owns reconnection lifecycle
EndpointResolver -> maps endpoint when it reappears
AudioRouter -> replans/rebinds
```

Do not create circular ownership where AudioRouter directly calls low-level BlueZ pairing/connection APIs.

A future higher-level coordinator can orchestrate both.

---

# 62. MINIMUM DEFINITION OF DONE

Do not declare Phase 5 complete unless all of these are true:

- [ ] repository inspected before implementation,
- [ ] Phase 4 architecture reused,
- [ ] no duplicate PipeWire registry stack introduced,
- [ ] AudioRoute model exists,
- [ ] explicit route state/error model exists,
- [ ] routable sources can be enumerated,
- [ ] output endpoints can be selected,
- [ ] route plans are deterministic/tested,
- [ ] native PipeWire links can be created,
- [ ] native PipeWire links can be destroyed,
- [ ] Auralis tracks link ownership,
- [ ] link states/errors are monitored,
- [ ] one-source -> one-output routing works,
- [ ] one-source -> multiple-output planning/routing works where supported,
- [ ] route activation rollback works,
- [ ] route deactivation is clean,
- [ ] source disappearance handled,
- [ ] destination disappearance handled,
- [ ] new node IDs after reappearance handled,
- [ ] no stale IDs are reused,
- [ ] volume/mute works on supported endpoints,
- [ ] unsupported volume does not break routing,
- [ ] minimal UI/API can exercise the workflow,
- [ ] all new unit tests pass,
- [ ] prior Phase 0–4 tests still pass,
- [ ] live routing test exists and is opt-in,
- [ ] no production shell-command dependency exists,
- [ ] no stale Auralis links remain after deactivation/shutdown,
- [ ] implementation documentation updated,
- [ ] Phase 5 exit condition demonstrated.

---

# 63. FINAL VERIFICATION COMMANDS

At completion, run the actual repository commands.

At minimum:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Then launch the desktop app using the actual built target, for example if unchanged:

```bash
./build/apps/desktop/auralis-desktop
```

Do not assume that path if the repository differs; report the actual path.

Run the opt-in Phase 5 live integration test using the environment variables implemented by the test.

---

# 64. REQUIRED FINAL REPORT FROM THE AI IDE

After implementing Phase 5, do not merely say “done”.

Return a detailed implementation report containing the following sections.

## A. Repository baseline discovered

List the relevant existing Phase 4 classes/files that were reused.

## B. Files added

List every new file and its purpose.

## C. Files modified

List every modified file and why.

## D. Architecture implemented

Explain:

- AudioRouter,
- RoutePlanner,
- LinkManager,
- source model,
- route state model,
- volume backend,
- graph-change handling.

## E. Native PipeWire implementation

State exactly how links are created/destroyed and how their states are observed.

## F. Routing policy

State whether routing is additive or exclusive.

## G. Multi-destination behavior

Explain how one source is routed to multiple destinations and any observed PipeWire/hardware limitations.

## H. Recovery behavior

Explain:

- source removal,
- destination removal,
- node ID changes,
- PipeWire disconnect,
- rebind policy.

## I. Volume behavior

Explain supported controls and unsupported-endpoint behavior.

## J. Tests added

List all new tests.

## K. Test results

Provide exact summary:

```text
Configure: PASS/FAIL
Build: PASS/FAIL
Unit tests: X/Y PASS
Integration tests: ...
Phase 0–4 regression: PASS/FAIL
Desktop launch: PASS/FAIL
Live route test: PASS/FAIL/SKIPPED with reason
```

## L. Manual test commands

Provide exact commands to reproduce the live test.

## M. Known limitations

Be explicit.

Examples:

- hardware fan-out limitation,
- stream disappears and cannot be semantically matched,
- endpoint volume unsupported,
- destination remains paused until playback starts.

## N. Phase 5 gate decision

End with exactly one of:

```text
PHASE 5 STATUS: COMPLETE
```

or

```text
PHASE 5 STATUS: NOT COMPLETE
```

If not complete, enumerate the blockers.

---

# 65. IMPLEMENTATION QUALITY BAR

The Phase 5 implementation must feel like the natural next layer of Auralis, not a demo bolted onto it.

The desired properties are:

```text
Native
Deterministic
Event-driven
Transport-independent
Thread-safe
Leak-free
Testable
Observable
Incremental
Phase-boundary-aware
```

Avoid cleverness that makes later debugging difficult.

Prefer explicit models, explicit ownership, and explicit state transitions.

---

# 66. PIPEWIRE TECHNICAL NOTES TO FOLLOW

Use these implementation facts as guidance, but verify everything against the **installed PipeWire headers** on the target machine and the existing Phase 4 wrappers.

## Graph model

PipeWire is graph based:

```text
Node output port -> Link -> Node input port
```

## Link creation

The native API provides creation of server objects from factories. The link factory can create a `PW_TYPE_INTERFACE_Link` using link endpoint properties.

## Dynamic graph

Registry global objects can appear and disappear because of hotplug/reconfiguration. Route plans must therefore use current runtime IDs and react to removal events.

## Link state

A link is not simply boolean. It negotiates formats/buffers and can be in init/negotiating/allocating/paused/active/error-like states.

## Volume

Node parameters can expose properties including scalar/per-channel volume and mute. Capability must be checked; not every endpoint supports the same control.

## Source classes

Do not confuse:

```text
Stream/Output/Audio  // application playback stream
Audio/Source         // source device
Audio/Sink           // playback destination
Stream/Input/Audio   // capture stream
```

## Target identities

Prefer stable Auralis identity at the domain layer; PipeWire numeric object/node/port IDs remain runtime bindings.

---

# 67. NON-NEGOTIABLE PROHIBITIONS

Do **not**:

- rewrite completed Phases 0–4,
- replace native Phase 4 PipeWire integration with PulseAudio CLI calls,
- implement routing through `QProcess("pw-link ...")`,
- implement routing through `QProcess("pactl ...")`,
- parse `wpctl status` in production,
- store only volatile PipeWire node IDs as endpoint identity,
- destroy links not owned by Auralis,
- treat link creation request as proof of successful routing,
- block the GUI indefinitely,
- run PipeWire callbacks against destroyed C++ objects,
- leave stale links after route deactivation,
- reconnect/pair Bluetooth inside AudioRouter,
- implement Phase 6 sessions,
- implement synchronization DSP,
- claim audible success without performing live validation where hardware is available,
- weaken previous test gates.

---

# 68. ACCEPTANCE SCENARIOS

The finished implementation should satisfy these scenarios.

## Scenario 1 — Basic single endpoint

```text
Given:
  PipeWire ready
  source S exists
  endpoint A exists

When:
  route(S -> A) is activated

Then:
  valid port plan is created
  Auralis-owned links are created
  route becomes operational
  route can be deactivated
  links are cleaned up
```

## Scenario 2 — Bluetooth reconnect changes node ID

```text
Given:
  endpoint A maps to node 120
  route active

When:
  Bluetooth disconnect removes node 120
  endpoint becomes unavailable
  later endpoint A maps to node 175

Then:
  route never reuses node 120
  route replans against 175
  recovery succeeds if enabled
```

## Scenario 3 — Two destinations

```text
Given:
  stereo source S
  stereo endpoints A and B

When:
  route(S -> [A, B]) activates

Then:
  expected per-channel link pairs are created
  all required links are tracked
  route state reflects aggregate health
```

## Scenario 4 — Partial failure

```text
Given:
  destination A links successfully
  destination B fails during activation

Then:
  route does not falsely report Active
  activation rolls back owned links
  structured failure is returned
```

## Scenario 5 — Volume unsupported

```text
Given:
  route is active
  endpoint A routes audio but does not expose writable volume

When:
  volume change is requested

Then:
  routing remains active
  API reports volume unsupported
  UI can disable/indicate control limitation
```

## Scenario 6 — Manual deactivation during recovery

```text
Given:
  route is degraded waiting for endpoint A

When:
  user deactivates route
  endpoint A reappears immediately afterward

Then:
  stale recovery callback must not reactivate the route
  route remains Inactive
```

---

# 69. PHASE 5 COMPLETION PHILOSOPHY

This phase is the point where Auralis changes from:

```text
“I can see Bluetooth audio endpoints.”
```

into:

```text
“I can deliberately control audio graph routing to those endpoints.”
```

That is the milestone.

Do not dilute it with unrelated future work, and do not shortcut it with command-line wrappers.

Build a small, robust, native routing engine that Phase 6 can trust.

---

# 70. START NOW

Proceed with Phase 5 implementation in the existing repository.

Your first concrete action is to **audit the Phase 4 implementation and map the existing audio/PipeWire types to the responsibilities in this document**.

Then implement Phase 5 incrementally using the ordered plan above.

Do not stop after scaffolding.

Continue through implementation, tests, regression verification, and the final Phase 5 status report.

