# Auralis

Auralis is a Linux desktop foundation for eventually managing multiple Bluetooth/hearing audio devices and routing audio via BlueZ and PipeWire.

This repository currently contains the **Phase 1 project foundation**: a layered C++20 / Qt 6 application shell with logging, configuration, service skeletons, a minimal QML status UI, and hardware-independent tests. It does not yet talk to Bluetooth hardware or the PipeWire graph.

## Current status

```text
Phase 0: Complete
Phase 1: Implemented
Phase 2+: Not implemented
```

`Ready` on the Phase 1 status screen means a **service skeleton object initialized successfully**. It does **not** mean a Bluetooth adapter was found, BlueZ was queried, or a live PipeWire graph was inspected.

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
| Qt | 6.10.2 |
| BlueZ | 5.85 |
| PipeWire | 1.6.2 |

Phase 1 itself only requires a C++20 toolchain, CMake, Ninja, and Qt 6 (Core, Gui, Qml, Quick, Test). It does not require Bluetooth hardware.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

```bash
./build/apps/desktop/auralis-desktop
```

If no display server is available:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

The QML/backend smoke test uses `QT_QPA_PLATFORM=offscreen` so it can run without a window server.

## Architecture summary

| Module | Target | Phase 1 role |
|---|---|---|
| `auralis-core` | Lifecycle, logging, configuration, `ApplicationCore` | Implemented |
| `auralis-bluetooth` | Future BlueZ D-Bus manager | Lifecycle stub |
| `auralis-audio` | Future PipeWire manager | Lifecycle stub |
| `auralis-devices` | Future high-level device model | Lifecycle stub |
| `auralis-session` | Future multi-device sessions | Lifecycle stub |
| `auralis-ui` | QML resources | Minimal status shell |
| `auralis-desktop` | Process entry point | Thin bootstrap |

QML never owns service lifetimes. `ApplicationCore` is exposed to QML as the `AppCore` singleton (`qmlRegisterSingletonInstance`). Service implementations are injected so tests can substitute fakes.

See [docs/architecture.md](docs/architecture.md) for dependency direction and lifecycle ownership.

## Documentation

All project documentation is under [docs/](docs/README.md):

| Location | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Phase 1 modules, lifecycle, QML exposure |
| [docs/phase-1-validation.md](docs/phase-1-validation.md) | Phase 1 clean-room validation checklist |
| [docs/specification/](docs/specification/README.md) | Product specification pack |
| [docs/roadmap/](docs/roadmap/) | Detailed phased development plan |
| [docs/diagrams/](docs/diagrams/) | Architecture diagrams |
| [docs/prompts/](docs/prompts/) | Historical AI-IDE implementation prompts |

## Explicit non-features

The current foundation does **not**:

- scan, pair, connect, or disconnect Bluetooth devices;
- enumerate PipeWire nodes, ports, or links;
- route or duplicate audio;
- create multi-device sessions;
- call `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, `busctl`, or `pw-cli`.

## License

Licensing terms have not yet been selected. See `LICENSE`.
