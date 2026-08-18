# Auralis Architecture — Phase 2

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

Phase 2 implements Bluetooth discovery over the Linux **system D-Bus**. PipeWire, high-level DeviceManager, and sessions remain Phase 1 stubs.

## Bluetooth discovery

```text
QML
  -> AppCore.bluetooth (BluetoothManager)
       -> DeviceRegistry + BluetoothDeviceListModel
       -> AdapterManager + DiscoveryManager
       -> IBlueZClient
            -> BlueZDbusClient (QDBusConnection::systemBus)
            -> FakeBlueZClient (tests)
                 -> org.bluez
```

- Production talks to `org.bluez` only through QtDBus. There is no `bluetoothctl` backend.
- `GetManagedObjects` bootstraps adapters/devices; `InterfacesAdded` / `InterfacesRemoved` / `PropertiesChanged` keep state live.
- Signal subscriptions are established before snapshot reconciliation so devices are not missed.
- Device identity is the BlueZ object path. Name is never used as identity. LE random addresses are not durable across restarts.
- `scanning` means Auralis owns a `StartDiscovery` session. Adapter1 `Discovering` is also exposed because other clients may scan.
- Phase 2 does not set Adapter1 `Powered` and does not apply a discovery UUID filter.

## Module responsibilities

| Module | Responsibility |
|---|---|
| `auralis-core` | `ServiceStatus`, `Logger`, `ConfigurationManager`, `ApplicationCore` |
| `auralis-bluetooth` | BlueZ discovery: client, adapters, registry, model, scan lifecycle |
| `auralis-audio` | Future PipeWire data/control plane. Phase 1 stub. |
| `auralis-devices` | Future high-level Auralis device state. Not equal to a BlueZ Device1 object. |
| `auralis-session` | Future multi-device session orchestration. |
| `auralis-ui` | QML module (`Auralis.Ui`) with status + discovery UI. |
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
  +--> DeviceManager
  +--> SessionManager
  +--> BluetoothManager (IBluetoothManager + QObject uiObject)
  +--> PipeWireManager interface/stub
  +--> ConfigurationManager
  +--> Logger
```

`ApplicationCore` still depends only on `IBluetoothManager`. Production `BluetoothManager` is a `QObject` exposed as `AppCore.bluetooth`. Fakes return `uiObject() == nullptr`.

## Lifecycle ownership

Startup order is unchanged. `BluetoothManager::initialize()` returns true even if BlueZ is missing so the desktop still launches. Unavailability is reported through `available` / `statusText`.

## QML backend exposure

```cpp
qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);
```

QML uses `AppCore.bluetooth.startScan()`, `stopScan()`, `refresh()`, and `AppCore.bluetooth.devices`. QML does not own the manager.

## Testing seams

`IBlueZClient` is injected into `BluetoothManager`. Unit tests use `FakeBlueZClient`. Live tests require `AURALIS_RUN_BLUETOOTH_INTEGRATION=1`.

## No shell-command production rule

Production code must not execute or parse `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, `busctl`, or `pw-cli`. Bluetooth discovery uses BlueZ D-Bus APIs. Future audio integration uses the native PipeWire API where practical.
