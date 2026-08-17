# 3. Technology Stack

## 3.1 Selected laptop prototype stack

| Layer / Area | Technology | Role | Status |
|---|---|---|---|
| Target OS | Linux | First engineering platform | Proposed baseline |
| Core language | C++20 | Audio engine, routing, sync, device/session logic | Required |
| Build system | CMake | Cross-platform project configuration | Required |
| Build tool | Ninja | Fast builds | Recommended |
| UI framework | Qt 6 | Desktop framework | GUI phase |
| UI | QML / Qt Quick | Device and playback UI | GUI phase |
| Bluetooth stack | BlueZ | Linux Bluetooth implementation | Required |
| Bluetooth control | BlueZ D-Bus interfaces | Scan/pair/connect/disconnect/state | Required |
| D-Bus client | QtDBus | C++ integration with BlueZ | Required |
| Audio system | PipeWire | Capture and output routing | Required |
| Native audio API | libpipewire / pw_stream | Capture/output streams | Required |
| Internal audio representation | PCM | Canonical uncompressed pipeline | Required |
| Per-device buffers | C++ ring buffers | Delay and isolation | Required |
| Timing | monotonic clock + available stream timing | Stable internal timing | Required |
| Synchronization | Custom C++ engine | Per-device delay/drift handling | V1 |
| Logging | Structured session logs | Reproducible debugging | Required early |
| Unit tests | GoogleTest or Catch2 | Pure logic tests | Recommended |
| Analysis | Python | Log analysis/plots/experiments | Recommended |
| Packaging | AppImage/Flatpak later | Distribution | Later |

## 3.2 Rationale

### C++20

Chosen because the audio path will involve:

- frequent callbacks;
- PCM buffers;
- timing;
- synchronization state;
- low-overhead data movement;
- real-time-safe constraints.

Python can still be used outside the real-time path for measurements and tooling.

### Qt 6/QML

Provides:

- native desktop application shell;
- modern UI;
- data models;
- signals/slots;
- settings;
- D-Bus integration;
- future cross-platform reuse.

### BlueZ + D-Bus

BlueZ is the Linux Bluetooth stack. The application should use its exported control interfaces rather than trying to implement raw Bluetooth pairing logic.

### PipeWire

PipeWire is proposed as the Linux audio graph and stream layer.

The application will use it for:

- system-audio capture;
- discovering usable output sinks;
- opening output streams to individual sinks;
- monitoring stream state/timing where available.

## 3.3 Dependencies

Indicative build dependencies:

```text
C++20 compiler
CMake
Ninja
Qt 6 Core
Qt 6 Quick
Qt 6 Qml
Qt 6 DBus
PipeWire development headers/libraries
BlueZ runtime
D-Bus
pkg-config
```

Exact package names depend on the selected Linux distribution.

## 3.4 Technologies deliberately not selected for the audio core

### Electron / Node.js

Not recommended for the real-time audio engine. A web-based shell could be considered later, but it would add an additional process/runtime boundary without solving the transport problem.

### Python as shipping real-time engine

Useful for experiments but not selected as the core streaming engine.

### Custom Bluetooth protocol stack

Not justified at the initial stage. The host Bluetooth stack should be used first.

### Custom transmitter hardware

Explicitly deferred until evidence shows software-only operation cannot meet the requirement.

## 3.5 Future technology paths

- Windows: WASAPI plus Windows Bluetooth APIs.
- LE Audio: evaluate against target hardware and host support.
- Auracast: evaluate for higher receiver counts.
- Dedicated transmitter: conditional architecture if laptop stack/hardware is inadequate.
