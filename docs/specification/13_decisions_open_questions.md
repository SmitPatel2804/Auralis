# 13. Architecture Decisions and Open Questions

## 13.1 Decisions currently adopted for the prototype

### AD-001 — Laptop before Android

**Decision:** Build the first technical prototype on a laptop.

**Reason:** Matches the supplied specification's priority and reduces mobile platform restrictions during proof-of-concept work.

### AD-002 — Linux as the first reproducible engineering baseline

**Decision:** Use Linux for the first implementation.

**Status:** Proposed implementation decision, not a requirement from the source specification.

### AD-003 — C++20 core

**Decision:** Implement the audio/router/sync core in C++20.

### AD-004 — BlueZ for Bluetooth control

**Decision:** Use the host Linux Bluetooth stack; control it through exposed D-Bus APIs.

### AD-005 — PipeWire for Linux audio graph

**Decision:** Use PipeWire for source capture and target audio sinks.

### AD-006 — Host handles Bluetooth codec/transport initially

**Decision:** Do not implement custom A2DP/codec transport in the first prototype.

### AD-007 — CLI probes before GUI

**Decision:** Prove device and transport feasibility with small command-line tools before building the product UI.

### AD-008 — Per-device independent queues

**Decision:** Every active output gets independent buffering/state so one failing device does not block all others.

### AD-009 — Manual delay before automatic synchronization

**Decision:** Prove controllable per-output delay before assuming automatic latency measurement is possible.

## 13.2 Open technical questions

### OQ-001 — Exact target hearing-device models

Required before Bluetooth implementation can be considered final.

### OQ-002 — Actual audio transport/profile

Does each target device appear as a standard host audio sink? If not, what mechanism is required?

### OQ-003 — First Linux environment

Choose exact distribution/version, kernel, PipeWire version and BlueZ version.

### OQ-004 — Bluetooth adapter

Record internal adapter chipset/firmware. Determine whether it is sufficient.

### OQ-005 — BlueZ-device to PipeWire-node mapping

How reliably can the application correlate discovered Bluetooth identity with the audio endpoint?

### OQ-006 — Maximum simultaneous sinks

Must be determined experimentally for the exact setup.

### OQ-007 — End-to-end latency measurement

What observable or external mechanism will measure final device playback timing?

### OQ-008 — Synchronization tolerance

What numeric inter-device skew is acceptable for the intended environment?

### OQ-009 — Source latency tolerance

What maximum end-to-end delay is acceptable for speech/music/video?

### OQ-010 — Join/rejoin behavior

Should a recovering device fade in, hard-join after prebuffering, or use another policy?

### OQ-011 — Volume semantics

Master gain, per-device gain, device hardware volume, or combination?

### OQ-012 — Supported-device definition

Does V1 support only validated models, or best-effort generic Bluetooth sinks?

### OQ-013 — Security/privacy requirements

Define treatment of:

- paired-device identifiers;
- exported diagnostic logs;
- user audio (should not be persisted unless explicitly required);
- permissions and local storage.

## 13.3 Decision tree after two-device experiment

```text
Two simultaneous target devices work?
 |
 +-- YES --> Measure stability and sync
 |             |
 |             +--> 3-4 devices viable? --> Continue software V1
 |             |
 |             +--> scaling fails --> investigate radio/transport
 |
 +-- NO --> Why?
             |
             +--> target device/profile issue --> change compatibility path
             |
             +--> host audio/BT stack issue --> alternate host/architecture
             |
             +--> adapter/radio issue --> test known USB adapter
             |
             +--> fundamental independent-link limit --> evaluate LE Audio/
                                                      Auracast/dedicated HW
```
