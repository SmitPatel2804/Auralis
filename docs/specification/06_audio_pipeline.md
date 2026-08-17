# 6. Audio Pipeline

## 6.1 V1 pipeline

```text
Laptop application / browser / media player
                 |
                 v
             PipeWire
                 |
        system-audio capture
                 |
                 v
        Audio Capture Engine
                 |
          canonical PCM
                 |
                 v
       Audio Processing Engine
                 |
                 v
       Multi-Output Distributor
           /      |      \
          v       v       v
       Queue A  Queue B  Queue C
          |       |       |
       Sync A   Sync B   Sync C
          |       |       |
      PW Out A PW Out B PW Out C
          |       |       |
       BT Sink A BT Sink B BT Sink C
          |       |       |
       Device A Device B Device C
```

## 6.2 Canonical internal format

The source specification says the internal path should remain high quality and avoid unnecessary compression.

A proposed prototype canonical format is:

- PCM;
- fixed sample rate selected for the experiment;
- consistent channel count;
- float or integer representation selected once for the internal engine.

The exact production format is not specified by the source and should be chosen during implementation/testing.

## 6.3 Source capture

Development order:

### Stage 1 — WAV/file source

Use a known local test file first.

Advantages:

- deterministic;
- repeatable;
- no system-capture variables;
- easier verification.

### Stage 2 — generated tone/pulse

Useful for:

- basic route checks;
- latency experiments;
- dropout detection.

### Stage 3 — system-audio capture

Capture audio currently rendered by applications on the laptop.

This is the product-relevant source path.

## 6.4 Distribution

The distributor should preserve a single source frame/time sequence.

A block can conceptually carry:

```cpp
struct AudioBlock {
    uint64_t sequence;
    uint64_t firstFrameIndex;
    Timestamp sourceTime;
    AudioBuffer pcm;
};
```

Each output consumes the same logical sequence.

## 6.5 Per-device buffering

Each output requires its own queue because:

- transport timing differs;
- one endpoint can stall;
- compensation delays differ;
- reconnecting endpoints need refill;
- a device may be removed independently.

## 6.6 Backpressure policy

One bad endpoint must not block the entire source.

Possible policy:

- maintain bounded queues;
- if an output falls behind beyond a threshold, mark it degraded;
- discard/resync that endpoint according to policy;
- keep other outputs running.

Exact thresholds are open V1 requirements.

## 6.7 Encoding

Initial architecture:

```text
Auralis PCM
   |
PipeWire
   |
host Bluetooth audio integration
   |
Bluetooth codec/transport
```

Auralis should not perform extra Bluetooth decode/re-encode cycles.

## 6.8 Volume

The source specification includes volume in the later UI but does not define semantics.

Open decision:

- one master gain only;
- per-device digital gain;
- device hardware gain;
- combination.

For the feasibility prototype, keep volume control minimal to avoid confounding synchronization and routing tests.

## 6.9 Output health metrics

Minimum per-output telemetry:

- stream state;
- frames submitted;
- queue depth;
- underruns;
- overruns;
- device disconnects;
- time since last state transition;
- current compensation target.
