# 9. Testing and Validation Plan

## 9.1 Testing philosophy

The project is fundamentally hardware-and-stack dependent. Automated unit tests are necessary but insufficient.

Use three layers:

1. pure software tests;
2. OS integration tests;
3. real hearing-device hardware tests.

## 9.2 Phase 0 — compatibility research

For each target model record:

- exact model;
- manufacturer;
- Bluetooth technology/profile information available from manufacturer/system;
- pairing behavior;
- whether Linux sees it as an audio sink;
- any required companion-app behavior;
- connection limits observed.

Deliverable: compatibility matrix.

## 9.3 Probe A — Bluetooth CLI

Target executable:

```text
auralis-bt-probe
```

Capabilities:

- list adapters;
- scan;
- list devices;
- pair selected device;
- connect/disconnect;
- print state changes.

Pass condition:

> One target device can be managed reproducibly.

## 9.4 Probe B — audio endpoint CLI

Target executable:

```text
auralis-audio-probe
```

Capabilities:

- enumerate PipeWire audio sinks;
- identify Bluetooth sink;
- play deterministic test PCM;
- report stream state.

Pass condition:

> One target device receives stable audio.

## 9.5 Probe C — two-output CLI

Target executable:

```text
auralis-multi-output-probe
```

Capabilities:

- load WAV/test source;
- select two Bluetooth sinks;
- open two output streams;
- duplicate the same source timeline;
- log queue/stream state;
- run for a configured duration.

Pass condition:

> Both target devices receive continuous audio simultaneously.

This is the core feasibility experiment.

## 9.6 Probe D — system audio

Capture laptop audio and route it to two outputs.

Test sources:

- speech;
- music;
- video.

Pass condition:

> Product-relevant source capture works without destroying two-output stability.

## 9.7 Probe E — manual synchronization

Expose delay adjustment per device.

Procedure:

- play impulse/pulse or suitable sync content;
- adjust delay;
- record chosen value;
- repeat;
- observe stability.

Purpose: prove controllable per-output compensation.

## 9.8 Hardware test matrix

| Test | Devices | Duration | Key measurements |
|---|---:|---:|---|
| Basic audio | 1 | 10 min | errors, underruns |
| Core proof | 2 | 30 min | continuity, skew observation |
| Long run | 2 | 2+ hr | disconnects, drift, underruns |
| Same model | 2 | TBD | relative timing |
| Different models | 2 | TBD | compatibility/timing |
| Scale | 3 | TBD | stability/resources |
| Scale | 4 | TBD | stability/resources |
| Disconnect B | 2+ | repeated | A continuity, B recovery |
| Power cycle B | 2+ | repeated | reconnect/resync |
| Range | 2+ | TBD | failures/recovery |
| Interference | 2+ | TBD | dropouts |
| Video | 2+ | TBD | lip-sync/source latency |
| Low battery | target | TBD | real-world state |

## 9.9 Session record

Every hardware run should produce:

```text
Session ID
Date/time
Git revision
OS version
Kernel
PipeWire version
BlueZ version
Bluetooth adapter
Device models
Device IDs
Audio source
Sample format
Run duration
Connection events
Output stream events
Underruns/overruns
Buffer statistics
Latency/sync observations
Result
Tester notes
```

## 9.10 Unit tests

Good candidates:

- ring-buffer correctness;
- overflow/underflow behavior;
- frame/timestamp conversions;
- distributor fan-out;
- state machines;
- reconnect backoff;
- compensation calculations;
- drift-controller math;
- metrics aggregation.

## 9.11 Acceptance discipline

A test should not be recorded as “pass” based only on “both devices connected”.

For a multi-output pass, both devices must actually render the intended audio for the required test duration.
