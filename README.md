# Auralis

Auralis is a cross-platform desktop application for managing multiple Bluetooth/hearing audio devices and routing one audio source to multiple outputs. Linux uses BlueZ and PipeWire; Windows and macOS use Qt's native Bluetooth and multimedia integrations.

The Qt/QML UI, device model, session persistence, route planner, volume controls, recovery policy, and diagnostics are shared on Linux, Windows, and macOS. Platform selection occurs only in the desktop composition and backend build targets.

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

`Bluetooth Ready` on the status screen means the Bluetooth **discovery subsystem** initialized. It does **not** mean an adapter was found. Use the Bluetooth Discovery panel for adapter and scan state.

`Audio Connected` means Auralis attached to the selected platform audio backend and completed endpoint discovery. An **Active** route means Auralis owns the links or native streams for the current selection.

Licensing terms are **not yet selected** (`LICENSE`). Package maintainer contact is also pending. Local packages are for engineering validation; public redistribution remains blocked until the owner approves a license and contact metadata.

## Requirements

Linux reference baseline:

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

Windows and macOS builds require Qt 6.8 or newer with Bluetooth (`qtconnectivity`) and Multimedia. Windows specifically requires the **MSVC 2022 64-bit Qt kit and compiler**: Qt 6 does not provide a functional Windows Bluetooth backend in its MinGW builds. CMake rejects Windows+MinGW so a build cannot silently fall back to Qt's dummy Bluetooth backend. Linux additionally requires Qt DBus and the `libpipewire-0.3` development package. The cross-platform CI workflow builds and tests all three desktop families.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

On Windows, run these commands from an **x64 Native Tools Command Prompt for VS 2022** (or another shell initialized with `vcvars64.bat`) and point CMake at the MSVC Qt kit when it is not already discoverable:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
cmake --build build
ctest --test-dir build --output-on-failure
```

Do not configure the Windows application against `mingw_64`; that Qt build uses a non-functional Bluetooth dummy backend.

On Linux, configure fails if Qt DBus or `libpipewire-0.3` development files are missing. Windows and macOS do not compile or link either Linux dependency.

## Run

```bash
./build/apps/desktop/auralis-desktop       # Linux/macOS build tree
# build\\apps\\desktop\\auralis-desktop.exe  # Windows build tree
```

If no display server is available:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

In the Bluetooth Discovery panel: **Start Scan**, **Stop Scan**, **Refresh**, and per-device **Pair / Trust / Connect / Disconnect / Reconnect / Forget** actions. Linux pairing prompts are handled by the BlueZ Agent1 integration; Windows and macOS use the operating system's pairing UI and policy. Each device row also has **Device buttons: ALLOW | DISALLOW** (default ALLOW). DISALLOW suppresses that device’s consumer-control/media keys via an exclusive evdev grab when Linux exposes an input node matched to the Bluetooth address; otherwise the UI reports that button control is unsupported on the transport. Audio (A2DP) and routing are unaffected. See `docs/BLUETOOTH_BUTTON_EVENT_PATH.md`. If `/dev/input` open fails, grant session access with a udev `uaccess` rule rather than running as root.

The **Audio Endpoints** list shows platform playback devices. Bluetooth rows distinguish **Connected** from **Audio: Available / Initializing...**.

The **Audio Routing** panel selects a source and one or more playback endpoints, then Activate/Deactivate. Linux routes playback streams through PipeWire. Windows/macOS fan out a selected native capture source through Qt Multimedia over WASAPI/Core Audio.

Optional AddressSanitizer/UBSan build:

```bash
cmake -S . -B build-asan -G Ninja -DAURALIS_ENABLE_SANITIZERS=ON
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

## Package

```bash
cmake -S . -B build -G Ninja
cmake --build build
cd build && cpack
```

The default artifact is a DEB on Linux, an NSIS installer plus ZIP on Windows when NSIS is installed (otherwise ZIP), and a DMG on macOS.

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
| `auralis-bluetooth` | BlueZ D-Bus on Linux; native Qt Bluetooth on Windows/macOS | Platform-selected |
| `auralis-audio` | PipeWire on Linux; Qt Multimedia over WASAPI/Core Audio on Windows/macOS | Platform-selected |
| `auralis-devices` | Future high-level device model | Lifecycle stub |
| `auralis-session` | Multi-device session engine (`SessionManager`) | Phase 6 |
| `auralis-recovery` | Shared recovery + logind/Windows/NSWorkspace power monitoring | Phase 8 |
| `auralis-ui` | QML resources | Status + discovery + device actions + endpoints + routing |
| `auralis-desktop` | Process entry point | Thin bootstrap |

QML uses `AppCore.bluetooth` for scan controls and the device list, and `AppCore.audio` (including `AppCore.audio.router`) for status, endpoints, and routing. QML never calls D-Bus, PipeWire, WASAPI, or Core Audio directly.

See [docs/architecture/overview.md](docs/architecture/overview.md) and [docs/architecture/cross-platform-backends.md](docs/architecture/cross-platform-backends.md). The full documentation map is in [docs/README.md](docs/README.md).

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
