# AURALIS — PHASE 7 RIGOROUS INDEPENDENT VALIDATION & AUDIT MASTER PROMPT

## Document Purpose

This document is a **rigorous independent verification prompt for Cursor AI IDE running on the Auralis Linux development machine**.

It is intended to be run **after the Phase 7 correction implementation prompt has been executed**.

Your role in this prompt is fundamentally different:

> You are the independent validation engineer trying to prove that Phase 7 is still broken.

Do not assume the implementer fixed anything correctly.

Do not trust:

- commit messages;
- comments;
- implementation summaries;
- `PHASE_7_IMPLEMENTATION_AUDIT.md`;
- a previous `40/40 PASS`;
- screenshots;
- successful application startup.

Reproduce evidence yourself.

Your goal is to validate or reject the Phase 7 exit gate using:

1. source audit;
2. clean configure/build;
3. complete automated tests;
4. targeted adversarial tests;
5. QML runtime inspection;
6. manual GUI workflow;
7. optional real Bluetooth/PipeWire hardware integration;
8. persistence/relaunch tests;
9. route/session isolation tests;
10. an independently written audit document.

---

# 1. OPERATING MODE

Default mode is **verification-first and source-preserving**.

Do not modify production source merely to make a failing test pass during the audit.

Allowed:

- inspect source;
- build;
- run tests;
- run application;
- use existing test hooks/fakes;
- create temporary scripts/files outside production source for diagnostics;
- create the final audit document;
- create a dedicated audit test file only if required and if the user expects the auditor to add tests.

Preferred behavior:

If a code defect is found:

1. capture exact evidence;
2. identify file/line/symbol;
3. create a minimal reproduction;
4. mark the relevant gate FAILED;
5. continue testing the rest of Phase 7.

Do not silently fix defects during this audit unless explicitly instructed later.

---

# 2. EXPECTED PHASE 7 SCOPE

The validated GUI must provide:

```text
Dashboard
Devices
Sessions
Audio Routing
Diagnostics
Settings
```

and support the primary Auralis workflow without terminal dependence.

The core remains C++.

QML is presentation.

BlueZ D-Bus remains the Bluetooth backend.

PipeWire native APIs remain the audio backend.

SessionManager remains the session authority.

AudioRouter/RoutingCoordinator remain routing authorities.

---

# 3. SPECIFIC DEFECTS THAT MUST BE RETESTED

A previous independent audit found these exact classes of problems:

1. selected session UI could display active-session values while sending edits to another selected session;
2. SessionManager `Q_PROPERTY` notification signals did not fully match value-change paths for source and mute;
3. Audio Routing could edit `routes_.front()` and therefore mutate a session-managed route;
4. device details were incomplete and supported services/UUIDs were not actually shown;
5. session Duplicate was missing;
6. user-facing Restore was missing/incomplete;
7. Diagnostics did not provide enough device/PipeWire/route/recovery detail;
8. `Copy visible` was a placeholder rather than a real clipboard action;
9. dark-theme surfaces contained legacy dark text colors/default black Text;
10. file-logging UI persisted a setting without clearly applying or explaining runtime behavior;
11. GUI tests were too shallow to prove these behaviors.

Your audit must explicitly verify every one.

Do not write a generic report that merely says "all tests passed."

---

# 4. CREATE A FRESH AUDIT WORKSPACE

Start:

```bash
cd <ACTUAL_AURALIS_REPOSITORY>

pwd
git status --short
git branch --show-current
git rev-parse HEAD
git log --oneline --decorate -n 20
```

Record:

```text
Audit timestamp
Git commit
Branch
Working tree status
OS
Kernel
Compiler
CMake
Ninja
Qt version
BlueZ version
PipeWire version
```

Commands may include:

```bash
uname -a
lsb_release -a || cat /etc/os-release
gcc --version | head -1
g++ --version | head -1
cmake --version | head -1
ninja --version
qmake6 --version || true
bluetoothctl --version || true
pkg-config --modversion libpipewire-0.3 || true
pkg-config --modversion Qt6Core || true
pkg-config --modversion Qt6Quick || true
```

Do not treat missing optional hardware utilities as application failures unless they are declared project requirements.

---

# 5. INSPECT THE CURRENT SOURCE BEFORE BUILDING

Search for the previously risky patterns.

## Session selected-vs-active state

```bash
rg -n \
  "selectedId|groupVolume|currentSourceId|currentMuted|recoveryPolicy|setGroupVolume|setSource|setSessionMuted|setRecoveryPolicy" \
  ui apps src include tests
```

## Qt property notifications

```bash
rg -n \
  "Q_PROPERTY|NOTIFY|currentSourceIdChanged|currentMutedChanged|groupVolumeChanged|sessionUpdated|currentSessionIdChanged" \
  include src
```

## Routing ownership

```bash
rg -n \
  "currentRouteId|routes_\.front|routes_\.first|ownerType|ownerId|Session|Manual|createRoute|setRouteSource|setRouteDestinations" \
  include src ui tests
```

## Device services/details

```bash
rg -n \
  "UUID|uuids|servicesResolved|ShowServices|Services|DeviceDetails|selectedPath|objectPath|addressType|appearance|lastSeen" \
  ui src include tests
```

## Duplicate/restore

```bash
rg -n \
  "duplicate|Duplicate|restore|Restore|restoreLastSession|clone|copySession" \
  ui src include tests
```

## Diagnostics placeholder

```bash
rg -n \
  "Copy visible|clipboard|not wired|TODO|FIXME|placeholder|coming soon|not implemented" \
  ui apps src include
```

## Theme regressions

```bash
rg -n \
  'color:\s*"#[0-9A-Fa-f]{6}"|Text\s*\{' \
  ui apps
```

## Shell dependencies

```bash
rg -n \
  "QProcess|system\(|popen|bluetoothctl|wpctl|pactl|pw-cli|pw-link" \
  apps src include ui
```

Classify each shell reference:

```text
production runtime
test-only
documentation
diagnostic helper
```

Any production dependency for primary functionality is a Phase 7 failure.

---

# 6. CLEAN BUILD FROM ZERO

Delete the build directory:

```bash
rm -rf build
```

Configure:

```bash
cmake -S . -B build -G Ninja
```

Build:

```bash
cmake --build build
```

List tests:

```bash
ctest --test-dir build -N
```

Run full suite:

```bash
ctest --test-dir build --output-on-failure
```

Then rerun with verbose output for any failures:

```bash
ctest --test-dir build -V -R '<FAILING_PATTERN>'
```

Record exact:

```text
number of tests
passed
failed
not run
skipped
```

Never copy counts from an old audit.

---

# 7. TEST INVENTORY REVIEW

Do not assume quantity equals coverage.

List all test sources:

```bash
find tests -type f | sort
```

Search test intent:

```bash
rg -n \
  "Session|selected|source|mute|Route|owner|Manual|Device|UUID|Duplicate|Restore|Dashboard|Diagnostics|Clipboard|Theme|QML" \
  tests
```

Create a coverage map:

```text
Requirement                               Automated evidence
----------------------------------------------------------------
selected session isolation               <test name / NONE>
source notify property                    <test name / NONE>
mute notify property                      <test name / NONE>
manual/session route isolation            <test name / NONE>
device UUID/services display              <test name / NONE>
duplicate                                 <test name / NONE>
restore                                   <test name / NONE>
dashboard reactive source/mute            <test name / NONE>
diagnostics clipboard                     <test name / NONE>
QML all-pages load                        <test name / NONE>
```

A missing test does not automatically mean code is broken, but it reduces confidence and must be documented.

---

# 8. ADVERSARIAL TEST A — SELECTED SESSION ISOLATION

This is a mandatory Phase 7 audit test.

Create or use test fixtures for:

```text
Session A
Session B
```

Give them deliberately different values.

Example:

```text
A:
  name A
  volume 0.31
  source source-A
  muted false
  policy policy-A

B:
  name B
  volume 0.79
  source source-B
  muted true
  policy policy-B
```

Activate A.

Select B in the Sessions editor.

Verify UI state for B:

```text
volume 79%
source-B
muted
policy-B
```

Then change B to:

```text
volume 65%
source source-C
muted false
policy policy-C
```

Verify:

```text
B changed
A unchanged
Dashboard still reflects active A
```

Switch back to A.

Verify A still shows its original values.

If the UI displays A's values while B is selected, Phase 7 FAILS.

Capture:

- test;
- screenshot if useful;
- log;
- relevant code path.

---

# 9. ADVERSARIAL TEST B — Q_PROPERTY NOTIFY SEMANTICS

Use C++ unit tests with `QSignalSpy` where available.

For every QML-exposed property, specifically:

```text
currentSourceId
currentMuted
groupVolume
currentSessionId/current session state
```

test:

```text
before value
perform backend operation
after value
expected NOTIFY count
```

Source test:

```text
active session source = A
setSource(activeSession, B)
currentSourceId == B
correct NOTIFY emitted
```

Mute test:

```text
currentMuted == false
setSessionMuted(activeSession, true)
currentMuted == true
correct NOTIFY emitted
```

Then verify QML binding changes without page reload.

A property getter returning the right value after manual reevaluation is insufficient.

The QML engine must be notified.

---

# 10. ADVERSARIAL TEST C — ROUTE OWNERSHIP / SESSION ISOLATION

This is mandatory.

Establish:

```text
Active session S
Session-managed route RS
Manual route RM
```

Record complete RS state:

```text
route ID
owner
source
destinations
active
volume
mute
```

Through the Audio Routing GUI:

1. select RM;
2. change source;
3. change destination;
4. change volume;
5. mute;
6. activate;
7. deactivate.

After every action verify RS remains unchanged except for legitimate external backend changes.

Then reorder route model if possible:

```text
remove/add another route
sort/filter
session creates another route
```

Verify the editor continues to target RM by stable ID.

Search the production code for:

```text
routes_.front()
routes_.first()
index 0
```

as the selected/editable route identity.

If model ordering can retarget a user action, Phase 7 FAILS.

---

# 11. ADVERSARIAL TEST D — SESSION ROUTE READ-ONLY SEMANTICS

If session routes appear in Audio Routing:

Verify they are either:

```text
read-only
```

or explicitly edited through a safe session-aware operation.

Try to mutate a session-owned route via manual controls.

Expected:

```text
operation not offered
or
operation rejected safely
```

Unexpected:

```text
session route silently modified
```

is a blocker.

---

# 12. ADVERSARIAL TEST E — DEVICE DETAILS AND SERVICES

Use a fake or real device containing:

```text
name
alias
address
addressType
RSSI
paired
trusted
connected
servicesResolved
UUID list
icon/class/appearance if available
audio endpoint mapping
```

Open Details.

Verify the UI presents useful information.

Test fallback device:

```text
name empty
alias empty
address present
```

The UI should fall back safely to address.

Test services:

```text
UUID list non-empty
```

Activate `Services` or open Details.

Verify UUID/services are actually visible.

A button that does nothing visible is a FAIL.

Remove the selected device while details are open.

Verify no crash, stale dereference, or invalid command target.

---

# 13. ADVERSARIAL TEST F — SESSION DUPLICATE

Create:

```text
Session Original
```

with:

```text
2 devices
source X
policy Y
volume Z
```

Duplicate from GUI.

Verify:

```text
new stable session ID
name unique
device membership copied
source copied
policy copied
volume copied if intended/persisted
active runtime route IDs NOT copied
original unchanged
copy not accidentally active
persistence contains both
```

Restart/reload and verify both remain.

Failure to provide a working Duplicate flow is a Phase 7 requirement failure.

---

# 14. ADVERSARIAL TEST G — RESTORE

First determine exact backend semantics.

Document what `restoreLastSession()` or equivalent means.

Then test it using that definition.

Typical expected flow:

```text
create/configure session
activate/save as needed
terminate application cleanly
launch again
invoke Restore if not auto-restored
```

Verify:

```text
correct session restored
correct source
correct member devices
correct policy
state consistent with currently available hardware
missing devices produce degraded/recovery state rather than fake ACTIVE
```

If auto-restore is enabled in Settings, test both:

```text
auto restore enabled
auto restore disabled + manual Restore
```

where supported.

---

# 15. ADVERSARIAL TEST H — DASHBOARD REACTIVITY

With active Session A:

Change:

```text
source
mute
volume
member connectivity
endpoint availability
session state
```

Observe Dashboard.

It must update without:

```text
switching pages
restarting app
forcing refresh
```

Test:

```text
ACTIVE -> DEGRADED -> RECOVERING -> ACTIVE
```

to the extent supported by fakes/hardware.

Capture stale bindings as failures.

---

# 16. ADVERSARIAL TEST I — DIAGNOSTICS

Generate real/fake changes.

### Bluetooth

```text
scan on/off
device inserted
connected
disconnected
```

### Audio

```text
endpoint inserted
removed
route activated
```

### Session

```text
activate
degrade
recover
deactivate
```

Verify Diagnostics represents the current state.

Check:

```text
adapter
devices
PipeWire readiness
nodes/endpoints
routes/links
session
recovery
errors
logs
```

Compare UI values to direct backend model state in the test harness.

Do not use terminal output as the application's state source.

---

# 17. ADVERSARIAL TEST J — CLIPBOARD

If `Copy visible` exists:

1. create logs;
2. filter logs;
3. click Copy visible;
4. inspect Qt/system clipboard;
5. verify copied contents match visible rows.

If the button reports:

```text
not wired
coming soon
```

or has no effect, mark FAIL.

If the button was removed, verify there is no dead placeholder.

---

# 18. ADVERSARIAL TEST K — DARK THEME

Launch every page in the actual default theme.

Inspect:

```text
page titles
card titles
secondary labels
device rows
endpoint rows
source rows
route panel
buttons
disabled states
dialogs
warnings/errors
log viewer
settings
```

Search for hardcoded colors.

Do not judge only by appearance at one screen.

Test:

```text
normal
hover
focused
selected
disabled
warning
error
active
```

At minimum verify no default black/dark-gray text is placed on dark surfaces.

If accessible tooling is available, use it; otherwise perform a careful visual/manual inspection and source review.

---

# 19. ADVERSARIAL TEST L — FILE LOGGING SETTINGS

Determine intended runtime contract.

### If enable/disable is live

Toggle in Settings.

Verify Logger's real state changes immediately.

Write a test log event and verify behavior.

### If restart-required

Toggle.

Verify UI explicitly says restart is required.

Restart.

Verify behavior matches saved setting.

Change log path.

Verify path semantics and restart messaging.

A UI that implies immediate behavior while only persisting a startup setting is a FAIL.

---

# 20. ADVERSARIAL TEST M — MODEL REORDER / STALE SELECTION

For each dynamic list:

```text
devices
sessions
sources
endpoints
routes
```

test:

1. select item by ID;
2. insert another item before it;
3. remove another item;
4. update sorting/filter;
5. perform an action on selected item.

Verify correct stable object receives the action.

Then remove the selected object.

Verify selection clears safely.

No command should target the next row simply because it inherited the same index.

---

# 21. ADVERSARIAL TEST N — ASYNC FAILURE

Use backend fakes or controlled failure paths.

### Device

```text
connect requested
operation fails
```

Verify:

```text
pending state clears
connected remains false
error visible
retry possible
```

### Session

```text
activate requested
route fails
```

Verify:

```text
not fake ACTIVE
FAILED/DEGRADED according to backend
error visible
```

### Route

```text
endpoint disappears during activation
```

Verify safe state.

No infinite spinner.

No optimistic lie.

---

# 22. ADVERSARIAL TEST O — QML WARNINGS

Launch:

```bash
./build/apps/desktop/auralis-desktop
```

Capture stdout/stderr:

```bash
./build/apps/desktop/auralis-desktop \
  > /tmp/auralis-gui.out \
  2> /tmp/auralis-gui.err
```

Exercise all pages.

Inspect:

```bash
cat /tmp/auralis-gui.err
```

Flag application-caused:

```text
ReferenceError
TypeError
binding loop
undefined
Cannot read property
Cannot assign
module unavailable
component load failure
invalid Connections target
```

No fatal QML runtime warning is acceptable for the Phase 7 exit gate.

---

# 23. ADVERSARIAL TEST P — WINDOW RESIZING

Test at approximately:

```text
minimum supported size
1280x800
1440x900
1920x1080
```

Verify all six pages.

Look for:

```text
overlap
clipped primary action
off-screen dialog
unusable horizontal scrolling
truncated status with no tooltip
inaccessible buttons
broken navigation
```

Test long names:

```text
very long Bluetooth alias
very long session name
long error message
```

---

# 24. ADVERSARIAL TEST Q — KEYBOARD ACCESS

Without mouse for the sequence:

```text
navigate sidebar
open Devices
activate Scan
select a device
open Details
close dialog
open Sessions
select session
activate/deactivate as safe
open Settings
change a harmless control
```

Verify:

```text
Tab
Shift+Tab
Enter
Space
Escape
```

and visible focus.

Icon-only controls need accessible names/tooltips.

---

# 25. REAL BLUETOOTH INTEGRATION

Only if a safe test device is available.

Before testing:

```bash
bluetoothctl show
bluetoothctl devices
```

These shell commands are allowed for **external validation only**.

Do not infer application success from `bluetoothctl`.

Run the application's own GUI flow:

```text
scan
discover
pair
trust if supported
connect
endpoint appears
disconnect
reconnect
forget only if safe
```

Cross-check with system state when useful.

If no safe hardware is available:

```text
NOT RUN — no controlled Bluetooth test device
```

Do not mark PASS.

---

# 26. REAL PIPEWIRE INTEGRATION

For external validation only, use tools such as:

```bash
wpctl status
pw-cli ls Node
pw-link -l
```

when installed.

These are ground-truth inspection tools for the auditor, not application dependencies.

Compare GUI/Diagnostics route and endpoint state with PipeWire.

Test active audio/session if safe.

---

# 27. MULTI-DEVICE SESSION HARDWARE TEST

If two controlled Bluetooth audio devices are available:

1. connect both;
2. verify two endpoints;
3. create session with both;
4. select source;
5. activate;
6. verify session ACTIVE;
7. adjust group volume;
8. disconnect one device;
9. verify DEGRADED;
10. allow reconnect/recovery;
11. verify expected recovery;
12. deactivate;
13. restart and restore.

If two devices are not available:

```text
NOT RUN — requires two controlled Bluetooth audio devices
```

Do not simulate PASS.

Use fakes/automated tests to cover state logic separately.

---

# 28. PERSISTENCE / RESTART TEST

Create a distinctive test session and safe settings.

Record:

```text
session ID/name
members
source
policy
volume
mute if persisted
settings
```

Close app normally.

Relaunch.

Verify all intended persisted values.

Then clean up test data.

Do not leave the user's environment altered unnecessarily.

---

# 29. PRODUCTION SHELL-DEPENDENCY AUDIT

This is mandatory.

Inspect all `QProcess`, `system`, shell command constructions.

For each occurrence document:

```text
file
purpose
production/test/doc
command
required for primary functionality? yes/no
```

The primary application must not depend on:

```text
bluetoothctl
wpctl
pactl
```

for its implementation.

A test helper is okay.

A production backend fallback is not okay under the Phase 7 architecture.

---

# 30. MEMORY / LOG BOUNDING

For Diagnostics log model:

Generate more entries than its configured capacity.

Verify:

```text
row count stays bounded
oldest rows evicted
UI stays responsive
```

If the intended capacity is N:

```text
insert N + 500
rowCount <= N
```

unless implementation defines a different precise policy.

---

# 31. RAPID EVENT STRESS

Where fakes allow:

```text
rapid RSSI updates
device connect/disconnect sequence
endpoint add/remove
route updates
session member state changes
log storm
```

Verify:

```text
no crash
no model assertion
no QML binding explosion
no unbounded row resets
no stale selected object
```

Pay special attention to Qt model begin/end insert/remove correctness.

---

# 32. SHUTDOWN TEST

Start active backend activity if safe:

```text
discovery
logs
session state callbacks
```

Close Auralis.

Verify:

```text
clean exit
no crash
no QObject-after-destruction callback errors
no hanging process
```

Check:

```bash
pgrep -a auralis || true
```

No zombie/hung desktop process.

---

# 33. AUDIT THE EXISTING PHASE 7 REPORT

Open:

```text
docs/PHASE_7_IMPLEMENTATION_AUDIT.md
```

For every factual claim, verify supporting evidence.

Examples:

```text
"all six screens complete"
"40/40 tests pass"
"theme coherent"
"duplicate implemented"
"restore implemented"
"diagnostics complete"
```

Mark any inaccurate claim.

Do not edit the original report to conceal history.

---

# 34. CREATE AN INDEPENDENT AUDIT DOCUMENT

Create:

```text
docs/PHASE_7_RIGOROUS_VALIDATION_AUDIT.md
```

Do not overwrite the implementation audit.

The document must use this structure.

---

# Auralis Phase 7 Rigorous Validation Audit

## 1. Audit Identity

```text
Date:
Machine:
OS:
Git branch:
Git commit:
Working tree:
Qt:
BlueZ:
PipeWire:
Auditor mode:
```

## 2. Executive Verdict

Exactly one:

```text
PHASE 7 VALIDATION VERDICT: PASSED
```

or:

```text
PHASE 7 VALIDATION VERDICT: FAILED
```

Then summarize why.

## 3. Clean Build

```text
Configure:
Build:
Warnings:
```

Include exact commands.

## 4. Automated Test Results

```text
Total:
Passed:
Failed:
Skipped/Not Run:
```

List failures.

## 5. Required Defect Regression Matrix

| Previously Found Issue | Result | Evidence |
|---|---|---|
| Selected-session state isolation | PASS/FAIL | |
| Source Q_PROPERTY notify | PASS/FAIL | |
| Mute Q_PROPERTY notify | PASS/FAIL | |
| Route ownership isolation | PASS/FAIL | |
| Device details | PASS/FAIL | |
| Supported services/UUIDs | PASS/FAIL | |
| Session Duplicate | PASS/FAIL | |
| Session Restore | PASS/FAIL | |
| Diagnostics completeness | PASS/FAIL | |
| Clipboard placeholder removed/wired | PASS/FAIL | |
| Dark-theme contrast | PASS/FAIL | |
| File logging semantics | PASS/FAIL | |
| Behavioral GUI tests | PASS/FAIL | |

## 6. Source Architecture Audit

Document:

```text
C++/QML boundary
BlueZ usage
PipeWire usage
route ownership
stable IDs
thread-affinity concerns
shell dependencies
```

## 7. Dashboard

Test results and evidence.

## 8. Devices

Test results and evidence.

## 9. Sessions

Test results and evidence.

## 10. Audio Routing

Test results and evidence.

## 11. Diagnostics

Test results and evidence.

## 12. Settings

Test results and evidence.

## 13. QML Runtime Warning Audit

Include warnings observed.

## 14. Responsiveness/Accessibility

Window-size and keyboard tests.

## 15. Persistence

Results.

## 16. Real Bluetooth Hardware

Use:

```text
PASS
FAIL
NOT RUN
```

with exact reason.

## 17. Real PipeWire/Audio

Use exact result.

## 18. Multi-Device Session

Use exact result.

## 19. Stress/Shutdown

Results.

## 20. Discrepancies With Implementation Audit

List inaccurate or unverified claims.

## 21. Blocking Defects

Numbered list.

For each:

```text
Severity:
Subsystem:
File/symbol:
Reproduction:
Expected:
Actual:
Impact:
```

## 22. Non-Blocking Improvements

Only after blockers.

## 23. Phase 7 Exit Gate

Exactly one:

```text
PHASE 7 EXIT GATE: PASSED
```

or:

```text
PHASE 7 EXIT GATE: FAILED
```

---

# 35. SEVERITY CLASSIFICATION

Use:

## BLOCKER

Prevents trustworthy Phase 7 workflow.

Examples:

```text
selected session modifies wrong session
manual route corrupts session route
app crashes
fake operational state
primary button placeholder
```

## HIGH

Major required feature missing or incorrect.

Examples:

```text
Duplicate missing
Restore missing
services UI missing
Dashboard stale source/mute
```

## MEDIUM

Important diagnostic/UX issue with workaround.

## LOW

Polish/non-critical issue.

Do not mark missing mandatory behavior LOW.

---

# 36. PASS/FAIL RULES

Phase 7 may receive:

```text
PHASE 7 VALIDATION VERDICT: PASSED
```

only when:

- clean configure succeeds;
- clean build succeeds;
- automated suite passes;
- no known blocker remains;
- selected-session isolation passes;
- property notification tests pass;
- route ownership isolation passes;
- device details/services requirement passes;
- duplicate passes;
- restore passes;
- diagnostics has no fake action;
- theme readability passes;
- settings behavior is truthful;
- QML warning gate passes;
- primary GUI workflow works without terminal;
- Phase 0–6 regression tests remain green.

Hardware tests may be `NOT RUN` only if hardware is genuinely unavailable and equivalent domain logic has automated coverage.

A code defect cannot be excused as a hardware limitation.

---

# 37. DO NOT BE FOOLED BY THESE FALSE POSITIVES

## "CTest passes"

Tests may not cover the bug.

## "QML loads"

Controls may still target wrong objects.

## "Screen exists"

A required action may be a stub.

## "Backend property is correct when queried"

QML can still remain stale if NOTIFY is wrong.

## "Route activation works"

It may be activating the wrong route.

## "The first route is usually the manual route"

Ordering is not identity.

## "Details button opens something"

It must present actual useful device details.

## "Restore happens automatically"

That does not necessarily satisfy the required explicit workflow unless the defined product behavior says so.

## "Copy visible shows a toast"

The clipboard must actually contain data, or the action must be removed.

---

# 38. OPTIONAL TEST AUTOMATION SCRIPT

If useful, create:

```text
scripts/audit_phase7.sh
```

or a temporary audit script outside source.

It may automate:

```text
clean configure
build
ctest
QML smoke
grep checks
version capture
```

Do not make destructive Bluetooth/device operations automatic.

Do not silently modify user system settings.

If you add a permanent script, document it and keep it safe/idempotent.

---

# 39. FINAL CONSOLE SUMMARY

At the end of the audit, print:

```text
AURALIS PHASE 7 RIGOROUS VALIDATION

Git: <commit>

Clean configure: PASS/FAIL
Clean build: PASS/FAIL
CTest: <passed>/<total>
QML runtime: PASS/FAIL

Selected-session isolation: PASS/FAIL
Q_PROPERTY notifications: PASS/FAIL
Route ownership: PASS/FAIL
Device details/services: PASS/FAIL
Duplicate: PASS/FAIL
Restore: PASS/FAIL
Diagnostics: PASS/FAIL
Theme: PASS/FAIL
Settings logging semantics: PASS/FAIL

Bluetooth hardware: PASS/FAIL/NOT RUN
PipeWire hardware: PASS/FAIL/NOT RUN
Multi-device session: PASS/FAIL/NOT RUN

PHASE 7 EXIT GATE: PASSED/FAILED

Audit:
docs/PHASE_7_RIGOROUS_VALIDATION_AUDIT.md
```

---

# 40. MASTER VALIDATION DIRECTIVE

Approach this audit adversarially.

The question is not:

> "Does Auralis look finished?"

The question is:

> **Can every Phase 7 requirement survive a clean build, source audit, behavioral tests, state-isolation tests, route-ownership tests, QML reactivity checks, restart/persistence checks, and real Linux runtime validation without relying on fake state or terminal-driven application logic?**

Only evidence may pass the gate.
