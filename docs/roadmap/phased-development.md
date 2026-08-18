# Auralis Laptop Development Roadmap
## Detailed Phased Implementation Plan

**Project:** Auralis  
**Target Platform:** Laptop / Ubuntu Linux  
**Current OS:** Ubuntu 26.04 LTS  
**Kernel:** Linux 7.0.0-29-generic  
**Architecture:** x86_64  
**Document Purpose:** Define the phased implementation roadmap, system architecture, deliverables, validation gates, and hardware/software tracks for building the Auralis multi-hearing-device audio hub on a Linux laptop.

---

# 1. Project Objective

Auralis is intended to provide a unified software platform for discovering, connecting, managing, grouping, and routing audio to multiple hearing or Bluetooth audio devices from a laptop.

The implementation should be modular enough that Bluetooth transport, audio routing, device control, session management, and user interface logic remain separated.

The system should eventually support:

- Bluetooth Classic audio devices
- Bluetooth Low Energy device discovery and control
- Hearing-device-oriented workflows
- Multi-device audio routing
- Device grouping
- Persistent sessions
- Automatic reconnection
- Per-device and group-level controls
- PipeWire-based audio routing
- BlueZ-based Bluetooth management
- Qt/QML desktop UI
- Future Bluetooth LE Audio integration

The project will be built using strict phase gates. Each phase must produce a functioning, testable result before the next phase begins.

---

# 2. Verified Development Environment

The laptop environment has already been validated and can be considered the official Auralis development baseline.

## 2.1 Operating System

| Component | Verified Value |
|---|---|
| Distribution | Ubuntu |
| Release | Ubuntu 26.04 LTS |
| Codename | resolute |
| Kernel | 7.0.0-29-generic |
| Architecture | x86_64 |

## 2.2 Compiler and Build Toolchain

| Component | Version |
|---|---|
| GCC | 15.2.0 |
| G++ | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Git | 2.53.0 |

## 2.3 Qt Stack

| Component | Version |
|---|---|
| Qt | 6.10.2 |
| Qt Bluetooth | 6.10.2 |
| Qt Multimedia | 6.10.2 |
| qmake6 | Available |

Qt will be used primarily for:

- Desktop application framework
- Qt Quick
- QML UI
- Application lifecycle
- Signals and slots
- Device models exposed to UI
- Optional Qt Bluetooth helpers
- Optional Qt Multimedia helpers

Low-level Bluetooth lifecycle management should still prefer BlueZ D-Bus APIs where direct control is required.

## 2.4 Bluetooth Stack

| Component | Verified Value |
|---|---|
| BlueZ | 5.85 |
| Bluetooth daemon | Active |
| Controller | Qualcomm Atheros |
| USB ID | 0cf3:e009 |
| Controller name | hci0 |
| Bluetooth address | E8:9E:B4:13:4C:CC |
| Manufacturer ID | 29 |
| Controller version | 8 |
| BR/EDR | Supported |
| BLE | Supported |
| Advertising | Supported |
| Central role | Supported |
| Peripheral role | Supported |
| Secure Connections | Supported |
| Privacy | Supported |
| Wide-band speech | Supported |

The kernel Bluetooth stack is operational using the `btusb` driver.

## 2.5 Audio Stack

| Component | Version / Status |
|---|---|
| PipeWire | 1.6.2 |
| WirePlumber | Running |
| pipewire-pulse | Running |
| PulseAudio compatibility | Active on PipeWire |
| D-Bus | 1.16.2 |
| Built-in audio | Detected |
| Audio input | Detected |
| Audio output | Detected |

The current audio architecture is therefore:

```text
Applications
     |
     v
PipeWire / pipewire-pulse
     |
     v
WirePlumber
     |
     v
ALSA / BlueZ Audio Endpoints
     |
     v
Linux Kernel
     |
     v
Physical Audio / Bluetooth Hardware
```

---

# 3. High-Level Auralis Architecture

The application must avoid mixing UI, Bluetooth logic, audio routing, and device state into one codebase layer.

The intended architecture is:

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
          |                            |
          +-------------+--------------+
                        |
                        v
                 Linux Kernel
                        |
           +------------+-------------+
           |                          |
           v                          v
 Bluetooth Hardware            Audio Hardware
```

The architecture is intentionally transport-independent at the application level.

A higher-level Auralis audio endpoint should not need to know whether the underlying transport is:

- A2DP
- HFP/HSP
- LE Audio
- Another future transport

This separation is essential for long-term maintainability.

---

# 4. Development Principles

The project should follow the principles below from Phase 1 onward.

## 4.1 Separation of Concerns

Keep the following areas independent:

- UI
- Bluetooth management
- Audio graph management
- device state
- session state
- configuration
- logging
- persistence
- transport-specific implementation

## 4.2 No Shell Command Dependency in Production

Command-line utilities such as:

```bash
bluetoothctl
wpctl
pactl
btmgmt
```

are useful for debugging and validation.

However, the production application should not depend on launching these commands and parsing their output.

Instead:

- BlueZ should be controlled via D-Bus.
- PipeWire should be integrated via its native API where practical.
- Qt should be used for application/UI state and selected helper APIs.

## 4.3 Phase Gates

Each phase follows:

```text
IMPLEMENT
   |
   v
TEST
   |
   v
VERIFY
   |
   v
COMMIT / TAG
   |
   v
NEXT PHASE
```

No phase should be considered complete merely because code exists.

It must meet its exit condition.

---

# 5. Phase Overview

| Phase | Name | Goal | Status |
|---|---|---|---|
| 0 | Development Environment | Prepare and validate Ubuntu development platform | COMPLETE |
| 1 | Project Foundation | Establish production-ready codebase structure | NEXT |
| 2 | Bluetooth Device Discovery | Discover nearby Classic and BLE devices | Planned |
| 3 | Device Management | Pair, trust, connect, disconnect, forget, reconnect | Planned |
| 4 | Audio Device Integration | Map connected Bluetooth devices to PipeWire endpoints | Planned |
| 5 | Audio Routing Engine | Route audio to selected devices | Planned |
| 6 | Multi-Device Session Engine | Control multiple devices as logical sessions | Planned |
| 7 | Complete Auralis GUI | Implement full desktop interaction layer | Planned |
| 8 | Reliability and Production Hardening | Stabilize, test, package, recover from failures | Planned |

A separate LE Audio development track will run alongside later phases.

---

# 6. Phase 0 — Development Environment

## 6.1 Objective

Prepare a Linux workstation capable of developing and testing:

- Bluetooth Classic
- Bluetooth Low Energy
- PipeWire audio
- Qt/QML UI
- BlueZ D-Bus integration
- native C/C++ code

## 6.2 Completed Work

The environment currently contains:

```text
Ubuntu 26.04 LTS
|
+-- Linux Kernel 7.0
|
+-- GCC 15.2
+-- G++ 15.2
+-- CMake 4.2
+-- Ninja 1.13
+-- Git 2.53
|
+-- Qt 6.10.2
|   +-- Qt Bluetooth
|   +-- Qt Multimedia
|
+-- BlueZ 5.85
+-- D-Bus
|
+-- PipeWire 1.6.2
+-- WirePlumber
|
+-- Qualcomm Bluetooth Controller
```

## 6.3 Validation Results

Bluetooth:

- Adapter detected
- Adapter powered
- Adapter not blocked
- BlueZ service active
- BLE supported
- BR/EDR supported
- Controller capable of central and peripheral roles

Audio:

- PipeWire active
- WirePlumber active
- pipewire-pulse active
- built-in sink available
- built-in source available

## 6.4 Exit Gate

**Status: PASSED**

No further OS-level work should be performed unless required by a later implementation phase.

---

# 7. Phase 1 — Project Foundation

## 7.1 Objective

Create the official Auralis repository and establish a maintainable build and code structure before implementing hardware functionality.

The focus of Phase 1 is architecture and reproducibility.

## 7.2 Proposed Repository Structure

```text
auralis/
|
+-- CMakeLists.txt
|
+-- apps/
|   +-- desktop/
|       +-- CMakeLists.txt
|       +-- main.cpp
|
+-- src/
|   +-- core/
|   +-- bluetooth/
|   +-- audio/
|   +-- devices/
|   +-- session/
|
+-- include/
|   +-- auralis/
|       +-- core/
|       +-- bluetooth/
|       +-- audio/
|       +-- devices/
|       +-- session/
|
+-- ui/
|   +-- qml/
|   +-- components/
|   +-- assets/
|
+-- tests/
|   +-- unit/
|   +-- integration/
|
+-- tools/
|
+-- config/
|
+-- docs/
|
+-- cmake/
|
+-- README.md
+-- LICENSE
+-- .gitignore
```

## 7.3 Core Components

Phase 1 should define interfaces or skeleton implementations for:

### ApplicationCore

Responsible for:

- startup
- shutdown
- subsystem initialization
- subsystem lifetime
- global error handling

### Logger

Responsible for:

- structured logging
- log levels
- timestamps
- module identification
- optional file logging

Suggested levels:

```text
TRACE
DEBUG
INFO
WARN
ERROR
FATAL
```

### ConfigurationManager

Responsible for:

- loading application configuration
- persistent settings
- environment overrides
- future feature flags

### BluetoothManager Interface

Initially a stub.

Later responsible for:

- adapter state
- discovery
- device lifecycle
- D-Bus communication

### PipeWireManager Interface

Initially a stub.

Later responsible for:

- PipeWire registry
- devices
- nodes
- streams
- links
- route creation

### DeviceManager Interface

Responsible for higher-level Auralis device state.

### SessionManager Interface

Responsible for logical device groups and persistent sessions.

## 7.4 Initial UI

The first UI should remain intentionally simple.

Example:

```text
AURALIS

System Status
--------------------------------
Bluetooth     Ready
Audio         Ready
PipeWire      Ready

Auralis Core Ready
```

The objective is not visual quality.

The objective is proving:

- Qt launches
- QML loads
- C++ backend initializes
- status reaches the UI
- architecture works

## 7.5 Build Target

Primary executable:

```text
auralis-desktop
```

## 7.6 Required Build Commands

The following must work:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/apps/desktop/auralis-desktop
```

## 7.7 Testing

Introduce test infrastructure during Phase 1.

Suggested test groups:

```text
tests/
|
+-- unit/
|   +-- core/
|   +-- config/
|   +-- devices/
|
+-- integration/
```

The important design decision is that hardware-independent components should be testable without Bluetooth devices connected.

## 7.8 Phase 1 Exit Condition

Phase 1 is complete only when:

- repository exists
- clean CMake configuration succeeds
- Ninja build succeeds
- desktop executable launches
- logging initializes
- configuration initializes
- basic UI loads
- service layer skeletons exist
- unit test framework runs successfully

Suggested Git commit:

```text
phase-1: initialize Auralis project foundation
```

---

# 8. Phase 2 — Bluetooth Device Discovery

## 8.1 Objective

Enable Auralis to discover nearby Bluetooth Classic and BLE devices through BlueZ.

The application must perform discovery through D-Bus rather than shelling out to `bluetoothctl`.

## 8.2 Primary Components

```text
BluetoothManager
|
+-- AdapterManager
+-- DiscoveryManager
+-- BluetoothDevice
+-- DeviceRegistry
+-- BlueZDbusClient
```

## 8.3 BlueZ Integration

Auralis should communicate with:

```text
org.bluez
```

through the system D-Bus.

Important BlueZ interfaces may include:

```text
org.bluez.Adapter1
org.bluez.Device1
org.freedesktop.DBus.ObjectManager
org.freedesktop.DBus.Properties
```

## 8.4 Discovery Flow

```text
User selects Scan
      |
      v
BluetoothManager
      |
      v
BlueZ Adapter1.StartDiscovery()
      |
      v
BlueZ discovers devices
      |
      v
D-Bus interfaces / properties update
      |
      v
DeviceRegistry updates
      |
      v
Qt model changes
      |
      v
QML UI updates
```

## 8.5 Device Model

A Bluetooth device should include at least:

```text
BluetoothDevice
|
+-- internalId
+-- objectPath
+-- address
+-- addressType
+-- name
+-- alias
+-- rssi
+-- paired
+-- connected
+-- trusted
+-- blocked
+-- servicesResolved
+-- class
+-- icon
+-- appearance
+-- uuids[]
+-- manufacturerData
+-- serviceData
+-- lastSeen
```

## 8.6 Device Registry

DeviceRegistry should:

- deduplicate devices
- preserve stable objects while properties change
- emit add/update/remove events
- maintain last-seen timestamps
- support filtering
- expose a Qt model to QML

## 8.7 Initial UI

Example:

```text
Nearby Devices
---------------------------------------

Hearing Aid L
AA:BB:CC:DD:EE:01
RSSI: -51 dBm
BLE

Hearing Aid R
AA:BB:CC:DD:EE:02
RSSI: -56 dBm
BLE

Headphones
AA:BB:CC:DD:EE:03
RSSI: -67 dBm
Classic / BLE
```

## 8.8 Required Controls

- Start scan
- Stop scan
- Refresh
- Show adapter state
- Show device count
- Show signal strength
- Show device address/type

## 8.9 Error Cases

Phase 2 should handle:

- Bluetooth powered off
- adapter missing
- BlueZ unavailable
- discovery already running
- D-Bus errors
- adapter removed
- device disappears
- malformed or incomplete device properties

## 8.10 Exit Condition

Auralis can independently:

1. start discovery
2. receive real BlueZ device events
3. build/update DeviceRegistry
4. stop discovery
5. display nearby devices in Qt/QML

No terminal interaction should be required.

---

# 9. Phase 3 — Bluetooth Device Management

## 9.1 Objective

Implement the complete lifecycle of a Bluetooth device.

## 9.2 Device State Model

Recommended logical state machine:

```text
UNKNOWN
   |
   v
DISCOVERED
   |
   v
PAIRING
   |
   v
PAIRED
   |
   v
TRUSTED
   |
   v
CONNECTING
   |
   v
CONNECTED
```

Failure/recovery states should include:

```text
DISCONNECTED
RECONNECTING
FAILED
REMOVED
```

## 9.3 Supported User Operations

- Pair
- Cancel pairing
- Trust
- Untrust
- Connect
- Disconnect
- Remove/Forget
- Reconnect
- View supported services

## 9.4 Pairing Agent

BlueZ pairing requires a D-Bus agent.

Auralis may need its own implementation of:

```text
org.bluez.Agent1
```

Potential capabilities:

```text
NoInputNoOutput
DisplayOnly
DisplayYesNo
KeyboardOnly
KeyboardDisplay
```

The selected behavior should depend on target hearing devices.

## 9.5 Device Persistence

Device metadata should eventually persist across application launches:

```text
DeviceRecord
|
+-- internalId
+-- address
+-- alias
+-- trusted
+-- preferredRole
+-- sessionMembership
+-- lastConnected
+-- userLabel
```

## 9.6 Connection Monitoring

Auralis must react to asynchronous changes from BlueZ.

The UI should never assume a connect request succeeded simply because the user clicked a button.

Connection state should come from BlueZ properties.

## 9.7 Exit Condition

The application UI successfully performs:

```text
Discover
Pair
Trust
Connect
Disconnect
Forget
Reconnect
```

with state changes correctly reflected in the application model.

---

# 10. Phase 4 — PipeWire Audio Integration

## 10.1 Objective

Monitor and understand the PipeWire graph, then correlate Bluetooth devices with audio-capable PipeWire objects.

## 10.2 Important Architectural Rule

A Bluetooth device and an audio endpoint are not the same logical entity.

Example:

```text
BluetoothDevice
      |
      | mapped to
      v
AudioEndpoint
```

One physical device may expose multiple audio profiles or nodes.

## 10.3 PipeWire Components

```text
PipeWireManager
|
+-- CoreConnection
+-- RegistryMonitor
+-- DeviceMonitor
+-- NodeMonitor
+-- PortMonitor
+-- LinkMonitor
+-- AudioEndpointRegistry
```

## 10.4 Objects to Observe

- PipeWire core
- devices
- nodes
- ports
- links
- metadata
- streams

## 10.5 AudioEndpoint Model

Suggested:

```text
AudioEndpoint
|
+-- id
+-- pipeWireObjectId
+-- name
+-- description
+-- direction
+-- mediaClass
+-- deviceId
+-- nodeName
+-- profile
+-- sampleRate
+-- channelCount
+-- availability
+-- bluetoothDeviceId
```

## 10.6 PipeWire Graph Example

```text
PipeWire
|
+-- Built-in Audio Analog Stereo
|
+-- Built-in Microphone
|
+-- Bluetooth Headset A2DP Sink
|
+-- Bluetooth Headset HFP Source
|
+-- Application Streams
```

## 10.7 Mapping Strategy

Auralis will require a resolver that connects:

```text
BlueZ Device
     |
     v
PipeWire Device
     |
     v
PipeWire Audio Node(s)
```

Possible correlation information may include:

- Bluetooth address
- BlueZ D-Bus path
- PipeWire device properties
- media class
- device API
- node description

## 10.8 UI Deliverable

The UI should show connected audio endpoints independently from discovered Bluetooth devices.

Example:

```text
Audio Endpoints

Built-in Audio
  Output
  Available

Hearing Device A
  Bluetooth Output
  Available

Hearing Device B
  Bluetooth Output
  Available
```

## 10.9 Exit Condition

When a Bluetooth audio device connects:

1. BlueZ reports the device as connected.
2. PipeWire creates an audio-capable device/node.
3. Auralis detects the PipeWire node.
4. Auralis maps it to the Bluetooth device.
5. The endpoint becomes available for routing.

---

# 11. Phase 5 — Audio Routing Engine

## 11.1 Objective

Allow Auralis to choose an audio source and route it to one or more available output endpoints.

## 11.2 Core Components

```text
AudioRouter
|
+-- SourceSelector
+-- EndpointSelector
+-- RoutePlanner
+-- LinkManager
+-- StreamMonitor
+-- VolumeController
+-- RouteState
```

## 11.3 Basic Routing Goal

First milestone:

```text
Laptop Audio
     |
     v
Auralis Audio Router
     |
     v
Bluetooth Device
```

Then:

```text
                  +--> Device A
Laptop Audio -----+
                  +--> Device B
```

## 11.4 Routing Model

```text
AudioRoute
|
+-- routeId
+-- source
+-- destinations[]
+-- active
+-- status
+-- latency
+-- volumePolicy
+-- recoveryPolicy
```

## 11.5 Volume Management

Support:

- source volume
- destination volume
- per-device volume
- group volume
- mute
- optional normalization

## 11.6 Stream Monitoring

Auralis should detect:

- new application streams
- ended streams
- route breakage
- endpoint removal
- destination reappearance

## 11.7 Error Cases

- target disappears
- PipeWire node ID changes
- stream is recreated
- Bluetooth reconnect changes profile
- route cannot be linked
- incompatible audio format
- destination suspended

## 11.8 Exit Condition

From Auralis:

1. choose source
2. choose one or more outputs
3. activate route
4. confirm audio reaches selected endpoint(s)
5. deactivate route cleanly

---

# 12. Phase 6 — Multi-Device Session Engine

## 12.1 Objective

Treat several physical hearing/audio devices as one logical Auralis session.

This is a major differentiator from a normal Bluetooth settings utility.

## 12.2 Session Model

Example:

```text
Session: Living Room

Source:
  Laptop Audio

Devices:
  Hearing Aid Left
  Hearing Aid Right

State:
  Active

Group Volume:
  70%
```

## 12.3 Core Components

```text
SessionManager
|
+-- Session
+-- SessionDevice
+-- SessionStateMachine
+-- SessionPersistence
+-- RecoveryManager
+-- VolumeCoordinator
+-- RoutingCoordinator
```

## 12.4 Session Data Model

```text
AuralisSession
|
+-- sessionId
+-- name
+-- source
+-- devices[]
+-- routes[]
+-- state
+-- groupVolume
+-- autoReconnect
+-- recoveryPolicy
+-- createdAt
+-- lastUsedAt
```

## 12.5 Multi-Device Concerns

This phase must address:

- one device disconnecting
- one device reconnecting
- left/right device association
- device availability
- individual volume
- group volume
- route consistency
- automatic session restoration
- partial session state
- degraded mode

## 12.6 Suggested Session States

```text
IDLE
STARTING
ACTIVE
DEGRADED
RECOVERING
STOPPING
FAILED
```

## 12.7 Degraded Mode

Example:

```text
Session expects:
  Left Hearing Device
  Right Hearing Device

Current:
  Left  -> Connected
  Right -> Disconnected

Session state:
  DEGRADED
```

Auralis should not necessarily destroy the entire session when one endpoint drops.

## 12.8 Exit Condition

The user can:

- create a session
- add multiple devices
- select a source
- activate the session
- control devices as a group
- survive a device disconnect/reconnect
- restore a saved session

---

# 13. Phase 7 — Complete Auralis GUI

## 13.1 Objective

Build the full desktop control surface after the backend architecture is stable.

Technology:

```text
Qt Quick + QML
```

with the core application remaining primarily C++.

## 13.2 Main Screens

Recommended:

```text
Dashboard
Devices
Sessions
Audio Routing
Diagnostics
Settings
```

## 13.3 Dashboard

Should summarize:

- Bluetooth status
- current session
- connected devices
- audio source
- overall volume
- errors/warnings
- quick actions

Example:

```text
+------------------------------------------+
| AURALIS                                  |
+------------------------------------------+
| ACTIVE SESSION                           |
| Living Room                              |
|                                          |
| Source                                   |
| Laptop Audio                             |
|                                          |
| Connected Devices                        |
|  Hearing Aid L                           |
|  Hearing Aid R                           |
|                                          |
| Volume                                   |
| ---------------O------ 72%               |
+------------------------------------------+
```

## 13.4 Devices Screen

Should include:

- scan control
- discovered devices
- known devices
- pairing
- trust
- connect
- disconnect
- device details
- signal strength
- supported services
- audio endpoint state

## 13.5 Sessions Screen

Should support:

- create session
- rename session
- add/remove devices
- select source
- choose route policy
- activate/deactivate
- save
- restore
- duplicate
- delete

## 13.6 Audio Routing Screen

Could visualize:

```text
Sources
   |
   v
Routes
   |
   v
Endpoints
```

Potentially with a graph-like UI later.

## 13.7 Diagnostics Screen

Important during development and support.

Should display:

- Bluetooth adapter state
- BlueZ connection state
- PipeWire state
- current devices
- active nodes
- active links
- errors
- reconnection attempts
- session state
- application logs

## 13.8 Exit Condition

The complete primary workflow can be performed without a terminal.

---

# 14. Phase 8 — Reliability, Testing and Production Hardening

## 14.1 Objective

Convert the prototype into a dependable desktop application.

## 14.2 Failure Conditions to Handle

```text
Bluetooth adapter disappears
Bluetooth service restarts
PipeWire restarts
WirePlumber restarts
Device turns off
Device battery dies
Device leaves range
Device reconnects
Audio node changes
Audio profile changes
Pairing fails
Connection times out
Application restarts
Laptop suspends
Laptop resumes
D-Bus reconnects
```

## 14.3 RecoveryManager

Introduce centralized recovery logic.

Example responsibilities:

- service reconnect
- device reconnect
- route restoration
- session restoration
- retry backoff
- failure escalation

## 14.4 Logging

Logs should support:

```text
timestamp
severity
subsystem
event
device/session ID
error code
human-readable message
```

Example:

```text
2026-08-18T00:00:00.000
WARN
BluetoothManager
DeviceDisconnected
device=ha-left
reason=link-loss
```

## 14.5 Persistence

Persist:

- application settings
- known devices
- user labels
- sessions
- preferred routes
- volume state
- last active session
- recovery preferences

## 14.6 Testing Strategy

### Unit Tests

For:

- state machines
- registry behavior
- config
- persistence
- session logic
- mapping logic
- route planning

### Integration Tests

For:

- BlueZ D-Bus layer
- PipeWire registry
- device lifecycle
- reconnect logic

### Hardware Tests

For:

- real Bluetooth device discovery
- pairing
- connection
- route setup
- multi-device scenarios
- disconnect/recovery

### Stress Tests

Test:

- repeated connect/disconnect
- repeated scans
- session restore
- long-running playback
- multiple device churn
- suspend/resume loops

## 14.7 Packaging

Initial target:

```text
.deb
```

Possible later formats:

```text
AppImage
Flatpak
```

Packaging decisions should be made after runtime dependencies are stable.

## 14.8 Exit Condition

Auralis should:

- install predictably
- launch reliably
- recover from expected service/device failures
- preserve sessions
- pass automated tests
- function without manual terminal intervention

---

# 15. Dedicated Bluetooth LE Audio Track

## 15.1 Why This Is Separate

The current QCA9377-based controller is useful for:

- Bluetooth Classic
- BLE scanning
- BLE GATT
- BlueZ integration
- pairing
- device state
- Classic audio development

However, standards-based Bluetooth LE Audio requires a controller/platform with the necessary modern isochronous transport capabilities.

Therefore LE Audio should not block the core Auralis architecture.

## 15.2 Parallel Track

```text
Core Auralis Development
        |
        +------------------------------+
        |                              |
        v                              v
Classic/BLE Control              LE Audio Track
        |                              |
        v                              v
BlueZ / D-Bus                Controller Validation
        |                              |
        v                              v
PipeWire                  BlueZ LE Audio Capability
                                       |
                                       v
                                      LC3
                                       |
                                       v
                                BAP / PACS / ASCS
                                       |
                                       v
                               Hearing Device Tests
```

## 15.3 LE Audio Topics

Eventually evaluate:

- Bluetooth 5.2+ controller requirements
- ISO channels
- CIS
- BIS
- LC3
- BAP
- PACS
- ASCS
- coordinated sets
- broadcast audio
- Auracast
- hearing-device-specific interoperability

## 15.4 Hardware Strategy

The existing internal controller should remain available for normal Bluetooth work.

A modern USB Bluetooth adapter can later be added specifically for LE Audio experimentation.

This keeps the development environment flexible.

---

# 16. Transport Abstraction

Auralis should define a transport-independent application model.

Example:

```text
AudioEndpoint
     ^
     |
+----+---------+-------------+
|              |             |
A2DP         LE Audio       Future
|              |             |
BlueZ          BlueZ         ...
```

The upper-level code should consume capabilities rather than transport names where possible.

For example:

```text
EndpointCapabilities
|
+-- playback
+-- capture
+-- volumeControl
+-- groupable
+-- lowLatency
+-- microphone
+-- batteryStatus
+-- transportType
```

This avoids rewriting the SessionManager when LE Audio is introduced.

---

# 17. Recommended Core Domain Objects

The following object model is recommended for the full application.

## 17.1 BluetoothAdapter

```text
BluetoothAdapter
|
+-- id
+-- address
+-- name
+-- powered
+-- discoverable
+-- pairable
+-- discovering
+-- capabilities
```

## 17.2 BluetoothDevice

```text
BluetoothDevice
|
+-- id
+-- address
+-- name
+-- alias
+-- rssi
+-- paired
+-- trusted
+-- connected
+-- servicesResolved
+-- uuids
+-- metadata
```

## 17.3 AudioEndpoint

```text
AudioEndpoint
|
+-- id
+-- name
+-- direction
+-- transport
+-- pipeWireNodeId
+-- deviceReference
+-- available
+-- capabilities
```

## 17.4 AudioRoute

```text
AudioRoute
|
+-- id
+-- source
+-- outputs[]
+-- active
+-- state
```

## 17.5 AuralisSession

```text
AuralisSession
|
+-- id
+-- name
+-- source
+-- deviceIds[]
+-- routeIds[]
+-- volume
+-- state
+-- recoveryPolicy
```

---

# 18. Service Ownership

Recommended subsystem ownership:

| Concern | Owner |
|---|---|
| Bluetooth adapter | BluetoothManager |
| Bluetooth discovery | DiscoveryManager |
| Bluetooth devices | DeviceRegistry |
| Bluetooth connection lifecycle | BluetoothManager |
| Pairing agent | BlueZAgent |
| PipeWire registry | PipeWireManager |
| Audio endpoints | AudioEndpointRegistry |
| Bluetooth-to-audio mapping | EndpointResolver |
| Routing | AudioRouter |
| Sessions | SessionManager |
| Persistence | PersistenceManager |
| Configuration | ConfigurationManager |
| Logging | Logger |
| Recovery | RecoveryManager |
| GUI state | Qt/QML presentation layer |

---

# 19. Proposed Milestone Gates

## Milestone 0

Environment works.

**Status:** Complete.

## Milestone 1

Application shell compiles and launches.

## Milestone 2

Nearby devices appear in Auralis.

## Milestone 3

Device pairing and connection works.

## Milestone 4

Bluetooth audio endpoints appear inside Auralis.

## Milestone 5

Audio can be routed to selected endpoint.

## Milestone 6

Multiple devices operate inside one session.

## Milestone 7

Complete GUI controls the full workflow.

## Milestone 8

Application survives failures and is distributable.

---

# 20. Suggested Git Strategy

Use commits that correspond to meaningful working increments.

Examples:

```text
phase-1: initialize Auralis project foundation

phase-1: add application core and logging

phase-1: add Qt QML desktop shell

phase-2: add BlueZ D-Bus client

phase-2: implement adapter discovery

phase-2: implement Bluetooth device registry

phase-3: implement pairing agent

phase-3: implement device connection state machine

phase-4: add PipeWire registry monitor

phase-4: map Bluetooth devices to audio endpoints

phase-5: implement initial audio route creation

phase-6: add multi-device session manager
```

Tags may be created at completed gates:

```text
v0.1-phase1
v0.2-phase2
v0.3-phase3
...
```

---

# 21. Immediate Next Step

The development environment is complete.

The next implementation activity is therefore:

```text
PHASE 1
PROJECT FOUNDATION
```

The first Phase 1 task should create:

1. Auralis repository
2. folder structure
3. root `CMakeLists.txt`
4. desktop application target
5. Qt/QML shell
6. core library
7. Bluetooth module skeleton
8. audio module skeleton
9. device module skeleton
10. session module skeleton
11. logging system
12. configuration system
13. testing infrastructure
14. `.gitignore`
15. initial `README.md`

The first executable should be intentionally minimal.

Its purpose is to prove the architecture before any hardware-specific implementation begins.

---

# 22. Final Roadmap

```text
PHASE 0
Development Environment
COMPLETE
       |
       v
PHASE 1
Project Foundation
       |
       v
PHASE 2
Bluetooth Device Discovery
       |
       v
PHASE 3
Bluetooth Device Management
       |
       v
PHASE 4
PipeWire Audio Integration
       |
       v
PHASE 5
Audio Routing Engine
       |
       v
PHASE 6
Multi-Device Session Engine
       |
       v
PHASE 7
Complete Auralis GUI
       |
       v
PHASE 8
Reliability / Testing / Packaging
```

Parallel technology path:

```text
LE AUDIO TRACK
       |
       v
Modern Controller Validation
       |
       v
BlueZ LE Audio
       |
       v
LC3 / BAP / PACS / ASCS
       |
       v
Multi-Device Hearing Hardware Tests
```

---

# 23. Project Status

```text
Environment            COMPLETE
Architecture           DEFINED
Development roadmap    DEFINED
Project foundation     NEXT
Bluetooth discovery    PENDING
Device management      PENDING
PipeWire integration   PENDING
Audio routing          PENDING
Session engine         PENDING
Complete GUI           PENDING
Hardening              PENDING
LE Audio hardware      FUTURE TRACK
```

The project should now proceed with **Phase 1 — Project Foundation** and should not advance to Phase 2 until the Phase 1 build, application shell, core architecture, and tests are verified.
