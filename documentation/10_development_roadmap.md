# 10. Development Roadmap

## Milestone 0 — Reproducible development machine

Lock:

- Linux distribution/version;
- compiler version;
- Qt version;
- PipeWire environment;
- BlueZ environment;
- Bluetooth adapter;
- first target hearing-device model.

Deliverable:

- buildable repository;
- environment/setup notes.

## Milestone 1 — Bluetooth probe

Implement:

- adapter enumeration;
- scan;
- device model;
- pair;
- connect;
- disconnect;
- event logging.

No GUI required.

Exit gate:

> One target hearing device is reproducibly managed.

## Milestone 2 — Single audio output

Implement:

- PipeWire sink enumeration;
- deterministic test source;
- output stream;
- metrics.

Exit gate:

> One target hearing device plays stable audio.

## Milestone 3 — Two simultaneous outputs

Implement:

- source block sequence;
- distributor;
- two independent output sessions;
- per-output buffers;
- structured diagnostics.

Exit gate:

> Same program content plays continuously on both devices.

This is the highest-value milestone in the project.

## Milestone 4 — System-audio capture

Implement:

- laptop/system audio capture;
- start/stop;
- capture metrics.

Exit gate:

> Browser/media-player audio can be delivered to two active target devices.

## Milestone 5 — Recovery

Implement:

- output fault isolation;
- disconnect detection;
- reconnect;
- endpoint rebind;
- output rejoin.

Exit gate:

> One device can disconnect/reconnect while unaffected devices continue.

## Milestone 6 — Synchronization v0/v1

Implement:

- manual per-device delay;
- queue-depth control;
- timing observations;
- synchronization state.

Exit gate:

> Controlled delay measurably improves relative alignment.

## Milestone 7 — Automatic synchronization research

Implement only after measurement feasibility is understood.

Work:

- latency estimator;
- reference target;
- automated compensation;
- drift observation;
- resync policy.

## Milestone 8 — Qt/QML product UI

Build:

- home;
- scan/add device;
- connected device list;
- source controls;
- connection status;
- sync status;
- diagnostics view.

## Milestone 9 — Three/four-device scaling

Test:

- same models;
- mixed models;
- long-duration playback;
- radio load;
- CPU/memory;
- recovery.

Exit gate:

> Determine whether independent links meet V1 2–4 device target.

## Milestone 10 — Architecture review

Based on evidence choose:

- remain software-only;
- test alternate USB Bluetooth adapter;
- deepen LE Audio investigation;
- evaluate Auracast;
- evaluate dedicated transmitter hardware;
- start Windows port.

## Repository layout

```text
auralis-audio-hub/
├── CMakeLists.txt
├── cmake/
├── src/
│   ├── app/
│   ├── bluetooth/
│   ├── audio/
│   ├── sync/
│   ├── session/
│   ├── diagnostics/
│   └── ui/
├── tools/
│   ├── bt_probe/
│   ├── audio_probe/
│   ├── multi_output_probe/
│   └── latency_probe/
├── tests/
├── scripts/
└── docs/
```
