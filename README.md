# Auralis

Auralis is a Linux desktop application for eventually managing multiple Bluetooth/hearing audio devices and routing audio via BlueZ and PipeWire.

This repository currently contains **Phase 5**: Phase 1 foundation, Phase 2 BlueZ discovery, Phase 3 Bluetooth device management, Phase 4 PipeWire endpoint observation, and Phase 5 **additive native PipeWire routing**.

## Current status

```text
Phase 0: Complete
Phase 1: Implemented
Phase 2: Implemented
Phase 3: Implemented
Phase 4: Implemented
Phase 5: Implemented
Phase 6+: Not implemented
```

`Bluetooth Ready` on the status screen means the Bluetooth **discovery subsystem** initialized. It does **not** mean an adapter was found. Use the Bluetooth Discovery panel for BlueZ/adapter/scan state.

`PipeWire Connected` means Auralis attached to the user PipeWire server and is watching the registry. An **Active** route in the Audio Routing panel means Auralis-owned links exist for the current selection.

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

The **Audio Routing** panel selects a playback source and one or more playback endpoints, then Activate/Deactivate. Volume/mute appear when the destinations support `SPA_PROP_volume`.

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

Live audio routing tests are skipped unless explicitly enabled:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 ctest --test-dir build -R tst_AudioRoutingLiveIntegration --output-on-failure
```

Quote `AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22"` to prefer that mapped Bluetooth sink as the live destination.

## Architecture summary

| Module | Target | Current role |
|---|---|---|
| `auralis-core` | Lifecycle, logging, configuration, `ApplicationCore` | Implemented |
| `auralis-bluetooth` | BlueZ D-Bus discovery + device lifecycle (QtDBus / system bus) | Phase 3 |
| `auralis-audio` | Native PipeWire graph + additive routing (`AudioRouter`) | Phase 5 |
| `auralis-devices` | Future high-level device model | Lifecycle stub |
| `auralis-session` | Multi-device session engine (`SessionManager`) | Phase 6 |
| `auralis-ui` | QML resources | Status + discovery + device actions + endpoints + routing |
| `auralis-desktop` | Process entry point | Thin bootstrap |

QML uses `AppCore.bluetooth` for scan controls and the device list, and `AppCore.audio` (including `AppCore.audio.router`) for PipeWire status, endpoints, and routing. QML never talks D-Bus or native PipeWire.

See [docs/architecture.md](docs/architecture.md). The full documentation map is in [docs/README.md](docs/README.md).

## Documentation

| Location | Contents |
|---|---|
| [docs/README.md](docs/README.md) | Documentation hub |
| [docs/phase-6-session-engine.md](docs/phase-6-session-engine.md) | Session engine architecture, persistence, recovery, tests |
| [docs/validation/](docs/validation/README.md) | Phase exit gates and independent audits |
| [docs/specification/](docs/specification/README.md) | Product specification pack |
| [docs/roadmap/](docs/roadmap/README.md) | Detailed phased development plan |
| [docs/diagrams/](docs/diagrams/README.md) | Architecture diagrams |
| [docs/prompts/](docs/prompts/README.md) | Historical AI-IDE implementation prompts |

## Explicit non-features

The current application does **not**:

- destroy WirePlumber or other clients' PipeWire links (AdditiveRouting);
- hijack or replace WirePlumber session policy;
- put `pw_stream` in the production application (test binary only);
- call `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, `busctl`, `pw-cli`, or `pw-link`.

## License

Licensing terms have not yet been selected. See `LICENSE`.
