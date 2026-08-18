# Auralis — PHASE 4 Rigorous Linux Verification and Audit Prompt
## Cursor AI IDE — Clean Build, Regression Testing, Live PipeWire/BlueZ Validation, and Formal Audit Document

Use this document inside **Cursor AI IDE on the actual Auralis Linux development machine**.

This is an **independent verification and audit task**.

Do not assume Phase 4 is complete merely because the code exists.

Do not silently modify or repair the repository during the audit.

Your job is to inspect, build, test, exercise, and document the actual implementation, then produce a detailed Markdown audit artifact.

---

# 0. ROLE

Act as an independent senior reviewer with expertise in:

```text
C++ systems programming
Qt 6 / Qt Quick / QML
Linux
BlueZ
D-Bus
PipeWire
WirePlumber
CMake
CTest
threading
RAII
integration testing
audio graph architecture
```

You are auditing the Auralis repository after implementation of:

```text
PHASE 0
PHASE 1
PHASE 2
PHASE 3
PHASE 4
```

Phase 5 must not yet be implemented.

---

# 1. PRIMARY DELIVERABLE

Create this file in the repository root:

```text
Auralis_PHASE_4_IMPLEMENTATION_AUDIT.md
```

The audit must be evidence-driven.

It must include:

```text
repository revision/status
Linux environment
runtime services
architecture findings
source-level verification
clean-build results
CTest enumeration
unit test results
integration test results
live BlueZ results
live PipeWire results
Bluetooth↔PipeWire mapping evidence
manual desktop/UI validation
threading/lifetime review
regression review
Phase 5 boundary review
defects
warnings
non-blocking improvements
final verdict
```

Do not merely summarize what the implementation intends to do.

Prove what it actually does.

---

# 2. AUDIT MODE — DO NOT MODIFY SOURCE CODE

This is primarily a read/test/audit task.

Do not change production or test source code during the audit.

Allowed repository changes:

```text
Auralis_PHASE_4_IMPLEMENTATION_AUDIT.md
temporary audit logs
temporary build directories
```

Do not:

```text
fix source files
fix CMake
weaken assertions
disable tests
change environment-gating logic
patch failures and then certify the patched result
```

If you discover a defect:

1. document it;
2. capture evidence;
3. classify severity;
4. explain the likely correction;
5. continue auditing all independent areas possible.

The final audit must represent the repository as received.

---

# 3. AUDIT EVIDENCE DIRECTORY

Create:

```bash
mkdir -p audit-phase4
```

Use it for command output where useful:

```text
audit-phase4/environment.txt
audit-phase4/git-status.txt
audit-phase4/runtime-services.txt
audit-phase4/cmake-configure.txt
audit-phase4/build.txt
audit-phase4/ctest-list.txt
audit-phase4/ctest-full.txt
audit-phase4/bluez-live.txt
audit-phase4/pipewire-live.txt
audit-phase4/wpctl-status.txt
audit-phase4/bluetooth-info.txt
audit-phase4/source-searches.txt
```

Do not store passwords, tokens, or secrets.

The required persistent deliverable is:

```text
Auralis_PHASE_4_IMPLEMENTATION_AUDIT.md
```

---

# 4. CAPTURE REPOSITORY STATE FIRST

Run and record:

```bash
pwd
git status --short
git branch --show-current
git rev-parse HEAD
git log -1 --oneline
```

If this is not a Git checkout, record that explicitly.

If the working tree is dirty before the audit, record the pre-existing changes.

Do not clean/reset user work.

---

# 5. CAPTURE LINUX DEVELOPMENT ENVIRONMENT

Run:

```bash
{
echo "===== OS ====="
cat /etc/os-release

echo "===== KERNEL ====="
uname -a

echo "===== COMPILER ====="
gcc --version | head -1
g++ --version | head -1

echo "===== CMAKE ====="
cmake --version | head -1

echo "===== NINJA ====="
ninja --version

echo "===== GIT ====="
git --version

echo "===== QT ====="
qmake6 --version || true

echo "===== BLUEZ ====="
bluetoothctl --version || true

echo "===== PIPEWIRE ====="
pipewire --version || true
pw-cli --version || true

echo "===== PKG CONFIG ====="
pkg-config --modversion libpipewire-0.3 || true
pkg-config --modversion dbus-1 || true
pkg-config --modversion Qt6Bluetooth || true
pkg-config --modversion Qt6Multimedia || true
} | tee audit-phase4/environment.txt
```

Include important versions in the audit document.

---

# 6. CAPTURE RUNTIME SERVICE STATE

Run:

```bash
{
systemctl --user status pipewire --no-pager || true
systemctl --user status pipewire-pulse --no-pager || true
systemctl --user status wireplumber --no-pager || true
systemctl status bluetooth --no-pager || true
} | tee audit-phase4/runtime-services.txt
```

Also:

```bash
wpctl status 2>&1 | tee audit-phase4/wpctl-status.txt
```

The audit must distinguish between:

```text
code defect
test defect
service/configuration problem
machine/environment limitation
hardware unavailable
```

Do not conflate them.

---

# 7. REPOSITORY ARCHITECTURE AUDIT

Inspect the actual source tree.

At minimum inspect:

```text
CMakeLists.txt
src/audio/
include/auralis/audio/
src/bluetooth/
include/auralis/bluetooth/
apps/desktop/
tests/
tests/unit/
tests/integration/
```

Identify actual classes/files responsible for:

```text
PipeWire connection
PipeWire manager
registry monitoring
PipeWire object store
AudioEndpoint
AudioEndpointRegistry
EndpointResolver
QML/Qt endpoint model
ApplicationCore integration
```

If names differ, document the actual mapping.

---

# 8. VERIFY NATIVE PIPEWIRE INTEGRATION

Prove production Phase 4 uses native PipeWire.

Run:

```bash
grep -RIn   "pw_init\|pw_thread_loop_new\|pw_context_new\|pw_context_connect\|pw_core_get_registry\|pw_registry_add_listener\|pw_registry_bind"   include src apps tests   2>&1 | tee audit-phase4/source-searches.txt || true
```

Inspect CMake for:

```text
libpipewire-0.3
PkgConfig::PIPEWIRE
or equivalent native linkage
```

Then search production code for prohibited CLI dependence:

```bash
grep -RIn   "wpctl\|pw-dump\|pw-cli\|pactl"   include src apps || true
```

Classification:

```text
PASS:
production discovery/state uses native PipeWire API.

ACCEPTABLE:
CLI names appear only in docs/comments/debug instructions.

FAIL:
production endpoint discovery depends on shell execution or parsing CLI output.
```

---

# 9. THREADING AUDIT

Determine exactly:

```text
which thread executes PipeWire callbacks
which thread owns PipeWire object store updates
which thread owns AudioEndpointRegistry
which thread owns QAbstractListModel/QML state
how cross-thread handoff happens
how shutdown prevents queued callbacks after destruction
```

Inspect for:

```text
pw_thread_loop
pw_thread_loop_lock
pw_thread_loop_unlock
pw_thread_loop_stop
QMetaObject::invokeMethod
Qt::QueuedConnection
QObject thread affinity
```

Look specifically for:

```text
QML/model mutation directly from PipeWire callback
slow domain/UI work under PipeWire lock
pw_thread_loop_stop while lock held
queued callback targeting destroyed QObject
unsafe raw reference capture across threads
```

Document exact files/functions.

---

# 10. MEMORY AND LIFETIME AUDIT

Verify:

```text
spa_dict data is copied before callback lifetime ends
no raw PipeWire dictionary pointer is retained by domain objects
PipeWire proxies are tracked/destroyed safely
listeners/hooks are safely removed
core/context/loop ownership is explicit
global_remove invalidates/removes tracked objects
shutdown ordering is safe
repeated stop/shutdown is guarded or idempotent
```

Inspect raw pointer members and ownership carefully.

Classify findings:

```text
BLOCKER
MAJOR
MINOR
OBSERVATION
```

---

# 11. PIPEWIRE REGISTRY AUDIT

Verify implementation of:

```text
registry global callback
registry global_remove callback
Device observation
Node observation
property normalization
object-store insertion
object-store update
object-store removal
```

Confirm graph changes after application startup are handled.

Phase 4 is not complete if it only performs one startup snapshot.

---

# 12. ENDPOINT CLASSIFICATION AUDIT

Inspect classifier logic and tests.

Verify:

```text
Audio/Sink -> Playback
Audio/Source -> Capture
Audio/Duplex -> Duplex or valid equivalent
application audio streams excluded as physical/user endpoints
non-audio nodes excluded
built-in/ALSA endpoints supported
Bluetooth endpoints supported
```

Find the tests proving each relevant case.

---

# 13. AUDIOENDPOINT MODEL AUDIT

Document actual fields for:

```text
logical endpoint identity
PipeWire runtime/global ID
PipeWire serial if used
name
description
direction
availability
transport
profile
codec
sample rate/channels if supported
Bluetooth device ID
Bluetooth address
BlueZ path
mapping status/confidence if present
```

Determine whether PipeWire global ID is incorrectly treated as permanent physical identity.

---

# 14. AUDIOENDPOINTREGISTRY AUDIT

Verify:

```text
insert/upsert
update
deduplication
remove
lookup by logical endpoint ID
lookup by PipeWire ID
query by Bluetooth device
multiple endpoints for one Bluetooth device
```

If QAbstractListModel is used, verify:

```text
beginInsertRows/endInsertRows
beginRemoveRows/endRemoveRows
dataChanged
correct thread ownership
stable roles
```

---

# 15. BLUETOOTH ↔ PIPEWIRE RESOLVER AUDIT

Inspect mapping logic carefully.

Document actual priority and signals.

Verify support where present for:

```text
api.bluez5.address
api.bluez5.path
api.bluez5.device
node -> PipeWire device ownership
api.bluez5.profile
api.bluez5.codec
```

Verify:

```text
strong identity beats name matching
weak name matching is unique-only if used
ambiguous duplicate names are not guessed
unresolved endpoint remains a valid endpoint rather than disappearing
```

---

# 16. BLUETOOTH ADDRESS NORMALIZATION

Inspect or test normalization for intended forms such as:

```text
AA:BB:CC:DD:EE:FF
aa:bb:cc:dd:ee:ff
AA_BB_CC_DD_EE_FF
AA-BB-CC-DD-EE-FF
```

Only claim support for forms the implementation deliberately handles.

Invalid address data must not accidentally match a device.

---

# 17. MANDATORY NODE → PIPEWIRE DEVICE → BLUETOOTH CASE

Verify this mapping path:

```text
PipeWire Device:
  globalId = 50
  api.bluez5.address = AA:BB:CC:DD:EE:FF

PipeWire Node:
  globalId = 60
  device.id = 50
  media.class = Audio/Sink
```

The node may not itself contain `api.bluez5.address`.

Auralis must be able to resolve:

```text
Node 60
 -> PipeWire Device 50
 -> BlueZ identity
 -> existing Phase 3 BluetoothDevice
```

Find a unit test proving this.

If the implementation cannot do this, classify as a Phase 4 blocker.

---

# 18. EVENT ORDER AUDIT

Find tests/evidence for:

```text
Bluetooth first, PipeWire later
PipeWire first, Bluetooth registry later
PipeWire removal first, Bluetooth disconnect later
Bluetooth disconnect first, PipeWire removal later
node removed and recreated with new PipeWire ID
```

The final endpoint model must converge correctly regardless of event order.

---

# 19. MULTIPLE ENDPOINTS PER BLUETOOTH DEVICE

Verify one physical Bluetooth device can own separate endpoints such as:

```text
A2DP Playback
HFP Playback
HFP Capture
```

Confirm they remain distinct endpoint records while all map to the same Phase 3 Bluetooth device.

Find test evidence.

---

# 20. PHASE 5 BOUNDARY AUDIT

Search for active routing implementation:

```bash
grep -RIn   "pw_link\|pw_stream\|pw_filter\|create.*link\|route.*endpoint\|duplicate.*audio"   include src apps tests || true
```

Inspect matches manually.

Phase 4 may include stubs, comments, or future interfaces.

It must not yet implement:

```text
user-selected PipeWire route creation
stream duplication
multi-destination playback
session routing
latency compensation
Phase 5 audio routing engine
```

---

# 21. CLEAN BUILD IN A FRESH DIRECTORY

Do not use the normal existing `build/` directory as primary audit evidence.

Run:

```bash
rm -rf build-audit

set -o pipefail
cmake -S . -B build-audit -G Ninja   2>&1 | tee audit-phase4/cmake-configure.txt
CM_RC=${PIPESTATUS[0]}
echo "CMAKE_EXIT_CODE=${CM_RC}" | tee -a audit-phase4/cmake-configure.txt
test "${CM_RC}" -eq 0
```

Then:

```bash
set -o pipefail
cmake --build build-audit   2>&1 | tee audit-phase4/build.txt
BUILD_RC=${PIPESTATUS[0]}
echo "BUILD_EXIT_CODE=${BUILD_RC}" | tee -a audit-phase4/build.txt
test "${BUILD_RC}" -eq 0
```

Do not label configure/build PASS if the real exit code is non-zero.

If build fails, continue static audit where possible and classify execution coverage accordingly.

---

# 22. CTEST REGISTRATION AUDIT

Run:

```bash
ctest --test-dir build-audit -N   2>&1 | tee audit-phase4/ctest-list.txt
```

Verify expected Phase 0–4 tests remain registered.

Specifically verify:

```bash
ctest --test-dir build-audit -N | grep -F tst_BlueZLiveIntegration
ctest --test-dir build-audit -N | grep -F tst_PipeWireLiveIntegration
```

Use actual names if they differ.

A test source file existing without CTest registration is a regression.

---

# 23. FULL NORMAL CTEST RUN

Run:

```bash
set -o pipefail
ctest --test-dir build-audit   --output-on-failure   2>&1 | tee audit-phase4/ctest-full.txt
CTEST_RC=${PIPESTATUS[0]}
echo "CTEST_EXIT_CODE=${CTEST_RC}" | tee -a audit-phase4/ctest-full.txt
```

Record:

```text
total tests
passed
failed
skipped/not-run
failed test names
unexpected skipped test names
```

Do not hide skipped tests.

Legitimate opt-in live-test skips must be clearly separated from failures.

---

# 24. VERBOSE RERUN FOR UNEXPECTED FAILURES

For every unexpected failure:

```bash
ctest --test-dir build-audit   -R '^EXACT_TEST_NAME$'   -V
```

Use this only to diagnose.

Do not modify code.

Capture relevant evidence in the audit.

---

# 25. VERIFY BLUEZ LIVE TEST EXISTS IN CTEST

Run:

```bash
ctest --test-dir build-audit -N | grep -F tst_BlueZLiveIntegration
```

If absent:

```text
RESULT: FAIL
SEVERITY: BLOCKER
```

because a Phase 3 regression test has disappeared.

---

# 26. VERIFY PIPEWIRE LIVE TEST EXISTS IN CTEST

Run:

```bash
ctest --test-dir build-audit -N | grep -F tst_PipeWireLiveIntegration
```

Use actual repository naming if different.

---

# 27. SELECT A REAL BLUETOOTH AUDIO DEVICE

Run:

```bash
bluetoothctl devices
bluetoothctl devices Connected || true
bluetoothctl devices Paired || true
```

Choose an appropriate real audio/hearing device only if present.

Record its actual address.

Then:

```bash
bluetoothctl info "AA:BB:CC:DD:EE:FF"   2>&1 | tee audit-phase4/bluetooth-info.txt
```

Record:

```text
name
address
paired
trusted
connected
services/UUIDs where relevant
```

Never invent a device address.

If no suitable device is available:

```text
LIVE BLUETOOTH HARDWARE VALIDATION: NOT EXECUTED
Reason: no suitable device available
```

Do not mark it PASS.

---

# 28. PHASE 3 BLUEZ LIVE TEST

If hardware is available, run using the actual device address:

```bash
set -o pipefail
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" ctest --test-dir build-audit   -R '^tst_BlueZLiveIntegration$'   --output-on-failure   2>&1 | tee audit-phase4/bluez-live.txt
BLUEZ_RC=${PIPESTATUS[0]}
echo "BLUEZ_LIVE_EXIT_CODE=${BLUEZ_RC}" | tee -a audit-phase4/bluez-live.txt
```

Use the test source as authority if its environment contract differs.

Report:

```text
PASS
FAIL
NOT EXECUTED
```

with evidence.

---

# 29. PHASE 4 PIPEWIRE LIVE TEST — GENERIC

Run:

```bash
set -o pipefail
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 ctest --test-dir build-audit   -R '^tst_PipeWireLiveIntegration$'   --output-on-failure   2>&1 | tee audit-phase4/pipewire-live.txt
PW_RC=${PIPESTATUS[0]}
echo "PIPEWIRE_LIVE_EXIT_CODE=${PW_RC}" | tee -a audit-phase4/pipewire-live.txt
```

Expected minimum:

```text
PipeWire connects
registry becomes populated
one or more legitimate audio endpoints exist
shutdown is clean
```

If no endpoints appear, cross-check service and `wpctl status` evidence before assigning root cause.

---

# 30. LIVE BLUETOOTH ↔ PIPEWIRE MAPPING

If supported by the test and hardware is available:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" ctest --test-dir build-audit   -R '^tst_PipeWireLiveIntegration$'   --output-on-failure
```

If the repository defines a separate mapping integration test, run that exact test instead.

The audit must state whether a real PipeWire endpoint mapped to the expected Phase 3 Bluetooth device.

---

# 31. PIPEWIRE GRAPH CROSS-CHECK

For diagnostics only:

```bash
wpctl status 2>&1 | tee audit-phase4/wpctl-status.txt
```

Optionally:

```bash
pw-dump > audit-phase4/pw-dump.json
```

Do not use CLI output as proof that Auralis itself uses native PipeWire.

Use it only to cross-check what should be visible.

Do not paste a huge `pw-dump` into the audit document.

---

# 32. DESKTOP APPLICATION SMOKE TEST

Run:

```bash
./build-audit/apps/desktop/auralis-desktop
```

If that path differs, locate the actual executable.

Observe:

```text
clean startup
Phase 2/3 Bluetooth UI functional
PipeWire state visible/healthy
Audio Endpoints list/model present
built-in audio endpoints visible
no immediate crash
no runaway error logging
```

Do not mark desktop validation PASS unless the application was actually launched.

---

# 33. MANUAL REAL BLUETOOTH ENDPOINT VALIDATION

If hardware is available, perform through Auralis:

```text
1. Launch Auralis.
2. Locate the real device using existing Phase 2/3 UI.
3. Pair if required.
4. Trust if required.
5. Connect through Auralis.
6. Confirm Bluetooth Connected=true.
7. Observe that the PipeWire endpoint appears after connection.
8. Confirm endpoint direction is plausible.
9. Confirm profile/codec if exposed.
10. Confirm endpoint maps to the exact Phase 3 BluetoothDevice.
11. Disconnect through Auralis.
12. Confirm endpoint disappears or becomes unavailable.
13. Reconnect.
14. Confirm the endpoint reappears.
15. Confirm a changed PipeWire runtime/global ID does not break logical mapping.
```

Do not use terminal control as a substitute for the app flow unless diagnosing a failure.

---

# 34. RAPID CONNECT/DISCONNECT CHURN

If practical:

```text
connect
disconnect
connect
disconnect
connect
```

Observe:

```text
no crash
no duplicate endpoints
no stale endpoint shown available
final endpoint state matches real connection
```

Record outcome.

---

# 35. APPLICATION RESTART WHILE DEVICE ALREADY CONNECTED

With a Bluetooth audio device connected:

```text
1. Close Auralis.
2. Keep the device connected if possible.
3. Relaunch Auralis.
4. Confirm Phase 3 sees the existing device state.
5. Confirm PipeWire enumerates the existing graph.
6. Confirm endpoint appears without forcing reconnect.
7. Confirm resolver maps it to the existing Phase 3 device.
```

Record PASS/FAIL/NOT EXECUTED.

---

# 36. PIPEWIRE START FAILURE CONTRACT REVIEW

Inspect startup implementation carefully.

Verify:

```text
synchronous fatal startup failure -> return/report failure
asynchronous runtime failure -> state/error signal mechanism
```

Review branches around:

```text
thread-loop creation
context creation
pw_context_connect
pw_core_get_registry
listener setup
thread-loop start
```

If safe test infrastructure already exists, run it.

Do **not** stop the user's PipeWire service merely to manufacture a failure unless doing so is explicitly safe and reversible.

Static source verification is acceptable for this specific failure-path contract when safe runtime injection is not available.

---

# 37. HARDWARE-INDEPENDENCE OF NORMAL CTEST

The default:

```bash
ctest --test-dir build-audit --output-on-failure
```

must not require:

```text
real Bluetooth hardware
manual pairing
a specific headset
manual interaction
```

Hardware tests may be registered but skip unless explicitly enabled.

If normal CTest fails because live hardware tests execute unconditionally:

```text
FAIL
```

---

# 38. DOCUMENTATION AUDIT

Inspect:

```text
README*
docs/
Phase 3 documentation
Phase 4 documentation
developer test instructions
```

Verify:

```text
Phase 4 = PipeWire audio endpoint integration
Phase 5 = routing engine
live-test commands match actual test names
environment variables match source behavior
device address examples use quoted real-format addresses
```

Flag this as incorrect shell syntax if present:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

Preferred:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

---

# 39. COMPILER WARNING REVIEW

Inspect `audit-phase4/build.txt` for:

```text
warning:
deprecated
unused
conversion
narrowing
uninitialized
```

Determine whether warnings come from:

```text
Auralis Phase 4 code
pre-existing Auralis code
third-party/system headers
```

Do not misclassify unrelated warnings as Phase 4 defects.

---

# 40. OPTIONAL SANITIZER VALIDATION

Only use sanitizers if the repository already supports them cleanly.

If supported, run relevant Phase 4 tests under:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

If not already supported, record:

```text
SANITIZER VALIDATION: NOT EXECUTED
Reason: no established sanitizer build path
```

Do not redesign CMake merely for this audit.

---

# 41. REQUIRED PHASE 4 TEST COVERAGE MATRIX

Include a table like:

| Scenario | Evidence | Result |
|---|---|---|
| Audio/Sink → Playback | test/file | PASS/FAIL |
| Audio/Source → Capture | test/file | PASS/FAIL |
| Application stream ignored | test/file | PASS/FAIL |
| Non-audio node ignored | test/file | PASS/FAIL |
| Built-in ALSA endpoint | test/file | PASS/FAIL |
| Exact Bluetooth address mapping | test/file | PASS/FAIL |
| BlueZ path mapping | test/file | PASS/FAIL |
| Node→PW Device→BT mapping | test/file | PASS/FAIL |
| Ambiguous name rejected | test/file | PASS/FAIL |
| Multiple endpoints/device | test/file | PASS/FAIL |
| Node/global removal | test/file | PASS/FAIL |
| BT-first event ordering | test/file | PASS/FAIL |
| PW-first event ordering | test/file | PASS/FAIL |
| PipeWire ID churn/reconnect | test/file | PASS/FAIL |
| Clean shutdown | test/file | PASS/FAIL |
| Live PipeWire graph | live test | PASS/FAIL/NOT EXECUTED |
| Live Bluetooth mapping | live test/manual | PASS/FAIL/NOT EXECUTED |

Do not leave required scenarios as assumed.

---

# 42. REQUIRED REGRESSION MATRIX

Include:

| Phase | Area | Evidence | Result |
|---|---|---|---|
| 0 | clean build/toolchain | build-audit | PASS/FAIL |
| 1 | foundation/core | CTest | PASS/FAIL |
| 2 | Bluetooth discovery | CTest/manual | PASS/FAIL |
| 3 | device management | CTest/manual | PASS/FAIL |
| 3 | BlueZ live test registered | `ctest -N` | PASS/FAIL |
| 3 | BlueZ live behavior | live test | PASS/FAIL/NOT EXECUTED |
| 4 | native PipeWire | source/build | PASS/FAIL |
| 4 | endpoint classification | tests | PASS/FAIL |
| 4 | endpoint registry | tests | PASS/FAIL |
| 4 | Bluetooth correlation | tests/live | PASS/FAIL |
| 4 | live PipeWire | live test | PASS/FAIL/NOT EXECUTED |

---

# 43. DEFECT SEVERITY RULES

## BLOCKER

Prevents the Phase 4 milestone or regresses a completed earlier phase.

Examples:

```text
clean build fails
normal test suite fails
Phase 3 regression test removed
PipeWire cannot connect
no valid AudioEndpoint is produced
Bluetooth correlation fundamentally absent
serious use-after-free/crash
Phase 5 routing improperly introduced in a way that breaks scope
```

## MAJOR

Important Phase 4 scenario broken, but not total failure.

## MINOR

Non-gating correctness/maintenance issue with clear impact.

## OBSERVATION

Hardening/future improvement that does not block Phase 4.

Use proportional severity.

---

# 44. FINAL VERDICT RULES

Return:

```text
PHASE 4: COMPLETE
```

only if all of these are true:

```text
clean configure passes
clean build passes
normal CTest passes
Phase 0–3 test registration is preserved
Phase 4 tests are registered
native PipeWire integration is verified
AudioEndpoint model/registry is verified
Bluetooth resolver strong identity mapping is verified
event ordering/object churn is covered
no blocker defect exists
Phase 5 routing is not implemented
```

Additionally:

- If suitable Bluetooth hardware is available, live Bluetooth endpoint mapping must pass.
- If no suitable hardware is available, clearly state that live Bluetooth hardware validation was not executed.
- Never convert `NOT EXECUTED` into `PASS`.

Return:

```text
PHASE 4: INCOMPLETE
```

if any blocker exists.

---

# 45. REQUIRED AUDIT DOCUMENT STRUCTURE

Write `Auralis_PHASE_4_IMPLEMENTATION_AUDIT.md` with:

```markdown
# Auralis Phase 4 Implementation Audit

## 1. Executive Summary

## 2. Final Verdict

## 3. Audit Scope

## 4. Repository Revision and Working Tree

## 5. Linux Development Environment

## 6. Runtime Services

## 7. Phase 4 Architecture Found

## 8. Native PipeWire Integration Review

## 9. Threading and Lifetime Review

## 10. PipeWire Registry and Object Store Review

## 11. AudioEndpoint Model Review

## 12. AudioEndpointRegistry Review

## 13. Bluetooth ↔ PipeWire Mapping Review

## 14. Phase 4 Unit-Test Coverage

## 15. CTest Registration Audit

## 16. Clean Configure and Build Results

## 17. Full Regression Test Results

## 18. Phase 3 BlueZ Live Integration Result

## 19. Phase 4 PipeWire Live Integration Result

## 20. Real Bluetooth Endpoint Mapping Result

## 21. Desktop UI / Manual Validation

## 22. Disconnect / Reconnect / Churn Validation

## 23. Phase 5 Boundary Verification

## 24. Documentation Review

## 25. Defects and Findings

## 26. Non-Blocking Improvements

## 27. Regression Matrix

## 28. Phase 4 Acceptance Matrix

## 29. Commands Executed

## 30. Evidence Files

## 31. Conclusion
```

---

# 46. EXECUTIVE SUMMARY REQUIREMENT

Keep the executive summary concise and factual.

Example only:

```text
Phase 4 implements native PipeWire registry monitoring, normalized
AudioEndpoint objects, endpoint lifecycle handling, and Bluetooth-device
correlation using BlueZ5 PipeWire properties.

A clean Ninja build succeeded.
47/47 normal tests passed.
The Phase 3 BlueZ live test remained registered.
The PipeWire live test passed.
A real Bluetooth endpoint was observed and mapped to the expected
Phase 3 BluetoothDevice.

No Phase 5 routing implementation was detected.

VERDICT: PHASE 4 COMPLETE
```

Only state evidence actually obtained.

---

# 47. REQUIRED COMMAND LIST

The audit must list exact commands executed.

At minimum include whichever were actually run from:

```bash
cmake -S . -B build-audit -G Ninja
cmake --build build-audit
ctest --test-dir build-audit -N
ctest --test-dir build-audit --output-on-failure
bluetoothctl devices
bluetoothctl info "..."
wpctl status
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 ...
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 ...
./build-audit/apps/desktop/auralis-desktop
```

Include outcomes/exit codes.

---

# 48. EVERY FAIL NEEDS EVIDENCE

For each FAIL include:

```text
file/test/command
observed result
expected result
why it matters
severity
recommended correction
```

Avoid vague claims.

Bad:

```text
PipeWire looks wrong.
```

Good:

```text
BLOCKER — tests/integration/tst_BlueZLiveIntegration.cpp exists but
`ctest -N` does not list tst_BlueZLiveIntegration. Phase 3 regression
coverage was removed from CTest.
```

---

# 49. EVERY COMPLETE VERDICT NEEDS EVIDENCE

A `PHASE 4: COMPLETE` verdict must be backed by:

```text
successful clean configure
successful clean build
full default CTest pass
Phase 3 live-test registration
Phase 4 live-test registration
resolver/object-churn unit tests
native PipeWire source/API evidence
no production CLI parsing
no Phase 5 route/link creation
```

and real-device evidence when hardware is available.

---

# 50. NO SUCCESS BIAS

Do not try to prove Phase 4 is complete.

Determine whether it is complete.

A failure is valid audit evidence.

A missing test is valid audit evidence.

A hardware limitation is valid audit evidence.

Do not patch the repository to improve the verdict.

---

# 51. FINAL CURSOR RESPONSE

After writing:

```text
Auralis_PHASE_4_IMPLEMENTATION_AUDIT.md
```

return a short summary:

```text
Audit document: Auralis_PHASE_4_IMPLEMENTATION_AUDIT.md

Clean build: PASS/FAIL
Normal CTest: X/Y PASS
BlueZ live: PASS/FAIL/NOT EXECUTED
PipeWire live: PASS/FAIL/NOT EXECUTED
Bluetooth endpoint mapping: PASS/FAIL/NOT EXECUTED
Desktop smoke test: PASS/FAIL/NOT EXECUTED
Blockers: N
Major: N
Minor: N

PHASE 4: COMPLETE
```

or:

```text
PHASE 4: INCOMPLETE
```

Do not replace the detailed audit artifact with only this short answer.

---

# 52. FINAL INSTRUCTION

Perform the verification on the **actual Linux development machine**.

Use a fresh build directory.

Do not assume old build output is valid.

Do not modify source code during the audit.

Run the real test binaries.

Use real PipeWire and BlueZ services.

Use actual Bluetooth hardware when available.

Cross-check Auralis's endpoint state against the live PipeWire graph.

Preserve command evidence.

Write the complete Markdown audit document.

Then issue exactly one final verdict:

```text
PHASE 4: COMPLETE
```

or:

```text
PHASE 4: INCOMPLETE
```
