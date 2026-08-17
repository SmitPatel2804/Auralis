# 11. Risk Register

## R1 — Hearing-device transport incompatibility

**Risk:** The target hearing device may not expose the expected standard Bluetooth audio sink behavior.

**Impact:** Core architecture may not work for the intended hardware.

**Mitigation:**

- identify exact models first;
- build compatibility matrix;
- test one device before building multi-output features;
- investigate actual transport/profile used.

**Gate:** Phase 0 / single-device proof.

---

## R2 — OS or Bluetooth stack prevents multiple audio outputs

**Risk:** Linux host stack may not permit or reliably sustain the desired independent Bluetooth sinks for the target setup.

**Impact:** Software-only architecture may fail.

**Mitigation:**

- build two-output probe early;
- keep test environment reproducible;
- isolate audio-routing vs Bluetooth-control failure;
- evaluate alternate transport only after evidence.

---

## R3 — Bluetooth adapter/radio capacity

**Risk:** Adapter cannot maintain required links/bandwidth.

**Impact:** Dropouts, connection failures or low practical device count.

**Mitigation:**

- log adapter information;
- test two/three/four devices;
- test external USB adapter only if evidence points to radio/adapter limits.

---

## R4 — Synchronization observability

**Risk:** Host-visible timestamps do not represent final acoustic latency.

**Impact:** Automatic alignment may be inaccurate.

**Mitigation:**

- implement manual delay first;
- instrument all observable stages;
- develop external measurement rig if needed;
- do not promise automatic sync before validating measurement.

---

## R5 — Clock drift / variable latency

**Risk:** Devices align initially but diverge over time.

**Impact:** Audible echo/comb-like effects for nearby listeners.

**Mitigation:**

- long-duration tests;
- track queue/timing changes;
- build controlled resync/drift strategy.

---

## R6 — One bad device affects all outputs

**Risk:** Shared blocking or backpressure stalls the whole session.

**Impact:** Poor reliability.

**Mitigation:**

- independent bounded queues;
- isolate per-output failures;
- keep source timeline independent of a single sink.

---

## R7 — Reconnection creates severe resynchronization artifacts

**Risk:** A returned device joins late or with incorrect buffer phase.

**Impact:** User hears echo/misalignment.

**Mitigation:**

- rebuffer before join;
- explicit Resyncing state;
- controlled rejoin policy.

---

## R8 — System audio capture adds instability

**Risk:** The initial WAV proof succeeds but live system capture adds format/routing problems.

**Impact:** Product path fails despite basic transport proof.

**Mitigation:**

- introduce system capture only after deterministic two-output proof;
- normalize format at capture boundary.

---

## R9 — V1 acceptance criteria remain subjective

**Risk:** Terms like “acceptable latency” or “sufficient synchronization” are not measurable.

**Impact:** No defensible release gate.

**Mitigation:**

- define numeric thresholds after early measurements and intended use-case review;
- attach every hardware test to recorded metrics.

---

## R10 — Premature GUI/product work

**Risk:** Significant effort is spent before core transport feasibility is established.

**Impact:** Schedule waste and architectural churn.

**Mitigation:**

- CLI probes first;
- GUI after two-device/system-audio proof.
