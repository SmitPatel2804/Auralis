# Cross-platform desktop backends

Auralis keeps one application model and chooses only the operating-system boundary at build time. The supported desktop families are Linux, Windows, and macOS.

## Stable contracts

- `IBluetoothManager` owns discovery, device state, pairing actions, and managed reconnect.
- `IAudioManager` owns endpoint discovery, route access, connection health, and reconnect.
- `SessionManager`, `RecoveryManager`, `ApplicationCore`, and all QML consume those contracts and do not select an operating system.
- The legacy `IPipeWireManager` name remains as an alias so existing tests and downstream code do not break.

## Selected implementations

- Linux: `BluetoothManager` uses BlueZ through Qt DBus, `PipeWireManager` uses native PipeWire, and `SystemPowerMonitor.cpp` observes systemd-logind.
- Windows: `NativeBluetoothManager` uses Qt Bluetooth over the Windows backend, `NativeAudioManager` uses Qt Multimedia over WASAPI, and `SystemPowerMonitor_windows.cpp` observes `WM_POWERBROADCAST`. The Windows build requires Qt's MSVC 2022 kit because Qt 6's MinGW Bluetooth module contains only a dummy backend; CMake rejects that non-functional combination.
- macOS: `NativeBluetoothManager` uses Qt Bluetooth over the Apple backend, `NativeAudioManager` uses Qt Multimedia over Core Audio, and `SystemPowerMonitor_macos.mm` observes `NSWorkspace` sleep/wake notifications.

The native audio manager projects host inputs, outputs, and owned streams into the existing graph types. This lets `AudioRouter`, route persistence, per-destination volume/mute, session recovery, diagnostics, and QML stay unchanged. A route uses a PCM format supported by its capture source and every selected output, then fans captured frames out to each owned output stream.

## Platform policy differences

Bluetooth pairing and profile connection policy is ultimately controlled by the host operating system. Windows and macOS may display system confirmation UI or require the user to finish a profile action in Bluetooth settings. Auralis reports that state as an explicit operation error; it never fabricates a connected device.

The native Qt audio route captures a selected input device. It does not claim process-level system-playback capture when the operating system has not exposed such a capture endpoint. Linux retains its existing PipeWire playback-stream routing unchanged. WASAPI loopback and macOS system-output taps remain hardware-gated enhancements if exact system-mix capture is required on those platforms.

## Build and package gates

Linux alone compiles the BlueZ, Qt DBus, and PipeWire sources. Windows and macOS compile the Qt Bluetooth/Multimedia sources and have no DBus or PipeWire dependency. CPack selects DEB, NSIS/ZIP, or DragNDrop respectively. The macOS bundle declares Bluetooth and audio-capture privacy descriptions.

The workflow in `.github/workflows/cross-platform.yml` configures, builds, tests, and packages each desktop family, with the Windows job pinned to `win64_msvc2022_64`. Real Bluetooth hardware, multi-output audio, suspend/resume, and long-duration clock-drift checks still require actual-machine validation on each target OS.
