# 2. Laptop Technical Architecture

## 2.1 Proposed high-level architecture

```text
                         LINUX LAPTOP
+------------------------------------------------------------------+
|                                                                  |
|  Audio Sources                                                   |
|  Browser / Media Player / System / Microphone                    |
|          |                                                       |
|          v                                                       |
|      PipeWire                                                    |
|          | system-audio capture                                  |
|          v                                                       |
|  +------------------------------------------------------------+  |
|  | AURALIS APPLICATION                                        |  |
|  |                                                            |  |
|  | Device Manager <----> BlueZ / D-Bus                        |  |
|  | Audio Capture Engine                                       |  |
|  | Audio Processing Engine                                    |  |
|  | Multi-Output Router                                        |  |
|  | Synchronization Engine                                     |  |
|  | Session Manager                                            |  |
|  | Diagnostics / Metrics                                      |  |
|  | Qt/QML UI                                                  |  |
|  +-------------------------+----------------------------------+  |
|                            | per-device PCM streams              |
|                            v                                     |
|                        PipeWire                                  |
|                            |                                     |
|                    Bluetooth audio sinks                         |
|                            |                                     |
|                         BlueZ                                    |
|                            |                                     |
|                    Bluetooth adapter                             |
+----------------------------+-------------------------------------+
                             |
               +-------------+-------------+
               |             |             |
               v             v             v
          Hearing A      Hearing B      Hearing C
```

## 2.2 Two control planes

The architecture should distinguish two paths.

### Bluetooth/device-control plane

Responsible for:

- adapter state;
- scanning;
- discovery;
- pairing/bonding;
- connection/disconnection;
- reconnection;
- metadata and capability state.

Conceptually:

```text
Qt UI
  |
Device Manager
  |
QtDBus
  |
BlueZ
  |
Bluetooth Adapter
  |
Hearing Device
```

### Audio/data plane

Responsible for:

- capturing source PCM;
- processing/resampling if needed;
- distributing one source timeline;
- independent per-device buffers;
- output stream health;
- latency compensation.

Conceptually:

```text
System Audio
   |
PipeWire Capture
   |
Canonical PCM
   |
Audio Distributor
   +--------+--------+
   |        |        |
Queue A  Queue B  Queue C
   |        |        |
PW A     PW B     PW C
   |        |        |
BT A     BT B     BT C
```

## 2.3 Why the planes are separated

Bluetooth connection state and audio stream state are not identical.

A device may be:

- paired but not connected;
- connected at the Bluetooth level but not usable as an audio sink;
- exposed as a sink but not currently streaming;
- streaming but experiencing underruns;
- reconnecting while other devices remain healthy.

The application should model these independently.

## 2.4 Core runtime objects

A practical runtime hierarchy is:

```text
Application
 |
 +-- BluetoothManager
 |    +-- DeviceSession A
 |    +-- DeviceSession B
 |
 +-- AudioEngine
 |    +-- CaptureStream
 |    +-- OutputSession A
 |    +-- OutputSession B
 |
 +-- SyncEngine
 |    +-- SyncState A
 |    +-- SyncState B
 |
 +-- SessionManager
 |
 +-- Diagnostics
 |
 +-- UI
```

## 2.5 Threading model

The implementation should avoid doing blocking D-Bus, filesystem or UI work inside real-time audio callbacks.

Recommended conceptual split:

- UI/main thread — QML, user commands, presentation state.
- Bluetooth/control thread or asynchronous D-Bus operations — discovery/pairing/state.
- PipeWire processing context — audio callbacks.
- Worker thread(s) — non-real-time analysis, logs, resampling setup, control calculations.
- Diagnostics writer — batched structured logging.

The exact PipeWire threading integration should be decided during implementation, but real-time callback code must remain minimal.

## 2.6 Audio ownership

The application should own the **logical source timeline** and per-device queues, while the operating system Bluetooth/audio stack should initially own Bluetooth codec negotiation and transport.

The first prototype should not implement its own A2DP encoder unless the host stack proves insufficient.

## 2.7 Future platform abstraction

Do not over-engineer this before Linux works, but module boundaries should allow future equivalents:

| Logical service | Linux | Future Windows |
|---|---|---|
| Bluetooth control | BlueZ D-Bus | Windows Bluetooth APIs |
| System audio capture | PipeWire | WASAPI loopback |
| Output endpoints | PipeWire sinks | Windows audio endpoints |
| UI | Qt/QML | Qt/QML can remain |
| Core router/sync | C++ | C++ can remain |
