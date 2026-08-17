# 4. Component Design

## 4.1 Application shell

Responsibilities:

- initialize services;
- load settings;
- coordinate shutdown;
- expose application-level state;
- start diagnostics.

Suggested interface:

```cpp
class ApplicationController {
public:
    bool initialize();
    void shutdown();
};
```

## 4.2 Device Manager

Responsibilities:

- enumerate Bluetooth adapters;
- start/stop discovery;
- maintain discovered-device model;
- initiate pairing;
- request connection/disconnection;
- track device state;
- expose metadata;
- trigger reconnect policy;
- map Bluetooth devices to audio endpoints where possible.

Important: the manager must not treat “Bluetooth connected” as equivalent to “audio ready”.

Suggested logical states:

```text
Unknown
Discovered
Pairing
Paired
Connecting
Connected
AudioEndpointPending
AudioReady
Disconnecting
Disconnected
Reconnecting
Failed
```

## 4.3 Audio Capture Engine

V1 input priority:

1. laptop/system audio;
2. optional microphone later;
3. external input later.

Responsibilities:

- open a capture stream;
- normalize input format;
- timestamp audio blocks;
- deliver blocks to the distributor;
- report capture underruns/state changes.

## 4.4 Audio Processing Engine

Responsibilities may include:

- channel layout normalization;
- sample-format conversion;
- resampling;
- level/headroom policy;
- optional limiter;
- source timeline management.

Keep this stage minimal during feasibility testing.

## 4.5 Multi-Output Router

The router duplicates references/copies of each source block into independent output pipelines.

Responsibilities:

- maintain active output set;
- add/remove an output without restarting unaffected outputs;
- feed per-device queues;
- isolate a slow/failing device;
- expose queue health.

Desired rule:

> A problem on Device B must not automatically stall Devices A and C.

## 4.6 Output Session

One per active hearing device.

Responsibilities:

- own PipeWire output stream;
- own ring buffer;
- maintain target buffer depth;
- receive sync compensation;
- report frames sent;
- detect underruns/overruns;
- transition safely when device disappears.

Possible structure:

```cpp
struct OutputMetrics {
    double targetDelayMs;
    double bufferDepthMs;
    double estimatedLatencyMs;
    double driftPpm;
    uint64_t framesWritten;
    uint64_t underruns;
    uint64_t overruns;
};
```

## 4.7 Synchronization Engine

Responsibilities:

- define the common source timeline;
- receive timing observations;
- calculate per-output target delay;
- perform initial alignment;
- track long-duration relative drift;
- request safe resynchronization;
- avoid abrupt audible corrections where possible.

The exact automatic latency measurement mechanism is an open technical question and must be experimentally validated.

## 4.8 Session Manager

Coordinates the user-visible playback session.

Responsibilities:

- start/stop source capture;
- choose active devices;
- wait for required output readiness;
- initiate playback;
- handle device join/leave;
- coordinate reconnect;
- apply failure policy.

## 4.9 Diagnostics Service

Must be introduced early.

Record:

- run/session ID;
- software version;
- OS/kernel;
- Bluetooth adapter;
- target device identity/model;
- connection events;
- audio stream events;
- buffer statistics;
- underruns;
- output timing observations;
- reconnect attempts;
- error codes/messages.

## 4.10 UI

Initial production-oriented pages:

### Home
- source status;
- connected devices;
- start/stop.

### Add device
- scanning;
- device list;
- pair/connect state;
- errors.

### Device details
- name;
- Bluetooth state;
- audio readiness;
- latency/sync status;
- remove/disconnect.

### Diagnostics
- session duration;
- output health;
- underruns;
- reconnects;
- debug export.

The UI should be built only after the CLI probes prove the basic transport.
