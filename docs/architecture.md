# Auralis Architecture — Phase 1

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

Phase 1 implements only the application skeleton and contracts. Bluetooth, PipeWire, device, and session modules are lifecycle stubs.

## Module responsibilities

| Module | Responsibility |
|---|---|
| `auralis-core` | `ServiceStatus`, `Logger`, `ConfigurationManager`, `ApplicationCore` |
| `auralis-bluetooth` | Future BlueZ control plane. Phase 1: initialize/shutdown/status only. |
| `auralis-audio` | Future PipeWire data/control plane. Phase 1: initialize/shutdown/status only. |
| `auralis-devices` | Future high-level Auralis device state. Not equal to a Bluetooth device or audio endpoint. |
| `auralis-session` | Future multi-device session orchestration. |
| `auralis-ui` | QML module (`Auralis.Ui`) with the Phase 1 status shell. |
| `auralis-desktop` | Process bootstrap. Creates production services and loads QML. |

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
  +--> BluetoothManager interface/stub
  +--> PipeWireManager interface/stub
  +--> ConfigurationManager
  +--> Logger
```

CMake target graph:

```text
auralis-core
   ^
   |
   +----------------+----------------+----------------+
   |                |                |                |
auralis-bluetooth auralis-audio auralis-devices auralis-session
   \                |                |               /
    \_______________|________________|______________/
                         |
                         v
                  auralis-desktop
                         |
                         +--> auralis-ui
```

Backend libraries do not depend on the UI. There are no circular CMake target dependencies.

`ApplicationCore` lives in `auralis-core` and depends only on **interfaces**. Production stub construction happens in `apps/desktop` so `auralis-core` does not link the Bluetooth/audio libraries.

## Lifecycle ownership

Startup order:

```text
Logger
  -> ConfigurationManager
  -> BluetoothManager stub
  -> PipeWireManager stub
  -> DeviceManager stub
  -> SessionManager stub
  -> ApplicationCore Ready
  -> QML root load
```

Shutdown is the reverse, with the logger last. Repeated `shutdown()` is safe.

If a required subsystem fails, `ApplicationCore` enters `Error`, logs the failing subsystem, and shuts down any services that already initialized.

## Why BlueZ and PipeWire are stubs

Phase 1 must be testable without Bluetooth hardware, `org.bluez`, or a live PipeWire graph.

- `Bluetooth Ready` means the `BluetoothManager` object initialized. It does not mean an adapter was found.
- `PipeWire Ready` means the `PipeWireManager` object initialized. It does not mean a graph was inspected.
- `audioStatus` currently mirrors PipeWire skeleton readiness.

Real BlueZ D-Bus work begins in Phase 2. Native PipeWire registry work begins in Phase 4.

## QML backend exposure

The desktop bootstrap registers `ApplicationCore` with:

```cpp
qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);
```

This keeps a stable `AppCore` type for later UI growth without giving QML ownership of service lifetimes.

QML resources are packaged with `qt_add_qml_module()` under URI `Auralis.Ui` so the executable does not depend on the working directory.

## Testing seams

`ApplicationCore` takes an `ApplicationServices` struct of `std::unique_ptr` interfaces. Unit tests inject fakes that can succeed or fail initialization. Tests use isolated `QSettings` files and never write the developer's real Auralis configuration.

## No shell-command production rule

Production code must not execute or parse `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, `busctl`, or `pw-cli`. Future Bluetooth integration uses BlueZ D-Bus APIs. Future audio integration uses the native PipeWire API where practical.
