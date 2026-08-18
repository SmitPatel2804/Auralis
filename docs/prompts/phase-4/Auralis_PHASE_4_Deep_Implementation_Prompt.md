# Auralis — PHASE 4 Implementation Prompt
## PipeWire Audio Integration, Audio Endpoint Registry, and Bluetooth ↔ PipeWire Correlation

**Use this document as the implementation prompt for the AI coding IDE.**

---

# 0. ROLE AND EXECUTION MODE

You are implementing **PHASE 4 of Auralis** in an existing C++/Qt/QML codebase.

Act as a senior Linux audio systems engineer, C++ architect, Qt engineer, and test engineer.

Your job is **not** to redesign Auralis from scratch.

Your job is to inspect the existing repository, understand the implementations already completed in Phases 0–3, and then implement Phase 4 in a way that:

- preserves the existing architecture;
- preserves all working Phase 0–3 behavior;
- integrates with the existing CMake structure;
- uses the existing logging/error-handling conventions;
- uses the existing Bluetooth device model and registry rather than duplicating them;
- uses the native PipeWire client API for production functionality;
- provides deterministic unit tests for hardware-independent logic;
- provides opt-in live integration tests where a real PipeWire/BlueZ environment is required;
- leaves Phase 5 audio routing intentionally unimplemented.

Do not blindly create files named in this prompt if equivalent files/classes already exist. First inspect the repository and adapt this design to the actual codebase.

Do not delete or rewrite working Phase 1–3 code simply to make Phase 4 easier.

Do not use shell command parsing as production application logic.

---

# 1. PROJECT STATUS

Treat the following project state as authoritative:

```text
PHASE 0 — Development Environment          COMPLETE
PHASE 1 — Project Foundation               COMPLETE
PHASE 2 — Bluetooth Device Discovery       COMPLETE
PHASE 3 — Bluetooth Device Management      COMPLETE
PHASE 4 — PipeWire Audio Integration       IMPLEMENT NOW
PHASE 5 — Audio Routing Engine             DO NOT IMPLEMENT YET
PHASE 6 — Multi-Device Session Engine      FUTURE
PHASE 7 — Complete GUI                     FUTURE
PHASE 8 — Reliability / Packaging          FUTURE
```

Phases 0–3 are already implemented, tested, and working.

Your implementation must therefore begin with a repository audit rather than with greenfield scaffolding.

---

# 2. PHASE 4 OBJECTIVE

The purpose of Phase 4 is to make Auralis understand the **PipeWire audio graph** and expose audio-capable endpoints as stable Auralis domain objects.

Auralis must be able to observe PipeWire, identify audio devices and nodes, track their lifecycle, and correlate Bluetooth audio endpoints with the Bluetooth devices already managed by Phases 2 and 3.

The required relationship is:

```text
BlueZ / Phase 3 BluetoothDevice
               |
               | correlation
               v
       PipeWire Device
               |
               | owns / references
               v
       PipeWire Audio Node
               |
               | normalized into
               v
        Auralis AudioEndpoint
```

A Bluetooth device and an audio endpoint are **not** the same domain object.

A physical Bluetooth device may expose:

- one playback node;
- one capture node;
- multiple nodes for multiple profiles;
- an A2DP playback endpoint;
- an HFP/HSP capture/playback endpoint;
- different endpoints after an active-profile change;
- different PipeWire object IDs after reconnect.

Phase 4 must model this correctly.

---

# 3. PHASE 4 EXIT GATE

Phase 4 is complete only when the following real-world flow works:

```text
1. A Bluetooth audio device is discovered/known by Auralis.
2. The user pairs/trusts/connects it using the existing Phase 3 flow.
3. BlueZ reports the device connected.
4. PipeWire/WirePlumber exposes the corresponding audio device/node.
5. Auralis detects the relevant PipeWire object(s) without using wpctl/pactl parsing.
6. Auralis creates or updates AudioEndpoint objects.
7. Auralis maps the Bluetooth endpoint(s) back to the existing BluetoothDevice.
8. Endpoint availability is reflected when PipeWire objects appear/disappear/change.
9. The application remains stable if the Bluetooth device disconnects or PipeWire objects are recreated.
10. The endpoint can be displayed in Auralis as an available audio endpoint.
```

Phase 4 is **not complete** merely because PipeWire initializes.

It is **not complete** merely because the application prints registry objects.

It is **not complete** merely because `wpctl status` shows a Bluetooth sink.

It is complete when the Auralis model tracks and correlates the endpoint.

---

# 4. HARD PHASE BOUNDARY

## 4.1 Implement in Phase 4

Implement:

- native PipeWire initialization;
- connection to the user PipeWire server;
- PipeWire registry monitoring;
- global object add/remove handling;
- relevant device monitoring;
- relevant node monitoring;
- relevant port/link observation where useful for graph diagnostics;
- object-property normalization;
- Auralis `AudioEndpoint` domain model;
- endpoint registry;
- endpoint availability lifecycle;
- endpoint-to-Bluetooth mapping;
- mapping refresh after BlueZ or PipeWire state changes;
- clean startup and teardown;
- PipeWire status surfaced to application/UI;
- a minimal endpoint list/status UI if the existing UI architecture supports it;
- deterministic tests for registry and mapping behavior;
- opt-in live integration tests;
- diagnostics/logging sufficient to verify the mapping.

## 4.2 Explicitly DO NOT implement in Phase 4

Do **not** implement:

- creation of audio routes;
- PipeWire link creation for user-selected routing;
- stream duplication;
- system-audio capture;
- multiple-destination playback;
- group routing;
- synchronization;
- latency compensation;
- per-device buffering;
- route recovery;
- session orchestration;
- audio encoding/transcoding;
- LE Audio-specific transport logic;
- Auracast;
- Phase 5 source selection;
- Phase 6 session behavior;
- a large GUI redesign.

If route-related classes already exist as stubs from Phase 1, leave them as stubs or interfaces.

Phase 4 should produce reliable **observability and endpoint identity**, which Phase 5 will consume.

---

# 5. AUTHORITATIVE PLATFORM BASELINE

The development platform is currently:

```text
Ubuntu 26.04 LTS
Linux 7.x
x86_64

GCC/G++ 15.x
CMake 4.x
Ninja

Qt 6.10.x
Qt Quick / QML
Qt Bluetooth
Qt Multimedia

BlueZ 5.85
D-Bus
PipeWire 1.6.x
WirePlumber
pipewire-pulse
```

Production architecture:

```text
Auralis
  |
  +-- BlueZ via D-Bus             <- Bluetooth control/state
  |
  +-- PipeWire native API         <- audio graph/state
  |
  +-- Qt/QML                      <- app/UI state
```

Do not replace native integrations with shell wrappers.

---

# 6. FIRST ACTION: REPOSITORY AUDIT

Before changing any code, inspect the complete repository.

Produce an internal implementation map containing:

1. current top-level directory layout;
2. root and nested `CMakeLists.txt`;
3. current C++ standard;
4. Qt modules currently linked;
5. existing `src/audio` and `include/auralis/audio` contents;
6. any existing `PipeWireManager` interface/stub;
7. `ApplicationCore` startup/shutdown sequence;
8. logger API and subsystem naming conventions;
9. error/result abstraction, if any;
10. current Bluetooth manager;
11. current `BluetoothDevice` type;
12. current `DeviceRegistry`;
13. how stable device IDs are generated;
14. how Bluetooth address and BlueZ D-Bus object path are represented;
15. signals/events emitted when device state changes;
16. QML registration/context-property patterns;
17. test framework and naming conventions;
18. current unit/integration test organization;
19. feature flags/environment-variable conventions;
20. any prior Phase 3 live integration-test gating.

Then implement Phase 4 **using existing patterns**.

If a proposed class in this prompt already exists under a different name, extend the existing class instead of duplicating it.

---

# 7. ARCHITECTURAL RULES

The following rules are mandatory.

## 7.1 Separation of domains

Keep these concepts separate:

```text
BluetoothDevice
PipeWire raw/global object
PipeWire device
PipeWire node
AudioEndpoint
AudioRoute   <- Phase 5, not Phase 4
```

Do not add PipeWire proxy pointers directly to the Phase 3 `BluetoothDevice` domain object.

Do not turn `BluetoothDevice` into a catch-all audio object.

## 7.2 Stable application identity versus ephemeral PipeWire identity

PipeWire global IDs are runtime identifiers.

Treat them as ephemeral.

For example:

```text
connection 1:
  Bluetooth headphones node id = 72

disconnect / reconnect:

connection 2:
  same headphones node id = 91
```

Auralis must not assume that a PipeWire object ID is a permanent device identity.

`AudioEndpoint` should have:

- an Auralis-side stable/logical ID;
- the current PipeWire global/object ID as runtime metadata.

## 7.3 No production CLI dependency

The following are allowed only for manual developer diagnostics and test instructions:

```bash
wpctl
pw-cli
pw-dump
pactl
bluetoothctl
```

The application must not:

- execute them;
- parse their output;
- depend on their formatting;
- poll them.

## 7.4 Thread correctness

PipeWire callback context must never mutate QML/UI state directly in an unsafe way.

All cross-thread handoff must be explicit and deterministic.

Prefer:

```text
PipeWire callback thread
       |
       v
small immutable/plain event snapshot
       |
       v
queued handoff to owning Qt thread
       |
       v
registry/model update
       |
       v
Qt signals / QML
```

Do not let QML objects be accessed from a PipeWire thread.

## 7.5 Phase 4 remains read-mostly

PipeWire use in this phase is primarily:

- connect;
- enumerate;
- bind where necessary;
- subscribe;
- read object state/properties;
- maintain local normalized model.

Do not create routing links.

---

# 8. TARGET PHASE 4 COMPONENT MODEL

Adapt names to the existing repository, but preserve these responsibilities.

```text
PipeWireManager
|
+-- PipeWireConnection / CoreConnection
|     +-- initialize PipeWire
|     +-- own loop/context/core
|     +-- detect connected/disconnected/error state
|
+-- PipeWireRegistryMonitor
|     +-- registry global
|     +-- registry global_remove
|     +-- normalized raw object snapshots
|
+-- PipeWireObjectStore
|     +-- currently known devices
|     +-- currently known nodes
|     +-- optionally ports
|     +-- optionally links
|
+-- AudioEndpointRegistry
|     +-- normalized playback/capture endpoints
|     +-- stable add/update/remove semantics
|     +-- Qt-facing model if appropriate
|
+-- EndpointResolver
      +-- map PipeWire endpoint -> BluetoothDevice
      +-- map BluetoothDevice -> current endpoints
      +-- resolve by strong properties first
      +-- diagnostics for ambiguous/unresolved mapping
```

Do not over-engineer with unnecessary abstract factories.

Do create clear seams that can be unit-tested without a live PipeWire server.

---

# 9. BUILD SYSTEM INTEGRATION

Use `pkg-config`/CMake integration appropriate to the existing project.

The development system already exposes:

```text
libpipewire-0.3
```

Implement CMake so the production audio module links against native PipeWire.

A likely approach is conceptually:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

target_link_libraries(auralis_audio
    ...
    PkgConfig::PIPEWIRE
)
```

However:

- inspect current target names first;
- preserve existing target structure;
- prefer target-scoped include/link flags;
- do not globally inject compiler or linker flags;
- do not duplicate `find_package(PkgConfig)` if already present;
- preserve Ninja clean-build support.

If PipeWire is intended to be a required dependency for Phase 4, configuration should fail with a clear message when development headers are absent.

If the existing project has an optional-feature architecture, integrate PipeWire consistently with it, but do not silently compile a fake implementation in the normal supported Ubuntu environment.

---

# 10. PIPEWIRE LIFECYCLE DESIGN

Implement a clean RAII-driven PipeWire lifecycle.

A suitable conceptual lifecycle is:

```text
construct manager
     |
     v
initialize()
     |
     +-- pw_init(...)
     +-- create PipeWire loop
     +-- create context
     +-- connect core
     +-- get registry
     +-- register listeners
     +-- start loop
     v
CONNECTED
     |
     +-- registry events
     +-- object events
     v
shutdown()
     |
     +-- stop event delivery
     +-- remove/destroy proxies/listeners
     +-- disconnect core
     +-- destroy context
     +-- destroy loop
     v
STOPPED
```

The exact use of `pw_init()` / `pw_deinit()` should follow a process-level ownership strategy.

Do not call global init/deinit repeatedly from transient endpoint objects.

If `ApplicationCore` is the obvious process owner, initialize PipeWire once through the audio subsystem lifecycle.

---

# 11. LOOP / THREAD MODEL

Auralis is a Qt application, so do not block the Qt event loop with a PipeWire main loop.

Use an appropriate asynchronous design.

A strong default for this codebase is a PipeWire **thread loop** owned by the PipeWire subsystem.

Important behavior:

- the PipeWire loop runs separately from the Qt UI thread;
- PipeWire API access associated with that loop follows PipeWire locking requirements;
- `pw_thread_loop_stop()` must not be called while holding its loop lock;
- callbacks should do minimal work;
- normalized updates should be handed to the Qt/application thread;
- teardown must guarantee callbacks cannot target destroyed Qt/application objects.

If the repository already has a safe event-loop integration layer, use it instead.

The final implementation must document the chosen thread ownership.

---

# 12. CONNECTION STATE MODEL

Expose an explicit PipeWire connection state.

Suggested values:

```cpp
enum class PipeWireConnectionState {
    Stopped,
    Starting,
    Connected,
    Error,
    Stopping
};
```

Optional:

```text
Unavailable
Reconnecting
```

only if the existing architecture needs them now.

Phase 8 will handle broad recovery/hardening, so do not build a giant retry framework in Phase 4.

At minimum expose:

- current connection state;
- last error string/code;
- whether registry enumeration is active;
- number of known relevant objects;
- number of current `AudioEndpoint`s.

---

# 13. REGISTRY ENUMERATION

Use the native PipeWire registry.

Core flow:

```text
pw_context_connect(...)
        |
        v
pw_core_get_registry(...)
        |
        v
pw_registry_add_listener(...)
        |
        +-- global(...)
        |
        +-- global_remove(...)
```

On `global`:

1. snapshot the provided object ID;
2. snapshot the interface type;
3. snapshot the registry properties;
4. classify whether Auralis cares about this object;
5. bind only when additional object-specific information is required;
6. update the internal object store;
7. reevaluate affected endpoints.

On `global_remove`:

1. locate every tracked object derived from that global ID;
2. remove or invalidate it;
3. update endpoint availability;
4. recalculate mappings;
5. emit deterministic model updates.

Do not assume `global_remove` will be preceded by a Bluetooth state change.

---

# 14. RELEVANT PIPEWIRE OBJECT TYPES

At minimum understand and classify:

```text
PipeWire:Interface:Device
PipeWire:Interface:Node
```

Also observe when useful:

```text
PipeWire:Interface:Port
PipeWire:Interface:Link
PipeWire:Interface:Metadata
```

For Phase 4:

- `Device` helps represent hardware/logical device ownership and profile state;
- `Node` is essential for playback/capture endpoint discovery;
- ports/links are useful for diagnostics and future Phase 5 preparation;
- streams may appear as nodes, so classification by `media.class` matters.

Do not expose every PipeWire node as an Auralis endpoint.

---

# 15. PROPERTY SNAPSHOT ABSTRACTION

Do not spread raw `spa_dict*` access throughout the codebase.

Create a safe snapshot representation.

For example:

```cpp
using PipeWireProperties = std::unordered_map<std::string, std::string>;
```

or an existing Qt equivalent:

```cpp
QHash<QString, QString>
```

The raw `spa_dict` memory belongs to PipeWire.

Copy needed properties during callbacks before crossing thread/lifetime boundaries.

Create helper accessors:

```text
value(key)
optionalValue(key)
uintValue(key)
boolValue(key)
contains(key)
```

Property access must be tolerant of:

- missing keys;
- malformed integer values;
- unexpected capitalization in human-readable text;
- empty values.

Never crash because a PipeWire object does not expose an optional property.

---

# 16. PIPEWIRE / SPA PROPERTY KEYS TO SUPPORT

Support standard identifying/classification properties relevant to endpoint creation.

Examples include:

```text
object.id
object.serial

device.id
device.name
device.nick
device.description
device.api
device.bus
device.bus-path
device.icon-name

node.id
node.name
node.nick
node.description
node.pause-on-idle
node.driver
node.virtual

media.class
media.type
media.category
media.role

audio.channels
audio.position
audio.rate
```

Not every property exists on every object.

For Bluetooth correlation, support BlueZ5/SPA properties when present:

```text
api.bluez5.path
api.bluez5.device
api.bluez5.connection
api.bluez5.transport
api.bluez5.profile
api.bluez5.address
api.bluez5.codec
api.bluez5.class
api.bluez5.icon
api.bluez5.role
```

Do not hardcode a design that requires all of these keys simultaneously.

Different object types and profile states may expose different subsets.

---

# 17. NORMALIZED RAW OBJECT TYPES

Introduce internal normalized representations that are independent of PipeWire pointer lifetimes.

Example:

```cpp
struct PipeWireObjectSnapshot {
    uint32_t globalId;
    std::string interfaceType;
    std::unordered_map<std::string, std::string> properties;
};
```

Then more domain-specific normalized types:

```cpp
struct PipeWireDeviceInfo {
    uint32_t globalId;
    std::optional<uint64_t> serial;
    std::string name;
    std::string description;
    std::string api;
    std::string bus;
    std::optional<std::string> bluezAddress;
    std::optional<std::string> bluezPath;
    std::optional<std::string> bluezProfile;
    std::optional<std::string> bluezCodec;
    Properties properties;
};

struct PipeWireNodeInfo {
    uint32_t globalId;
    std::optional<uint64_t> serial;
    std::optional<uint32_t> deviceId;
    std::string name;
    std::string description;
    std::string mediaClass;
    std::optional<std::string> bluezAddress;
    std::optional<std::string> bluezPath;
    std::optional<std::string> bluezProfile;
    std::optional<std::string> bluezCodec;
    std::optional<uint32_t> channels;
    std::optional<uint32_t> sampleRate;
    Properties properties;
};
```

Use project naming/types rather than literally copying these declarations if a domain style already exists.

---

# 18. ENDPOINT CLASSIFICATION

Create a deterministic classification function.

A node should only become an `AudioEndpoint` if it represents a meaningful audio input/output endpoint.

Handle common media classes such as:

```text
Audio/Sink
Audio/Source
Audio/Duplex
```

Potentially also classify variants found in the running graph, but avoid broad string guessing.

Do not expose:

- video nodes;
- arbitrary application streams;
- monitor nodes as user playback destinations unless explicitly intended;
- internal processing nodes;
- irrelevant virtual nodes.

Implement classification as a pure/testable function where possible.

Example conceptual API:

```cpp
std::optional<AudioEndpointCandidate>
classifyAudioEndpoint(const PipeWireNodeInfo& node,
                      const PipeWireObjectStore& store);
```

---

# 19. AUDIO ENDPOINT DOMAIN MODEL

Create or complete an `AudioEndpoint` type.

Minimum fields:

```text
AudioEndpoint
|
+-- id                        Auralis logical endpoint ID
+-- pipeWireObjectId          current runtime PipeWire node/global ID
+-- pipeWireSerial            if available
+-- name
+-- description
+-- direction
+-- mediaClass
+-- pipeWireDeviceId          owning PipeWire device/global ID if known
+-- nodeName
+-- profile
+-- codec                     when available
+-- sampleRate                when reliably available
+-- channelCount              when reliably available
+-- availability
+-- transport
+-- bluetoothDeviceId         Auralis Phase 3 device ID, optional
+-- bluetoothAddress          normalized address, optional
+-- bluezObjectPath           optional
+-- mappingConfidence         optional diagnostic field
```

Recommended endpoint direction:

```cpp
enum class AudioEndpointDirection {
    Playback,
    Capture,
    Duplex,
    Unknown
};
```

Recommended availability:

```cpp
enum class AudioEndpointAvailability {
    Available,
    Unavailable
};
```

Do not conflate availability with Bluetooth connected state.

A device can be BlueZ-connected but its audio endpoint may not yet be present.

---

# 20. ENDPOINT ID STRATEGY

Define an explicit endpoint identity strategy.

The goal is stable logical identity across ordinary graph refreshes while still distinguishing multiple profiles/directions from one physical device.

Strong conceptual key order:

For Bluetooth endpoints:

```text
bluetooth stable device ID
+ endpoint direction
+ profile / role where meaningful
+ stable node identity component
```

For non-Bluetooth endpoints:

```text
PipeWire serial if stable
or stable device/name combination
+ direction/profile
```

Avoid using only:

```text
PipeWire global ID
```

because it can change.

Do not spend Phase 4 building a database-backed identity system; implement a deterministic in-memory logical identity suitable for the current application lifecycle, with clear comments about runtime IDs.

---

# 21. AUDIO ENDPOINT REGISTRY

Implement a registry with stable add/update/remove semantics.

Responsibilities:

- store current endpoint objects;
- prevent duplicates;
- preserve endpoint identity where possible;
- update changed properties;
- mark/remove endpoints when backing nodes disappear;
- query by endpoint ID;
- query by PipeWire global ID;
- query by Bluetooth device ID;
- list playback endpoints;
- list capture endpoints;
- emit add/update/remove/change events.

Conceptual API:

```cpp
class AudioEndpointRegistry {
public:
    std::vector<AudioEndpoint> endpoints() const;
    std::vector<AudioEndpoint> playbackEndpoints() const;
    std::vector<AudioEndpoint> endpointsForBluetoothDevice(DeviceId id) const;

    const AudioEndpoint* findById(AudioEndpointId id) const;
    const AudioEndpoint* findByPipeWireObjectId(uint32_t id) const;

    void upsert(AudioEndpoint endpoint);
    void removeByPipeWireObjectId(uint32_t id);
    void clear();
};
```

Use Qt model semantics if this project already exposes registries through `QAbstractListModel`.

Avoid exposing mutable internal containers directly.

---

# 22. QT MODEL / QML EXPOSURE

If the current UI architecture already uses Qt models, expose endpoints consistently.

Suggested roles:

```text
endpointId
name
description
direction
available
transport
profile
codec
pipeWireObjectId
bluetoothDeviceId
bluetoothAddress
mapped
```

Do not expose raw C pointers.

Do not require QML to perform Bluetooth/PipeWire correlation.

QML should receive already-normalized state.

A minimal Phase 4 UI can show:

```text
Audio Endpoints
-------------------------------------------------
Built-in Audio Analog Stereo
Output
Available
ALSA

Headphones
Bluetooth Output
Available
A2DP
Mapped: <device name>
```

The UI is a validation surface, not the focus of the phase.

---

# 23. PIPEWIRE OBJECT STORE

Maintain a dedicated object store rather than deriving everything ad hoc directly from callbacks.

At minimum:

```text
devicesByGlobalId
nodesByGlobalId
```

Optional for diagnostics:

```text
portsByGlobalId
linksByGlobalId
metadataByGlobalId
```

The store should support graph churn.

When a device/node is updated, only affected endpoint candidates should need recomputation, though a small full refresh is acceptable at Phase 4 scale if deterministic and safe.

Keep the logic simple and correct.

---

# 24. BINDING TO DEVICE/NODE PROXIES

Registry `global` properties may be sufficient for some data and insufficient for other data.

Where additional device/node info is required:

- bind to the global using `pw_registry_bind`;
- use the correct interface/version;
- attach the appropriate listener;
- snapshot information into safe local types;
- handle proxy destruction;
- remove listener hooks safely;
- cap the requested interface version using the version advertised by the registry and the version supported by the client.

Do not assume the server supports the newest client-side interface version.

Track proxy ownership in RAII objects or an explicit map keyed by global ID.

---

# 25. OBJECT REMOVAL AND PROXY LIFETIME

Object removal is a critical correctness case.

When a PipeWire global disappears:

```text
global_remove(id)
     |
     +-- remove corresponding proxy/listener safely
     +-- remove normalized object
     +-- update dependent endpoint(s)
     +-- emit endpoint removal/unavailable state
     +-- reevaluate Bluetooth mapping
```

Never dereference a proxy after its global is gone.

Never emit references to stack-owned property data.

Teardown must be idempotent.

---

# 26. CORE ERROR HANDLING

Listen to core errors/state as appropriate for the current API architecture.

At minimum:

- detect initial connection failure;
- log a clear PipeWire subsystem error;
- expose the error to application status;
- prevent endpoint registry use from crashing;
- allow clean application shutdown even if PipeWire never connected.

Do not hide a failed PipeWire connection behind a fake “Ready” status.

---

# 27. BLUETOOTH ↔ PIPEWIRE ENDPOINT RESOLVER

Implement a dedicated `EndpointResolver`.

Inputs:

```text
existing Phase 3 DeviceRegistry
PipeWireDeviceInfo
PipeWireNodeInfo
AudioEndpoint candidate
```

Output:

```text
mapped Bluetooth device ID
mapping confidence/reason
or unresolved
```

The resolver must not depend on QML.

The resolver should be unit-testable with synthetic properties.

---

# 28. BLUETOOTH ADDRESS NORMALIZATION

Implement one canonical address format for comparison.

For example:

```text
AA:BB:CC:DD:EE:FF
```

Input tolerance may include:

```text
aa:bb:cc:dd:ee:ff
AA_BB_CC_DD_EE_FF
AA-BB-CC-DD-EE-FF
```

Only normalize formats that are actually safe and unambiguous.

Do not match arbitrary substrings.

Use the existing Phase 2/3 address utility if one exists.

A direct normalized `api.bluez5.address` match to the Phase 3 device address should be considered a strong mapping signal.

---

# 29. BLUEZ OBJECT PATH NORMALIZATION / MATCHING

Phase 3 already knows BlueZ D-Bus object paths.

A typical device path can encode an address-like value.

Do not rely solely on parsing a D-Bus path if a direct address property exists.

Use path matching as another strong signal when:

```text
PipeWire api.bluez5.path
or
PipeWire api.bluez5.device
```

can be correlated with the Phase 3 `BluetoothDevice.objectPath`.

Normalize only what is necessary for exact semantic comparison.

---

# 30. MAPPING PRIORITY

Use deterministic priority, not fuzzy UI-name matching first.

Recommended signal order:

## Level A — Strong identity

1. direct normalized Bluetooth address match;
2. direct BlueZ device object-path match;
3. PipeWire device reference that itself maps by address/path.

## Level B — Supporting identity

4. PipeWire node → PipeWire device ownership + that device's BlueZ identity;
5. known BlueZ5-specific device property;
6. stable transport/profile relationship.

## Level C — Weak fallback

7. exact normalized alias/name/description match, only when unique and no stronger signal exists.

Never accept a weak-name match if multiple Bluetooth devices share that name.

Never map based only on “contains Headphones”.

---

# 31. OPTIONAL MAPPING CONFIDENCE MODEL

For diagnostics, use an internal confidence/reason.

Example:

```cpp
enum class EndpointMappingConfidence {
    None,
    Weak,
    Strong,
    Exact
};
```

And reason:

```text
ExactBluetoothAddress
ExactBlueZObjectPath
PipeWireDeviceOwnership
UniqueNameFallback
Ambiguous
InsufficientData
```

This is useful for tests and diagnostics.

The UI does not need to expose all reasons.

---

# 32. AMBIGUITY HANDLING

If two Phase 3 devices could match one endpoint using only weak fields:

```text
DO NOT GUESS
```

Leave the endpoint unresolved.

Log a structured diagnostic:

```text
EndpointResolver
AmbiguousMapping
endpoint=<...>
candidateCount=2
signals=<...>
```

An unresolved endpoint should still exist as an `AudioEndpoint`.

Mapping failure must not delete the endpoint.

---

# 33. MAPPING REEVALUATION TRIGGERS

Re-run affected mapping whenever:

- a PipeWire device appears;
- a PipeWire node appears;
- relevant PipeWire properties change;
- a PipeWire object disappears;
- a Bluetooth device appears;
- Bluetooth address/path metadata changes;
- a Bluetooth device connects;
- services become resolved;
- a Bluetooth device disconnects;
- a Bluetooth device is forgotten/removed.

Do not require application restart to map a node that appeared later.

---

# 34. BLUETOOTH CONNECTION DOES NOT EQUAL AUDIO READINESS

Model this temporal sequence correctly:

```text
BlueZ Connected=true
        |
        | maybe tens/hundreds of ms later
        v
PipeWire Bluetooth device appears
        |
        v
PipeWire A2DP node appears
        |
        v
AudioEndpoint Available=true
```

The UI and model must not fabricate endpoint availability immediately upon BlueZ connection.

Likewise:

```text
BlueZ Connected=false
```

may race with PipeWire global removal.

Both event orders must be safe.

---

# 35. PROFILE AWARENESS

Capture profile metadata when available.

Examples conceptually include:

```text
A2DP
HFP
HSP
BAP/LE Audio in future
```

Do not design Phase 4 around A2DP only.

Represent transport/profile text or enum in a transport-independent higher-level endpoint model.

A future LE Audio endpoint should be able to enter the same `AudioEndpointRegistry` without rewriting Phase 6 session logic.

---

# 36. TRANSPORT MODEL

If the existing project does not already have one, use a modest enum such as:

```cpp
enum class AudioTransport {
    Unknown,
    BuiltIn,
    Alsa,
    BluetoothClassic,
    BluetoothLE
};
```

Do not pretend the PipeWire property set can always reliably distinguish every future technology.

Keep raw profile/API properties for diagnostics.

---

# 37. NODE DIRECTION

Map `media.class` deterministically.

Conceptually:

```text
Audio/Sink     -> Playback
Audio/Source   -> Capture
Audio/Duplex   -> Duplex
```

Validate against the actual graph discovered on the development laptop.

Do not invert direction based on PipeWire stream terminology from Phase 5 APIs.

The user-facing endpoint direction should reflect whether Auralis can play to or capture from the endpoint.

---

# 38. SAMPLE RATE AND CHANNEL COUNT

The roadmap suggests `sampleRate` and `channelCount`.

Implement them only from reliable object/parameter information.

Do not fabricate:

```text
48000 Hz
2 channels
```

for every endpoint.

If not available during Phase 4 registry observation:

```text
optional / unknown
```

is acceptable.

If binding/querying node parameters is reasonably contained and consistent with the existing architecture, implement reliable extraction.

Do not allow optional format parsing to destabilize the core endpoint discovery gate.

---

# 39. PARAM ENUMERATION — OPTIONAL BUT WELL-BOUNDED

If required for endpoint format metadata:

- use native PipeWire node/device parameter APIs;
- parse SPA pods safely;
- keep format extraction isolated;
- test parsing with synthetic fixtures where practical;
- treat unsupported/missing params as unknown;
- do not block endpoint creation on successful format enumeration.

Do not implement audio negotiation or route format selection in Phase 4.

That belongs to Phase 5.

---

# 40. LOGGING REQUIREMENTS

Integrate with the existing Auralis logger.

Use subsystem labels such as:

```text
PipeWire
PipeWireRegistry
AudioEndpointRegistry
EndpointResolver
```

Log meaningful lifecycle events.

Examples:

```text
INFO  PipeWire             Connecting
INFO  PipeWire             Connected
DEBUG PipeWireRegistry     GlobalAdded id=57 type=Node mediaClass=Audio/Sink
DEBUG AudioEndpointRegistry EndpointAdded endpoint=...
INFO  EndpointResolver     Mapped endpoint=... device=... reason=ExactBluetoothAddress
DEBUG PipeWireRegistry     GlobalRemoved id=57
INFO  AudioEndpointRegistry EndpointRemoved endpoint=...
ERROR PipeWire             ConnectionFailed code=... message=...
```

Avoid logging every irrelevant PipeWire global at INFO.

Detailed graph enumeration can be DEBUG/TRACE.

Never log pointer addresses as stable object identity.

---

# 41. DIAGNOSTICS SNAPSHOT

Expose a developer-friendly diagnostics snapshot from the audio subsystem.

Suggested fields:

```text
PipeWire connection state
PipeWire core/server identity if available
known device count
known node count
known port count
known link count
audio endpoint count
mapped Bluetooth endpoint count
unmapped Bluetooth endpoint count
last PipeWire error
```

This can feed a minimal diagnostics view later.

---

# 42. APPLICATIONCORE INTEGRATION

Integrate Phase 4 into the existing application lifecycle.

Startup order should preserve Phase 1–3 behavior.

A reasonable conceptual order:

```text
ApplicationCore
|
+-- Logger
+-- Configuration
+-- Bluetooth / DeviceRegistry
+-- PipeWireManager
+-- AudioEndpointRegistry / EndpointResolver
+-- presentation binding
```

However inspect the real implementation first.

Do not force PipeWire to depend on QML.

Prefer dependency injection of the Phase 3 device registry/interface into the resolver.

---

# 43. OWNERSHIP DESIGN

Ownership must be obvious.

For example:

```text
ApplicationCore
  owns PipeWireManager

PipeWireManager
  owns connection/loop/registry monitor/object store

ApplicationCore or audio service
  owns AudioEndpointRegistry

EndpointResolver
  observes Bluetooth DeviceRegistry + audio object/endpoint changes
```

Avoid circular ownership.

Use:

- RAII;
- smart pointers when ownership is dynamic;
- QObject parent ownership only where it fits existing Qt patterns;
- weak/non-owning references for observation.

---

# 44. EVENT MODEL

Choose one consistent event model.

If the codebase is Qt-centric:

```text
QObject signals/slots
```

may be appropriate at subsystem boundaries.

If the domain layer is plain C++:

```text
callbacks / observer interfaces
```

may be appropriate internally.

Whatever is chosen:

- do not emit cross-thread direct UI mutations;
- ensure queued delivery where required;
- ensure observers cannot outlive the source unsafely.

Document thread affinity for each public signal.

---

# 45. MINIMAL PUBLIC API

Adapt to the existing architecture, but Phase 5 should be able to consume something like:

```cpp
class PipeWireManager {
public:
    bool start();
    void stop();

    PipeWireConnectionState state() const;
    std::string lastError() const;

    const PipeWireObjectStore& objectStore() const;
};

class AudioEndpointService {
public:
    const AudioEndpointRegistry& registry() const;

    std::vector<AudioEndpoint> playbackEndpoints() const;
    std::vector<AudioEndpoint> captureEndpoints() const;
    std::vector<AudioEndpoint> endpointsForDevice(DeviceId id) const;
};
```

Do not expose raw `pw_core*`, `pw_registry*`, or proxy pointers outside the low-level audio integration layer.

---

# 46. TESTABILITY REQUIREMENT

The core endpoint/mapping behavior must be testable without:

- Bluetooth hardware;
- a live PipeWire daemon;
- WirePlumber;
- a desktop session.

Separate:

```text
PipeWire event acquisition
```

from:

```text
object normalization
endpoint classification
endpoint registry
Bluetooth mapping
```

so synthetic data can drive the latter.

This is mandatory.

---

# 47. UNIT TESTS — PROPERTY UTILITIES

Add tests for:

- missing key;
- empty value;
- valid integer parsing;
- invalid integer parsing;
- Bluetooth address normalization;
- invalid Bluetooth address rejection;
- exact BlueZ path comparison;
- property snapshot lifetime independent of source `spa_dict`.

---

# 48. UNIT TESTS — ENDPOINT CLASSIFICATION

Synthetic node cases:

1. `media.class=Audio/Sink` → playback endpoint;
2. `media.class=Audio/Source` → capture endpoint;
3. non-audio node → ignored;
4. application playback stream → ignored as destination endpoint;
5. Bluetooth A2DP node → playback endpoint;
6. Bluetooth HFP source → capture endpoint;
7. missing description → still valid using fallback name;
8. missing sample rate → endpoint valid with unknown sample rate;
9. duplicate event → no duplicate endpoint;
10. same logical endpoint with changed description → update.

---

# 49. UNIT TESTS — ENDPOINT REGISTRY

Test:

- insert;
- update;
- idempotent upsert;
- find by logical ID;
- find by PipeWire ID;
- query playback endpoints;
- query by Bluetooth device;
- remove by backing PipeWire ID;
- no duplicate signals for no-op updates if the architecture tracks change events;
- clear;
- object-ID churn behavior.

---

# 50. UNIT TESTS — RESOLVER EXACT ADDRESS

Fixtures:

```text
BluetoothDevice:
  address = AA:BB:CC:DD:EE:FF
  id      = device-1

PipeWire endpoint:
  api.bluez5.address = aa:bb:cc:dd:ee:ff
```

Expected:

```text
mapped to device-1
confidence = Exact/Strong
reason = ExactBluetoothAddress
```

---

# 51. UNIT TESTS — RESOLVER BLUEZ PATH

Fixture:

```text
BluetoothDevice.objectPath
/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
```

PipeWire property:

```text
api.bluez5.path
or api.bluez5.device
```

matching the same device.

Expected exact mapping.

---

# 52. UNIT TESTS — PIPEWIRE DEVICE OWNERSHIP

Fixture:

```text
PipeWire Device globalId = 50
  api.bluez5.address = AA:BB:CC:DD:EE:FF

PipeWire Node globalId = 60
  device.id = 50
  media.class = Audio/Sink
```

Expected:

```text
Node -> Device 50 -> Bluetooth device
```

even if the node itself lacks `api.bluez5.address`.

This is an important case.

---

# 53. UNIT TESTS — AMBIGUITY

Two Phase 3 Bluetooth devices:

```text
name = "Hearing Aid"
name = "Hearing Aid"
```

PipeWire endpoint only exposes:

```text
description = "Hearing Aid"
```

Expected:

```text
unresolved
reason = Ambiguous
```

Never arbitrarily select one.

---

# 54. UNIT TESTS — EVENT ORDERING

Test at least:

## Case A

```text
Bluetooth connected
PipeWire device added
PipeWire node added
```

## Case B

```text
PipeWire node added
Bluetooth registry update arrives later
```

## Case C

```text
PipeWire node removed
Bluetooth disconnected later
```

## Case D

```text
Bluetooth disconnected
PipeWire node removed later
```

## Case E

```text
node removed
new node appears with different global ID
same Bluetooth device
```

All must end in deterministic state.

---

# 55. UNIT TESTS — MULTIPLE ENDPOINTS PER DEVICE

One Bluetooth device may expose:

```text
Playback / A2DP
Capture / HFP
Playback / HFP
```

Expected:

- endpoints are separate;
- all may map to the same Phase 3 Bluetooth device;
- endpoint IDs are distinct;
- registry query by Bluetooth device returns all relevant endpoints;
- no accidental overwrite.

---

# 56. UNIT TESTS — NON-BLUETOOTH AUDIO

Built-in ALSA sink fixture.

Expected:

- endpoint is visible;
- transport is built-in/ALSA/unknown as designed;
- `bluetoothDeviceId` is empty;
- resolver does not try to force a Bluetooth mapping.

This verifies that `AudioEndpointRegistry` is a general audio abstraction.

---

# 57. INTEGRATION TEST STRATEGY

Create integration tests at two levels.

## Level 1 — deterministic in-process/component integration

Feed synthetic PipeWire object events to:

```text
object store
endpoint builder
endpoint registry
resolver
```

No hardware.

Run by default under CTest.

## Level 2 — live PipeWire integration

Opt-in.

Connect to the real PipeWire server and verify:

- connection succeeds;
- registry events arrive;
- at least one audio endpoint appears on the normal desktop;
- built-in audio can be classified;
- if a Bluetooth test device is explicitly supplied, verify its mapping.

Do not make normal CI require Bluetooth hardware.

---

# 58. LIVE TEST ENVIRONMENT GATING

Follow the Phase 3 project convention if one already exists.

A suitable pattern is:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1
ctest --test-dir build \
  -R tst_PipeWireLiveIntegration \
  --output-on-failure
```

For Bluetooth mapping:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF \
ctest --test-dir build \
  -R tst_PipeWireBluetoothMappingIntegration \
  --output-on-failure
```

IMPORTANT:

Do not document placeholders using angle brackets in shell commands if the shell will interpret them as redirection.

Bad:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

Good:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF
```

or:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

If the variable is omitted, the hardware-specific test should SKIP with a clear message rather than fail mysteriously.

---

# 59. LIVE TEST ASSERTIONS

For a generic live PipeWire test:

```text
PipeWireManager starts
connection state becomes Connected
registry enumeration completes/settles
one or more AudioEndpoint objects exist
at least one playback or capture endpoint exists
shutdown is clean
```

For a Bluetooth mapping integration test:

```text
expected Bluetooth device is already paired/connected
Phase 3 registry contains the address
PipeWire endpoint eventually appears
endpoint maps to that device
endpoint direction/profile is plausible
test times out with useful diagnostics if mapping never appears
```

Do not use unbounded waits.

---

# 60. TEST TIMEOUTS

All live tests need bounded waits.

Example architecture:

```text
wait up to N seconds for PipeWire connected
wait up to N seconds for registry populated
wait up to N seconds for expected Bluetooth endpoint
```

Use the project's Qt Test/event-loop patterns.

On timeout print:

- current PipeWire state;
- known PipeWire devices;
- known nodes;
- known endpoints;
- relevant BlueZ properties;
- expected address.

This makes hardware failures debuggable.

---

# 61. TEST NAMES

Use the repository naming scheme.

Suggested names only if consistent:

```text
tst_PipeWireProperties
tst_AudioEndpointClassifier
tst_AudioEndpointRegistry
tst_EndpointResolver
tst_PipeWireObjectStore
tst_PipeWireIntegration
tst_PipeWireLiveIntegration
tst_PipeWireBluetoothMappingIntegration
```

Do not create dozens of tiny targets if the existing test architecture groups related cases.

---

# 62. MINIMAL PHASE 4 UI

Only if consistent with current application UI, add an **Audio Endpoints** section.

Required information:

```text
Endpoint name
Direction
Availability
Transport
Profile if known
Mapped Bluetooth device name if known
```

Example:

```text
AUDIO ENDPOINTS

Built-in Audio Analog Stereo
Playback
Available
ALSA

Hearing Device A
Playback
Available
Bluetooth / A2DP
Mapped: Hearing Device A
```

Optional developer detail:

```text
PW Node ID: 84
BlueZ address: AA:BB:CC:DD:EE:FF
Codec: SBC
```

Do not make raw IDs the primary UX.

---

# 63. UI STATE REQUIREMENTS

The UI must distinguish:

```text
Bluetooth Connected
```

from:

```text
Audio Endpoint Available
```

This is important.

Example transitional UI:

```text
Hearing Device A
Bluetooth: Connected
Audio: Initializing...
```

then:

```text
Hearing Device A
Bluetooth: Connected
Audio: Available
```

Do not show a route/start-audio action in Phase 4 unless it remains disabled/stubbed and clearly belongs to Phase 5.

---

# 64. QML MODEL UPDATE CORRECTNESS

If using `QAbstractListModel`:

- mutations occur on the model's owning Qt thread;
- roles are stable;
- inserts use `beginInsertRows/endInsertRows`;
- removals use `beginRemoveRows/endRemoveRows`;
- updates emit `dataChanged`;
- do not reset the full model for every property change unless unavoidable;
- indexes remain valid according to Qt rules.

Add model tests if the current repository already tests QML-facing models.

---

# 65. STARTUP WITH NO PIPEWIRE

Simulate or handle:

```text
PipeWire server unavailable
```

Expected:

- Auralis launches;
- Bluetooth Phase 2/3 features still work;
- audio subsystem shows error/unavailable;
- no crash;
- endpoint registry remains empty;
- logs explain the failure.

Phase 8 can add sophisticated reconnection.

---

# 66. STARTUP WITH NO BLUETOOTH

PipeWire can still expose built-in audio.

Expected:

- PipeWire works;
- non-Bluetooth `AudioEndpoint`s can appear;
- resolver simply leaves Bluetooth mapping absent;
- no crash.

This keeps the audio abstraction correctly decoupled.

---

# 67. STARTUP WITH BLUETOOTH CONNECTED ALREADY

A common case:

```text
device connected before Auralis starts
```

Expected:

- Phase 3 enumerates existing BlueZ device state;
- Phase 4 enumerates existing PipeWire graph;
- resolver maps them without requiring reconnect;
- endpoint becomes available.

Test this manually.

---

# 68. DEVICE CONNECTED AFTER STARTUP

Expected:

```text
Auralis running
  |
  +-- user connects Bluetooth device
  |
  +-- BlueZ state changes
  |
  +-- PipeWire creates audio objects
  |
  +-- endpoint appears
  |
  +-- resolver maps it
```

No manual refresh should be required.

---

# 69. DEVICE DISCONNECT / RECONNECT

Expected:

```text
connected endpoint PW id=81
disconnect
endpoint removed/unavailable
reconnect
new endpoint PW id=96
same Phase 3 Bluetooth device
```

Auralis must not retain stale `81` as active.

Mapping after reconnect must work.

---

# 70. PROFILE CHANGE

If the system switches from A2DP to an HFP/HSP profile:

- old node(s) may disappear;
- new node(s) may appear;
- media direction may change;
- node IDs may change;
- profile/codec may change.

Phase 4 should accurately reflect the new endpoint set.

Do not implement the command to change profiles unless already available for diagnostics and clearly separate from routing.

---

# 71. PIPEWIRE GRAPH CHURN

Be resilient to:

- registry add/remove bursts;
- nodes appearing before device metadata;
- devices appearing before nodes;
- rapidly changing object IDs;
- property updates;
- unrelated application streams starting/stopping;
- browser audio activity;
- GNOME audio services.

The endpoint list must remain deduplicated.

---

# 72. EVENT COALESCING

Do not prematurely optimize.

If many related updates arrive, a short queued recomputation on the Qt/application thread is acceptable.

But behavior must remain deterministic.

Avoid:

- uncontrolled timers;
- polling every second;
- sleeps inside production code.

Use event-driven state.

---

# 73. DATA RACES

Run the code mentally and through sanitizable patterns against:

```text
PipeWire callback while application is shutting down
global_remove while endpoint resolution is queued
Bluetooth registry update while PipeWire node is removed
QML model destroyed while PipeWire callback fires
```

Use lifetime guards/queued ownership to make these safe.

---

# 74. MEMORY SAFETY

No:

- leaked PipeWire proxies;
- leaked listeners/hooks;
- use-after-free of `spa_dict`;
- use-after-free of QObject;
- stale raw pointers in endpoint registry;
- double destroy of core/context/loop;
- destruction of PipeWire thread loop while still running.

Prefer RAII wrappers where they materially simplify correctness.

---

# 75. ERROR TYPES

Use existing project error conventions.

If no convention exists, distinguish:

```text
PipeWireInitializationError
PipeWireConnectionError
PipeWireRegistryError
PipeWireBindingError
PipeWireProtocolError
EndpointMappingWarning
```

Mapping ambiguity should normally be a warning/diagnostic, not a fatal application error.

---

# 76. CMAKE / CLEAN BUILD ACCEPTANCE

These must work from a clean checkout after Phase 4:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure

./build/apps/desktop/auralis-desktop
```

Adapt executable path only if the repository has changed it.

No hidden manual generation step.

No dependence on an already-existing build tree.

---

# 77. COMPILER QUALITY

Build with the current project warning settings.

Do not introduce:

- unused listener variables;
- narrowing warnings;
- sign conversion bugs;
- enum/int abuse;
- unchecked null core/registry pointers;
- unsafe string lifetimes;
- non-portable GNU extensions unless already accepted by the project.

If the project supports stricter warnings in CI, Phase 4 must pass them.

---

# 78. DOCUMENTATION

Update project documentation sufficiently for another developer to understand:

- what Phase 4 added;
- required PipeWire development dependency;
- architecture of PipeWire integration;
- endpoint model;
- Bluetooth mapping strategy;
- live integration-test environment variables;
- manual validation steps;
- Phase 4 limitations;
- explicit statement that routing is Phase 5.

Do not rewrite unrelated documentation.

---

# 79. SOURCE COMMENTS

Comments should explain why, not restate code.

Good:

```cpp
// PipeWire global IDs are runtime-scoped and can change after reconnect;
// keep the Auralis endpoint key independent from this value.
```

Bad:

```cpp
// Set global id.
globalId = id;
```

Document thread-affinity assumptions near the PipeWire/Qt handoff.

---

# 80. MANUAL DIAGNOSTIC COMMANDS

These may be used by the developer to validate the native implementation.

They must not become production dependencies.

Useful:

```bash
wpctl status
pw-cli ls
pw-dump
pactl info
```

Bluetooth:

```bash
bluetoothctl devices
bluetoothctl info AA:BB:CC:DD:EE:FF
```

Service state:

```bash
systemctl --user status pipewire pipewire-pulse wireplumber --no-pager
```

The Auralis endpoint model should broadly correspond to what these diagnostics reveal, but correctness comes from native PipeWire events.

---

# 81. MANUAL PHASE 4 VALIDATION — BUILT-IN AUDIO

Start with no Bluetooth requirement.

Steps:

```text
1. Launch Auralis.
2. Confirm PipeWire status = connected/ready.
3. Confirm built-in playback/capture objects are detected.
4. Confirm they appear as normalized AudioEndpoint objects.
5. Confirm they are NOT assigned a BluetoothDevice ID.
6. Start/stop an ordinary application audio stream.
7. Confirm the endpoint registry does not incorrectly expose the app stream as a physical endpoint.
```

---

# 82. MANUAL PHASE 4 VALIDATION — BLUETOOTH AUDIO DEVICE

Steps:

```text
1. Launch Auralis.
2. Use existing Phase 2 scan.
3. Use existing Phase 3 Pair/Trust/Connect.
4. Wait for BlueZ Connected=true.
5. Observe PipeWire endpoint appearance.
6. Confirm Auralis detects it.
7. Confirm endpoint direction is correct.
8. Confirm transport/profile is sensible.
9. Confirm endpoint maps to the same Phase 3 BluetoothDevice.
10. Disconnect through Auralis.
11. Confirm endpoint disappears/becomes unavailable.
12. Reconnect through Auralis.
13. Confirm a newly-created PipeWire object maps again.
```

No terminal command should be necessary for the workflow itself.

---

# 83. MANUAL PHASE 4 VALIDATION — APP RESTART

With a paired audio device already connected:

```text
1. Close Auralis cleanly.
2. Keep the Bluetooth device connected if the environment allows.
3. Relaunch Auralis.
4. Verify Phase 3 existing-device state appears.
5. Verify PipeWire registry enumerates existing endpoint.
6. Verify resolver maps without reconnect.
```

---

# 84. MANUAL PHASE 4 VALIDATION — RAPID CHURN

Perform:

```text
connect
disconnect
connect
disconnect
connect
```

Expected:

- no crash;
- no duplicate endpoints;
- no stale mapped endpoint;
- no leaked active proxies evident in logs;
- final state matches reality.

---

# 85. OPTIONAL DEBUG GRAPH DUMP

A debug-only internal method may provide a textual snapshot:

```text
PipeWire Objects:
Device 51 ...
Node 57 Audio/Sink ...
Node 58 Audio/Source ...

Auralis Endpoints:
endpoint=... pw=57 direction=Playback transport=ALSA
endpoint=... pw=84 direction=Playback transport=Bluetooth mapped=device-...
```

This must read Auralis's in-memory model.

Do not implement it by running `pw-dump`.

---

# 86. EXPECTED FILE AREAS

Do not treat these names as mandatory if the repo uses different names.

Likely areas:

```text
include/auralis/audio/
  AudioEndpoint.h
  AudioEndpointRegistry.h
  EndpointResolver.h
  PipeWireManager.h
  PipeWireObjectStore.h
  PipeWireTypes.h

src/audio/
  AudioEndpoint.cpp
  AudioEndpointRegistry.cpp
  EndpointResolver.cpp
  PipeWireManager.cpp
  PipeWireObjectStore.cpp
  PipeWireRegistryMonitor.cpp

tests/unit/audio/
  tst_AudioEndpointRegistry.cpp
  tst_EndpointResolver.cpp
  tst_PipeWireObjectStore.cpp

tests/integration/audio/
  tst_PipeWireLiveIntegration.cpp
```

Again: reuse existing files/classes wherever possible.

---

# 87. IMPLEMENTATION ORDER

Implement in this order unless repository structure strongly suggests otherwise.

## Step 1 — Audit and compile baseline

Before modifications:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Record baseline failures if any.

Do not attribute pre-existing failures to Phase 4.

## Step 2 — Add/confirm PipeWire build dependency

Make a trivial native PipeWire compile/link succeed.

## Step 3 — Implement connection lifecycle

Connect/disconnect cleanly.

## Step 4 — Implement registry enumeration

Observe globals and removals.

## Step 5 — Normalize objects

Create property snapshots/device/node info.

## Step 6 — Build object store

Track device/node churn.

## Step 7 — Implement endpoint classification

Produce general playback/capture endpoints.

## Step 8 — Implement endpoint registry

Stable updates/removal.

## Step 9 — Implement Bluetooth resolver

Use Phase 3 registry.

## Step 10 — Wire event reevaluation

Handle order-independent events.

## Step 11 — Add Qt/QML exposure

Minimal validation UI.

## Step 12 — Add deterministic tests

All run without hardware.

## Step 13 — Add opt-in live tests

PipeWire and Bluetooth mapping.

## Step 14 — Run full regression suite

All Phase 1–3 tests must remain green.

## Step 15 — Manual real-device verification

Prove Phase 4 exit gate.

---

# 88. DO NOT OVERWRITE EXISTING PHASE 3 DEVICE STATE

The Bluetooth device registry remains authoritative for Bluetooth state.

Do not create a second Bluetooth registry in the audio module.

Audio code may maintain a non-owning/reference view of:

```text
device ID
address
object path
name
connected
servicesResolved
```

for correlation.

When Phase 3 says a device was forgotten, remove stale mapping references.

---

# 89. DO NOT MOVE BLUETOOTH D-BUS CONTROL INTO PIPEWIRE

BlueZ remains the owner of:

- scanning;
- pairing;
- trusting;
- connecting;
- disconnecting;
- forgetting.

PipeWire remains the owner/source of truth for:

- audio graph objects;
- audio nodes;
- audio endpoint availability.

Do not use PipeWire as a replacement Bluetooth controller.

---

# 90. FUTURE-PROOFING FOR PHASE 5

Phase 5 will need to consume Phase 4 endpoints.

Therefore make it possible to later reference an endpoint by:

```text
AudioEndpointId
current PipeWire node/global ID
direction
availability
format/capabilities if known
```

But do not implement route/link creation now.

A future `AudioRouter` should not need to understand BlueZ object paths to route to an endpoint.

---

# 91. FUTURE-PROOFING FOR PHASE 6

Phase 6 sessions should operate in terms of stable Auralis devices/endpoints.

Therefore do not let session-facing types depend on:

```text
pw_node*
pw_device*
spa_dict*
```

Phase 4 is the abstraction boundary that prevents this leak.

---

# 92. FUTURE LE AUDIO COMPATIBILITY

The endpoint abstraction should allow:

```text
A2DP endpoint
HFP endpoint
LE Audio endpoint
future endpoint
```

to share the same higher-level model.

Do not add LE Audio implementation in Phase 4.

Do not hardcode all Bluetooth playback endpoints as `A2DP`.

Use actual properties when available and `Unknown` when not.

---

# 93. PERFORMANCE EXPECTATIONS

Phase 4 is metadata/event handling, not DSP.

It should have negligible CPU impact under normal graph churn.

Avoid:

- busy polling;
- repeated full `pw-dump`;
- sleep loops;
- high-frequency timers;
- rebuilding entire UI state on every unrelated global.

A small endpoint registry should be cheap.

---

# 94. SECURITY / PRIVILEGE EXPECTATIONS

Auralis should connect to the normal user PipeWire instance.

Do not require root.

Do not add setuid behavior.

Do not weaken D-Bus or PipeWire security configuration.

If an environment permission problem occurs, surface it as an error.

---

# 95. PHASE 4 DEFINITION OF DONE — CODE

All must be true:

- [ ] Native PipeWire library is linked.
- [ ] PipeWire connection lifecycle exists.
- [ ] Qt event loop is not blocked.
- [ ] Registry global events are handled.
- [ ] Registry remove events are handled.
- [ ] Device/node object data is normalized.
- [ ] Raw `spa_dict` lifetime does not leak across callbacks.
- [ ] Audio endpoints are classified.
- [ ] `AudioEndpointRegistry` exists and deduplicates.
- [ ] Built-in audio endpoints can exist without Bluetooth.
- [ ] Bluetooth endpoint mapping uses Phase 3 data.
- [ ] Address mapping works.
- [ ] BlueZ path/device mapping works when available.
- [ ] Node→PipeWire-device→Bluetooth mapping works.
- [ ] Ambiguous name fallback does not guess.
- [ ] Endpoint removal works.
- [ ] Reconnect with changed PipeWire ID works.
- [ ] Multiple endpoints for one Bluetooth device work.
- [ ] No Phase 5 routing logic was added.
- [ ] Clean shutdown is safe.

---

# 96. PHASE 4 DEFINITION OF DONE — TESTS

All must be true:

- [ ] Existing Phase 1–3 tests pass.
- [ ] Property utility tests pass.
- [ ] Endpoint classifier tests pass.
- [ ] Endpoint registry tests pass.
- [ ] Resolver tests pass.
- [ ] Ambiguity tests pass.
- [ ] Event-order tests pass.
- [ ] Reconnect/object-ID churn tests pass.
- [ ] Multi-endpoint-per-device tests pass.
- [ ] Non-Bluetooth endpoint tests pass.
- [ ] Normal `ctest` does not require Bluetooth hardware.
- [ ] Live PipeWire test is opt-in.
- [ ] Live Bluetooth mapping test is opt-in.
- [ ] Live tests have bounded timeouts and useful failure diagnostics.

---

# 97. PHASE 4 DEFINITION OF DONE — USER VISIBLE

All must be true:

- [ ] Auralis starts normally.
- [ ] Bluetooth Phase 2/3 UI continues working.
- [ ] PipeWire state is visible or diagnosable.
- [ ] Audio endpoints are visible in a minimal endpoint view/model.
- [ ] A connected Bluetooth audio device's endpoint appears.
- [ ] It maps to the correct Phase 3 Bluetooth device.
- [ ] Disconnect removes/unavailable-marks the endpoint.
- [ ] Reconnect restores a valid mapped endpoint.
- [ ] No terminal is required for the normal workflow.

---

# 98. REQUIRED COMMAND VERIFICATION

Run:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure

./build/apps/desktop/auralis-desktop
```

Then, if live PipeWire tests exist:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build \
  -R 'PipeWire.*Live|Live.*PipeWire' \
  --output-on-failure
```

Then, with a real connected Bluetooth audio device:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" \
ctest --test-dir build \
  -R 'PipeWire.*Bluetooth|Bluetooth.*PipeWire' \
  --output-on-failure
```

Adapt test regex to the actual target names.

---

# 99. REQUIRED FINAL IMPLEMENTATION REPORT

After implementation, return a structured report containing:

## A. Repository audit

- existing architecture discovered;
- relevant Phase 1–3 classes reused;
- important deviations from the proposed names.

## B. Files changed

For every file:

```text
path
reason
key change
```

## C. Phase 4 architecture

Explain:

```text
PipeWire connection
registry monitoring
object store
endpoint classification
endpoint registry
Bluetooth resolver
Qt handoff
```

## D. Threading model

State exactly:

- which thread runs PipeWire callbacks;
- which thread owns registries/models;
- how handoff occurs;
- how shutdown prevents use-after-free.

## E. Mapping strategy

State exactly which property keys/signals are used and the fallback order.

## F. Tests added

List every test target and important scenario.

## G. Commands run

Include actual commands and results.

## H. Remaining limitations

Only Phase 4 limitations.

Do not call Phase 5 work a defect.

## I. Phase 4 verdict

Return exactly one:

```text
PHASE 4: COMPLETE
```

or:

```text
PHASE 4: INCOMPLETE
```

If incomplete, list every blocking exit-gate item.

---

# 100. CODING STYLE EXPECTATIONS

Match the existing repository.

Unless the repository dictates otherwise:

- modern C++;
- RAII;
- const correctness;
- strongly typed enums;
- clear value types;
- `std::optional` for unknown metadata where appropriate;
- no unnecessary singletons;
- no global mutable state;
- no production shell execution;
- no exceptions crossing C callbacks;
- no unsafe ownership of PipeWire-provided memory;
- no UI logic in low-level PipeWire callbacks.

Use Qt types at Qt boundaries and plain C++ where it improves testability, but do not create gratuitous conversion layers.

---

# 101. IMPORTANT NATIVE PIPEWIRE API EXPECTATIONS

Implement against the native PipeWire client API.

Core concepts expected to appear in the low-level implementation include equivalents of:

```text
pw_init
pw_thread_loop_new
pw_thread_loop_start
pw_thread_loop_stop
pw_context_new
pw_context_connect
pw_core_get_registry
pw_registry_add_listener
pw_registry_bind
pw_core_disconnect
pw_context_destroy
pw_thread_loop_destroy
```

and registry callbacks equivalent to:

```text
global
global_remove
```

Use the actual installed headers and version guards rather than copying an outdated sample blindly.

If additional `pw_device` / `pw_node` listeners are needed, bind and subscribe correctly.

---

# 102. PIPEWIRE THREAD-LOOP LOCKING

If using `pw_thread_loop`:

- obey the PipeWire locking contract;
- hold the loop lock when invoking PipeWire functions on objects associated with the loop from external threads;
- do not hold the lock longer than necessary;
- never call stop while holding the lock;
- remember callbacks are invoked under the loop lock;
- do not perform slow UI/domain work while holding it.

A good callback pattern:

```text
PipeWire callback
  |
  +-- copy id/type/properties
  +-- enqueue small event
  +-- return
```

Then process the event on the owning application thread.

---

# 103. SERVER ROUNDTRIP / INITIAL ENUMERATION

If deterministic startup readiness requires knowing when the initial registry enumeration has reached a synchronization point, use the PipeWire core synchronization mechanism rather than a sleep.

Conceptually:

```text
attach registry listener
request sync/roundtrip
receive done
mark initial enumeration complete
```

Do not assume a fixed 500 ms sleep means enumeration is complete.

Expose an internal “initial graph snapshot ready” signal/state if useful for tests.

---

# 104. PROPERTY-DRIVEN CLASSIFICATION

The registry's `type` tells you object interface type.

Properties tell you what it represents.

Classification must use both.

Conceptual examples:

```text
type = PipeWire:Interface:Node
media.class = Audio/Sink
=> playback endpoint candidate

type = PipeWire:Interface:Node
media.class = Stream/Output/Audio
=> application stream, not physical destination endpoint
```

Build unit fixtures from realistic graph property shapes.

---

# 105. RAW PROPERTY RETENTION

For diagnostics/future use, it is acceptable for normalized objects to keep a copy of the complete property dictionary.

But:

- public UI-facing types should expose typed fields;
- business logic should use well-defined property helpers;
- tests should not depend on arbitrary unordered-map iteration order.

---

# 106. INTEGER RELATIONSHIP PROPERTIES

Properties such as a node's owning device reference may be string-valued dictionary entries.

Parse carefully.

If parsing fails:

- preserve raw property;
- leave typed field unknown;
- log at DEBUG/TRACE if useful;
- do not crash.

Never use `atoi()` without error detection for identity fields.

---

# 107. OBJECT SERIAL

When available, use object serial information as a useful stable-within-server-lifecycle identifier.

Do not assume it remains a permanent physical-device identifier across all server restarts unless verified.

The Bluetooth Phase 3 identity remains the strongest stable anchor for Bluetooth endpoints.

---

# 108. PIPEWIRE SERVER RESTART

Full recovery belongs to Phase 8, but Phase 4 must fail safely.

If the core connection is lost:

- mark PipeWire state error/unavailable;
- invalidate or clear current endpoint availability;
- do not leave stale endpoints marked available;
- do not crash on shutdown.

If a simple restart/reconnect mechanism already exists generically, integrate with it.

Do not build a complex exponential-backoff system unless required by existing architecture.

---

# 109. WIREPLUMBER ROLE

Auralis observes PipeWire objects created/managed under the current desktop audio policy.

Do not implement a custom session manager.

Do not replace WirePlumber.

Do not write system/user WirePlumber policy scripts as a Phase 4 requirement.

If mapping depends on properties that WirePlumber exposes, consume them through PipeWire.

---

# 110. PULSE COMPATIBILITY

The machine runs `pipewire-pulse`.

Auralis Phase 4 should still integrate directly with native PipeWire, not libpulse, for graph awareness.

PulseAudio-compatible commands are diagnostics only.

Do not build the core endpoint model from `pactl`.

---

# 111. REALISTIC BLUETOOTH ENDPOINT VARIATION

Be prepared for properties to vary by:

- headset model;
- codec;
- BlueZ profile;
- WirePlumber version;
- PipeWire version;
- whether the node or owning device contains the BlueZ key.

This is why the resolver must aggregate signals rather than require one magical property on every node.

Log the raw relevant property set at DEBUG when a Bluetooth-looking endpoint remains unresolved.

---

# 112. TEST FIXTURE BUILDER

Create concise test builders so resolver tests are readable.

For example conceptually:

```cpp
auto btDevice = makeBluetoothDevice(
    "device-1",
    "AA:BB:CC:DD:EE:FF",
    "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");

auto pwDevice = makePipeWireDevice(50, {
    {"api.bluez5.address", "AA:BB:CC:DD:EE:FF"}
});

auto pwNode = makePipeWireNode(60, {
    {"media.class", "Audio/Sink"},
    {"device.id", "50"}
});
```

Do not repeat huge property maps in every test.

---

# 113. DETERMINISTIC SIGNAL TESTING

If endpoint registries emit Qt signals, test:

- endpoint added exactly once;
- update emitted when meaningful fields change;
- remove emitted exactly once;
- duplicate global event does not duplicate;
- mapping change from unresolved → mapped emits an update;
- mapping change from mapped → unresolved after device removal emits an update.

This makes the UI trustworthy.

---

# 114. ENDPOINT AVAILABILITY SEMANTICS

Define one clear policy.

Recommended:

```text
backing audio node exists and is classified => Available
backing node removed => endpoint removed from current registry
```

or, if the registry intentionally preserves logical entries:

```text
backing node removed => Unavailable
```

Choose one model and apply it consistently.

For Phase 5, a “current endpoints only” registry is simpler.

If persistent unavailable entries are desired for UX, keep that persistence outside the low-level PipeWire object store.

Do not mix both semantics unpredictably.

---

# 115. ENDPOINT CAPABILITIES

If an existing `EndpointCapabilities` type exists, populate safe capabilities such as:

```text
playback
capture
microphone
transport type
```

Leave future fields unknown/false unless actually known:

```text
groupable
lowLatency
batteryStatus
volumeControl
```

Do not infer hearing-device capabilities from the word “hearing” in a name.

---

# 116. BLUETOOTH DEVICE SERVICES

Phase 3 may expose UUIDs/services.

Use them only as supporting evidence if useful.

The endpoint mapping should primarily rely on direct PipeWire/BlueZ identity.

Do not require service UUIDs for every audio endpoint.

---

# 117. TESTING WITHOUT A GUI

All core tests must run headless where the existing Qt test framework permits.

Endpoint discovery/mapping logic must not require QML engine initialization.

Keep presentation optional.

---

# 118. CLEAN SHUTDOWN TEST

Add a test that:

```text
start manager
receive some synthetic/live events
stop manager
destroy manager
```

and verify:

- no crash;
- no callbacks after destruction;
- no endpoint-registry mutation after teardown;
- repeated `stop()` is harmless if public API permits it.

For live tests, repeat start/stop a few times if reliable.

---

# 119. PHASE 0–3 REGRESSION PROTECTION

Before final verdict, explicitly run the existing tests for:

- core;
- configuration;
- Bluetooth discovery;
- BlueZ device registry;
- pairing agent;
- device lifecycle/state machine;
- any Phase 3 live integration tests that are safe in the current environment.

Do not silently disable a pre-existing test to make Phase 4 green.

If an existing test needs a legitimate adaptation due to a new dependency, explain it.

---

# 120. NO TEST CHEATING

Do not:

- change assertions to weaker statements without reason;
- skip failing unit tests;
- mark core tests `WILL_FAIL`;
- disable tests globally;
- add unconditional environment skips;
- catch all exceptions and report pass;
- fake endpoints in production code.

Opt-in skipping is appropriate only for genuinely environment-dependent live tests.

---

# 121. MANUAL VALIDATION OUTPUT

When validating a real Bluetooth endpoint, print/log a concise mapping line:

```text
Bluetooth:
  id: <auralis-device-id>
  address: AA:BB:CC:DD:EE:FF
  name: ...

PipeWire:
  device global id: ...
  node global id: ...
  node: ...
  media.class: Audio/Sink
  profile: ...
  codec: ...

Auralis:
  endpoint id: ...
  direction: Playback
  mapping: ExactBluetoothAddress
```

This makes the Phase 4 gate auditable.

---

# 122. EXPECTED PHASE 4 MILESTONE

This phase corresponds to:

```text
MILESTONE 4
Bluetooth audio endpoints appear inside Auralis.
```

Do not claim Milestone 5.

Milestone 5 is:

```text
Audio can be routed to selected endpoint.
```

That is Phase 5.

---

# 123. SUGGESTED GIT COMMITS

Use meaningful commits, for example:

```text
phase-4: add native PipeWire connection and registry monitor

phase-4: add audio endpoint model and registry

phase-4: map BlueZ devices to PipeWire audio endpoints

phase-4: expose audio endpoints to desktop UI

phase-4: add PipeWire and endpoint integration tests
```

Do not create empty/noisy commits solely to match this list.

A final tag may follow the project's existing phase-tag convention.

---

# 124. FINAL REVIEW CHECKLIST

Before declaring Phase 4 complete, answer all of these:

```text
Can Auralis connect to PipeWire natively?                 YES/NO
Can it enumerate current audio nodes?                     YES/NO
Can it react to node addition/removal?                    YES/NO
Can it distinguish Audio/Sink from app streams?           YES/NO
Can it create stable AudioEndpoint objects?               YES/NO
Can it show built-in audio without Bluetooth?             YES/NO
Can it map Bluetooth endpoint by address?                 YES/NO
Can it map through PipeWire device ownership?             YES/NO
Can it avoid ambiguous name guessing?                     YES/NO
Can one Bluetooth device own multiple endpoints?          YES/NO
Can disconnect remove/invalidate the endpoint?            YES/NO
Can reconnect with a new PipeWire ID remap correctly?     YES/NO
Are all normal unit tests hardware-independent?           YES/NO
Are live tests opt-in and bounded?                        YES/NO
Do all Phase 1–3 tests still pass?                        YES/NO
Is Phase 5 routing still unimplemented?                   YES/NO
```

Any `NO` on a required item means:

```text
PHASE 4: INCOMPLETE
```

---

# 125. SUCCESS SCENARIO

The final working behavior should look conceptually like this:

```text
Auralis starts
   |
   +-- BluetoothManager loads known devices
   |
   +-- PipeWireManager connects
   |
   +-- RegistryMonitor discovers:
   |      Device 51 -> built-in ALSA
   |      Node 57   -> Audio/Sink
   |      Node 58   -> Audio/Source
   |
   +-- AudioEndpointRegistry:
   |      Built-in Output
   |      Built-in Input
   |
User connects Bluetooth hearing/audio device
   |
   +-- Phase 3:
   |      BluetoothDevice connected=true
   |
   +-- PipeWire/WirePlumber:
   |      Bluetooth device global added
   |      Bluetooth Audio/Sink node added
   |
   +-- PipeWireObjectStore updates
   |
   +-- Endpoint classifier creates playback endpoint
   |
   +-- EndpointResolver:
   |      api.bluez5.address
   |              |
   |              v
   |      existing BluetoothDevice.address
   |
   +-- AudioEndpointRegistry:
          Hearing Device A
          Playback
          Bluetooth
          Available
          mappedDeviceId=<Phase3 ID>
```

Then:

```text
disconnect
   |
   +-- PipeWire node removed
   +-- endpoint removed/unavailable

reconnect
   |
   +-- new PipeWire node ID
   +-- same Bluetooth device identity
   +-- endpoint mapped again
```

That is the Phase 4 target.

---

# 126. FINAL INSTRUCTION

Implement Phase 4 completely and conservatively.

Priorities, in order:

```text
1. Do not regress Phases 0–3.
2. Use the existing repository architecture.
3. Use native PipeWire API in production.
4. Make PipeWire lifecycle/threading safe.
5. Normalize PipeWire objects into Auralis domain types.
6. Build a correct AudioEndpoint registry.
7. Map Bluetooth endpoints to the existing Phase 3 devices using strong identity signals.
8. Make event ordering and reconnect behavior deterministic.
9. Add strong hardware-independent tests.
10. Add opt-in live integration tests.
11. Expose enough UI/diagnostics to prove the milestone.
12. Do not implement Phase 5 routing.
```

Do not stop after scaffolding.

Do not return only pseudocode.

Modify the actual repository, build it, run the tests, fix failures introduced by the implementation, and return the required final implementation report.

The completion target is:

```text
PHASE 4: COMPLETE
MILESTONE 4: BLUETOOTH AUDIO ENDPOINTS APPEAR INSIDE AURALIS
```
