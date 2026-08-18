# Auralis

Auralis is a Linux desktop application for eventually managing multiple Bluetooth/hearing audio devices and routing audio via BlueZ and PipeWire.

This repository currently contains **Phase 4**: Phase 1 foundation, Phase 2 BlueZ discovery, Phase 3 Bluetooth device management, and Phase 4 **PipeWire endpoint observation** with Bluetooth correlation. Audio routing is not implemented.

## Current status

```text
Phase 0: Complete
Phase 1: Implemented
Phase 2: Implemented
Phase 3: Implemented
Phase 4: Implemented
Phase 5+: Not implemented
```

`Bluetooth Ready` on the status screen means the Bluetooth **discovery subsystem** initialized. It does **not** mean an adapter was found. Use the Bluetooth Discovery panel for BlueZ/adapter/scan state.

`PipeWire Connected` means Auralis attached to the user PipeWire server and is watching the registry. It does **not** mean a route exists.

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

Configure fails if Qt 6 DBus or `libpipewire-0.3` development files are missing.

## Run

```bash
./build/apps/desktop/auralis-desktop
```

If no display server is available:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

In the Bluetooth Discovery panel: **Start Scan**, **Stop Scan**, **Refresh**, and per-device **Pair / Trust / Connect / Disconnect / Reconnect / Forget** actions. Pairing prompts appear when BlueZ invokes the exported Agent1.

The **Audio Endpoints** list shows classified PipeWire sinks/sources. Bluetooth rows distinguish **Connected** from **Audio: Available / Initializing...**.

## Test

```bash
ctest --test-dir build --output-on-failure
```

Live BlueZ tests are skipped unless explicitly enabled:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

Destructive forget in live tests requires `AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1` in addition to the flags above.

If that env var is set and BlueZ/system bus is missing, the live test fails with a clear message.

Live PipeWire tests are skipped unless explicitly enabled:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 ctest --test-dir build -R tst_PipeWireLiveIntegration --output-on-failure
```

Add `AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"` to also require a mapped Bluetooth audio endpoint.

## Architecture summary

| Module | Target | Current role |
|---|---|---|
| `auralis-core` | Lifecycle, logging, configuration, `ApplicationCore` | Implemented |
| `auralis-bluetooth` | BlueZ D-Bus discovery + device lifecycle (QtDBus / system bus) | Phase 3 |
| `auralis-audio` | Native PipeWire graph observation + endpoint registry | Phase 4 |
| `auralis-devices` | Future high-level device model | Lifecycle stub |
| `auralis-session` | Future multi-device sessions | Lifecycle stub |
| `auralis-ui` | QML resources | Status + discovery + device actions + endpoints |
| `auralis-desktop` | Process entry point | Thin bootstrap |

QML uses `AppCore.bluetooth` for scan controls and the device list, and `AppCore.audio` for PipeWire status and endpoints. QML never talks D-Bus or native PipeWire.

See [docs/architecture.md](docs/architecture.md).

## Documentation

| Location | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Modules, BlueZ, PipeWire observation, QML exposure |
| [docs/phase-1-validation.md](docs/phase-1-validation.md) | Phase 1 clean-room checklist |
| [docs/phase-2-validation.md](docs/phase-2-validation.md) | Phase 2 discovery validation |
| [docs/phase-3-validation.md](docs/phase-3-validation.md) | Phase 3 device management validation |
| [docs/phase-4-validation.md](docs/phase-4-validation.md) | Phase 4 PipeWire endpoint validation |
| [docs/specification/](docs/specification/README.md) | Product specification pack |
| [docs/roadmap/](docs/roadmap/) | Detailed phased development plan |
| [docs/diagrams/](docs/diagrams/) | Architecture diagrams |
| [docs/prompts/](docs/prompts/) | Historical AI-IDE implementation prompts |

## Explicit non-features

The current application does **not**:

- route or duplicate audio;
- create PipeWire links;
- create multi-device sessions;
- call `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, `busctl`, or `pw-cli`.

## License

Licensing terms have not yet been selected. See `LICENSE`.
