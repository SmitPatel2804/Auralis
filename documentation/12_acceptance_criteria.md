# 12. Acceptance Criteria Framework

## 12.1 Source-derived V1 themes

The source specification defines V1 success around:

- multiple simultaneous target devices;
- stable connections;
- same source audio;
- no unnecessary degradation;
- acceptable synchronization;
- reconnect/recovery;
- simple device management.

The source does **not** define final numeric thresholds.

This document therefore provides a framework, not invented final numbers.

## 12.2 Required measurable criteria before V1 lock

### Connectivity

Define:

- supported target models;
- minimum simultaneous device count;
- connection success rate;
- maximum allowed unexpected disconnects during a fixed test.

### Audio continuity

Define:

- minimum long-run test duration;
- maximum underrun/dropout count;
- behavior when one endpoint becomes unhealthy.

### Synchronization

Define:

- maximum inter-device skew;
- measurement method;
- allowed drift over time;
- target resynchronization time.

### Source latency

Define:

- use cases requiring video lip-sync;
- maximum acceptable source-to-audible delay;
- measurement method.

### Recovery

Define:

- reconnect retry behavior;
- maximum recovery time;
- whether unaffected devices must remain uninterrupted;
- rejoin synchronization threshold.

### Usability

Define:

- number of user actions required to add a paired device;
- visible states;
- recoverable errors;
- whether pairing is fully app-orchestrated or invokes OS UI.

## 12.3 Example acceptance-test template

```text
Requirement ID:
Scenario:
Environment:
Target devices:
Source:
Preconditions:

Action:

Expected:
- Device A:
- Device B:
- Unaffected outputs:
- UI state:

Metrics:
- Run duration:
- Underruns:
- Disconnects:
- Inter-device skew:
- Recovery time:

Pass/Fail:
Evidence:
```

## 12.4 Core proof criterion

Before calling the software-only architecture feasible:

> Two exact target hearing devices must simultaneously render the same deterministic source from one laptop, with independently observable output streams and sufficient stability to proceed to synchronization testing.

## 12.5 V1 release criterion

Do not define V1 release only as “up to four devices connected”.

V1 must combine:

```text
Supported hardware
+ actual simultaneous rendering
+ stability
+ measurable synchronization
+ recovery
+ diagnostics
+ usable management
```
