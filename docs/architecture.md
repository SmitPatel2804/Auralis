# Auralis Architecture — Phase 4

This is the **as-built** design of the current tree. Product-intent architecture, including unimplemented routing and sessions, lives in [specification/](specification/README.md). Phase exit gates are in [validation/](validation/README.md).

## Target architecture

```text
+--------------------------------------------------+
|                  AURALIS DESKTOP                 |
|                  Qt / QML UI                     |
+-------------------------+------------------------+
                          |
                          v
+--------------------------------------------------+
|                Application Services              |
|                                                  |
| DeviceManager | AudioRouter | SessionManager     |
+----------------------+---------------------------+
                       |
          +------------+-------------+
          |                          |
          v                          v
+-------------------+      +-----------------------+
| BluetoothManager  |      | PipeWireManager       |
| BlueZ / D-Bus     |      | Graph observation     |
+---------+---------+      +-----------+-----------+
          |                            |
          v                            v
+-------------------+      +-----------------------+
| BlueZ             |      | PipeWire              |
+---------+---------+      +-----------+-----------+
```

Phase 4 observes the PipeWire graph, classifies playback/capture endpoints, and correlates Bluetooth audio nodes with the Phase 3 `DeviceRegistry`. Routing, link creation, DeviceManager, and sessions remain unimplemented.

## Bluetooth stack

```text
QML
  -> AppCore.bluetooth (BluetoothManager)
       -> DeviceRegistry + BluetoothDeviceListModel
       -> DeviceLifecycleManager + ReconnectPolicy
       -> BlueZAgent (Agent1 at /auralis/agent)
       -> AdapterManager + DiscoveryManager
       -> IBlueZClient
            -> BlueZDbusClient (QDBusConnection::systemBus)
            -> FakeBlueZClient (tests)
                 -> org.bluez
```

- Production talks to `org.bluez` only through QtDBus. There is no `bluetoothctl` backend.
- BlueZ properties (`Paired`, `Trusted`, `Connected`, `UUIDs`, …) are authoritative for Bluetooth state.
- A Bluetooth device and an `AudioEndpoint` are different domain objects.

## PipeWire stack

```text
QML
  -> AppCore.audio (PipeWireManager)
       -> AudioEndpointListModel
       -> AudioEndpointRegistry
       -> EndpointResolver  --non-owning--> DeviceRegistry
       -> PipeWireObjectStore
       -> PipeWireConnection (pw_thread_loop)
            -> pw_context / pw_core / pw_registry
```

### Threading

- PipeWire callbacks run on `pw_thread_loop` with the loop lock held.
- Callbacks copy `id` / interface / `spa_dict` into value snapshots and return.
- `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` delivers events to the Qt thread that owns `PipeWireManager`.
- Shutdown bumps a generation token, stops the thread loop **without** holding the lock, then drains posted Qt events so callbacks cannot mutate destroyed objects.

### Endpoint identity

PipeWire global IDs are runtime-scoped. Logical endpoint IDs are:

- Bluetooth: `bt:{normalizedAddress}:{direction}:{profileOrNodeName}`
- Other: `pw:{serial|nodeName}:{direction}:{profileOrNodeName}`

When a node is removed from the graph, the endpoint is **removed** from the current registry (not left Available).

### Bluetooth mapping priority

1. Normalized `api.bluez5.address`
2. Exact `api.bluez5.path` / `api.bluez5.device` vs BlueZ `objectPath`
3. Node `device.id` → PipeWire Device that maps by address/path
4. Unique exact name/alias fallback only if a single Phase 3 device matches

Ambiguous weak matches stay unresolved. The endpoint still exists.

BlueZ `Connected=true` does not fabricate endpoint availability. The endpoint appears only after PipeWire exposes a classified node.

## Module responsibilities

| Module | Responsibility |
|---|---|
| `auralis-core` | `ServiceStatus`, `Logger`, `ConfigurationManager`, `ApplicationCore` |
| `auralis-bluetooth` | BlueZ discovery + lifecycle: client, registry, model, agent, reconnect |
| `auralis-audio` | Native PipeWire observation, endpoint registry, Bluetooth correlation |
| `auralis-devices` | Future high-level device state. Not equal to a BlueZ Device1 object. |
| `auralis-session` | Future multi-device session orchestration. |
| `auralis-ui` | QML module (`Auralis.Ui`) with status, devices, pairing, audio endpoints. |
| `auralis-desktop` | Process bootstrap. |

`auralis-bluetooth` is the only target that links `Qt6::DBus`. `auralis-audio` links `libpipewire-0.3` and may observe `DeviceRegistry` (Bluetooth does not depend on audio).

## Dependency direction

```text
QML/UI
  |
  v
Desktop App / Presentation Bridge
  |
  v
ApplicationCore
  |
  +--> DeviceManager (stub)
  +--> SessionManager (stub)
  +--> BluetoothManager (IBluetoothManager + QObject uiObject)
  +--> PipeWireManager (IPipeWireManager + QObject uiObject)
  +--> ConfigurationManager
  +--> Logger
```

PipeWire initialize failure is non-fatal: ApplicationCore can still be Ready while `AppCore.audio` shows Error/Stopped.

## QML backend exposure

```cpp
qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);
```

QML uses `AppCore.bluetooth` for scan/lifecycle and `AppCore.audio` for connection state and the endpoint list. Device id for Bluetooth invokables is the model `objectPath` role. QML never talks D-Bus or native PipeWire.

## Phase 3 operations

| Operation | BlueZ surface |
|---|---|
| Pair / CancelPairing | `Device1.Pair`, `Device1.CancelPairing` |
| Trust / Untrust | `Properties.Set(Device1, Trusted, bool)` |
| Connect / Disconnect | `Device1.Connect`, `Device1.Disconnect` |
| Forget | `Adapter1.RemoveDevice` |
| Agent | `AgentManager1.RegisterAgent`, exported `Agent1` |

Auto-reconnect is bounded via `ReconnectPolicy` and suppressed after explicit disconnect/forget.

See [Phase 3 validation](validation/phase-3.md) and [Phase 4 validation](validation/phase-4.md).
