# Auralis

Auralis is a Linux desktop application for managing multiple Bluetooth/hearing audio devices and routing audio via BlueZ and PipeWire.

This repository currently contains **Phase 8**: reliability orchestration, service recovery, suspend/resume coordination, log rotation, automated failure-injection tests, and `.deb` packaging on top of Phases 0–7.

## Current status

```text
Phase 0: Complete
Phase 1: Implemented
Phase 2: Implemented
Phase 3: Implemented
Phase 4: Implemented
Phase 5: Implemented
Phase 6: Software complete (live two-device hardware opt-in)
Phase 7: Implemented (GUI + hardened file logging)
Phase 8: Software exit PASS (hardware validation pending)
```

`Bluetooth Ready` on the status screen means the Bluetooth **discovery subsystem** initialized. It does **not** mean an adapter was found. Use the Bluetooth Discovery panel for BlueZ/adapter/scan state.

`PipeWire Connected` means Auralis attached to the user PipeWire server and is watching the registry. An **Active** route in the Audio Routing panel means Auralis-owned links exist for the current selection.

Licensing terms are **not yet selected** (`LICENSE`). Local `.deb` builds are for engineering validation; public redistribution remains blocked until the owner approves a license.

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

Configure fails if Qt 6 DBus or `libpipewire-0.3` development files are missing.

Optional AddressSanitizer/UBSan build:

```bash
cmake -S . -B build-asan -G Ninja -DAURALIS_ENABLE_SANITIZERS=ON
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

## Package (`.deb`)

```bash
cmake -S . -B build -G Ninja
cmake --build build
cd build && cpack -G DEB
```

Install validation (no root required for extract/launch smoke):

```bash
dpkg-deb -I build/auralis_0.1.0_amd64.deb
dpkg-deb -c build/auralis_0.1.0_amd64.deb
mkdir -p /tmp/auralis-prefix && dpkg-deb -x build/auralis_0.1.0_amd64.deb /tmp/auralis-prefix
timeout 3 env QT_QPA_PLATFORM=offscreen /tmp/auralis-prefix/usr/bin/auralis-desktop
```

Runtime does not require root. System install via `sudo dpkg -i` is optional.

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

Live session tests are skipped unless explicitly enabled:

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure
```

Two-device membership smoke (addresses only; does not require both sinks to be playing):

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="AA:BB:CC:DD:EE:FF;11:22:33:44:55:66" \
ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure
```

## Architecture summary

| Module | Target | Current role |
|---|---|---|
| `auralis-core` | Lifecycle, logging, configuration, `ApplicationCore` | Implemented |
| `auralis-bluetooth` | BlueZ D-Bus discovery + device lifecycle (QtDBus / system bus) | Phase 3 |
| `auralis-audio` | Native PipeWire graph + additive routing (`AudioRouter`) | Phase 5 |
| `auralis-devices` | Future high-level device model | Lifecycle stub |
| `auralis-session` | Multi-device session engine (`SessionManager`) | Phase 6 |
| `auralis-recovery` | Service recovery orchestration + logind power monitor | Phase 8 |
| `auralis-ui` | QML resources | Status + discovery + device actions + endpoints + routing |
| `auralis-desktop` | Process entry point | Thin bootstrap |

QML uses `AppCore.bluetooth` for scan controls and the device list, and `AppCore.audio` (including `AppCore.audio.router`) for PipeWire status, endpoints, and routing. QML never talks D-Bus or native PipeWire.

See [docs/architecture/overview.md](docs/architecture/overview.md). The full documentation map is in [docs/README.md](docs/README.md).

## Documentation

| Location | Contents |
|---|---|
| [docs/README.md](docs/README.md) | Documentation hub |
| [docs/architecture/](docs/architecture/README.md) | As-built architecture (overview + session engine) |
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
