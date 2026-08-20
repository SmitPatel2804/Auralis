# AURALIS — PHASE 8 RELIABILITY, TESTING & PRODUCTION HARDENING MASTER IMPLEMENTATION PROMPT

**Project:** Auralis  
**Target:** Linux desktop / Ubuntu engineering baseline  
**Implementation phase:** Phase 8 — Reliability, Testing and Production Hardening  
**Assumed completed baseline:** Phases 0, 1, 2, 3, 4, 5, 6 and 7 are completed and must remain working  
**Primary language:** C++20  
**Desktop framework:** Qt 6 / Qt Quick / QML  
**Bluetooth integration:** BlueZ over D-Bus  
**Audio integration:** native PipeWire integration  
**Build system:** CMake + Ninja  
**Initial package target:** Debian package (`.deb`)  
**Purpose:** Give Cursor/another AI coding agent a repository-aware, implementation-grade contract for taking the existing Auralis prototype from the completed Phase 7 baseline to a resilient, testable, installable desktop application without rewriting already-working subsystems.

---

# 1. ROLE AND MISSION

You are the senior Linux desktop reliability engineer, C++/Qt architect, Bluetooth/PipeWire integration engineer, test engineer, and release engineer responsible for implementing **Phase 8 of Auralis** inside the existing repository.

Phases 0–7 are the established baseline. Treat the code currently in the repository as authoritative. Do not replace working systems simply because a different architecture would be aesthetically cleaner.

Your Phase 8 mission is to convert the existing functional Auralis application into a **dependable desktop application that can survive expected service, device, graph, process, and laptop lifecycle failures; preserve user intent; produce actionable diagnostics; pass rigorous automated and hardware-backed tests; and install predictably as a `.deb` package.**

The authoritative long-form roadmap defines the Phase 8 objective as:

> Convert the prototype into a dependable desktop application.

The authoritative Phase 8 exit condition requires Auralis to:

- install predictably;
- launch reliably;
- recover from expected service/device failures;
- preserve sessions;
- pass automated tests;
- function without manual terminal intervention during normal product use.

This is **not** a cleanup-only phase and **not** a documentation-only phase. Phase 8 is complete only when the reliability behavior exists in real code, is covered by deterministic tests, is exercised on the actual Linux stack where appropriate, and the package/install path is verified.

---

# 2. AUTHORITATIVE PHASE 8 FAILURE SET

The roadmap explicitly requires handling the following conditions:

```text
Bluetooth adapter disappears
Bluetooth service restarts
PipeWire restarts
WirePlumber restarts
Device turns off
Device battery dies
Device leaves range
Device reconnects
Audio node changes
Audio profile changes
Pairing fails
Connection times out
Application restarts
Laptop suspends
Laptop resumes
D-Bus reconnects
```

Do not reduce Phase 8 to only “add more reconnect retries”.

The Phase 8 implementation must reason about the complete chain:

```text
USER INTENT
    |
    v
PERSISTED LOGICAL STATE
    |
    v
SERVICE AVAILABILITY
BlueZ / D-Bus / PipeWire / policy graph
    |
    v
PHYSICAL DEVICE AVAILABILITY
    |
    v
AUDIO ENDPOINT AVAILABILITY
    |
    v
AURALIS ROUTE AVAILABILITY
    |
    v
SESSION HEALTH
    |
    v
RECOVERY / RESTORATION / ESCALATION
```

The recovery system must preserve the distinction between:

- **desired state** — what the user asked Auralis to maintain;
- **observed state** — what BlueZ/PipeWire currently report;
- **transient recovery state** — what Auralis is currently attempting;
- **terminal/error state** — what requires user action or exhausted recovery.

Never fabricate observed success from desired state.

---

# 3. REPOSITORY-SPECIFIC BASELINE FROM THE SUPPLIED PHASE 7 SNAPSHOT

Before editing anything, verify these statements against the repository. They are important because Phase 8 must extend existing mechanisms rather than duplicate them.

The supplied repository snapshot already contains significant reliability groundwork:

## 3.1 Existing Bluetooth reliability mechanisms

The current tree contains concepts/classes such as:

```text
BlueZDbusClient
BluetoothManager
DeviceLifecycleManager
ReconnectPolicy
DeviceReconnectMetadata
BluetoothDbusError
DeviceRegistry
```

The existing BlueZ client uses `QDBusServiceWatcher` for BlueZ registration/unregistration observation.

The existing reconnect path already has:

- retry scheduling;
- bounded attempts;
- exponential/backoff-style timing;
- pause/resume support;
- reconnect exhaustion reporting;
- terminal failure reporting;
- contention handling;
- user-disconnect suppression semantics;
- tests around reconnect timing and cancellation.

**Do not create a second independent Bluetooth retry engine.**

## 3.2 Existing session recovery mechanisms

The current session layer already contains recovery concepts including policies equivalent to:

```text
None
RestoreRoutesOnly
ReconnectAndRestore
```

It already includes degraded/recovering session behavior, route reconciliation, managed reconnect integration, retry semantics, generation/stale-callback protections, and session persistence.

**Do not replace SessionManager recovery with a competing global session engine.**

Phase 8 must coordinate service-level recovery around it.

## 3.3 Existing persistence

The repository already contains:

- `ConfigurationManager` backed by `QSettings`;
- session persistence using JSON;
- atomic session writes using `QSaveFile`;
- schema-version awareness;
- saved session restoration behavior;
- window/UI preferences;
- logging settings;
- restore-last-session setting.

Extend persistence deliberately. Do not discard the existing format without a migration story.

## 3.4 Existing logging and diagnostics

The current project already has:

- central Qt message-handler based logging;
- timestamp/severity/category output;
- optional file logging;
- runtime enable/disable handling;
- hardened file-logging activation failure behavior;
- a diagnostics model exposed to the GUI;
- bounded diagnostics-model capacity.

The Phase 7 audit records a final file-logging correction and a passing Phase 7 exit gate.

Phase 8 should make logging **more operationally useful**, not create a parallel logger.

## 3.5 Existing PipeWire architecture

The repository already has native PipeWire classes such as:

```text
PipeWireConnection
PipeWireManager
PipeWireObjectStore
AudioEndpointRegistry
EndpointResolver
AudioRouter
RoutePlanner
LinkManager
```

The manager already uses lifecycle/generation concepts around asynchronous events.

Do not replace this with shell calls or a different audio framework.

## 3.6 Current Phase 8 gaps visible in the supplied snapshot

The supplied snapshot does **not** appear to contain a dedicated centralized `RecoveryManager` or equivalent service-level recovery orchestrator.

The supplied snapshot does **not** appear to contain a complete suspend/resume coordinator using the Linux session/system lifecycle signal.

The supplied snapshot does **not** appear to contain a full `.deb` packaging/install pipeline (`install(...)`, CPack Debian configuration, desktop integration metadata, etc.).

The current `README.md` appears stale relative to the Phase 7 audit and may still state that Phase 7+ is not implemented. Correct stale documentation only after implementation/test evidence supports the new status.

These observations are reconnaissance hints, **not permission to assume the exact code is unchanged**. Inspect before editing.

---

# 4. NON-NEGOTIABLE PHASE BOUNDARY

## 4.1 In scope

Phase 8 includes:

1. Centralized recovery orchestration.
2. BlueZ service loss/restart reconciliation.
3. D-Bus availability/reconnect handling.
4. PipeWire loss/restart/re-enumeration handling.
5. WirePlumber/policy-driven graph churn recovery.
6. Bluetooth adapter disappearance/return handling.
7. Device disconnect/reconnect recovery integration.
8. Audio endpoint disappearance/reappearance and changed-ID rebinding.
9. Bluetooth audio profile/node changes.
10. Route restoration after external graph/service changes.
11. Session restoration after recoverable infrastructure failures.
12. Retry/backoff/escalation policy coordination.
13. Laptop suspend/resume handling.
14. Persistence hardening and missing Phase 8 state persistence.
15. Logging/diagnostics hardening.
16. Long-running/stress/repetition tests.
17. Failure-injection tests.
18. Sanitizer-oriented verification where practical.
19. Real-stack/live-hardware validation with explicit opt-in gates.
20. Clean shutdown and repeated startup/shutdown reliability.
21. CMake install rules.
22. Initial `.deb` packaging.
23. Package installation/launch/uninstall verification.
24. Documentation of recovery behavior, operational limits, and test evidence.
25. Final Phase 8 implementation audit / validation report.

## 4.2 Out of scope

Do **not** turn Phase 8 into unrelated future development.

Do not implement, unless the current roadmap has been explicitly changed in-repository:

- a new LE Audio stack;
- LC3/BAP/PACS/ASCS;
- Auracast;
- a Windows port;
- an Android port;
- a completely new synchronization engine;
- automatic acoustic latency estimation research;
- a total GUI redesign;
- a custom Bluetooth transport stack;
- a replacement for BlueZ;
- a replacement for PipeWire;
- a permanent dependency on CLI tools such as `bluetoothctl`, `wpctl`, `pactl`, `pw-cli`, or `pw-link` in production logic;
- an application-owned replacement for WirePlumber;
- a root daemon merely to make recovery easier;
- hidden automatic restarts of system services as normal product behavior.

Developer/test scripts may use system tools to **observe** or deliberately inject faults under explicit test gates. Production Auralis behavior must not depend on those tools.

---

# 5. FIRST ACTION — DEEP REPOSITORY RECONNAISSANCE

Do not begin by creating `RecoveryManager.cpp` just because this prompt names it.

First establish the exact current architecture.

Run at minimum:

```bash
pwd
git status --short
git branch --show-current
git log --oneline --decorate -n 40

find . -maxdepth 4 -type f | sort | sed -n '1,500p'

rg -n \
  "Recovery|Reconnect|retry|backoff|pauseAll|resumeAll|QDBusServiceWatcher|PipeWireConnection|connectionState|SessionPersistence|QSaveFile|ConfigurationManager|Logger|Diagnostics|suspend|resume|PrepareForSleep|logind|install\\(|CPack|CPACK_" \
  CMakeLists.txt cmake apps src include ui tests docs
```

Inspect at minimum:

- root `CMakeLists.txt`;
- CMake options/warnings files;
- `ApplicationCore`;
- desktop application lifecycle;
- `BlueZDbusClient`;
- `BluetoothManager`;
- `DeviceLifecycleManager`;
- `ReconnectPolicy`;
- D-Bus error mapping;
- `PipeWireConnection`;
- `PipeWireManager`;
- `AudioEndpointRegistry`;
- `EndpointResolver`;
- `AudioRouter` and route/link reconciliation;
- `SessionManager`;
- `SessionStateMachine`;
- `RoutingCoordinator`;
- `SessionPersistence`;
- `ConfigurationManager`;
- `Logger`;
- `DiagnosticsLogModel`;
- `NotificationController`;
- Phase 7 GUI pages that expose health/errors/settings;
- all unit and integration test registration;
- current validation/audit docs.

Build an internal dependency map before coding:

```text
Concern                         Existing owner                    Phase 8 extension
-----------------------------------------------------------------------------------
BlueZ presence                  <actual class>                    <needed?>
System D-Bus availability       <actual class>                    <needed?>
Adapter availability            <actual class>                    <needed?>
Device reconnect                ReconnectPolicy / lifecycle       coordinate only
PipeWire connection             <actual class>                    restart/retry
Endpoint rebind                 <actual resolver>                 harden
Route restore                   AudioRouter/session coordinator   orchestrate
Session recovery               SessionManager                    service-aware trigger
Suspend/resume                  <actual/none>                     add if absent
Settings persistence            ConfigurationManager              extend
Session persistence             SessionPersistence               harden/migrate
Operational logs                Logger/DiagnosticsLogModel        structure/rotation
Packaging                       <actual/none>                     add
```

Do not implement until you know which layer already owns each behavior.

---

# 6. BASELINE BUILD AND EVIDENCE BEFORE EDITING

Do not trust an old build directory.

Capture a baseline before Phase 8 changes.

Preferred:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Also enumerate tests:

```bash
ctest --test-dir build -N
```

Record:

```text
Git revision:
Working tree state:
Compiler:
CMake:
Qt:
BlueZ:
PipeWire:
Kernel:
Baseline test count:
Baseline pass/fail/skip count:
```

If opt-in live tests report as CTest “passed” while internally skipping, document that distinction. **A skipped live test is not hardware evidence.**

If baseline fails before edits, determine whether it is:

- a real repository regression;
- missing environment dependency;
- unavailable hardware/service;
- stale test assumption.

Do not bury baseline failures under Phase 8 changes.

---

# 7. PHASE 8 ARCHITECTURAL PRINCIPLE — CENTRALIZED ORCHESTRATION, DECENTRALIZED OWNERSHIP

The roadmap asks for centralized recovery logic. Implement that without creating a god object.

A strong target model is:

```text
                    ApplicationCore
                          |
                          v
                   RecoveryManager
                   /      |       \
                  /       |        \
                 v        v         v
            Bluetooth   PipeWire   Power/Sleep
            health      health      lifecycle
                 \        |         /
                  \       |        /
                   v      v       v
                  Recovery decisions
                         |
              +----------+----------+
              |                     |
              v                     v
      existing reconnect       existing session /
      policy/lifecycle         route reconciliation
```

The central recovery component should:

- observe subsystem health;
- correlate failures;
- coordinate when retries should start/pause/resume;
- prevent conflicting recovery actions;
- trigger safe reconciliation in existing subsystem owners;
- track attempt/outcome/escalation metadata;
- expose recovery status for diagnostics/UI;
- preserve user intent through transient infrastructure loss;
- invalidate stale asynchronous work through generations/tokens;
- avoid owning low-level D-Bus or PipeWire mechanics that existing components already own.

It should **not**:

- directly manipulate raw PipeWire links if `AudioRouter` owns that;
- directly call BlueZ Device1 methods if `BluetoothManager`/lifecycle owns that;
- directly rewrite session internals if `SessionManager` owns that;
- become a second persistence backend;
- treat every failure as a full-application restart.

Name it `RecoveryManager` if that fits the current repository. If a better existing abstraction already exists, extend it instead. The behavior matters more than the filename.

---

# 8. RECOVERY DATA MODEL

Create a clear recovery model rather than scattering booleans across services.

A reasonable conceptual model may include:

```cpp
enum class RecoveryDomain {
    SystemBus,
    BluetoothService,
    BluetoothAdapter,
    BluetoothDevice,
    PipeWire,
    AudioGraph,
    Session,
    SuspendResume
};

enum class RecoveryState {
    Healthy,
    Waiting,
    Recovering,
    Reconciling,
    Degraded,
    Exhausted,
    Suspended
};

enum class RecoveryCause {
    ServiceVanished,
    ServiceRestarted,
    AdapterRemoved,
    DeviceDisconnected,
    EndpointRemoved,
    GraphReset,
    ProfileChanged,
    Timeout,
    Resume,
    Unknown
};
```

Do not mechanically use these exact enums if the repository already has compatible types.

Each tracked recovery operation should be able to answer, where relevant:

```text
What domain failed?
What resource ID is affected?
What user/session intent is being preserved?
When did recovery begin?
What attempt are we on?
What is the next retry time?
What was the last error?
Is the error retryable?
Is recovery paused because of suspend/service loss?
Has recovery been exhausted?
What event returned the system to Healthy?
What generation does this work belong to?
```

Avoid storing raw QObject pointers in persistent recovery records.

---

# 9. RECOVERY INVARIANTS

These invariants are non-negotiable.

## 9.1 Idempotency

Repeated equivalent failure events must not create duplicate timers, duplicate links, duplicate connect calls, or duplicate sessions.

Examples:

```text
BlueZ unregistered twice -> one recovery state
PipeWire error + graph removed burst -> one service recovery sequence
Device PropertiesChanged Connected=false burst -> one managed reconnect intent
Endpoint added multiple property updates -> one route reconciliation outcome
```

## 9.2 Generation safety

Any asynchronous callback from a prior service/session generation must be ignored after:

- shutdown;
- service restart;
- session stop;
- route replacement;
- suspend/resume epoch change;
- a newer recovery attempt supersedes it.

Existing generation mechanisms should be reused where possible.

## 9.3 Preserve unaffected outputs

If one device fails, healthy devices should continue whenever the underlying stack allows it.

Do not stop a whole session merely because one member is temporarily unavailable unless the session policy explicitly requires all-or-nothing behavior.

## 9.4 Never resurrect explicit user intent to stop

Recovery must distinguish an unexpected failure from:

- user disconnect;
- user deactivation;
- device removal/forget;
- session stop;
- session delete;
- app shutdown.

An explicitly stopped route/session/device must not be resurrected by a delayed recovery callback.

## 9.5 Observed-state authority

Do not mark a device connected, endpoint ready, route active, or session recovered until the corresponding authoritative subsystem reports it.

## 9.6 Bounded retries

All automatic retry loops must be bounded or otherwise safely throttled.

Never create a hot retry loop against BlueZ, D-Bus, or PipeWire.

## 9.7 No timer leaks

Cancellation, shutdown, successful recovery, terminal failure, and policy changes must release or neutralize pending timers.

## 9.8 No duplicate PipeWire links

Route restoration after a graph/service event must remain additive and Auralis-owned. It must not create duplicates or remove third-party/WirePlumber links.

---

# 10. RETRY / BACKOFF POLICY

Reuse the established Bluetooth `ReconnectPolicy` for device reconnect timing.

For service-level recovery where no equivalent currently exists, implement a testable policy with:

- small initial delay;
- exponential or capped exponential backoff;
- sensible maximum delay;
- bounded attempt count **or** an indefinite but very low-frequency “service still absent” monitor only if the product UX explicitly requires it;
- optional jitter to avoid synchronized retries;
- immediate cancellation on success/shutdown;
- pause on suspend;
- deterministic injected timing in unit tests.

Do not make unit tests sleep for real multi-second production delays. Use configurable short test policies or an injectable timer/clock seam consistent with the codebase.

A conceptual service policy could be:

```text
attempt 1: 250 ms
attempt 2: 500 ms
attempt 3: 1 s
attempt 4: 2 s
attempt 5: 4 s
cap:       5–10 s
```

These are examples, not mandatory constants. Choose values from actual behavior and document them.

Do not conflate:

- a BlueZ **service** retry;
- a Bluetooth **device** reconnect;
- a PipeWire **daemon** reconnect;
- a session **route restoration**.

Each is a different layer and should only begin when its prerequisite layer is ready.

---

# 11. BLUETOOTH SERVICE RESTART RECOVERY

The current BlueZ client already watches service registration/unregistration. Phase 8 must make the rest of the application robust around those events.

## 11.1 On BlueZ disappearance

Expected behavior:

1. Observe BlueZ becoming unavailable.
2. Mark Bluetooth subsystem degraded/unavailable.
3. Stop/neutralize discovery operations that cannot complete.
4. Cancel or pause inappropriate in-flight lifecycle operations.
5. Pause device reconnect timers so attempts are not wasted against a missing service.
6. Preserve logical known-device/session intent.
7. Notify session recovery that Bluetooth prerequisites are unavailable without converting every session to irreversible failure.
8. Keep unaffected non-Bluetooth application state alive.
9. Emit one clear operational event, not hundreds of duplicate warnings.

## 11.2 On BlueZ return

Expected sequence:

```text
BlueZ service registered
    |
    v
re-establish signal subscriptions if needed
    |
    v
request fresh ObjectManager snapshot
    |
    v
re-register Agent1 / default agent as required
    |
    v
reconcile adapters
    |
    v
reconcile known devices
    |
    v
resume managed reconnect only where user/session policy permits
    |
    v
wait for audio endpoint reappearance
    |
    v
restore eligible routes/sessions
```

Do not assume BlueZ object paths/global state are identical after restart until a fresh snapshot confirms them.

Pairing state is OS-owned; Auralis must not corrupt it or blindly re-pair devices on service return.

## 11.3 Pairing and connection failure classification

Use existing D-Bus error mapping and extend it only where evidence shows gaps.

Classify at least:

- retryable timeout/temporary transport errors;
- not-ready/service-unavailable errors;
- authentication/pairing failures;
- already-exists/already-connected-like benign races;
- terminal unsupported/not-permitted errors;
- user-cancelled flows.

The recovery layer should not retry terminal pairing/authentication errors forever.

---

# 12. SYSTEM D-BUS LOSS / RECONNECT

BlueZ lives on the system bus, so Phase 8 must handle D-Bus disruption as a first-class reliability condition.

Inspect what Qt guarantees for the current `QDBusConnection` lifecycle on the deployed Qt version. Do not assume an old connection object magically repairs all subscriptions after a bus daemon restart.

Required behavior:

- detect loss of usable system-bus communication;
- transition Bluetooth control to unavailable/degraded rather than crashing;
- invalidate stale pending D-Bus operations;
- recreate/reinitialize the D-Bus client/connection if required by Qt semantics;
- re-establish service watcher and signal subscriptions;
- wait for BlueZ availability;
- request a complete fresh snapshot;
- re-register the pairing agent as necessary;
- resume higher-level recovery only after the new connection is authoritative.

Testing must not require killing the host system D-Bus daemon. Provide a fake/injected D-Bus client path for deterministic tests. Any destructive live D-Bus restart test must be explicitly opt-in and must never be run automatically.

---

# 13. BLUETOOTH ADAPTER DISAPPEARANCE / RETURN

Treat adapter loss separately from BlueZ service loss.

Examples:

- USB adapter unplugged;
- controller reset;
- rfkill state change;
- adapter temporarily removed from BlueZ ObjectManager;
- laptop radio reset across suspend/resume.

On adapter loss:

- stop discovery state cleanly;
- show adapter unavailable;
- keep known devices in logical/persisted state;
- do not burn device reconnect attempts while no usable adapter exists;
- degrade affected sessions rather than deleting them;
- avoid repeated “device failed” noise for every member when the root cause is one adapter outage.

On adapter return:

- refresh adapter state;
- reselect a valid adapter according to existing policy;
- re-enable discovery only if product policy/user intent requires it;
- resume device reconnect attempts;
- reconcile devices/endpoints/routes.

If multiple adapters are possible, do not hard-code one object path unless the existing architecture deliberately does so.

---

# 14. PIPEWIRE SERVICE RESTART RECOVERY

This is a central Phase 8 requirement.

Inspect current `PipeWireConnection`/`PipeWireManager` behavior and implement robust daemon loss/reconnect.

## 14.1 On PipeWire loss

Expected behavior:

1. Detect connection/core failure or destroyed connection.
2. Transition `PipeWireManager` to an explicit unavailable/recovering state.
3. Invalidate the prior PipeWire generation.
4. Clear stale graph objects from the old generation.
5. Ensure Auralis does not retain stale global IDs as if still valid.
6. Mark routes that depended on the old graph as unavailable/degraded without deleting user intent.
7. Inform sessions that audio infrastructure is temporarily unavailable.
8. Schedule service reconnection through a bounded service retry policy.
9. Do not block the Qt GUI thread.
10. Do not leak PipeWire thread-loop/listener resources.

## 14.2 On PipeWire return

Required sequence:

```text
new PipeWire connection established
        |
        v
new generation begins
        |
        v
registry listeners ready
        |
        v
initial graph enumeration completes
        |
        v
AudioEndpointRegistry rebuilt
        |
        v
Bluetooth-to-endpoint mapping reconciled
        |
        v
route destinations rebound by stable logical identity
        |
        v
eligible routes restored
        |
        v
session health recomputed
```

Do not restore routes before initial graph synchronization is complete.

## 14.3 Stable identity vs volatile PipeWire IDs

PipeWire global/node IDs may change after restart.

Recovery must rely on the strongest available logical identity from the current `EndpointResolver`, such as:

- Bluetooth device address/object ownership;
- stable node/device properties;
- endpoint stable ID abstraction;
- other current repository mapping rules.

Do not persist raw ephemeral PipeWire global IDs as the only route restoration key.

## 14.4 Stale callback safety

Events from the old PipeWire generation arriving after reconnect must not mutate the new graph or restore old links.

Add explicit tests for this.

---

# 15. WIREPLUMBER RESTART / POLICY GRAPH CHURN

WirePlumber may restart or reconfigure the graph while PipeWire itself stays running.

Do not assume there is always a stable D-Bus service to monitor for WirePlumber. Prefer behavior-based resilience to graph changes unless the deployed environment provides a stable API that is already part of project architecture.

The application must tolerate:

- Bluetooth sink node disappearing and reappearing;
- profile-driven replacement of a node;
- ports changing;
- default route/policy links being recreated;
- device/node IDs changing;
- rapid remove/add property bursts.

Expected Auralis behavior:

- observe graph churn;
- remove stale endpoint representation;
- keep logical user/session target intent;
- wait for a replacement endpoint that resolves to the same logical device;
- recreate only Auralis-owned links;
- never remove unrelated policy-client links;
- avoid duplicate replan storms through debouncing/coalescing if necessary;
- transition session state through Degraded/Recovering appropriately;
- settle back to Active when the required endpoint and route are observed healthy.

A “WirePlumber restart test” can therefore be validated by graph disappearance/reappearance even if production code does not talk directly to WirePlumber.

---

# 16. DEVICE POWER-OFF / BATTERY / RANGE LOSS

Device-off, battery exhaustion, and out-of-range often look similar at the host layer: unexpected disconnect and endpoint disappearance.

Do not invent a specific reason when BlueZ does not provide one.

Required policy:

1. Detect unexpected disconnect.
2. Preserve explicit distinction from user-requested disconnect.
3. Mark only affected session members recovering/unavailable.
4. Keep healthy members playing where possible.
5. Use the existing managed device reconnect policy.
6. On successful Bluetooth reconnect, wait for a valid audio endpoint.
7. Rebind endpoint.
8. Recreate route.
9. Reapply session/per-device volume/mute state.
10. Rejoin the recovered member.
11. Clear recovery state only after observed route/session health is restored.
12. If attempts are exhausted, leave the session in a coherent Degraded state and surface a user-actionable error.

No infinite reconnect loop.

Do not automatically re-pair a paired device just because connection retries fail.

---

# 17. AUDIO NODE / PROFILE CHANGE RECOVERY

Bluetooth profile changes can replace or invalidate audio endpoints.

Test cases must include a logical device whose audio endpoint changes identity while the device remains known/connected.

Required behavior:

```text
old endpoint removed
     |
route becomes unavailable / member degrades
     |
new endpoint appears for same logical BT device
     |
resolver remaps
     |
route replans using new endpoint
     |
volume/mute state reapplied
     |
session returns Active/Degraded as appropriate
```

Never keep routing toward a removed global ID.

If the new profile is not playback-capable, report that as `AudioUnavailable`/equivalent rather than pretending connection alone is sufficient.

---

# 18. SESSION AND ROUTE RECOVERY COORDINATION

The existing session engine already owns session recovery semantics. Phase 8 should make service failures feed it correctly.

## 18.1 Dependency ordering

Recovery prerequisites should follow:

```text
D-Bus healthy
  -> BlueZ healthy
      -> adapter healthy
          -> device connected
              -> PipeWire healthy
                  -> endpoint resolved
                      -> route ready/active
                          -> session member healthy
```

Not every route requires Bluetooth, so keep the architecture capability-based rather than hard-coding Bluetooth into all route logic.

## 18.2 Do not fail early during infrastructure restoration

A session that was Active before a short PipeWire restart should normally become Recovering/Degraded, not immediately terminal Failed merely because zero routes are active for a transient restoration window.

Reuse the Phase 7 race fixes and make sure Phase 8 does not regress them.

## 18.3 Restoration ownership

RecoveryManager may request reconciliation, but the existing owners should perform it:

```text
Bluetooth reconnect     -> BluetoothManager / DeviceLifecycleManager
Endpoint resolution     -> EndpointResolver / PipeWireManager
Route reconstruction    -> AudioRouter / RoutingCoordinator
Session health          -> SessionManager / SessionStateMachine
Volume restoration      -> VolumeCoordinator / existing controls
```

## 18.4 Explicit stop/delete wins

If a session is stopped/deleted while recovery is pending:

- cancel/neutralize recovery for that session;
- prevent delayed endpoint events from reactivating it;
- restore any device auto-reconnect behavior that the session temporarily altered;
- leave no orphan Auralis-owned links.

---

# 19. LAPTOP SUSPEND / RESUME

Implement suspend/resume as a real lifecycle state, not as “hope BlueZ reconnects later”.

On Linux, a strong integration path is the system D-Bus `org.freedesktop.login1.Manager.PrepareForSleep(bool)` signal. Use the actual supported mechanism on the target Ubuntu environment.

## 19.1 Before suspend

When `PrepareForSleep(true)` is observed:

- enter an application recovery-suspended state;
- record that the sleep transition is intentional;
- pause retry timers/backoff where appropriate;
- prevent new reconnect storms while hardware/services are being suspended;
- invalidate or fence unsafe asynchronous operations if necessary;
- flush important persistence state;
- flush log file buffers;
- do not deliberately forget or disconnect paired devices merely because the laptop is suspending.

Do not block suspend with a long synchronous operation.

## 19.2 On resume

When `PrepareForSleep(false)` is observed:

1. start a new resume/recovery epoch;
2. allow a short stack-stabilization period if evidence shows it is necessary;
3. verify system D-Bus;
4. reconcile BlueZ service and adapters;
5. request fresh Bluetooth state;
6. reconcile PipeWire connection/graph;
7. resume appropriate device reconnect policies;
8. wait for endpoints;
9. restore eligible routes;
10. recompute sessions;
11. emit a clear recovery-complete or recovery-degraded outcome.

The resume path must be idempotent if multiple relevant signals arrive.

## 19.3 Testing suspend/resume

Automated unit/integration tests should inject the sleep signal through a fake lifecycle monitor.

**Do not automatically suspend the developer’s machine in the normal test suite.**

If an actual suspend/resume hardware test is added, gate it behind an explicit variable such as:

```bash
AURALIS_ALLOW_SUSPEND_TESTS=1
```

and require an operator-controlled test workflow.

---

# 20. APPLICATION RESTART BEHAVIOR

Application restart is explicitly in the Phase 8 failure list.

Required properties:

- exiting Auralis must not corrupt BlueZ pairing state;
- saved sessions must load without corruption;
- unsupported/future persistence versions fail safely;
- corrupt persistence does not crash startup;
- incomplete/temporary atomic-write files do not become authoritative;
- user settings survive restart;
- last active/selected session semantics follow the configured product policy;
- **audio must not automatically begin playing unless the existing restore policy explicitly represents user intent and it is considered safe**;
- devices may reconnect according to established policy, but the app must not unexpectedly create audio output solely because it launched;
- logs clearly distinguish restored configuration from restored runtime activity.

Add a real process-level restart integration test where practical:

```text
launch -> create distinctive saved state -> exit cleanly
launch again -> verify state restored -> verify no unintended active playback
```

Use isolated test config/data directories so tests do not mutate the developer’s real Auralis settings.

---

# 21. PERSISTENCE HARDENING

The roadmap says persist:

```text
application settings
known devices
user labels
sessions
preferred routes
volume state
last active session
recovery preferences
```

Audit what already exists and implement only missing pieces.

## 21.1 Storage separation

Prefer clear responsibility:

```text
QSettings / config backend
  -> application preferences and lightweight UI/recovery settings

SessionPersistence
  -> user session definitions and session-owned state

Device metadata persistence (if missing)
  -> Auralis logical labels/preferences keyed by stable device identity
```

Do not attempt to duplicate BlueZ’s pairing database.

Auralis “known device” persistence should mean **application metadata/reference**, not a private copy of security keys.

## 21.2 Atomicity

Use atomic write semantics for structured JSON persistence.

The existing session persistence uses `QSaveFile`; preserve that quality for new structured stores.

## 21.3 Schema versions

Every structured persistence document should have a schema version.

Rules:

- old supported schema -> migrate/read safely;
- current schema -> read normally;
- future unsupported schema -> refuse destructive overwrite, surface clear error;
- missing optional fields -> use safe defaults;
- invalid individual records -> skip only when safe and report diagnostics;
- malformed whole file -> do not crash.

## 21.4 Stable identity

Persist stable logical IDs such as:

- session UUID;
- Bluetooth address or other current stable device key;
- route preference by logical endpoint/device identity.

Do not make recovery depend solely on ephemeral BlueZ/PipeWire object/global IDs.

## 21.5 Persistence failure behavior

Disk-full, permission-denied, read-only directory, invalid path, and malformed JSON scenarios must be tested.

Failed persistence must:

- not corrupt the previous valid file;
- surface a clear error;
- keep runtime state internally coherent;
- avoid claiming a save succeeded.

---

# 22. RECOVERY PREFERENCES

If not already represented, persist the minimal Phase 8 recovery preferences needed by the product.

Examples may include:

```text
automatic device reconnect enabled
service recovery enabled
maximum device retry policy (if user-configurable)
restore routes after service recovery
restore-last-session behavior
```

Do not expose dozens of engineering tuning knobs to normal users.

Keep low-level retry constants internal/configurable for tests unless there is a real product requirement.

If Phase 7 Settings has an appropriate area, add only the minimum user-facing controls needed. Do not redesign the Settings page.

---

# 23. LOGGING HARDENING

The roadmap requires logs to support:

```text
timestamp
severity
subsystem
event
device/session ID
error code
human-readable message
```

The existing logger already supplies timestamp/severity/category/message. Extend message structure so critical lifecycle/recovery events are machine-searchable and human-readable.

A preferred log style is stable key/value events, for example:

```text
2026-08-20T18:00:00.123 INFO recovery event=RecoveryStarted domain=PipeWire generation=4 cause=ServiceVanished
2026-08-20T18:00:00.456 WARN session event=MemberDegraded session=<uuid> device=<stable-id> reason=EndpointRemoved
2026-08-20T18:00:02.001 INFO recovery event=RecoverySucceeded domain=PipeWire attempt=2 generation=5 duration_ms=1878
```

Do not make every log line JSON unless the current logging architecture deliberately moves that way.

## 23.1 Required events

Log at appropriate severity:

- application start/shutdown;
- version/build info;
- BlueZ service lost/returned;
- system D-Bus unavailable/recovered;
- adapter lost/returned;
- reconnect scheduled/attempted/succeeded/exhausted;
- PipeWire disconnected/reconnected;
- graph generation reset;
- endpoint rebound;
- route restoration requested/succeeded/failed;
- session degraded/recovering/recovered/exhausted;
- suspend/resume;
- persistence load/save/migration failures;
- package/build version where useful.

Avoid high-frequency property spam at INFO.

## 23.2 Log file growth

Production hardening should prevent unbounded disk growth.

Audit the current file logger. If it is append-only with no bound, implement a simple, deterministic rotation/size policy or another bounded solution consistent with the codebase.

Requirements:

- bounded file size/disk footprint;
- rotation does not crash logging;
- rotation failure falls back safely;
- current log remains writable when possible;
- unit tests use tiny thresholds;
- no unbounded recursive logging from the logger itself.

Keep the solution small. Do not build a full logging framework.

---

# 24. DIAGNOSTICS / RECOVERY VISIBILITY

Phase 7 already has a Diagnostics screen. Extend existing models only where required to make Phase 8 diagnosable.

The GUI should be able to show, at minimum:

```text
Bluetooth service health
adapter health
PipeWire health
recovery state
current/last recovery cause
retry attempt / exhausted state when relevant
session degraded/recovering status
last actionable error
```

A compact recovery status model/property is acceptable.

Do not expose raw internal pointer addresses or cryptic enum integers to the user.

Diagnostics are not a substitute for correct recovery behavior.

---

# 25. ERROR ESCALATION MODEL

Define a consistent escalation path.

Conceptually:

```text
transient failure
   |
   v
automatic retry/reconcile
   |
   +--> success -> Healthy
   |
   +--> still failing -> bounded retries
                       |
                       +--> success -> Healthy
                       |
                       +--> exhausted -> Degraded / user action required
```

Terminal failure should provide:

- domain/resource;
- last system error;
- attempts made;
- user-actionable next step where known;
- a retry action if safe;
- no misleading “everything ready” status.

Do not display internal implementation jargon as the only user-facing message.

---

# 26. THREADING AND LIFETIME SAFETY

Phase 8 increases asynchronous failure/restart paths. Treat lifetime correctness as a release requirement.

Audit:

- PipeWire callbacks crossing thread boundaries;
- QObject destruction order;
- queued callbacks after shutdown;
- timers owned by recovery policy;
- D-Bus watchers/watcher callbacks;
- logger observer lifetime;
- QML references during service reset;
- repeated initialize/shutdown;
- application destruction while recovery is active.

Required rules:

- no callback into destroyed QObject;
- no blocking PipeWire operation on GUI thread;
- no manual `delete` races where QObject ownership can solve it;
- generation/token checks around stale async work;
- deterministic shutdown order;
- recovery timers cancelled or fenced on shutdown;
- no `QObject::connect` duplication after repeated service reinitialize unless explicitly `UniqueConnection`/disconnected.

Add repeated lifecycle tests.

---

# 27. RECOVERYMANAGER TESTABILITY

The recovery orchestrator must be independently testable without physical hardware.

Provide seams/fakes for:

- BlueZ service present/absent;
- adapter present/absent;
- device state;
- PipeWire connected/disconnected;
- graph ready/not ready;
- endpoint present/replaced;
- session intent;
- suspend/resume events;
- retry timing.

Do not create a fake copy of production business logic. Fakes should inject observations and record commands.

Tests should assert commands and state transitions, not just log strings.

---

# 28. UNIT TEST REQUIREMENTS

Add focused unit tests for all new logic.

At minimum cover:

## 28.1 Service recovery state machine

- healthy -> unavailable -> waiting -> recovering -> healthy;
- duplicate failure event is idempotent;
- success cancels pending retry;
- retry exhaustion;
- shutdown cancels recovery;
- new generation invalidates old callback;
- suspend pauses recovery;
- resume restarts reconciliation;
- explicit user stop prevents restoration.

## 28.2 Retry policy

- expected attempt progression;
- delay cap;
- optional jitter bounds if used;
- cancel;
- reset on success;
- no extra timer after exhaustion;
- no timer leak under hundreds/thousands of schedule/cancel cycles.

## 28.3 Persistence

- missing file;
- valid current schema;
- older supported schema;
- unsupported future schema;
- malformed JSON;
- duplicate IDs;
- partial invalid record;
- atomic save failure;
- unwritable path;
- stable device metadata;
- recovery preferences;
- volume/preferred-route restoration if implemented.

## 28.4 Logging

- structured recovery event formatting;
- file-logging negative paths remain correct;
- rotation threshold;
- rotation retains bounded files;
- failure to rotate does not crash;
- diagnostics bound remains enforced under `capacity + 500` or larger insertion.

## 28.5 Session/route recovery

- service loss does not delete intent;
- route restore after endpoint replacement;
- no duplicate link creation;
- healthy peer continues when one member fails;
- stale recovery after session stop ignored;
- policy `None` does not restore;
- `RestoreRoutesOnly` does not request Bluetooth reconnect;
- `ReconnectAndRestore` requests existing managed reconnect exactly as intended;
- volume/mute reapplied after rejoin.

---

# 29. INTEGRATION TEST REQUIREMENTS — HARDWARE-INDEPENDENT

Create deterministic integration tests using actual production layers plus fake external boundaries.

Required scenarios:

## 29.1 BlueZ service bounce

```text
active/known logical device state
 -> BlueZ disappears
 -> reconnect timers pause / state degrades
 -> BlueZ returns
 -> fresh snapshot arrives
 -> agent/subscriptions restored
 -> managed device/session reconciliation resumes
```

## 29.2 PipeWire service bounce

```text
active route/session
 -> PipeWire connection lost
 -> old graph invalidated
 -> new connection established
 -> new graph IDs
 -> same logical endpoint resolved
 -> route restored exactly once
 -> session returns healthy
```

## 29.3 Endpoint replacement without service restart

Replace endpoint A with endpoint B representing the same Bluetooth device and verify rebind.

## 29.4 One member disconnects

With two logical members:

```text
A healthy, B healthy
 -> B lost
 -> A route remains active
 -> session Degraded/Recovering
 -> B returns
 -> B route restored
 -> session Active
```

## 29.5 Recovery exhausted

- B cannot reconnect;
- attempts exhaust;
- A remains healthy;
- session ends Degraded rather than oscillating forever;
- error includes meaningful reason/attempt count;
- manual retry path works if supported.

## 29.6 Suspend/resume injection

- active recovery timers paused;
- resume epoch starts;
- services reconciled once;
- stale pre-suspend callbacks ignored;
- desired session remains coherent.

## 29.7 Shutdown during recovery

- schedule recovery;
- immediately shut down;
- process/test object exits without timer/callback use-after-free.

---

# 30. LIVE BLUEZ / PIPEWIRE / HARDWARE TESTS

Automated fake tests are necessary but insufficient for Phase 8.

Reuse the project’s existing environment-gated live test style.

Never make destructive hardware tests run by default.

Suggested gates, adapting existing naming:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1
AURALIS_RUN_PIPEWIRE_INTEGRATION=1
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1
AURALIS_RUN_SESSION_INTEGRATION=1
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
AURALIS_EXPECT_DEVICE_ADDRESSES="AA:...;BB:..."
```

For new potentially disruptive scenarios use explicit gates such as:

```bash
AURALIS_ALLOW_SERVICE_RESTART_TESTS=1
AURALIS_ALLOW_SUSPEND_TESTS=1
AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1
```

Do not assume these exact names if the repository has a naming convention; keep them clear and opt-in.

## 30.1 Live Bluetooth scenarios

Where hardware permits:

- repeated connect/disconnect/reconnect;
- device power cycle;
- device out-of-range simulation if practical;
- two-device behavior;
- reconnect exhaustion or delayed return;
- adapter disable/enable if safely controlled;
- BlueZ service restart only under explicit operator permission.

## 30.2 Live PipeWire scenarios

- PipeWire restart under explicit permission;
- graph returns with changed IDs;
- route restoration;
- no duplicate Auralis links;
- unaffected non-Auralis links remain untouched.

## 30.3 WirePlumber scenario

Under explicit permission:

- restart user WirePlumber service;
- verify graph churn is handled;
- verify Bluetooth endpoint is remapped;
- verify route/session recovers.

Do not permanently alter user audio policy configuration.

## 30.4 Live suspend/resume

Only operator-controlled.

Evidence should include:

- session state before sleep;
- service/device state after resume;
- whether device reconnect occurred;
- endpoint restoration time;
- route/session outcome;
- no duplicate links/timers.

---

# 31. STRESS TEST REQUIREMENTS

Phase 8 explicitly calls for stress testing.

Add repeatable stress harnesses/tests for:

## 31.1 Repeated connect/disconnect

Target at least tens/hundreds of fake cycles automatically and a reasonable physical-device cycle count manually/live.

Assert:

- no state leak;
- no timer leak;
- no monotonically growing connection list;
- no duplicate reconnect actions;
- no crash.

## 31.2 Repeated scans

Start/stop discovery repeatedly.

Assert:

- state remains coherent;
- no duplicate signal subscriptions;
- no unbounded device-model duplication;
- scan failure is recoverable.

## 31.3 Service bounce loops

With fake services, run many BlueZ/PipeWire disappear/return cycles.

Assert:

- generation increments correctly;
- stale events ignored;
- memory/object counts stable;
- final state healthy.

## 31.4 Endpoint churn

Repeated remove/re-add/re-ID endpoint events.

Assert route count and Auralis-owned link count remain bounded and correct.

## 31.5 Session restore loops

Repeated create/save/load/restore/stop cycles using temporary directories.

## 31.6 Long-running playback

On actual hardware, run a meaningful soak test where practical.

The existing specification suggests a two-device long run of **2+ hours** as a hardware-test target. If this cannot be completed during the implementation session, provide a reproducible soak-test command/harness and clearly mark the long-run hardware gate as not executed rather than pretending it passed.

## 31.7 Multiple device churn

For available hardware:

- two devices minimum;
- one repeatedly disappears/returns;
- healthy device must remain usable;
- recovering device rejoins cleanly.

## 31.8 Suspend/resume loops

Automated fake lifecycle loop: many iterations.

Physical suspend/resume: small operator-controlled count with evidence.

---

# 32. RESOURCE / MEMORY / LIFETIME TESTING

Run reliability builds using compiler sanitizers where compatible with the project and installed stack.

At minimum attempt:

- AddressSanitizer;
- UndefinedBehaviorSanitizer.

Example approach, adapting to current CMake style:

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
```

Do not blindly use flags that conflict with the current compiler/toolchain. Prefer a project option if one already exists.

ThreadSanitizer may be attempted on focused tests if compatible with Qt/PipeWire, but do not make Phase 8 completion depend on noisy unsupported TSan behavior without investigation.

If Valgrind is installed, run focused lifecycle/recovery tests. If not installed, state `NOT RUN` rather than adding an unnecessary dependency just to satisfy a checkbox.

Track at least:

- retry timer count after cancellation;
- QObject/listener lifecycle;
- PipeWire connection/listener cleanup;
- repeated model entries;
- RSS trend during long fake stress loops if useful.

---

# 33. QML / DESKTOP RELIABILITY REGRESSION TESTS

Phase 8 must preserve Phase 7 GUI quality.

Run offscreen launch:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

Use a controlled timeout in scripted validation.

The gate should reject application-caused:

- `ReferenceError`;
- `TypeError`;
- QML binding loops;
- missing QML modules;
- invalid property assignments;
- QObject-after-destruction warnings;
- recovery-status bindings that throw during service loss.

Add component tests for any new recovery UI/property model.

Test that the GUI remains alive while:

- Bluetooth service becomes unavailable;
- PipeWire becomes unavailable;
- endpoint model becomes empty and repopulates;
- session becomes Degraded/Recovering;
- configuration save fails.

The GUI must not require restarting the application to recover from a service bounce.

---

# 34. CLEAN START / CLEAN SHUTDOWN TESTS

Implement and test repeated application lifecycle.

At minimum:

```text
initialize -> shutdown
initialize -> shutdown
many times in test harness where supported
```

For full desktop process:

- launch offscreen;
- remain up for several seconds;
- terminate cleanly;
- verify no orphan `auralis-desktop` process;
- verify no fatal warnings;
- verify persistence/log files remain valid.

Shutdown while each of these is active:

- Bluetooth scan;
- pairing/connect operation fake;
- reconnect timer;
- PipeWire reconnect timer;
- route restoration;
- session recovering;
- diagnostics logging.

No hang.

---

# 35. PACKAGING — INITIAL `.deb` TARGET

The authoritative Phase 8 roadmap chooses `.deb` as the initial package target.

Implement a real CMake install/package path.

## 35.1 CMake install rules

Use `GNUInstallDirs` where appropriate.

The installed package should include at minimum:

```text
auralis-desktop executable
required runtime resources / QML availability
.desktop launcher
application icon
license
basic package metadata
```

Because the QML module is currently compiled/resource-backed, verify whether additional runtime QML files/plugins must be installed or whether the executable/plugin linkage already embeds what is needed.

Do not guess. Test the installed tree outside the build directory.

## 35.2 Desktop entry

Provide a valid desktop file, for example conceptually:

```ini
[Desktop Entry]
Type=Application
Name=Auralis
Comment=Multi-hearing-device audio hub
Exec=auralis-desktop
Icon=auralis
Terminal=false
Categories=AudioVideo;Audio;
```

Use final metadata consistent with project naming.

Validate with `desktop-file-validate` if available.

## 35.3 Icon

Install a valid application icon in an appropriate hicolor path.

If no product icon exists, add a simple project-owned placeholder asset suitable for Phase 8 packaging rather than embedding a copyrighted third-party image.

Do not turn Phase 8 into a branding redesign.

## 35.4 AppStream metadata

Prefer adding basic AppStream/metainfo metadata if it can be validated cleanly, but do not block the core `.deb` target solely on a tool absent from the environment.

## 35.5 CPack Debian

A reasonable path is CPack’s DEB generator.

Configure from the project version rather than duplicating version strings.

Inspect actual target names and architecture.

Possible concepts:

```cmake
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "auralis")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "...")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
```

Do not paste this blindly.

## 35.6 Runtime dependencies

Do not hard-code Ubuntu package names from memory.

Derive dependencies from the actual target environment using evidence such as:

- linked shared libraries;
- CPack `shlibdeps`;
- `dpkg-shlibdeps` behavior;
- `ldd`;
- Qt/QML runtime requirements;
- `dpkg -S` / `apt-cache` where appropriate.

Shared-library dependency discovery may not capture all QML runtime modules; verify launch from the installed/package environment.

Avoid bundling system BlueZ/PipeWire libraries unnecessarily if normal distro dependencies are correct.

## 35.7 No root requirement for normal runtime

Installing the `.deb` may require package-manager privilege, but running Auralis must not require root.

Do not add setuid binaries or privileged helper daemons.

---

# 36. PACKAGE VALIDATION

A `.deb` file existing is not a pass.

Perform all practical validations.

## 36.1 Staged install test without root

```bash
rm -rf /tmp/auralis-install-root
cmake --install build --prefix /usr --strip \
  --config Release \
  # use DESTDIR through the supported shell/environment pattern
```

Use a clean staging root/`DESTDIR` and inspect the complete file layout.

Verify there are no absolute build-tree references.

## 36.2 Build package

Example:

```bash
cmake --build build --target package
```

or use `cpack` according to the configured build.

## 36.3 Inspect package

Use available Debian tooling:

```bash
dpkg-deb -I <package>.deb
dpkg-deb -c <package>.deb
```

Check:

- package name/version/architecture;
- dependencies;
- files;
- permissions;
- desktop entry;
- icon;
- license/metadata location.

## 36.4 Install/launch test

Where safe on the actual machine or a disposable VM/container:

```text
install package
launch from installed path, not build tree
launch desktop file where practical
run offscreen smoke
verify config/data/log directories
remove package
verify user data is not destructively deleted on uninstall
```

A container can validate package mechanics but cannot prove host Bluetooth/PipeWire behavior. Keep those evidence types separate.

## 36.5 Build-tree independence

Temporarily rename/move the build directory after installation, then launch the installed executable. This catches accidental resource/plugin dependencies on build paths.

---

# 37. RELEASE BUILD VERIFICATION

Test both developer/debug and release-oriented builds.

At minimum:

```bash
rm -rf build-release
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

If the project uses a multi-config approach, adapt accordingly.

Warnings-as-errors policy should follow existing repository conventions.

No new compiler warnings from Phase 8 code.

---

# 38. PRODUCTION HARDENING OF CONFIG/DATA PATHS

Use Qt standard paths rather than working-directory-relative mutable data.

Audit:

- session JSON path;
- log file default path;
- settings backend;
- exported diagnostics if any;
- package resources.

Prefer `QStandardPaths` categories appropriate to config/data/log/cache semantics.

Tests must override paths using temporary directories/environment so they never overwrite a real user profile.

Do not store mutable user data under `/usr`.

Do not write next to the executable.

---

# 39. SECURITY / PRIVACY BOUNDARY

Phase 8 persistence and diagnostics must respect the existing product’s privacy scope.

Do not persist:

- raw user audio;
- Bluetooth security keys copied from BlueZ;
- unnecessary secrets;
- unrestricted environment dumps.

Device identifiers and diagnostic logs may be sensitive operational data. Keep files user-owned and use normal per-user storage permissions.

If adding log export, do not silently upload anything. Auralis is local-first in the current architecture.

---

# 40. SOURCE COMPATIBILITY / ARCHITECTURAL PRESERVATION

Do not break the core phase architecture.

Production code must continue to follow:

```text
QML/UI
  -> Qt-facing controllers/models
      -> core/session/audio/bluetooth services
          -> BlueZ D-Bus / PipeWire native APIs
```

QML must not directly call:

```text
org.bluez
raw QDBusConnection
PipeWire C APIs
systemctl
bluetoothctl
wpctl
pactl
```

Do not introduce shell-command parsing as recovery logic.

---

# 41. FILE / CLASS CREATION GUIDANCE

Do not mechanically create every file below. First adapt to the real repository.

A plausible Phase 8 extension might include:

```text
include/auralis/recovery/RecoveryManager.h
include/auralis/recovery/RecoveryTypes.h
include/auralis/recovery/ServiceRetryPolicy.h
include/auralis/recovery/SystemPowerMonitor.h

src/recovery/RecoveryManager.cpp
src/recovery/RecoveryTypes.cpp
src/recovery/ServiceRetryPolicy.cpp
src/recovery/SystemPowerMonitor.cpp
src/recovery/CMakeLists.txt

tests/unit/recovery/tst_RecoveryManager.cpp
tests/unit/recovery/tst_ServiceRetryPolicy.cpp
tests/unit/recovery/tst_SystemPowerMonitor.cpp

tests/integration/tst_ServiceRecoveryIntegration.cpp
tests/integration/tst_PersistenceRestartIntegration.cpp

data/io.github.<project>.Auralis.desktop   # or repository-consistent ID
data/icons/.../auralis.svg
data/...metainfo.xml
cmake/AuralisPackaging.cmake
```

But if the existing architecture is cleaner with recovery under `src/core`, use that.

Avoid directory churn for ceremony.

---

# 42. APPLICATIONCORE INTEGRATION

The current `ApplicationCore` is the lifecycle composition root.

Phase 8 should likely integrate recovery there or immediately beneath it.

Required properties:

- RecoveryManager created after its dependencies are available.
- RecoveryManager observes service health without circular ownership.
- Service construction order remains deterministic.
- Initialization can succeed in a **degraded but usable** mode where appropriate, e.g. PipeWire temporarily absent, rather than requiring application restart.
- Fatal dependency absence is distinguished from transient runtime service absence.
- Shutdown stops recovery before destroying the services it observes.

A strong shutdown order is conceptually:

```text
stop accepting new user/recovery work
 -> stop RecoveryManager / power monitor
 -> stop sessions/routes
 -> stop device manager
 -> stop PipeWire
 -> stop Bluetooth
 -> flush persistence/logs
 -> release core
```

Use the actual existing dependency order and test it.

---

# 43. STARTUP IN DEGRADED ENVIRONMENTS

A production desktop app should launch even if Bluetooth hardware or PipeWire is temporarily unavailable, as long as doing so is safe.

Audit current initialization behavior.

Expected Phase 8 approach:

- no Bluetooth adapter -> GUI launches, shows adapter unavailable, can recover if adapter appears;
- BlueZ temporarily absent -> GUI launches/degrades if architecture permits and recovers when service returns;
- PipeWire absent -> GUI launches, audio unavailable/recovering, retries service connection;
- no paired devices -> normal empty state, not error;
- corrupt session file -> GUI launches with warning and safe fallback rather than crashing;
- invalid log path -> console logging continues and setting reflects failure.

Do not turn genuinely missing mandatory compiled dependencies into runtime recovery; those are packaging/install errors.

---

# 44. FAILURE COALESCING / ROOT-CAUSE AWARENESS

A single root failure may create many downstream events.

Example:

```text
PipeWire dies
 -> 30 globals removed
 -> 4 endpoints removed
 -> 4 routes lose links
 -> 2 sessions degrade
```

The system must handle every state transition correctly but should avoid 40 top-level identical user notifications.

Implement root-cause correlation/coalescing where practical:

- operational logs can retain detailed events;
- UI notifications should prefer a primary cause such as “Audio service unavailable — reconnecting”; 
- sessions can still show member-specific impact.

Do not hide detailed diagnostics.

---

# 45. OBSERVABILITY METRICS

Without inventing a telemetry backend, expose useful local counters/timing for testing and diagnostics.

Possible metrics:

```text
service reconnect attempts
successful recoveries
failed/exhausted recoveries
last recovery duration
current retry attempt
route restoration count
endpoint rebind count
session recovery count
```

Keep these in-memory/local. Do not add cloud telemetry.

Metrics should enable assertions in tests rather than requiring string log scraping.

---

# 46. REAL-HARDWARE SESSION RECORD

For each significant live hardware run, record:

```text
Session ID
Date/time
Git revision
Build type
OS version
Kernel
Qt version
PipeWire version
WirePlumber version
BlueZ version
Bluetooth adapter
Device models
Device addresses/IDs (redact in externally shared artifacts if needed)
Audio source
Run duration
Failure injected
Connection events
Endpoint events
Route events
Recovery attempts
Recovery duration
Underruns/dropouts if observable
Final session state
Pass/Fail
Tester notes
```

Use a reproducible test log/report, not memory.

---

# 47. ACCEPTANCE CRITERIA — CONNECTIVITY

Before Phase 8 can pass, demonstrate:

- unexpected disconnect is detected;
- explicit user disconnect does not auto-reconnect against user intent;
- managed reconnect uses bounded policy;
- exhausted reconnect is surfaced;
- returning device is reconciled;
- one device failure does not unnecessarily destroy healthy peer playback;
- Bluetooth service restart is recoverable without restarting Auralis, when permitted by the stack;
- adapter disappearance/return is recoverable;
- pairing failure and timeout produce coherent non-stuck state.

---

# 48. ACCEPTANCE CRITERIA — AUDIO INFRASTRUCTURE

Demonstrate:

- PipeWire disconnect/restart is detected;
- old graph/global IDs are discarded;
- reconnection happens without app restart;
- initial graph resync completes;
- Bluetooth endpoint remaps using stable identity;
- Auralis-owned route is recreated at most once;
- unrelated PipeWire links remain untouched;
- session returns to correct health state;
- endpoint/profile replacement is handled;
- WirePlumber graph churn does not permanently wedge the application.

---

# 49. ACCEPTANCE CRITERIA — SESSION RECOVERY

Demonstrate:

- active two-member session can degrade when one device disappears;
- healthy member remains active where technically possible;
- recovering member follows configured policy;
- route rejoin occurs only after endpoint availability;
- stale callbacks after stop/delete cannot reactivate;
- recovery exhaustion leaves coherent Degraded state;
- retry action can recover when conditions later improve;
- restored device gets correct volume/mute state;
- route/session counts remain stable after repeated churn.

---

# 50. ACCEPTANCE CRITERIA — SUSPEND / RESUME

Demonstrate with deterministic tests and, where operator-approved, the real machine:

- suspend signal pauses inappropriate retries;
- resume triggers one reconciliation epoch;
- services can recover in any reasonable return order;
- stale pre-suspend work is ignored;
- paired-device state is not corrupted;
- session/user intent remains preserved;
- no recovery storm;
- app remains responsive.

---

# 51. ACCEPTANCE CRITERIA — PERSISTENCE

Demonstrate:

- application settings survive restart;
- sessions survive restart;
- user labels/device metadata survive if implemented;
- preferred routes/volume/recovery preferences survive if required;
- future schema is rejected safely;
- corrupt file does not crash;
- failed save does not destroy prior good data;
- restart does not auto-play unexpectedly;
- tests use isolated temporary data locations.

---

# 52. ACCEPTANCE CRITERIA — LOGGING / DIAGNOSTICS

Demonstrate:

- all major recovery events carry timestamp/severity/subsystem/event;
- relevant device/session ID is included when available;
- error code/system reason included where available;
- logs remain usable under repeated churn;
- file logging failure does not desynchronize settings from runtime;
- file growth is bounded if file logging remains enabled long-term;
- diagnostics model stays within capacity under stress;
- UI shows a meaningful recovery state without QML warnings.

---

# 53. ACCEPTANCE CRITERIA — PACKAGING

Demonstrate:

- Release build succeeds from clean tree;
- install rules succeed into a clean staging root;
- `.deb` is generated;
- package metadata is sane;
- installed executable launches without build tree;
- QML/resources are available from installed package;
- desktop entry validates if tool available;
- normal runtime requires no root;
- uninstall does not erase user data unexpectedly;
- package dependencies are based on actual environment evidence;
- package does not depend on developer-only build paths.

---

# 54. IMPLEMENTATION ORDER

Use this order unless repository discoveries justify a safer variation.

## Stage 0 — Baseline and map

- inspect architecture;
- clean build;
- full tests;
- document baseline.

## Stage 1 — Define recovery contracts

- identify event sources;
- define recovery states/causes;
- define orchestration API;
- add test seam.

## Stage 2 — Service retry foundation

- add service-level retry/backoff if absent;
- unit test it fully.

## Stage 3 — BlueZ/D-Bus orchestration

- service disappear/return;
- pause/resume reconnect;
- fresh snapshot;
- agent restore;
- tests.

## Stage 4 — PipeWire restart recovery

- disconnect detection;
- reconnect generation;
- graph reset/resync;
- tests.

## Stage 5 — Endpoint/route/session recovery

- endpoint rebind;
- route restoration;
- session health integration;
- idempotency/stale callback tests.

## Stage 6 — Suspend/resume

- power monitor;
- pause/resume orchestration;
- fake integration tests.

## Stage 7 — Persistence completion/hardening

- audit Phase 8 persistence list;
- implement missing metadata/preferences;
- schema/negative-path tests.

## Stage 8 — Logging/diagnostics hardening

- structured recovery events;
- bounded file logging/rotation if missing;
- recovery status exposure;
- tests.

## Stage 9 — Stress and sanitizer passes

- failure loops;
- endpoint churn;
- lifecycle loops;
- ASan/UBSan.

## Stage 10 — Packaging

- install rules;
- desktop integration;
- CPack DEB;
- package inspection.

## Stage 11 — Actual machine validation

- non-destructive live tests first;
- opt-in disruptive tests only when allowed;
- hardware recovery runs;
- package-installed app run.

## Stage 12 — Final clean regression

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Then Release/package build separately.

## Stage 13 — Documentation and audit

Update status only after evidence is complete.

---

# 55. REQUIRED TEST LABELING / SKIP DISCIPLINE

Improve test discoverability if the existing CMake test framework supports labels.

Useful categories:

```text
unit
integration
live
hardware
destructive
stress
packaging
```

A live test that is not enabled should use an explicit skip mechanism/message.

Final audit must report separately:

```text
Automated software tests: PASS/FAIL
Live BlueZ tests:          PASS/FAIL/NOT RUN
Live PipeWire tests:       PASS/FAIL/NOT RUN
Two-device recovery:       PASS/FAIL/NOT RUN
Service restart tests:     PASS/FAIL/NOT RUN
Suspend/resume live:       PASS/FAIL/NOT RUN
Soak test:                 PASS/FAIL/NOT RUN
Package install test:      PASS/FAIL/NOT RUN
```

Never collapse NOT RUN into PASS.

---

# 56. DISRUPTIVE TEST SAFETY RULES

Service restart, Bluetooth forget, and suspend can disrupt the developer machine.

Never run them merely because they exist in a test.

Rules:

- require explicit environment opt-in;
- print exactly what will be manipulated;
- never embed passwords;
- never try to defeat `sudo` prompts;
- prefer user services for PipeWire/WirePlumber where appropriate;
- preserve/restore prior state when safe;
- do not forget Bluetooth devices unless destructive gate is enabled;
- do not restart system D-Bus automatically;
- do not power off/reboot the machine;
- do not suspend automatically outside the explicit operator test.

If privileged BlueZ restart cannot be performed non-interactively and safely, produce the exact operator command and mark the automated portion NOT RUN. Do not fake it.

---

# 57. MANUAL LIVE RECOVERY MATRIX

When actual hardware is available, execute and record as many of these as practical:

| ID | Scenario | Expected |
|---|---|---|
| R8-BT-01 | Device B unexpected disconnect | A stays healthy; B recovers/degrades |
| R8-BT-02 | Device B power cycle | B reconnects and route rejoins |
| R8-BT-03 | Device B absent until retry exhaustion | Session stable Degraded; clear error |
| R8-BT-04 | Adapter unavailable/returns | retries pause then resume |
| R8-BT-05 | BlueZ restart | app survives; snapshot/agent/reconnect restore |
| R8-PW-01 | PipeWire restart | graph rebuilt; routes restore |
| R8-PW-02 | WirePlumber restart | endpoint churn reconciles |
| R8-PW-03 | Bluetooth profile/node replacement | route rebinds to new endpoint |
| R8-SR-01 | Laptop suspend/resume | app/session reconcile without restart |
| R8-APP-01 | App restart | sessions/settings preserved; no unintended audio |
| R8-ST-01 | Repeated reconnect loop | no leak/duplication |
| R8-ST-02 | Long two-device run | stable according to recorded evidence |
| R8-PKG-01 | Installed `.deb` launch | build-tree-independent launch |

Add actual device-specific notes in the audit, not in production code.

---

# 58. REGRESSION REQUIREMENTS FOR PHASES 0–7

Phase 8 must not regress completed behavior.

Re-run all existing test suites and preserve:

- project foundation/build;
- Bluetooth discovery;
- pairing/trust/connect/disconnect/reconnect/forget;
- PipeWire endpoint enumeration;
- Bluetooth endpoint mapping;
- additive route creation/removal;
- route volume/mute;
- multi-device session logic;
- session persistence;
- selected-session isolation;
- Phase 7 GUI navigation/workflows;
- QML warning gate;
- transactional file-logging behavior.

Explicitly re-run any tests added to fix the Phase 7 Starting→Failed race and ConfirmDialog/file-logging issues. Do not let Phase 8 resurrect them.

---

# 59. README / DOCUMENTATION UPDATES

After code and tests are complete, update stale documentation.

At minimum review:

```text
README.md
docs/README.md
docs/architecture/overview.md
docs/architecture/session-engine.md
docs/roadmap/phased-development.md status markers if treated as live status
docs/validation/README.md
docs/prompts/README.md
```

Add Phase 8 architecture documentation describing:

- RecoveryManager responsibility;
- service recovery sequencing;
- suspend/resume behavior;
- persistence behavior;
- package build/install commands;
- live-test environment gates;
- known limitations.

Do not rewrite historical prompt/audit documents to pretend old results were different.

---

# 60. REQUIRED PHASE 8 VALIDATION DOCUMENT

Create a final evidence document, preferably:

```text
docs/validation/phase-8.md
```

and/or a repository-consistent implementation audit such as:

```text
docs/PHASE_8_IMPLEMENTATION_AUDIT.md
```

It must contain concrete evidence, not prose assertions.

Required sections:

```text
1. Executive Summary
2. Repository Revision / Working Tree
3. Environment
4. Baseline Before Phase 8
5. Files Added/Changed
6. Recovery Architecture
7. BlueZ / D-Bus Recovery Results
8. Adapter Recovery Results
9. PipeWire Recovery Results
10. WirePlumber / Endpoint Churn Results
11. Device Reconnect Results
12. Session/Route Restoration Results
13. Suspend/Resume Results
14. Persistence Results
15. Logging/Diagnostics Results
16. Stress Results
17. Sanitizer/Memory Results
18. QML/Desktop Regression Results
19. Packaging Results
20. Live Hardware Results
21. Known Limitations
22. Automated Test Matrix
23. NOT RUN Items
24. Blocking Defects
25. Non-Blocking Improvements
26. Phase 8 Exit Gate
```

---

# 61. FINAL EXIT-GATE RULE

Do not mark Phase 8 complete because the code compiles.

Use one of these exact-style conclusions according to evidence:

## 61.1 Full pass

```text
PHASE 8 EXIT GATE: PASS
```

Only if:

- clean build passes;
- all mandatory hardware-independent tests pass;
- recovery scenarios pass;
- persistence passes;
- package is generated and install/launch path is verified;
- no known blocking defect remains;
- required actual-machine/hardware gates defined by the project have been executed successfully.

## 61.2 Software complete but live evidence pending

```text
PHASE 8 SOFTWARE IMPLEMENTATION: COMPLETE
PHASE 8 EXIT GATE: PENDING LIVE/HARDWARE VALIDATION
```

Use this if all code/software tests pass but one or more mandatory real-machine recovery gates were not executed.

## 61.3 Failure

```text
PHASE 8 EXIT GATE: FAIL
```

Use this if a blocking defect exists, a required test fails, or the package/recovery behavior is broken.

Never convert `NOT RUN` into `PASS`.

---

# 62. FINAL ENGINEERING QUALITY BAR

The implementation is acceptable only if all of the following are true:

- existing Phase 0–7 architecture remains recognizable;
- no shell-command production backends were introduced;
- service failures no longer require an application restart under expected recoverable conditions;
- recovery is observable and bounded;
- stale callbacks cannot resurrect stopped work;
- one bad device does not unnecessarily break healthy peers;
- PipeWire restart does not leave stale global IDs/links;
- endpoint/profile churn can rebind by logical identity;
- suspend/resume is intentionally handled;
- persistence survives failures and restart;
- logs are operationally useful and bounded;
- automated tests include failure injection and stress, not just happy paths;
- live tests clearly distinguish run vs skipped;
- release build passes;
- `.deb` package is real, inspectable, and launchable from installed location;
- normal user workflows still require no terminal;
- documentation matches actual behavior;
- final audit is evidence-based.

---

# 63. COMMAND CHECKLIST FOR THE IMPLEMENTING AGENT

Adapt paths/options to the repository rather than copying blindly.

## 63.1 Initial baseline

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

## 63.2 Focused development loops

```bash
cmake --build build
ctest --test-dir build -R 'Recovery|Reconnect|Session|PipeWire|Bluetooth|Persistence' --output-on-failure
```

Use the actual new test names.

## 63.3 Offscreen GUI

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

## 63.4 Sanitizer build

```bash
rm -rf build-asan
# configure using repository-compatible ASan/UBSan options/flags
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## 63.5 Release build

```bash
rm -rf build-release
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

## 63.6 Package

```bash
cmake --build build-release --target package
```

Then inspect generated `.deb` with Debian tooling.

## 63.7 Final clean build

Return to a fresh tree/build and repeat the complete standard test suite after all packaging/doc changes that affect build files.

---

# 64. REQUIRED FINAL RESPONSE FROM CURSOR / AI IDE

When implementation is complete, do not respond with only “Phase 8 done”.

Return a structured final summary containing:

```text
PHASE 8 IMPLEMENTATION SUMMARY

1. Recovery architecture implemented
2. Key existing mechanisms reused
3. New files/classes
4. Existing files materially changed
5. BlueZ/D-Bus recovery behavior
6. PipeWire/WirePlumber recovery behavior
7. Device/session/route recovery behavior
8. Suspend/resume behavior
9. Persistence changes
10. Logging/diagnostics changes
11. Packaging changes
12. Unit tests added
13. Integration tests added
14. Stress tests added
15. Live tests executed
16. Sanitizer results
17. Clean build/CTest result
18. Release build result
19. `.deb` package path/name
20. Package install/launch result
21. Hardware evidence
22. NOT RUN items
23. Remaining non-blocking limitations
24. Final Phase 8 exit-gate status
```

Include exact test counts and exact failing/skipped test names if any.

---

# 65. DO NOT DO THESE THINGS

The following are explicit anti-patterns for this phase:

```text
DO NOT declare recovery successful because a timer fired.
DO NOT declare a route active before PipeWire observation confirms it.
DO NOT reconnect a device after the user explicitly disconnected it.
DO NOT retry pairing failures forever.
DO NOT preserve stale PipeWire global IDs across daemon restart.
DO NOT stop every route because one endpoint disappeared.
DO NOT auto-restart system services as normal product logic.
DO NOT call systemctl from QML.
DO NOT build production logic around bluetoothctl/wpctl/pactl output.
DO NOT run destructive service/suspend/forget tests without explicit opt-in.
DO NOT write tests that depend on multi-second production sleeps when injectable timing is possible.
DO NOT make CTest “skipped internally” look like hardware PASS in the audit.
DO NOT store Bluetooth keys or user audio in Auralis persistence.
DO NOT introduce root runtime requirements.
DO NOT create a `.deb` that only works while the build directory exists.
DO NOT overwrite a future-version persistence file with current defaults.
DO NOT allow log files to grow forever without a defined bound.
DO NOT rewrite Phase 0–7 subsystems merely to centralize recovery.
DO NOT mark the roadmap complete until the Phase 8 exit gate has evidence.
```

---

# 66. DEFINITION OF DONE

Phase 8 is truly done when a user can install Auralis from the generated `.deb`, launch it normally, use the completed Phase 7 GUI, and the application remains coherent through expected real-world disruptions.

The intended end state is:

```text
Auralis launches
    |
    +--> BlueZ unavailable? ----------> show degraded + recover automatically
    |
    +--> adapter temporarily gone? ---> preserve intent + recover on return
    |
    +--> device leaves range? --------> healthy peers continue + reconnect affected device
    |
    +--> PipeWire restarts? ----------> rebuild graph + rebind endpoint + restore route
    |
    +--> WirePlumber churns graph? ---> reconcile without duplicate/destructive links
    |
    +--> laptop suspends/resumes? ----> pause + reconcile + restore safely
    |
    +--> app restarts? ---------------> restore saved configuration safely
    |
    +--> recovery exhausts? ----------> coherent degraded state + actionable error
    |
    +--> logs/diagnostics ------------> enough evidence to debug what happened
    |
    +--> package ---------------------> installed app runs independently of build tree
```

The user should not need to open a terminal to recover Auralis from ordinary device/service failures.

The developer should have deterministic automated tests and explicit live-test procedures to prove that statement.

**Implement Phase 8 to that standard.**
