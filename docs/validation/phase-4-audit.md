# Auralis Phase 4 Implementation Audit

Location: `docs/validation/phase-4-audit.md` (moved from the repository root
during documentation reorganization).

Independent verification of the repository as received at
`abf8f195526c51a09f1cf5076b27e439a441020c` on Ubuntu 26.04 LTS.

No production or test source was modified during this audit.
Allowed artifacts: this document, `audit-phase4/`, and the fresh `build-audit/`
directory.

---

## 1. Executive Summary

Phase 4 implements native PipeWire registry monitoring, normalized
`AudioEndpoint` objects, live endpoint lifecycle handling, and Bluetooth-device
correlation using BlueZ5 PipeWire properties plus a unique-name fallback.

A clean Ninja configure/build in `build-audit/` succeeded with no compiler
warnings. Default CTest reported 22/22 passed; the two live tests are opt-in and
`QSKIP` unless environment flags are set. The Phase 3 BlueZ live test remained
registered and passed against real hardware (`88:08:94:9D:B4:22`, Smokin' Buds).
The PipeWire live test connected, enumerated built-in endpoints, shut down
cleanly, and with the same device connected mapped two Bluetooth endpoints to
the Phase 3 BlueZ object path `/org/bluez/hci0/dev_88_08_94_9D_B4_22`.

`auralis-desktop` launched on the live display, reached Application Core Ready,
showed PipeWire Connected, classified four endpoints (two mapped Bluetooth),
and survived a disconnect/reconnect graph churn without crash. No Phase 5
routing, `pw_stream`, `pw_filter`, or link-creation implementation was found.

No blocker or major defect was identified.

VERDICT: PHASE 4 COMPLETE

---

## 2. Final Verdict

```text
PHASE 4: COMPLETE
```

All completeness gates in the audit prompt were met:

| Gate | Result |
|---|---|
| Clean configure | PASS (`CMAKE_EXIT_CODE=0`) |
| Clean build | PASS (`BUILD_EXIT_CODE=0`) |
| Normal CTest | PASS (`22/22`, `CTEST_EXIT_CODE=0`) |
| Phase 0–3 CTest registration | PASS |
| Phase 4 CTest registration | PASS (`tst_AudioEndpoints`, `tst_EndpointResolver`, `tst_PipeWireManager`, `tst_PipeWireProperties`, `tst_PipeWireLiveIntegration`) |
| Native PipeWire | PASS |
| AudioEndpoint model/registry | PASS |
| Strong-identity Bluetooth resolver | PASS (unit tests) |
| Event order / object churn | PASS (unit tests + live graph) |
| Blocker defects | none |
| Phase 5 routing absent | PASS |
| Live Bluetooth mapping (hardware present) | PASS |

---

## 3. Audit Scope

Audited as implemented:

- PHASE 0 toolchain / clean-room build
- PHASE 1 foundation (`ApplicationCore`, logging, configuration, stubs)
- PHASE 2 Bluetooth discovery
- PHASE 3 Bluetooth device management
- PHASE 4 PipeWire endpoint observation and Bluetooth correlation

Out of scope for implementation (and confirmed absent): Phase 5 routing engine.

Mode: inspect, build, test, exercise, document. No source repairs.

---

## 4. Repository Revision and Working Tree

| Field | Value |
|---|---|
| Working directory | `/home/smit/Code/Auralis` |
| Branch | `master` (up to date with `origin/master`) |
| HEAD | `abf8f195526c51a09f1cf5076b27e439a441020c` |
| Tip commit | `abf8f19 Report PipeWire start failures correctly and register the BlueZ live integration test.` |

Pre-audit working tree: clean except the audit directory created by this run
(`?? audit-phase4/`). Existing user work was not reset.

Evidence: `audit-phase4/git-status.txt`.

---

## 5. Linux Development Environment

Evidence: `audit-phase4/environment.txt`.

| Component | Observed |
|---|---|
| OS | Ubuntu 26.04 LTS (resolute) |
| Kernel | Linux 7.0.0-29-generic x86_64 |
| gcc / g++ | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Git | 2.53.0 |
| Qt | 6.10.2 |
| BlueZ / bluetoothctl | 5.85 |
| PipeWire | 1.6.2 (compiled and linked) |
| pkg-config `libpipewire-0.3` | 1.6.2 |
| pkg-config `dbus-1` | 1.16.2 |
| pkg-config `Qt6Bluetooth` | 6.10.2 |
| pkg-config `Qt6Multimedia` | 6.10.2 |

This matches the development baseline documented in `README.md`.

---

## 6. Runtime Services

Evidence: `audit-phase4/runtime-services.txt`, `audit-phase4/wpctl-status.txt`.

| Service | State |
|---|---|
| `pipewire.service` (user) | active (running) |
| `pipewire-pulse.service` (user) | active (running) |
| `wireplumber.service` (user) | active (running) |
| `bluetooth.service` (system) | active (running) |

`wpctl status` at audit start (no Bluetooth audio connected):

- Device 49 Built-in Audio `[alsa]`
- Sink 56 Built-in Audio Analog Stereo (muted)
- Source 57 Built-in Audio Analog Stereo
- Video nodes present (webcam); not audio endpoints

WirePlumber logged a missing libcamera SPA plugin. That is a machine/environment
limitation, not an Auralis defect. It did not prevent audio endpoint enumeration.

Bluetooth adapter: `E8:9E:B4:13:4C:CC` (`smit`), powered on.

Suitable audio device: paired headset **Smokin' Buds** `88:08:94:9D:B4:22`
(Audio Sink + Handsfree UUIDs). It was disconnected at audit start.

bluetoothd journal contained prior HFP “Connection refused (111)” lines for this
device. That is a BlueZ/profile/hardware issue, not classified as an Auralis
Phase 4 code defect. A2DP/PipeWire nodes appeared after a successful connect.

---

## 7. Phase 4 Architecture Found

Actual classes (names match the intended design):

| Responsibility | Actual type / file |
|---|---|
| PipeWire connection | `PipeWireConnection` — `include/auralis/audio/PipeWireConnection.h`, `src/audio/PipeWireConnection.cpp` |
| PipeWire manager | `PipeWireManager` implements `IPipeWireManager` — `src/audio/PipeWireManager.cpp` |
| Registry monitoring | `pw_registry_add_listener` in `PipeWireConnection::Impl` |
| Object store | `PipeWireObjectStore` |
| Endpoint DTO | `AudioEndpoint` |
| Classifier | `classifyAudioEndpoint()` / `makeEndpointLogicalId()` |
| Registry | `AudioEndpointRegistry` |
| Resolver | `EndpointResolver` + `refreshEndpointsFromStore()` |
| QML list model | `AudioEndpointListModel` (`QAbstractListModel`) |
| ApplicationCore integration | `ApplicationServices.pipeWire`; `DesktopApplication::makeProductionServices()` constructs `PipeWireManager(bluetooth->deviceRegistry())` |
| QML | `AppCore.audio` in `ui/qml/Main.qml`; rows in `ui/components/EndpointRow.qml`; per-device `audioStatusForDevice()` in `DeviceRow.qml` |

CMake: `src/audio/CMakeLists.txt` requires `libpipewire-0.3` and links
`PkgConfig::PIPEWIRE`.

`DeviceManager` and `SessionManager` remain lifecycle stubs. Architecture docs
state that explicitly.

---

## 8. Native PipeWire Integration Review

**RESULT: PASS**

Native API usage in production (`src/audio/PipeWireConnection.cpp`):

| API | Line (approx.) |
|---|---|
| `pw_init` | 27 |
| `pw_thread_loop_new` | 333 |
| `pw_context_new` | 343 |
| `pw_context_connect` | 354 |
| `pw_core_get_registry` | 369 |
| `pw_registry_add_listener` | 382 |
| `pw_registry_bind` | 158 |

Configure output: `Found libpipewire-0.3, version 1.6.2`.

Production CLI dependence search (`wpctl`, `pw-dump`, `pw-cli`, `pactl` under
`include/`, `src/`, `apps/`): **no matches**. No `QProcess` / `system()` /
`popen()` in `src/audio/`.

Classification: production discovery uses the native PipeWire API. CLI names
appear only in documentation as developer diagnostics.

Evidence: `audit-phase4/source-searches.txt`, `audit-phase4/cmake-configure.txt`.

---

## 9. Threading and Lifetime Review

### Thread roles

| Concern | Finding |
|---|---|
| PipeWire callbacks | `pw_thread_loop` named `auralis-pipewire`. Registry/core/node/device listeners run on that loop, under the loop lock for bind/setup. |
| Object-store updates | `PipeWireManager::handleClientEvent()` on the Qt thread that owns `PipeWireManager`. |
| `AudioEndpointRegistry` | Same Qt thread (`unique_ptr` member of `PipeWireManager`, not moved). |
| QML / `QAbstractListModel` | `AudioEndpointListModel` parented to `PipeWireManager`; insert/remove use direct connections (required for `beginInsertRows`). `endpointUpdated` uses `Qt::QueuedConnection`. |
| Cross-thread handoff | `PipeWireManager::initialize()` registers a handler that `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)` with a copied `PipeWireClientEvent`. |
| Shutdown | `guard_->alive=false`, generation bumped, `connection_->stop()` (`pw_thread_loop_stop` **without** holding the lock), then `QCoreApplication::sendPostedEvents(this, QEvent::MetaCall)` to drain queued callbacks, then `endpoints_->clear()` / `store_->clear()`. |

`spa_dict` is copied in `copyDict()` into `QHash<QString,QString>` /
`PipeWireProperties` before the callback returns. Domain objects do not retain
raw PipeWire dictionary pointers.

Proxies: `BoundProxy` lives in `pw_proxy` user data; `destroyProxy()` removes
hooks, nulls `impl`, then `pw_proxy_destroy`. `global_remove` destroys the proxy
and emits `GlobalRemoved`. `stop()` is idempotent (`impl_ == nullptr` returns).
Repeated `PipeWireManager::shutdown()` is guarded by `Uninitialized`.

Synchronous start failures (`pw_thread_loop_new`, `pw_context_new`,
`pw_context_connect`, `pw_core_get_registry`, `pw_thread_loop_start`) return
`false` from `PipeWireConnection::start()` / `PipeWireManager::initialize()`.
Asynchronous core errors that look like a dead session emit
`PipeWireConnectionState::Error`. Object-local `ESTALE`/`ENOENT` are treated as
non-fatal (comment: Bluetooth profile-switch races).

`ApplicationCore` treats PipeWire initialize failure as non-fatal (core can
still be Ready). Covered by `tst_ApplicationCore::pipeWireFailureDoesNotAbortApplication`.
This is an intentional contract, documented in `docs/architecture.md`.

### Inspected hazards

| Hazard | Status |
|---|---|
| QML/model mutation from PipeWire callback | Not found. Callbacks only copy snapshots and queue to Qt. |
| Slow UI work under PipeWire lock | Handler only queues; store/registry work is on Qt thread. |
| `pw_thread_loop_stop` while lock held | `stop()` stops first, then locks to destroy objects. |
| Queued callback after destruction | Generation + `alive` guard + MetaCall drain. |
| Unsafe raw reference capture | Event copied by value; `std::shared_ptr<Guard>` kept alive. |

**OBSERVATION:** `AudioEndpointListModel` uses queued connection only for
updates. Insert/remove stay synchronous, which is correct. A queued update after
a later remove could theoretically target a stale index; live churn did not
exhibit a crash.

**MINOR:** if `start()` fails, `PipeWireManager::initialize()` always sets
`lastError` to `"PipeWire thread failed to start"`, which can mask the more
specific `PipeWireConnection` error until a queued `StateChanged` arrives.

---

## 10. PipeWire Registry and Object Store Review

`PipeWireConnection` implements:

- `registry.global` → `GlobalAdded` snapshot + bind Device/Node
- `registry.global_remove` → destroy proxy + `GlobalRemoved`
- `pw_node_events.info` / `pw_device_events.info` → `GlobalUpdated` with copied props
- Initial `pw_core_sync` → `InitialSyncDone`

`PipeWireObjectStore::upsert` merges Device/Node properties, tracks Port/Link/
Metadata counts, and evicts a previous kind when a global ID is reused
(`occupiedByOtherKind` → `remove`). Covered by
`TstAudioEndpoints::reusedGlobalIdEvictsPreviousKindAndEndpoint`.

Graph changes after startup are handled: desktop logs show `GlobalAdded` /
`GlobalRemoved` after initial sync, endpoint insert/remove, and remapping.
Phase 4 is not a one-shot snapshot.

Port/link objects are counted but not classified as user endpoints.

---

## 11. AudioEndpoint Model Review

Actual fields (`include/auralis/audio/AudioEndpoint.h`):

| Requested concept | Field |
|---|---|
| Logical identity | `id` (`bt:…` or `pw:…`) |
| PipeWire runtime/global ID | `pipeWireObjectId` |
| PipeWire serial | `pipeWireSerial` (`std::optional<quint64>`) |
| Name / description | `name`, `description` |
| Direction | `direction` (`Playback` / `Capture` / `Duplex` / `Unknown`) |
| Availability | `availability` |
| Transport | `transport` (`BuiltIn` / `Alsa` / `BluetoothClassic` / `BluetoothLE`) |
| Profile / codec | `profile`, `codec` |
| Sample rate / channels | `sampleRate`, `channelCount` |
| Bluetooth device ID | `bluetoothDeviceId` (BlueZ object path) |
| Bluetooth address | `bluetoothAddress` |
| BlueZ path | `bluezObjectPath` |
| Mapping status | `mappingConfidence`, `mappingReason`; `mapped()` |

PipeWire global ID is **not** used as permanent physical identity.
`makeEndpointLogicalId()` uses:

- Bluetooth: `bt:{address}:{direction}:{profileOrNodeName}`
- Else: `pw:{serial|nodeName|globalId}:{direction}:{profileOrNodeName}`

Churn tests (`objectIdChurnKeepsLogicalIdentity`, `reconnectChangesGlobalId`)
prove a new global ID keeps the same logical `id`.

QML roles expose id, name, description, direction, available, transport,
profile, codec, pipeWireObjectId, bluetooth fields, mapped, mediaClass,
sampleRate, channelCount.

---

## 12. AudioEndpointRegistry Review

Verified in `src/audio/AudioEndpointRegistry.cpp` and `tst_AudioEndpoints.cpp`:

| Operation | Evidence |
|---|---|
| Insert / upsert | `upsert()`; first insert returns true |
| Update | same `id` updates in place; no-op if `endpointPublicStateEqual` |
| Dedup by PipeWire ID | if PW id maps to a different logical id, previous row is removed |
| Remove | `removeById`, `removeByPipeWireObjectId`, `clear` |
| Lookup by logical id | `findById` |
| Lookup by PipeWire ID | `findByPipeWireObjectId` |
| Query by Bluetooth device | `endpointsForBluetoothDevice` (two endpoints / one device in unit test) |

`QAbstractListModel`: `beginInsertRows` / `endInsertRows`,
`beginRemoveRows` / `endRemoveRows`, `dataChanged`. Roles are stable in
`roleNames()`. Thread ownership: Qt thread of `PipeWireManager`.

---

## 13. Bluetooth ↔ PipeWire Mapping Review

Resolver priority in `EndpointResolver::applyMapping()`:

1. Normalized `api.bluez5.address` vs Phase 3 address → `ExactBluetoothAddress` (Exact)
2. `api.bluez5.path` or `api.bluez5.device` vs BlueZ object path → `ExactBlueZObjectPath` (Exact)
3. Node `device.id` → PipeWire Device address/path → `PipeWireDeviceOwnership` (Strong)
4. Unique case-insensitive name/alias match → `UniqueNameFallback` (Weak)
5. Ambiguous matches → unmapped (`Ambiguous`); endpoint remains
6. No data → unmapped (`InsufficientData`); endpoint remains

Property keys in `PipeWireTypes.cpp`: `api.bluez5.address`, `api.bluez5.path`,
`api.bluez5.device`, `api.bluez5.profile`, `api.bluez5.codec`, `device.id`.

Address normalization (`normalizeBluetoothAddress` in `BlueZTypes.cpp`), tested
in `tst_PipeWireProperties`:

| Input | Canonical |
|---|---|
| `aa:bb:cc:dd:ee:ff` | `AA:BB:CC:DD:EE:FF` |
| `AA_BB_CC_DD_EE_FF` | `AA:BB:CC:DD:EE:FF` |
| `AA-BB-CC-DD-EE-FF` | `AA:BB:CC:DD:EE:FF` |
| `AABBCCDDEEFF` | `AA:BB:CC:DD:EE:FF` |
| `not-an-address`, `AA:BB`, `GG:…`, empty | rejected (`nullopt`) |

Invalid addresses cannot match a device.

Mandatory node → device → Bluetooth path is implemented and unit-tested
(`TstEndpointResolver::pipeWireDeviceOwnership`: Device 50 with
`api.bluez5.address`, Node 60 with `device.id=50` and no node-level address).

Live graph on this machine often first mapped WirePlumber loopback nodes
(`bluez_output.88:08:94:9D:B4:22`, `bluez_input.…`) via **UniqueNameFallback**
because the first snapshot lacked usable BlueZ5 address on the node. After
disconnect/reconnect, the same endpoints mapped via **PipeWireDeviceOwnership**
and a transient `a2dp-sink` node via **ExactBluetoothAddress**. Strong identity
is implemented and tested; live first-pass may use the weak unique-name path.
That is an observation, not a missing resolver.

---

## 14. Phase 4 Unit-Test Coverage

| Test binary | Role |
|---|---|
| `tst_PipeWireProperties` | property copy/parse; BT address normalization |
| `tst_AudioEndpoints` | classification, ALSA built-in, registry, ID churn, monitors/ports |
| `tst_EndpointResolver` | mapping priority, event order, multi-endpoint, ID reconnect |
| `tst_PipeWireManager` | start/stop without requiring Connected; repeated shutdown |

`Audio/Duplex` is implemented in `directionFromMediaClass()` but has **no
dedicated unit test**. Classifier coverage for Sink/Source/Stream/Video is
present.

There is no dedicated unit test whose name asserts UniqueNameFallback success;
ambiguous-name rejection is tested. Live mapping exercised UniqueNameFallback.

---

## 15. CTest Registration Audit

`ctest --test-dir build-audit -N` registered **22** tests:

```text
tst_ServiceStatus
tst_Logger
tst_ConfigurationManager
tst_ApplicationCore
tst_BluetoothManager
tst_BlueZPropertyParser
tst_DeviceRegistry
tst_BluetoothDeviceListModel
tst_AdapterAndDiscovery
tst_BluetoothDbusError
tst_BlueZAgent
tst_DeviceLifecycle
tst_ReconnectPolicy
tst_PipeWireManager
tst_PipeWireProperties
tst_AudioEndpoints
tst_EndpointResolver
tst_DeviceManager
tst_SessionManager
tst_DesktopBackendSmoke
tst_BlueZLiveIntegration
tst_PipeWireLiveIntegration
```

`grep -F tst_BlueZLiveIntegration` and `grep -F tst_PipeWireLiveIntegration`
both matched. No Phase 3 live-test regression in registration.

Evidence: `audit-phase4/ctest-list.txt`.

---

## 16. Clean Configure and Build Results

Fresh directory: `build-audit` (existing `build/` was not used as evidence).

```text
cmake -S . -B build-audit -G Ninja
CMAKE_EXIT_CODE=0
```

```text
cmake --build build-audit
BUILD_EXIT_CODE=0
[183/183] Linking CXX executable tests/integration/tst_PipeWireLiveIntegration
```

Warnings (`warning:`, deprecated, unused, conversion, narrowing, uninitialized)
in `audit-phase4/build.txt`: **none**.

`AURALIS_WARNINGS_AS_ERRORS` is off by default; the warning set still includes
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` etc.
PipeWire headers are compiled with a local diagnostic push/pop in
`PipeWireConnection.cpp`.

---

## 17. Full Regression Test Results

```text
ctest --test-dir build-audit --output-on-failure
CTEST_EXIT_CODE=0
100% tests passed, 0 tests failed out of 22
Total Test time (real) = 5.75 sec
```

| Category | Count |
|---|---|
| Total | 22 |
| CTest “Passed” | 22 |
| Failed | 0 |
| Unexpected skip | 0 |

Default live tests **internally QSKIP** (Qt totals: 1 skipped slot each) but
CTest still reports them as Passed in 0.01s because `SKIP_RETURN_CODE` is not
set. Verbose confirmation:

```text
SKIP : TstBlueZLiveIntegration::liveBlueZRoundTrip() Set AURALIS_RUN_BLUETOOTH_INTEGRATION=1 ...
SKIP : TstPipeWireLiveIntegration::livePipeWireRoundTrip() Set AURALIS_RUN_PIPEWIRE_INTEGRATION=1 ...
```

This is the intended hardware-independent default. It is **not** a silent pass
of live behavior. See observation below.

Default CTest did **not** require Bluetooth hardware, pairing, or interaction.

---

## 18. Phase 3 BlueZ Live Integration Result

**RESULT: PASS**

Hardware: `88:08:94:9D:B4:22` Smokin' Buds — paired yes, trusted yes, connected
no at start. UUIDs include Audio Sink (`0000110b-…`) and Handsfree (`0000111e-…`).

```text
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" \
ctest --test-dir build-audit -R '^tst_BlueZLiveIntegration$' --output-on-failure
```

```text
1/1 Test #21: tst_BlueZLiveIntegration .........   Passed   12.09 sec
BLUEZ_LIVE_EXIT_CODE=0
```

12 seconds is inconsistent with a skip (0.01s) and is consistent with scan +
already-paired connect/disconnect/reconnect/disconnect as implemented in
`tst_BlueZLiveIntegration.cpp`. Destructive forget was not enabled.

Evidence: `audit-phase4/bluez-live.txt`, `audit-phase4/bluetooth-info.txt`.

---

## 19. Phase 4 PipeWire Live Integration Result

**RESULT: PASS**

```text
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build-audit -R '^tst_PipeWireLiveIntegration$' --output-on-failure -V
PIPEWIRE_LIVE_EXIT_CODE=0
```

Observed:

- `PipeWire Connecting` / `PipeWire Connected`
- Registry populated (Core, Modules, Device 49 ALSA, Node 56 `Audio/Sink`,
  Node 57 `Audio/Source`, video nodes, ports)
- Initial sync:
  `state=Connected devices=3 nodes=7 ports=11 links=0 endpoints=2 mappedBt=0 unmappedBt=0 error=none`
- Two legitimate endpoints (built-in sink + source), matching `wpctl`
- `Stream/` and `Video/` nodes not counted as endpoints
- `PipeWire Stopping` / manager shut down; `endpointCount()==0`, state Stopped

Evidence: `audit-phase4/pipewire-live.txt`.

---

## 20. Real Bluetooth Endpoint Mapping Result

**RESULT: PASS**

Setup: `bluetoothctl connect "88:08:94:9D:B4:22"` succeeded. `wpctl` then showed:

- Device 68 `Smokin' Buds` `[bluez5]`
- Sink 74 `Smokin' Buds`
- Filter `bluez_output.88:08:94:9D:B4:22` `[Audio/Sink]`
- Filter `bluez_input.88:08:94:9D:B4:22` `[Audio/Source]`
- Internal `Stream/*` loopback nodes (must be ignored by classifier)

```text
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" \
ctest --test-dir build-audit -R '^tst_PipeWireLiveIntegration$' --output-on-failure -V
PIPEWIRE_MAPPING_EXIT_CODE=0
```

Auralis logs:

```text
EndpointResolver Mapped endpoint= "bt:88:08:94:9D:B4:22:capture:bluez_input.88:08:94:9D:B4:22"
  device= "/org/bluez/hci0/dev_88_08_94_9D_B4_22" reason= "UniqueNameFallback"
EndpointResolver Mapped endpoint= "bt:88:08:94:9D:B4:22:playback:bluez_output.88:08:94:9D:B4:22"
  device= "/org/bluez/hci0/dev_88_08_94_9D_B4_22" reason= "UniqueNameFallback"
PipeWire initial registry sync complete
  "state=Connected devices=4 nodes=12 ports=24 links=3 endpoints=4 mappedBt=2 unmappedBt=0 error=none"
```

A real PipeWire endpoint mapped to the expected Phase 3 Bluetooth device
(object path derived from address `88:08:94:9D:B4:22`). Internal stream nodes
were not classified. Built-in ALSA endpoints remained in the set of four.

Evidence: `audit-phase4/pipewire-live-mapping.txt`,
`audit-phase4/wpctl-status-after-connect.txt`.

---

## 21. Desktop UI / Manual Validation

**RESULT: PASS** (application actually launched)

```text
./build-audit/apps/desktop/auralis-desktop
DISPLAY=:0  WAYLAND_DISPLAY=wayland-0
```

Log `audit-phase4/desktop-smoke.txt`:

| Observation | Evidence |
|---|---|
| Clean startup | Logger init, BlueZ available, Application core ready |
| QML loaded | `QML root loaded` (no objectCreationFailed) |
| Phase 2/3 Bluetooth | snapshot `adapters=1 devices=1`, agent registered |
| PipeWire healthy | `PipeWire Connected` |
| Endpoints present | `endpoints=4 mappedBt=2` |
| Built-in + Bluetooth | Nodes 56/57 Audio Sink/Source; mapped BT capture/playback |
| No immediate crash | process ran ~71s |
| No runaway errors | no `qCCritical` / lastError spam |

Device was **already connected** at this launch (restart-while-connected
scenario). Mapping appeared without an extra reconnect from the UI.

Interactive QML button sequence (scan / pair / Connect click / Disconnect
click) was **not** driven through the GUI. Pair/trust/connect/disconnect were
exercised by `tst_BlueZLiveIntegration` against the same `BluetoothManager`
the UI binds to. Connect for mapping used `bluetoothctl` as live-test setup,
not as a substitute claim that the QML buttons were clicked.

QML wiring inspected: `Main.qml` binds `root.audio.endpoints`, PipeWire
`connectionStateText` / `connected` / `lastError` / `diagnosticsText`, and
`audioStatusForDevice(objectPath)` (`Available` / `Initializing...` /
`Unavailable`).

---

## 22. Disconnect / Reconnect / Churn Validation

**RESULT: PASS** for one live graph cycle while `auralis-desktop` was running.
Five-cycle rapid churn was not repeated; one disconnect/reconnect was executed.

`bluetoothctl disconnect "88:08:94:9D:B4:22"` while the app ran:

```text
AudioEndpointRegistry EndpointRemoved ...capture:bluez_input... pw= 81
AudioEndpointRegistry EndpointRemoved ...playback:bluez_output... pw= 87
PipeWireRegistry GlobalRemoved id= 68
ReconnectScheduled ... attempt= 1
AutoReconnectRequested ... attempt= 1
```

Endpoints disappeared when PipeWire removed the nodes. Auralis Phase 3
auto-reconnect fired because the disconnect was not an explicit Auralis
`Device1.Disconnect` (expected Phase 3 policy). Subsequent
`bluetoothctl connect` returned `org.bluez.Error.InProgress br-connection-busy`
because reconnect was already in flight. Device ended **Connected: yes**.

On reappearance:

```text
Mapped ... playback:bluez_output.88_08_94_9D_B4_22.1 reason= PipeWireDeviceOwnership
Mapped ... capture:bluez_input... reason= PipeWireDeviceOwnership
Mapped ... playback:bluez_output.88:08:94:9D:B4:22 reason= PipeWireDeviceOwnership
Mapped ... playback:a2dp-sink reason= ExactBluetoothAddress
EndpointRemoved ...playback:a2dp-sink pw= 90
```

No crash. Transient extra playback rows during WirePlumber profile/loopback
settle were removed when node 90 became `Audio/Sink/Internal`. Final graph
again showed Smokin' Buds sink/source. Logical BT ids stayed address-based
across new PipeWire IDs.

**Application restart while already connected:** PASS — desktop launch in
section 21.

**QML-driven churn:** NOT EXECUTED.

---

## 23. Phase 5 Boundary Verification

**RESULT: PASS** (Phase 5 not implemented)

Search over `include/`, `src/`, `apps/`, `tests/`:

- `pw_link` / `pw_stream` / `pw_filter`: no production matches
- `create.*link` / `route.*endpoint` / `duplicate.*audio`: no matches
- No `AudioRouter` type

`pw_context_connect` is the only `pw_*` connect in the tree (session attach,
not routing).

Port/link **observation** (counting extras in `PipeWireObjectStore`) is not
route creation.

README / `docs/phase-4-validation.md` state routing is Phase 5+.

---

## 24. Documentation Review

| Document | Phase 4 statement | Live-test commands |
|---|---|---|
| `README.md` | Phase 4 = PipeWire observation; routing not implemented | Quoted `AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"`; correct test names |
| `docs/phase-4-validation.md` | Same; env table matches `tst_PipeWireLiveIntegration` | Quoted addresses |
| `docs/architecture.md` | Threading, identity, mapping priority match the code | n/a |
| `docs/phase-3-validation.md` | BlueZ live env vars | Unquoted `AA:BB:…` example; still valid for that literal |
| `docs/README.md` | Index | README row still says “Phase 1 status” (stale) |

`AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>` appears only in historical
`docs/prompts/` (and this audit prompt as a negative example). Developer-facing
README and phase-4-validation use quoted real-format addresses.

`docs/phase-2-validation.md` uses unquoted `AA:BB:CC:DD:EE:FF`; that is valid
shell for this token. The angle-bracket placeholder form is the incorrect one
and is not in the current developer runbooks.

---

## 25. Defects and Findings

### BLOCKER

None.

### MAJOR

None.

### MINOR

1. **No unit test for `Audio/Duplex`.**
   - File: `src/audio/AudioEndpointClassifier.cpp` implements Duplex;
     `tests/unit/audio/tst_AudioEndpoints.cpp` has Sink/Source only.
   - Observed: Duplex is unproven by CTest.
   - Expected: a classifier test for `media.class=Audio/Duplex`.
   - Why it matters: required classification case in the Phase 4 matrix.
   - Correction: add a `tst_AudioEndpoints` slot analogous to `sinkBecomesPlayback`.

2. **Generic PipeWire start-failure error text.**
   - File: `src/audio/PipeWireManager.cpp` `initialize()` on `!started`.
   - Observed: lastError forced to `"PipeWire thread failed to start"`.
   - Expected: surface the specific `PipeWireConnection` error (context,
     connect, registry, loop).
   - Correction: pass through `connection_` last error or emit only once.

3. **Stale docs index line.**
   - File: `docs/README.md` describes `../README.md` as “Phase 1 status”.
   - Correction: say Phase 4 status.

### OBSERVATION

1. CTest reports opt-in live tests as Passed when they `QSKIP`. QtTest skip is
   visible only with `-V`. Setting `SKIP_RETURN_CODE` would make skips explicit
   without changing gating.

2. First live Bluetooth mapping used UniqueNameFallback; later graph events used
   PipeWireDeviceOwnership / ExactBluetoothAddress. Strong identity exists;
   WirePlumber loopback nodes may not carry `api.bluez5.address` on first
   `global` callback.

3. Transient extra playback endpoints during A2DP/loopback settle; registry
   converges when internals become `Audio/Sink/Internal`.

4. `endpointUpdated` is queued in the list model; insert/remove are direct.

5. No CMake sanitizer option (`AURALIS_ENABLE_SANITIZER` or similar). Not
   redesigned for this audit.

6. Interactive QML Connect/Disconnect clicks were not performed.

7. Five-cycle rapid connect/disconnect was not fully executed (one cycle).

---

## 26. Non-Blocking Improvements

- Add `Audio/Duplex` and UniqueNameFallback-success unit tests.
- Teach CTest to treat Qt `QSKIP` as skipped.
- Prefer waiting for node `info` (bound properties) before weak name mapping,
  so live path uses address/device.id more often.
- Flush/log shutdown on SIGTERM (desktop log ended without “shutting down”
  after `kill -TERM`; process did exit).
- Screenshot/accessibility hooks would make GUI audits less log-dependent.

---

## 27. Regression Matrix

| Phase | Area | Evidence | Result |
|---|---|---|---|
| 0 | clean build/toolchain | `build-audit`, gcc 15.2, CMake 4.2.3, Ninja | PASS |
| 1 | foundation/core | `tst_ServiceStatus`, `tst_Logger`, `tst_ConfigurationManager`, `tst_ApplicationCore` | PASS |
| 2 | Bluetooth discovery | `tst_AdapterAndDiscovery`, `tst_BluetoothManager`, `tst_DeviceRegistry`, live BlueZ scan | PASS |
| 3 | device management | `tst_DeviceLifecycle`, `tst_BlueZAgent`, `tst_ReconnectPolicy`, BlueZ live connect cycle | PASS |
| 3 | BlueZ live test registered | `ctest -N` test #21 | PASS |
| 3 | BlueZ live behavior | `BLUEZ_LIVE_EXIT_CODE=0` vs `88:08:94:9D:B4:22` | PASS |
| 4 | native PipeWire | source + `PkgConfig::PIPEWIRE` + live connect | PASS |
| 4 | endpoint classification | `tst_AudioEndpoints` + live 2 then 4 endpoints | PASS |
| 4 | endpoint registry | `tst_AudioEndpoints` registry slots + live remove/add | PASS |
| 4 | Bluetooth correlation | `tst_EndpointResolver` + live mappedBt=2 | PASS |
| 4 | live PipeWire | `PIPEWIRE_LIVE_EXIT_CODE=0` | PASS |

---

## 28. Phase 4 Acceptance Matrix

| Scenario | Evidence | Result |
|---|---|---|
| Audio/Sink → Playback | `tst_AudioEndpoints::sinkBecomesPlayback`; live node 56 | PASS |
| Audio/Source → Capture | `tst_AudioEndpoints::sourceBecomesCapture`; live node 57 | PASS |
| Audio/Duplex → Duplex | classifier only; no test | FAIL (coverage only; implementation present) |
| Application stream ignored | `applicationStreamIgnored`; live `Stream/Input/Audio/Internal` not in endpoints | PASS |
| Non-audio node ignored | `nonAudioIgnored`; live `Video/Source` | PASS |
| Built-in ALSA endpoint | `nonBluetoothAlsaHasNoBluetoothId`; live Built-in Analog Stereo | PASS |
| Exact Bluetooth address mapping | `TstEndpointResolver::exactAddress`; live transient `a2dp-sink` | PASS |
| BlueZ path mapping | `TstEndpointResolver::exactBlueZPath` | PASS |
| Node→PW Device→BT mapping | `TstEndpointResolver::pipeWireDeviceOwnership`; live after reconnect | PASS |
| Ambiguous name rejected | `TstEndpointResolver::ambiguousNameDoesNotGuess` | PASS |
| Multiple endpoints/device | `multipleEndpointsPerDevice`; live playback+capture | PASS |
| Node/global removal | `nodeRemovedBeforeBluetoothDisconnect`; desktop `EndpointRemoved` | PASS |
| BT-first event ordering | `bluetoothThenPipeWire` | PASS |
| PW-first event ordering | `pipeWireThenBluetooth` | PASS |
| PipeWire ID churn/reconnect | `reconnectChangesGlobalId`, `objectIdChurnKeepsLogicalIdentity` | PASS |
| Clean shutdown | `tst_PipeWireManager`; live test endpointCount=0 | PASS |
| Live PipeWire graph | `tst_PipeWireLiveIntegration` enabled | PASS |
| Live Bluetooth mapping | mapping run + desktop logs | PASS |

The Duplex row is a **test-coverage gap**, not a missing classifier. It is
classified MINOR and does not meet the audit prompt’s blocker list (build,
default tests, native PipeWire, registry, strong mapping, Phase 5 scope).

---

## 29. Commands Executed

| Command | Exit |
|---|---|
| `pwd`; `git status --short`; `git branch --show-current`; `git rev-parse HEAD`; `git log -1 --oneline` | 0 |
| Environment capture (`gcc`, `cmake`, `qmake6`, `bluetoothctl`, `pipewire`, pkg-config) | 0 |
| `systemctl --user status pipewire pipewire-pulse wireplumber`; `systemctl status bluetooth` | 0 |
| `wpctl status` | 0 |
| Source greps (native API, CLI, routing, threading) | 0 |
| `cmake -S . -B build-audit -G Ninja` | 0 |
| `cmake --build build-audit` | 0 |
| `ctest --test-dir build-audit -N` | 0 |
| `ctest --test-dir build-audit --output-on-failure` | 0 |
| `ctest … -R '^tst_BlueZLiveIntegration$' -V` (default skip) | 0 |
| `ctest … -R '^tst_PipeWireLiveIntegration$' -V` (default skip) | 0 |
| `bluetoothctl devices` / `devices Connected` / `devices Paired` / `info "88:08:94:9D:B4:22"` | 0 |
| `AURALIS_RUN_BLUETOOTH_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" ctest … tst_BlueZLiveIntegration` | 0 |
| `AURALIS_RUN_PIPEWIRE_INTEGRATION=1 ctest … tst_PipeWireLiveIntegration` | 0 |
| `bluetoothctl connect "88:08:94:9D:B4:22"` | 0 |
| Mapping `AURALIS_RUN_PIPEWIRE_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" ctest …` | 0 |
| `./build-audit/apps/desktop/auralis-desktop` | launched; later SIGTERM |
| `bluetoothctl disconnect` / reconnect during desktop run | disconnect 0; reconnect InProgress |
| Restore: `bluetoothctl disconnect "88:08:94:9D:B4:22"` (post-audit) | connected → no |

Sanitizer build: **NOT EXECUTED**. Reason: no established sanitizer CMake path
(`cmake/AuralisOptions.cmake` has tests/warnings/file-logging only).

`pw-dump` was not captured (optional; `wpctl` used for graph cross-check only).

---

## 30. Evidence Files

```text
audit-phase4/environment.txt
audit-phase4/git-status.txt
audit-phase4/runtime-services.txt
audit-phase4/cmake-configure.txt
audit-phase4/build.txt
audit-phase4/ctest-list.txt
audit-phase4/ctest-full.txt
audit-phase4/source-searches.txt
audit-phase4/wpctl-status.txt
audit-phase4/wpctl-status-after-connect.txt
audit-phase4/bluetooth-info.txt
audit-phase4/bluetooth-devices-list.txt
audit-phase4/bluetooth-connect.txt
audit-phase4/bluez-live.txt
audit-phase4/pipewire-live.txt
audit-phase4/pipewire-live-mapping.txt
audit-phase4/desktop-smoke.txt
```

No passwords, tokens, or secrets stored.

---

## 31. Conclusion

The repository at `abf8f19` implements Phase 4 as native PipeWire graph
observation plus Bluetooth correlation. A fresh configure/build succeeded.
Default tests passed without hardware. With Smokin' Buds present, Phase 3 live
BlueZ and Phase 4 live PipeWire mapping both passed. The desktop process
started, enumerated built-in and Bluetooth endpoints, and tracked disconnect
and reconnect on the live graph. Phase 5 routing is not present.

Remaining items are coverage and polish (Duplex unit test, CTest skip
reporting, first-snapshot mapping reason). They do not block the Phase 4
milestone under the stated completeness rules.

```text
PHASE 4: COMPLETE
```
