# 1. Project Overview

## 1.1 Product definition

Auralis is a source-side multi-hearing-device audio hub.

The desired behavior is:

```text
One source
   |
   +--> Hearing Device A
   +--> Hearing Device B
   +--> Hearing Device C
   +--> ...
```

All active devices should receive the same program content while the application manages connection state, buffering, latency and synchronization.

## 1.2 Initial platform scope

The supplied specification identifies laptop as the initial technical target, with Android later.

For the first engineering implementation this pack proposes a narrower reproducible baseline:

- Linux laptop.
- One known Linux distribution/version.
- One known Bluetooth adapter/chipset.
- One exact hearing-device model for initial testing.
- A second device of the same model if available.
- A fixed test audio file before system-audio capture is introduced.

This reduces variables during feasibility testing.

## 1.3 V1 source-derived goals

The source specification expects the product to provide:

- Device discovery.
- Individual pairing and connection.
- Multiple simultaneous target devices.
- Same source audio delivered to all active devices.
- Basic synchronization.
- Reconnection handling.
- Device status.
- Diagnostics.
- A simple management UI.
- An initial two-device proof, scaling toward 2–4 devices.

## 1.4 Non-goals for the first laptop proof

The first laptop proof should **not** attempt to solve all of the following immediately:

- Android.
- Production packaging.
- Custom transmitter hardware.
- Large-scale 5–10+ receiver support.
- Auracast as a mandatory transport.
- Sophisticated automatic latency calibration.
- Polished GUI.
- Broad hearing-device compatibility.
- Cross-platform abstraction before the Linux proof works.

## 1.5 Architecture validation gates

### Gate A — Device compatibility

For the exact hearing-device model determine:

- Bluetooth technology used.
- Whether it exposes a standard audio sink.
- Supported profile/path.
- Pairing behavior.
- Whether host audio is accepted.
- Any manufacturer-specific requirements.

**Pass:** one target device can be paired/connected and receives stable audio.

### Gate B — Two-device transport

Establish two simultaneous device sessions.

**Pass:** both devices receive the same test stream continuously.

### Gate C — Observability

Instrument enough of the audio path to record:

- Output stream state.
- Buffer depth.
- Underruns/overruns.
- stream timing where available.
- disconnect/reconnect events.
- device identity.
- run duration.

**Pass:** failures can be distinguished from simple subjective reports.

### Gate D — Synchronization feasibility

Determine whether device-to-device timing can be measured or estimated well enough to support compensation.

**Pass:** controlled delay can improve measured/perceived alignment and remain stable for a useful duration.

### Gate E — Scaling

Repeat with three and four target devices.

**Pass:** connectivity, audio continuity and synchronization remain inside the selected V1 criteria.

## 1.6 Key architectural principle

Dedicated transmitter hardware is a fallback, not the starting point.

Use the laptop's own Bluetooth system first. Introduce:

1. an external USB adapter only if evidence points to the adapter/radio as the limiting factor;
2. dedicated transmitter hardware only if the host architecture cannot meet the requirements.

## 1.7 Product milestone definitions

To avoid ambiguity between “prototype”, “MVP” and “V1”, use:

| Milestone | Meaning |
|---|---|
| Feasibility Prototype | 1 target device, stable audio |
| Core Proof | 2 simultaneous target devices |
| V1 Laptop | 2–4 devices, recovery, diagnostics, synchronization, usable UI |
| V2 | 5–10+ devices and/or alternate scalable transport |
