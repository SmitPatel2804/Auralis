# Auralis

Auralis is a Linux desktop application for eventually managing multiple Bluetooth/hearing audio devices and routing audio via BlueZ and PipeWire.

This repository currently contains **Phase 3**: the Phase 1 foundation, Phase 2 BlueZ discovery, and Phase 3 Bluetooth **device management** (pair/trust/connect/disconnect/forget/reconnect + Agent1).

## Current status

```text
Phase 0: Complete
Phase 1: Implemented
Phase 2: Implemented
Phase 3: Implemented
Phase 4+: Not implemented
```

`Bluetooth Ready` on the status screen means the Bluetooth **discovery subsystem** initialized. It does **not** mean an adapter was found. Use the Bluetooth Discovery panel for BlueZ/adapter/scan state.

## Requirements

Validated development baseline:

| Component | Baseline |
|---|---|
| Distribution | Ubuntu 26.04 LTS (`resolute`) |
| Kernel | Linux 7.0.0-29-generic |
| Architecture | x86_64 |
| GCC / G++ | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Qt | 6.10.2 (including QtDBus) |
| BlueZ | 5.85 |
| PipeWire | 1.6.2 |

Hardware-independent tests do not require a Bluetooth adapter. The desktop scan UI does.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Configure fails if Qt 6 DBus is missing.

## Run

```bash
./build/apps/desktop/auralis-desktop
```

If no display server is available:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

In the Bluetooth Discovery panel: **Start Scan**, **Stop Scan**, **Refresh**, and per-device **Pair / Trust / Connect / Disconnect / Reconnect / Forget** actions. Pairing prompts appear when BlueZ invokes the exported Agent1.

## Test

```bash
ctest --test-dir build --output-on-failure
```

Live BlueZ tests are skipped unless explicitly enabled:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

Destructive forget in live tests requires `AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1` in addition to the flags above.

If that env var is set and BlueZ/system bus is missing, the live test fails with a clear message.

## Architecture summary

| Module | Target | Current role |
|---|---|---|
| `auralis-core` | Lifecycle, logging, configuration, `ApplicationCore` | Implemented |
| `auralis-bluetooth` | BlueZ D-Bus discovery + device lifecycle (QtDBus / system bus) | Phase 3 |
| `auralis-audio` | Future PipeWire manager | Lifecycle stub |
| `auralis-devices` | Future high-level device model | Lifecycle stub |
| `auralis-session` | Future multi-device sessions | Lifecycle stub |
| `auralis-ui` | QML resources | Status + discovery + device actions |
| `auralis-desktop` | Process entry point | Thin bootstrap |

QML uses `AppCore.bluetooth` for scan controls and the device list. QML never talks D-Bus.

See [docs/architecture.md](docs/architecture.md).

## Documentation

| Location | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Modules, BlueZ discovery, QML exposure |
| [docs/phase-1-validation.md](docs/phase-1-validation.md) | Phase 1 clean-room checklist |
| [docs/phase-2-validation.md](docs/phase-2-validation.md) | Phase 2 discovery validation |
| [docs/phase-3-validation.md](docs/phase-3-validation.md) | Phase 3 device management validation |
| [docs/specification/](docs/specification/README.md) | Product specification pack |
| [docs/roadmap/](docs/roadmap/) | Detailed phased development plan |
| [docs/diagrams/](docs/diagrams/) | Architecture diagrams |
| [docs/prompts/](docs/prompts/) | Historical AI-IDE implementation prompts |

## Explicit non-features

The current application does **not**:

- enumerate PipeWire nodes, ports, or links;
- route or duplicate audio;
- create multi-device sessions;
- call `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, `busctl`, or `pw-cli`.

## License

Licensing terms have not yet been selected. See `LICENSE`.
