# Auralis Architecture — Phase 3

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
| BlueZ / D-Bus     |      | Audio Graph Control   |
+---------+---------+      +-----------+-----------+
          |                            |
          v                            v
+-------------------+      +-----------------------+
| BlueZ             |      | PipeWire              |
+---------+---------+      +-----------+-----------+
```

Phase 3 extends Bluetooth with device lifecycle operations and an exported `org.bluez.Agent1`. PipeWire, high-level DeviceManager, and sessions remain stubs.

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
- BlueZ properties (`Paired`, `Trusted`, `Connected`, `UUIDs`, …) are authoritative.
- Auralis tracks transient `DeviceOperation` state only; UI logical state is derived.
- Per-device generation tokens drop stale D-Bus callbacks after forget/removal.
- Agent replies are async via delayed D-Bus replies; QML uses `pendingPairingRequest`.

## Module responsibilities

| Module | Responsibility |
|---|---|
| `auralis-core` | `ServiceStatus`, `Logger`, `ConfigurationManager`, `ApplicationCore` |
| `auralis-bluetooth` | BlueZ discovery + lifecycle: client, registry, model, agent, reconnect |
| `auralis-audio` | Future PipeWire data/control plane. Phase 1 stub. |
| `auralis-devices` | Future high-level Auralis device state. Not equal to a BlueZ Device1 object. |
| `auralis-session` | Future multi-device session orchestration. |
| `auralis-ui` | QML module (`Auralis.Ui`) with status, discovery, device actions, pairing prompt. |
| `auralis-desktop` | Process bootstrap. |

`auralis-bluetooth` is the only target that links `Qt6::DBus`.

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
  +--> PipeWireManager interface/stub
  +--> ConfigurationManager
  +--> Logger
```

## QML backend exposure

```cpp
qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);
```

QML uses `AppCore.bluetooth` for scan controls, the device list, lifecycle invokables, and `pendingPairingRequest`. Device id for invokables is the model `objectPath` role. QML does not own the manager and never talks D-Bus.

## Phase 3 operations

| Operation | BlueZ surface |
|---|---|
| Pair / CancelPairing | `Device1.Pair`, `Device1.CancelPairing` |
| Trust / Untrust | `Properties.Set(Device1, Trusted, bool)` |
| Connect / Disconnect | `Device1.Connect`, `Device1.Disconnect` |
| Forget | `Adapter1.RemoveDevice` |
| Agent | `AgentManager1.RegisterAgent`, exported `Agent1` |

Auto-reconnect is bounded via `ReconnectPolicy` and suppressed after explicit disconnect/forget.

See [docs/phase-3-validation.md](phase-3-validation.md) for validation steps and environment variables.
