# 7. Synchronization and Latency

## 7.1 Two different latency goals

The source specification correctly separates:

### Source-to-device latency

Time between the original source event and audible output.

Important for video/lip-sync.

### Device-to-device synchronization

Difference in playback time between receivers.

Important for simultaneous listening, especially when devices are acoustically near one another.

These are related but not identical.

## 7.2 Compensation concept

If observed latencies are:

```text
A = 100 ms
B = 135 ms
C = 117 ms
```

a conceptual alignment target is:

```text
A additional delay = 35 ms
B additional delay =  0 ms
C additional delay = 18 ms
```

The application intentionally delays faster paths toward a common target.

## 7.3 Critical unresolved question

The source specification requires “latency measurement” but does not define how end-to-end audible latency will be observed.

Software-visible timing may not include all of:

- application queues;
- OS audio graph;
- codec buffering;
- Bluetooth controller scheduling;
- wireless transport;
- hearing-device buffering;
- hearing-device DSP;
- acoustic output.

Therefore automatic compensation must not be assumed until measurement observability is proven.

## 7.4 Development sequence

### Sync 0 — no compensation

Send the same source timeline to two outputs.

Purpose: prove transport only.

### Sync 1 — manual per-device delay

Provide a test-only setting:

```text
Device A delay: +0 ms
Device B delay: +25 ms
```

Purpose: prove that independent controlled delay works.

### Sync 2 — instrumented estimation

Use whatever host/stream timing observations are actually available.

Purpose: estimate relative transport differences.

### Sync 3 — automated compensation

Calculate target delays and update per-device queue depth.

### Sync 4 — drift management

Track whether initially aligned devices diverge over long playback.

## 7.5 Drift

Initial alignment is not the same as long-term synchronization.

Potential long-duration issues include:

- endpoint clock differences;
- variable Bluetooth buffering;
- reconnection;
- transient radio congestion;
- host scheduling variation.

A future drift controller may need to:

- monitor relative timing;
- adjust target buffer depth slowly;
- perform controlled resynchronization;
- avoid abrupt audible discontinuities.

## 7.6 Synchronization state

Per-device logical state:

```text
Uncalibrated
Buffering
Aligned
Degraded
Resyncing
Aligned
```

## 7.7 Proposed metrics framework

The exact numeric values should be determined by product requirements and experiments.

Track:

| Metric | Definition |
|---|---|
| Estimated output latency | Host-observable estimate for the output path |
| Compensation delay | Intentional Auralis delay |
| Inter-device skew | Relative playback timing difference |
| Buffer depth | PCM time queued for an output |
| Drift rate | Relative change in timing over time |
| Resync duration | Time from degraded/reconnect to aligned |
| Underrun count | Output starvation events |

## 7.8 External measurement rig

If software timestamps cannot represent acoustic end-to-end timing, validation may require an external rig.

Conceptual approach:

- play an impulse/pulse;
- capture acoustic output from each receiver with independent measurement channels;
- compare arrival times;
- correlate with Auralis timestamps.

The exact hearing-device-safe and practical measurement method must be designed once target hardware is known.

## 7.9 Principle

Do not claim “synchronized” only because streams started at approximately the same time.

Synchronization must ultimately be tied to a measurable acceptance criterion.
