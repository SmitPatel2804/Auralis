# Multi-Hearing-Device Audio Hub
## Finalized Project Specification — V1

**Status:** Architecture baseline / technical validation phase  
**Date:** 17 August 2026

---

## 1. Executive Summary

The project is a **multi-Bluetooth audio transmission system** that takes one audio source and delivers the same audio simultaneously to multiple compatible Bluetooth hearing devices.

### Core user outcome

A user has a source such as a **Windows/Linux laptop or Android phone**. The user:

1. Opens the application.
2. Puts a hearing device into normal Bluetooth pairing mode.
3. Scans for available devices.
4. Pairs/connects one or more hearing devices individually.
5. Starts the source audio.
6. All connected hearing devices receive the same audio.

### Primary architecture

**Source device's own Bluetooth hardware is used first.**

No custom transmitter hardware is required for the initial prototype.

A dedicated hardware transmitter is introduced only if testing proves that the source operating system, Bluetooth stack, adapter, bandwidth, connection limits, or latency requirements prevent the software-only architecture from meeting the target.

---

# 2. Product Definition

## Product concept

> **One audio source → multiple individually paired Bluetooth hearing devices → synchronized simultaneous playback.**

The product consists primarily of:

- A source-side application.
- An audio capture/routing engine.
- A Bluetooth device manager.
- A multi-output audio engine.
- A synchronization/latency engine.
- A user interface for pairing and managing devices.

Optional future hardware can move the Bluetooth/audio engine outside the source device if required.

---

# 3. Target User Experience

## 3.1 Start

User launches the application.

Example:

```text
MULTI AUDIO HUB

Audio Source
● Laptop Audio: Connected

[ Add Hearing Device ]
```

## 3.2 Pair hearing devices

The hearing device is placed into its normal Bluetooth pairing mode.

The application scans for available devices.

Example:

```text
AVAILABLE DEVICES

Hearing Device A
Hearing Device B
Hearing Device C

[ Scan Again ]
```

The user selects a device and connects it.

## 3.3 Manage connected devices

Example:

```text
CONNECTED DEVICES

● Hearing Device A     Connected
● Hearing Device B     Connected
● Hearing Device C     Connected

[ Add Device ]
[ Remove Device ]
[ Start Audio ]
```

## 3.4 Playback

When playback begins, the application distributes the same source audio to every active device.

---

# 4. High-Level Architecture

```text
                         AUDIO SOURCE
                 ┌─────────────────────────┐
                 │                         │
                 │  Laptop / Android       │
                 │                         │
                 │  Music / Video / Media  │
                 │  Microphone / System    │
                 └────────────┬────────────┘
                              │
                        Audio Capture
                              │
                              ▼
                 ┌─────────────────────────┐
                 │     OUR APPLICATION     │
                 │                         │
                 │  Device Manager         │
                 │  Audio Engine           │
                 │  Output Router          │
                 │  Buffer Manager         │
                 │  Sync Engine            │
                 │  Latency Compensation   │
                 │  UI / Controls          │
                 └────────────┬────────────┘
                              │
                    Source Bluetooth
                              │
            ┌─────────────────┼─────────────────┐
            ▼                 ▼                 ▼
      Hearing Device A  Hearing Device B  Hearing Device C
            🎧                🎧                🎧
```

---

# 5. What This Project Is NOT

The first version is **not primarily an Auracast project**.

The requirement is specifically:

> The user should be able to put each hearing device into Bluetooth pairing mode and individually pair/connect it through our software.

Therefore, the initial architecture must investigate ordinary Bluetooth audio connections first.

Auracast/LE Audio remains an important future architecture and possible optimization.

---

# 6. Bluetooth Architecture

There are two major technology paths.

## 6.1 Bluetooth Classic / A2DP

Traditional Bluetooth audio commonly uses the Advanced Audio Distribution Profile (A2DP).

Conceptually:

```text
SOURCE
  │
  ├── A2DP → Hearing Device A
  ├── A2DP → Hearing Device B
  ├── A2DP → Hearing Device C
  └── A2DP → Hearing Device D
```

The challenge is that operating systems normally expose Bluetooth audio as a standard output and may not permit an ordinary application to create arbitrary simultaneous A2DP outputs.

The prototype must therefore determine:

- Maximum simultaneous audio sinks.
- Bluetooth adapter limitations.
- OS-level restrictions.
- Whether multiple A2DP connections can be maintained.
- Whether separate streams can be routed reliably.
- Practical audio bandwidth.
- Stability under sustained playback.

## 6.2 Bluetooth Low Energy / LE Audio

Bluetooth LE means **Bluetooth Low Energy**.

LE Audio is the newer Bluetooth audio architecture and uses the LC3 codec.

It can support architectures that are particularly attractive for this project.

Conceptually:

```text
SOURCE
  │
  ▼
LE Audio
  │
  ├── Hearing Device A
  ├── Hearing Device B
  └── Hearing Device C
```

If the target hearing devices support LE Audio, this must be evaluated as an alternative implementation.

## 6.3 Auracast

Auracast is a Bluetooth LE Audio broadcast capability.

Conceptually:

```text
SOURCE
  │
  ▼
AURACAST BROADCAST
  │
  ├── Hearing Device A
  ├── Hearing Device B
  ├── Hearing Device C
  └── Hearing Device D
```

Auracast is particularly attractive for large numbers of receivers because the source does not necessarily need to maintain a separate traditional audio stream for every receiver.

However, it requires compatible LE Audio/Auracast receiving devices.

**Auracast is therefore a future/alternative architecture, not a mandatory V1 requirement.**

---

# 7. Source Device Strategy

## V1 priority

Use the source device's existing Bluetooth hardware.

### Initial target

**Laptop**, preferably Windows or Linux for the first technical prototype.

Reasons:

- Better access to system audio.
- Easier debugging.
- Easier access to Bluetooth adapters.
- Easier experimentation with multiple Bluetooth connections.
- Easier use of external USB Bluetooth adapters.
- Greater control over audio routing.

## Android

Android is a second platform target.

Android support must be evaluated against the specific Android version, manufacturer implementation, Bluetooth hardware, and available APIs.

An Android application may not have unrestricted control over the system Bluetooth audio stack.

Therefore, Android is not assumed to support arbitrary numbers of simultaneous Classic Bluetooth audio outputs until experimentally verified.

---

# 8. Hardware Strategy

## V1: No custom hardware

Initial prototype:

```text
Laptop
 ├── Existing Bluetooth
 └── Our Application
        │
        ├── Hearing Device A
        ├── Hearing Device B
        └── Hearing Device C
```

If the internal Bluetooth adapter is unsuitable:

```text
Laptop
   │
   └── USB Bluetooth Adapter
           │
           ├── Hearing Device A
           ├── Hearing Device B
           └── Hearing Device C
```

## V2: Dedicated transmitter hardware

Only introduce dedicated hardware if the source device cannot satisfy the requirements.

Potential architecture:

```text
Laptop / Android / Other Source
             │
       USB / Bluetooth / AUX
             │
             ▼
      ┌────────────────┐
      │ OUR TRANSMITTER│
      │                │
      │ Bluetooth      │
      │ Audio Engine   │
      │ Buffers        │
      │ Sync Engine    │
      └───────┬────────┘
              │
       Bluetooth outputs
        ┌─────┼─────┐
        ▼     ▼     ▼
       🎧    🎧    🎧
```

A dedicated device could provide more predictable control over:

- Bluetooth connections.
- Audio processing.
- Buffering.
- Timing.
- Connection management.
- Hardware resources.
- Multiple radios/adapters if necessary.

---

# 9. Software Architecture

The application should be modular.

## 9.1 UI Layer

Responsibilities:

- Source status.
- Device discovery.
- Pairing.
- Connected-device list.
- Playback control.
- Device naming.
- Device removal.
- Connection health.
- Latency information.
- Error reporting.

## 9.2 Device Manager

Responsibilities:

- Bluetooth discovery.
- Pairing/connection management.
- Device capability detection.
- Connection state.
- Reconnection.
- Disconnect handling.
- Device metadata.
- Battery status where exposed by the platform/device.
- Supported profiles/codecs where detectable.

Example:

```text
Device Manager
│
├── Device A
│   ├── Connected
│   ├── Profile: TBD
│   ├── Codec: TBD
│   └── Latency: TBD ms
│
├── Device B
│   ├── Connected
│   ├── Profile: TBD
│   ├── Codec: TBD
│   └── Latency: TBD ms
│
└── Device C
    ├── Connected
    ├── Profile: TBD
    ├── Codec: TBD
    └── Latency: TBD ms
```

## 9.3 Audio Capture Engine

Captures the selected source audio.

Potential source types:

- System audio.
- Application audio.
- Microphone.
- USB audio.
- Line-in.
- Future external audio sources.

The first prototype should focus on **system audio from the laptop**.

## 9.4 Audio Processing Engine

Responsibilities:

- Audio format normalization.
- Buffering.
- Optional resampling.
- Level management.
- Duplication of source stream.
- Output preparation.

The internal processing path should remain high quality.

## 9.5 Output Router

Creates a logical output path for each connected device.

```text
             Source PCM
                 │
        ┌────────┼────────┐
        ▼        ▼        ▼
     Output A Output B Output C
        │        │        │
     Encoder   Encoder   Encoder
        │        │        │
       BT A     BT B     BT C
```

The exact encoding strategy depends on the supported Bluetooth profile and platform.

## 9.6 Synchronization Engine

The synchronization engine is a core component.

Different Bluetooth devices may introduce different buffering and transport latency.

Example:

```text
Device A: 100 ms
Device B: 135 ms
Device C: 117 ms
```

The engine can introduce controlled delay so that outputs align as closely as possible:

```text
Device A: +35 ms compensation
Device B: +0 ms compensation
Device C: +18 ms compensation
```

Target:

```text
A ────────────────►
B ────────────────►
C ────────────────►

Audio events occur as close together as technically practical.
```

---

# 10. Audio Quality Architecture

The system should avoid unnecessary generations of compression.

Preferred conceptual path:

```text
SOURCE
  │
  ▼
High-quality capture
  │
  ▼
Internal audio buffer
  │
  ├── Device-specific encoding
  ├── Device-specific encoding
  └── Device-specific encoding
       │
       ▼
   Bluetooth
```

Avoid unnecessary:

```text
Bluetooth
   ↓
Decode
   ↓
Re-encode
   ↓
Bluetooth
   ↓
Re-encode
```

Actual final sound quality depends on:

- Source quality.
- Bluetooth profile.
- Codec.
- Hearing device implementation.
- Bluetooth radio conditions.
- Connection stability.
- Audio processing inside the hearing device.

---

# 11. Latency

Latency is one of the most important technical challenges.

There are two separate goals:

## 11.1 Source-to-device latency

The total delay between the original source event and hearing the sound.

For video applications, excessive latency can cause lip-sync problems.

## 11.2 Device-to-device synchronization

The difference in arrival/playback time between hearing devices.

For example:

```text
Device A: 110 ms
Device B: 112 ms
Device C: 109 ms
```

is excellent synchronization.

But:

```text
Device A: 100 ms
Device B: 170 ms
Device C: 120 ms
```

may be noticeable.

The system should prioritize synchronized playback while maintaining acceptable total latency.

---

# 12. Connection Reliability

The application must handle:

- Device going out of range.
- Temporary interference.
- Device power-off.
- Device entering sleep mode.
- Bluetooth disconnects.
- Reconnection.
- Source interruption.
- Application restart.

Example behavior:

```text
Device B disconnected.

[ Reconnecting... ]

Device B connected.
Audio synchronization recalibrated.
```

The system should not require the user to restart the entire session whenever one device disconnects.

---

# 13. Scalability

The project should be developed in stages.

## MVP

Target:

**2 devices**

Prove:

- Pairing.
- Simultaneous playback.
- Audio quality.
- Stability.
- Basic synchronization.

## V1

Target:

**2–4 devices**

Add:

- Robust device management.
- Reconnection.
- Latency measurement.
- Synchronization.
- Diagnostics.
- Better UI.

## V2

Target:

**5–10+ devices**

At this stage, evaluate LE Audio/Auracast seriously.

The larger the number of devices, the less attractive independent Classic Bluetooth audio connections become.

---

# 14. Compatibility Requirements

A critical project requirement is to identify exactly what Bluetooth technology/profile the target hearing devices use.

A device advertised as “Bluetooth hearing device” does not automatically mean it behaves like a standard Bluetooth A2DP headphone.

Possible technologies include:

- Bluetooth Classic.
- A2DP.
- BLE.
- Bluetooth LE Audio.
- ASHA.
- MFi-related implementations.
- Proprietary Bluetooth protocols.
- Auracast/LE Audio broadcast.

Therefore:

> **Compatibility with the exact target hearing-device model must be verified before finalizing the Bluetooth implementation.**

This is the first major technical validation gate.

---

# 15. Development Roadmap

## Phase 0 — Device Compatibility Research

Identify the target hearing device models.

For each model determine:

- Bluetooth version.
- Bluetooth profile.
- Audio profile.
- Classic vs LE Audio.
- Codec.
- Pairing behavior.
- Number of simultaneous connections supported.
- Whether it exposes itself as a standard Bluetooth audio sink.
- Manufacturer-specific requirements.

**Deliverable:** compatibility matrix.

---

## Phase 1 — Single Device

Build:

```text
Laptop
  ↓
Our application
  ↓
One hearing device
```

Requirements:

- Discovery.
- Pairing/connection.
- Audio playback.
- Stable operation.

**Goal:** establish basic compatibility.

---

## Phase 2 — Two Devices

Build:

```text
Laptop
  ↓
Our application
  ├── Device A
  └── Device B
```

Requirements:

- Simultaneous connections.
- Same audio.
- Stable playback.
- Initial synchronization.

**Goal:** prove the core concept.

---

## Phase 3 — Multi-Device Audio Engine

Test:

- 3 devices.
- 4 devices.
- Different device models.
- Same-model devices.

Measure:

- Latency.
- Synchronization.
- Packet loss.
- Dropouts.
- CPU usage.
- Bluetooth bandwidth.
- Audio quality.

---

## Phase 4 — Synchronization Engine

Implement:

- Per-device buffering.
- Latency measurement.
- Controlled output delay.
- Clock/drift handling.
- Resynchronization.

---

## Phase 5 — User Application

Build production-oriented UI:

- Scan.
- Pair.
- Connect.
- Rename.
- Remove.
- Start/stop.
- Volume.
- Connection status.
- Diagnostics.
- Reconnect.

---

## Phase 6 — Android

Investigate:

- Android audio capture.
- Bluetooth APIs.
- Simultaneous output capabilities.
- Manufacturer-specific restrictions.
- LE Audio support.
- External Bluetooth hardware options.

---

## Phase 7 — Dedicated Hardware, If Needed

Only after software limitations are proven.

Evaluate:

- Bluetooth chipset.
- Multiple Bluetooth radios/adapters.
- LE Audio capability.
- USB audio.
- AUX/line input.
- DSP.
- Power supply.
- Enclosure.
- Firmware.

---

# 16. Testing Matrix

| Test | Purpose |
|---|---|
| 1 device | Basic compatibility |
| 2 devices | Core multi-device proof |
| 3 devices | Scalability |
| 4 devices | MVP/V1 target |
| Same device models | Synchronization consistency |
| Different models | Compatibility |
| Music | Audio quality |
| Speech | Intelligibility |
| Video | Lip-sync |
| Long-duration playback | Stability |
| Device disconnect | Recovery |
| Device reconnect | Robustness |
| Bluetooth interference | Reliability |
| Device at range limit | Connection stability |
| Battery low | Real-world behavior |

---

# 17. Success Criteria

The project should not be considered successful merely because multiple devices connect.

The V1 success criteria should include:

### Connectivity

- Multiple target hearing devices can be connected simultaneously.
- Connections remain stable during normal operation.

### Audio

- All connected devices receive the same source content.
- No unnecessary audio degradation.
- No repeated compression beyond what the Bluetooth path requires.

### Synchronization

- Device-to-device delay is sufficiently small for the intended use case.
- The system can compensate for known device latency differences.

### Reliability

- Temporary disconnections do not crash the application.
- Devices can reconnect.
- The system can recover from common Bluetooth failures.

### Usability

- Pairing is simple.
- Device status is obvious.
- User can add/remove devices without restarting the application.

---

# 18. Major Technical Risks

## Risk 1 — OS Bluetooth restrictions

The biggest initial risk is that the laptop/Android operating system may not expose enough control to create multiple simultaneous Bluetooth audio outputs.

**Mitigation:** test the Bluetooth stack early.

## Risk 2 — Hearing-device profile incompatibility

The target hearing device may not behave as a standard A2DP sink.

**Mitigation:** identify exact device models before committing to the architecture.

## Risk 3 — Synchronization

Independent Bluetooth links may have different latency.

**Mitigation:** build a dedicated synchronization/buffering layer.

## Risk 4 — Bluetooth bandwidth

Multiple independent streams can increase radio and processing requirements.

**Mitigation:** test scaling early and evaluate LE Audio/Auracast for larger deployments.

## Risk 5 — Android restrictions

Android manufacturer and OS behavior can differ.

**Mitigation:** make laptop/desktop the first development platform.

---

# 19. Architecture Decision: Why No Custom Hardware Initially?

The project should not begin by designing a custom Bluetooth transmitter.

Reasons:

1. The source already contains Bluetooth hardware.
2. A software prototype is faster.
3. It is cheaper.
4. It lets us discover OS limitations first.
5. It prevents premature hardware decisions.
6. We can determine the actual Bluetooth profile of the hearing devices.
7. If software is sufficient, the final product can be much simpler.

Custom hardware becomes justified when:

> **The source's Bluetooth stack or hardware cannot reliably provide the required number of simultaneous synchronized audio connections.**

---

# 20. Long-Term Architecture

The mature product may support multiple transmission modes.

```text
                 AUDIO SOURCE
                       │
                       ▼
                OUR AUDIO ENGINE
                       │
          ┌────────────┼─────────────┐
          │            │             │
          ▼            ▼             ▼
     Classic BT     LE Audio      Auracast
          │            │             │
          ▼            ▼             ▼
      Compatible   Compatible    Compatible
       devices      devices       devices
```

The application can select the best available architecture based on device capabilities.

This creates a path toward supporting:

- Traditional Bluetooth hearing devices.
- LE Audio devices.
- Auracast-compatible devices.

---

# 21. Finalized V1 Architecture

## Core

**Source device + software-defined multi-output Bluetooth audio engine.**

## Source

Initial:

**Laptop**

Later:

**Android**

## Bluetooth

Initial investigation:

**Bluetooth Classic / A2DP**

Parallel compatibility investigation:

**Bluetooth LE Audio**

Future scalable mode:

**Auracast**

## Hardware

**No custom hardware for V1 prototype.**

Optional:

**USB Bluetooth adapter**

Future:

**Dedicated transmitter hardware only if required.**

## Audio

One source stream is copied/routed to each connected device.

## Synchronization

Per-device buffering + latency measurement + controlled delay + resynchronization.

## Device management

Individual Bluetooth discovery, pairing, connection, disconnection, reconnection, and status monitoring.

## Initial device count

**2 devices**

## V1 target

**2–4 devices**

## Future target

**5–10+ devices**, with LE Audio/Auracast evaluated as the preferred scalable architecture.

---

# 22. Final Product Statement

> **The Multi-Hearing-Device Audio Hub is a source-side software system that uses the source device's Bluetooth hardware to discover and individually pair multiple compatible hearing devices, then routes the same audio stream to all connected devices while managing buffering, latency, synchronization, and connection reliability.**

The first prototype will **not require custom transmitter hardware**.

The first engineering task is to verify the exact Bluetooth profile and behavior of the target hearing devices and prove that **two simultaneous audio connections** can be established reliably on the selected laptop platform.

Only after that proof will the project scale to 3–4 devices, Android, LE Audio, Auracast, or dedicated hardware.
