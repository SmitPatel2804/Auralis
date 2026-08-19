# Auralis — Phase 6 Live Machine Validation Prompt
## Cursor AI IDE — Clean Build, Session Regression, Live Hardware Exercise, and Formal Validation

Use this document inside **Cursor AI IDE on the actual Auralis Linux development machine** (the machine with real Bluetooth adapters, connected audio devices, and a live PipeWire/WirePlumber session).

This is an **independent validation and audit task** for Phase 6.

Do **not** assume Phase 6 is complete merely because the code exists.

Do **not** edit:

```text
docs/prompts/phase-6/Auralis_Phase_6_Multi_Device_Session_Engine_Implementation_Prompt.md
```

Your job is to inspect, build, test, exercise on real hardware where available, and document what the implementation **actually** does.

Reference architecture doc: [docs/architecture/session-engine.md](../../architecture/session-engine.md).

---

# 0. ROLE

Act as an independent senior reviewer with expertise in:

```text
C++ / Qt 6
Linux audio (PipeWire, WirePlumber)
BlueZ / D-Bus
CMake / CTest
integration testing
multi-device session design
```

You are validating Phase 6 on top of an already-implemented Phase 0–5 tree.

Phase 7 GUI work is **out of scope**.

---

# 1. PRIMARY DELIVERABLES

Create or update these files:

| File | Purpose |
|---|---|
| [docs/validation/phase-6.md](../../validation/phase-6.md) | Durable validation runbook result: commands, pass/fail, environment |
| [docs/validation/phase-6-audit.md](../../validation/phase-6-audit.md) | Evidence-driven audit: architecture checks, defects, verdict |

Optional transient artifacts (gitignored is fine):

```text
audit-phase6/
  build.log
  ctest-default.txt
  ctest-session.txt
  ctest-live-prereq.txt
  ctest-session-live.txt
  grep-session.txt
  manual-notes.txt
```

The Markdown validation/audit files are the durable record. Do not rely only on terminal scrollback.

---

# 2. VALIDATION MODE

This is primarily a **read / build / test / observe / document** task.

Do **not** rewrite Phase 6 architecture during validation.

Allowed repository changes:

```text
docs/validation/phase-6.md
docs/validation/phase-6-audit.md
audit-phase6/* logs
temporary build directories
```

Do **not**, unless the user explicitly asks you to fix something found during validation:

```text
patch production session code
weaken assertions
disable tests
remove environment gating
silently “fix and certify” in the same pass
```

If you discover a defect:

1. capture evidence (command output, logs, state);
2. classify severity;
3. document in `phase-6-audit.md`;
4. stop claiming Phase 6 complete until resolved or explicitly waived.

Read-only host diagnostics are allowed for observation:

```text
pw-cli info all
wpctl status
bluetoothctl devices
journalctl user units for pipewire/wireplumber (if useful)
```

Production Auralis code must **not** call `pw-link`, `wpctl`, `pactl`, `bluetoothctl`, or shell out to them.

---

# 3. MACHINE PRE-FLIGHT

Record in the audit:

```bash
uname -a
lsb_release -a 2>/dev/null || cat /etc/os-release
cmake --version
ninja --version
qmake6 --version || qtpaths6 --version
pw-cli --version
bluetoothctl --version
git rev-parse HEAD
git status --short
```

Verify runtime services:

```bash
systemctl --user is-active pipewire pipewire-pulse wireplumber 2>/dev/null || true
pw-cli info 0 | head -20
busctl --system list org.bluez 2>/dev/null | head -5
```

Inventory connected Bluetooth audio devices:

```bash
bluetoothctl devices Connected
```

For each connected device, note:

```text
friendly name
Bluetooth address (AA:BB:CC:DD:EE:FF)
whether it currently exposes an A2DP / audio sink in PipeWire
```

Set these shell variables for later commands (replace with your real addresses):

```bash
export AURALIS_DEVICE_A="88:08:94:9D:B4:22"   # primary test speaker/headphones
export AURALIS_DEVICE_B="AA:BB:CC:DD:EE:02"   # second device if available; else leave empty
```

Always quote addresses in env vars passed to tests:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22"
```

Never write unquoted Bash placeholders like `<TEST_DEVICE_ADDRESS>` — Bash treats `<` as redirection.

---

# 4. CLEAN BUILD (MANDATORY)

From repository root:

```bash
rm -rf build
cmake -S . -B build -G Ninja 2>&1 | tee audit-phase6/cmake-configure.log
cmake --build build 2>&1 | tee audit-phase6/build.log
```

Record:

```text
configure: PASS/FAIL
build: PASS/FAIL
warning count (note notable -Wconversion / Qt warnings)
```

---

# 5. DEFAULT CTEST (MANDATORY)

Default suite must pass **without** live flags and **without** hardware:

```bash
ctest --test-dir build --output-on-failure 2>&1 | tee audit-phase6/ctest-default.txt
ctest --test-dir build -N > audit-phase6/ctest-list.txt
```

Record total tests, passed, failed, skipped.

Confirm live tests are present but **skipped by default**:

```bash
grep -E 'SKIP|Skipped' audit-phase6/ctest-default.txt | head -20
```

Expected skipped targets include at least:

```text
tst_BlueZLiveIntegration
tst_PipeWireLiveIntegration
tst_AudioRoutingLiveIntegration
tst_SessionLiveIntegration
```

---

# 6. PHASE 6 FOCUSED AUTOMATED TESTS (MANDATORY)

Run the session unit/integration subset:

```bash
ctest --test-dir build -R 'Session|session' --output-on-failure 2>&1 | tee audit-phase6/ctest-session.txt
```

Expected targets (names may vary slightly; verify with `ctest -N`):

```text
tst_SessionManager
tst_SessionStateMachine
tst_SessionPersistence
tst_RoutingCoordinator
tst_VolumeCoordinator
tst_SessionLifecycleIntegration
tst_SessionLiveIntegration   # should SKIP unless live flag set
```

Also confirm Phase 0–5 regression still green in section 5. Phase 6 must not break AudioRouter, Bluetooth, or desktop smoke tests.

Run one verbose harness for confidence:

```bash
./build/tests/unit/session/tst_SessionManager -v2 2>&1 | tee audit-phase6/tst_SessionManager-verbose.txt
./build/tests/integration/tst_SessionLifecycleIntegration -v2 2>&1 | tee audit-phase6/tst_SessionLifecycleIntegration-verbose.txt
```

---

# 7. STATIC VERIFICATION (MANDATORY)

Run and save:

```bash
rg -n 'pw-link|wpctl|pactl|bluetoothctl' src/session include/auralis/session tests/unit/session tests/integration/tst_Session* || true
rg -n 'BlueZClient|bluez/.*client' src/session include/auralis/session || true
```

Expected:

```text
no production shell-tool routing in src/session
session code does not include BlueZ client headers directly
test code may mention env vars; production session code must not shell out
```

Verify CMake links for session module:

```bash
rg -n 'auralis-audio|auralis-bluetooth' src/session/CMakeLists.txt
```

Verify desktop wiring constructs `SessionManager` with Bluetooth + PipeWire dependencies:

```bash
rg -n 'SessionManager' apps/desktop/DesktopApplication.cpp include/auralis/core/ApplicationCore.cpp
```

Verify `ApplicationCore` exposes sessions before shutdown ordering remains sessions → pipewire → bluetooth:

```bash
rg -n 'sessions' src/core/ApplicationCore.cpp
```

Document findings in the audit.

---

# 8. PREREQUISITE LIVE TESTS (RUN IF HARDWARE AVAILABLE)

Phase 6 depends on Phase 3–5 live behavior. Run these **before** claiming session live validation.

## 8.1 BlueZ live

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_DEVICE_A}" \
ctest --test-dir build -R '^tst_BlueZLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/bluez-live.txt
```

## 8.2 PipeWire live

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build -R '^tst_PipeWireLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/pipewire-live-generic.txt

AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_DEVICE_A}" \
ctest --test-dir build -R '^tst_PipeWireLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/pipewire-live-mapped.txt
```

## 8.3 Phase 5 audio routing live

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
ctest --test-dir build -R '^tst_AudioRoutingLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/audio-routing-live-generic.txt

AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_DEVICE_A}" \
ctest --test-dir build -R '^tst_AudioRoutingLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/audio-routing-live-exact.txt
```

If any prerequisite live test fails, document the failure and do **not** hand-wave Phase 6 live success.

Mark each: `PASS / FAIL / NOT RUN (reason)`.

---

# 9. PHASE 6 LIVE CTEST (MANDATORY ATTEMPT)

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
ctest --test-dir build -R '^tst_SessionLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/session-live-init.txt
```

Current minimum expectation for `tst_SessionLiveIntegration`:

```text
BluetoothManager + PipeWireManager + SessionManager initialize and shut down cleanly
```

If the live test is still init-only, say so explicitly in the audit. Do not claim full multi-device live coverage unless the test or manual procedure actually exercised it.

If a multi-address env convention exists in code, try it and document result:

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="${AURALIS_DEVICE_A};${AURALIS_DEVICE_B}" \
ctest --test-dir build -R '^tst_SessionLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/session-live-multi.txt
```

If the variable is not implemented yet, record `NOT IMPLEMENTED` — that is a coverage gap, not a pass.

---

# 10. MANUAL DESKTOP VALIDATION (MANDATORY)

There is **no Phase 7 Sessions screen**. Validate through the running desktop app and logs.

## 10.1 Launch desktop with developer visibility

```bash
AURALIS_UI_SHOW_DEVELOPER_STATUS=1 \
./build/apps/desktop/auralis-desktop 2>&1 | tee audit-phase6/desktop.log
```

Confirm in logs/UI:

```text
Bluetooth service reaches Ready
PipeWire/audio service reaches Ready
Session manager initializes (look for auralis.session category logs)
AppCore exposes sessions (QML: AppCore.sessions)
```

## 10.2 Identify a routable source id

Using the existing RoutePanel / developer endpoint list (Phase 5 UI):

1. Start or select a playback source (application stream or test tone from Phase 5 live workflow).
2. Record the stable source id shown/logged, e.g. `src:7:Stream/Output/Audio`.
3. Record endpoint ids for `${AURALIS_DEVICE_A}` and `${AURALIS_DEVICE_B}` if present.

## 10.3 Exercise SessionManager API

`SessionManager` exposes Q_INVOKABLE methods on `AppCore.sessions`:

```text
createSession(name)
addDevice(sessionId, address, role)
setSource(sessionId, sourceId)
activateSession(sessionId)
setGroupVolume(sessionId, value)
setDeviceVolume(sessionId, deviceId, value)
deactivateSession(sessionId)
deleteSession(sessionId)
restoreLastSession()
```

Because there is no Sessions UI yet, use one of:

```text
A) Qt/QML debugging console attached to the running app (preferred if available)
B) Temporary audit-only QML hook (only if user explicitly approves code changes)
C) Log-driven confirmation that persistence + lifecycle work via automated tests (section 6) plus desktop startup/shutdown
```

Minimum manual checks if you can invoke the API (A or B):

| Step | Action | Expected |
|---|---|---|
| 1 | `createSession("Living Room")` | non-empty session id; count = 1 |
| 2 | `addDevice(id, AURALIS_DEVICE_A, "Left")` | Accepted |
| 3 | add second device if available | Accepted |
| 4 | `setSource(id, <sourceId>)` | Accepted |
| 5 | `activateSession(id)` | state → Active (or Degraded if a member missing) |
| 6 | Observe PipeWire | one Auralis-owned route/link set **per active member**, not one multi-dest route |
| 7 | `setGroupVolume(id, 0.5)` | volume changes on active members; per-device trim preserved |
| 8 | disconnect one member (turn off second headphones) | session → Degraded; **first device keeps playing** |
| 9 | reconnect member | recovery attempt; route restored when endpoint returns |
| 10 | `deactivateSession(id)` | state → Idle; session-owned routes removed |
| 11 | restart app | persisted session loads Idle (not auto-active) |

## 10.4 RoutePanel overlap check (known limitation)

While a session is Active, **do not** also activate conflicting routes from RoutePanel.

Document whether you observed interference when both were used.

---

# 11. PERSISTENCE VALIDATION (MANDATORY)

Locate persistence file (default):

```text
$XDG_DATA_HOME/Auralis/sessions.json
or Qt AppDataLocation equivalent for app id Auralis
```

Verify:

```text
schemaVersion = 1
member deviceId fields are Bluetooth addresses, not PipeWire node ids
restart loads sessions as Idle even if previously Active
groupVolume and per-member trim survive round-trip
invalid/duplicate device entries are skipped without crash
```

Capture a redacted JSON snippet in the audit (mask addresses if publishing externally).

---

# 12. DEGRADED / RECOVERY SCENARIOS TO EXERCISE

Exercise on real hardware where possible. Mark each PASS / FAIL / NOT RUN.

| # | Scenario | Expected |
|---|---|---|
| 1 | Activate with 2 devices, both available | Active; 2 routes |
| 2 | Activate with 2nd device unavailable at start | Degraded; 1 route active |
| 3 | Lose one member during Active | Degraded; surviving member keeps route |
| 4 | Member returns | Recovering → Active or Degraded |
| 5 | Stop while Recovering | Stopping → Idle; recovery timers cancelled |
| 6 | Second activate while one session active | AlreadyActive rejection |
| 7 | Delete active session | stops routes first |
| 8 | Rapid activate/deactivate/activate | no stale route resurrection |
| 9 | App shutdown with active session | clean route teardown; persistence flush |

Automated coverage exists for several of these in `tst_SessionLifecycleIntegration` and `tst_SessionManager`. Map each scenario to automated vs manual evidence in the audit.

---

# 13. ARCHITECTURE COMPLIANCE CHECKLIST

Verify in source and behavior:

```text
[ ] One AudioRoute per session member (RoutingCoordinator)
[ ] Member identity = Bluetooth address
[ ] Source identity = Phase 5 AudioSource.id
[ ] Single active session policy enforced
[ ] Auto-restore off by default; restoreLastSession() explicit
[ ] Volume effective = clamp(groupVolume * memberTrim, 0, 1)
[ ] Reconnect goes through BluetoothManager::reconnectDevice only
[ ] Generation token cancels stale callbacks after stop/deactivate
[ ] SessionManager shuts down before PipeWire/Bluetooth in ApplicationCore
[ ] No production shell audio/bluetooth tools in session code
[ ] Phase 5 RoutePanel still works when no session is active
```

---

# 14. DEFECT CLASSIFICATION

For each defect use:

```text
BLOCKER   — wrong behavior, crash, data loss, stale route resurrection, tears down healthy peers
MAJOR     — missing policy, persistence bug, recovery broken, live test falsely passes
MINOR     — logging, docs drift, missing live coverage, UX gap acceptable for Phase 6
NOTE      — future Phase 7 work, latency sync, full Sessions UI
```

---

# 15. REQUIRED CONTENT IN docs/validation/phase-6.md

Use this outline:

```markdown
# Phase 6 Validation — Multi-Device Session Engine

Validated: <date>
Machine: <hostname / OS>
Git: <hash>

## Environment
## Clean build
## Default ctest
## Session-focused ctest
## Prerequisite live tests
## Session live test
## Manual desktop validation
## Persistence
## Known limitations confirmed
## Commands reference
## Verdict
```

Include exact commands with real addresses redacted if needed.

---

# 16. REQUIRED CONTENT IN docs/validation/phase-6-audit.md

Use this outline:

```markdown
# Phase 6 Independent Audit

## Scope
## Environment evidence
## Build evidence
## Test matrix (default + session + live)
## Static analysis
## Architecture compliance
## Manual/hardware evidence
## Regressions vs Phase 0–5
## Defects
## Coverage gaps
## Final verdict: PHASE 6 COMPLETE / NOT COMPLETE
```

Be evidence-driven. Every PASS claim should cite a command, log excerpt, or file/line reference.

---

# 17. UPDATE docs/validation/README.md

Add rows for:

```text
phase-6.md
phase-6-audit.md
```

---

# 18. REQUIRED IDE FINAL SUMMARY

At the end of the Cursor task, print:

```text
AURALIS PHASE 6 LIVE MACHINE VALIDATION

Clean configure/build:          PASS/FAIL
Default ctest (all):              PASS/FAIL
Session-focused ctest:            PASS/FAIL
Static session grep:              PASS/FAIL
BlueZ live prerequisite:          PASS/FAIL/NOT RUN
PipeWire live prerequisite:       PASS/FAIL/NOT RUN
Audio routing live prerequisite:  PASS/FAIL/NOT RUN
Session live ctest:               PASS/FAIL/NOT RUN
Manual desktop session exercise:  PASS/FAIL/PARTIAL
Persistence round-trip:           PASS/FAIL
Degraded keep-alive confirmed:    PASS/FAIL/NOT RUN
Recovery confirmed:               PASS/FAIL/NOT RUN

Deliverables:
  docs/validation/phase-6.md
  docs/validation/phase-6-audit.md

FINAL VERDICT:
PHASE 6: COMPLETE / NOT COMPLETE
```

---

# 19. STOP CONDITIONS

Do **not** stop because:

```text
default ctest is green but live/hardware was never attempted
only unit tests ran
desktop launched once with no session API exercise
persistence was not inspected
degraded/recovery was not tested on real hardware when devices were available
```

Stop when:

```text
mandatory sections 4–7 and 13 are complete
deliverables in section 1 exist
final verdict is justified by evidence
```

---

# 20. FINAL PRINCIPLE

Phase 6 is not “implemented” on the machine until:

```text
N-device session routing survives partial member loss
healthy peers keep playing in Degraded mode
session-owned routes are removed on stop/shutdown
persistence reload is safe and Idle-by-default
Phase 0–5 regressions remain green
```

Convert “the code compiles” into “the behavior is proven on this machine.”
