# AURALIS — PHASE 7 CORRECTION & COMPLETION MASTER IMPLEMENTATION PROMPT

## Document Purpose

This document is a **deep corrective implementation prompt for Cursor AI IDE**.

Use it against the current Auralis repository in which:

- Phase 0 is completed.
- Phase 1 is completed.
- Phase 2 is completed.
- Phase 3 is completed.
- Phase 4 is completed.
- Phase 5 is completed.
- Phase 6 is completed.
- Phase 7 has been substantially implemented, but an independent source-level audit found correctness, completeness, UI-state, routing-ownership, diagnostics, theming, and test-coverage gaps.

Your task is **NOT** to rebuild Phase 7 from scratch.

Your task is to:

1. inspect the actual current repository;
2. reproduce and understand the reported Phase 7 gaps;
3. correct them with the smallest clean architectural changes;
4. preserve all functioning Phase 0–6 behavior;
5. preserve the existing Phase 7 architecture wherever it is sound;
6. add rigorous regression coverage for every corrected defect;
7. run a clean build and complete test suite;
8. manually exercise the GUI;
9. update the Phase 7 implementation audit only after the exit gate is genuinely satisfied.

The current repository's own Phase 7 audit may claim:

```text
PHASE 7 EXIT GATE: PASSED
```

Do **not** trust that statement as evidence.

Re-establish the result independently.

---

# 1. YOUR ROLE

Act as the senior C++/Qt/QML engineer responsible for the **Phase 7 correction pass**.

You must think in terms of:

- authoritative state ownership;
- correct C++/QML notification semantics;
- asynchronous backend actions;
- model identity;
- session isolation;
- route ownership;
- safe UI state;
- clean QML presentation;
- testability;
- no regression to Bluetooth, PipeWire, routing, or session logic.

You are expected to edit the repository and resolve code defects.

Do not merely produce recommendations.

Do not stop once compilation succeeds.

Do not stop once `ctest` succeeds if the known behavioral problems remain untested.

---

# 2. PRIMARY OBJECTIVE

The final corrected application must satisfy the Phase 7 contract:

> The complete primary Auralis workflow can be operated from the Qt Quick/QML desktop GUI without relying on terminal commands, and every displayed operational state comes from the authoritative Phase 0–6 backend.

Required GUI areas remain:

1. Dashboard
2. Devices
3. Sessions
4. Audio Routing
5. Diagnostics
6. Settings

The implementation already contains substantial work in these areas. Preserve correct work.

---

# 3. STRICT PHASE BOUNDARY

## Preserve existing backend ownership

Continue to use the existing owners for:

```text
Bluetooth lifecycle       -> existing BlueZ/D-Bus backend
Device state              -> existing device registry/manager
PipeWire state            -> existing native PipeWire backend
Audio endpoints           -> existing audio endpoint registry/model
Routing                    -> existing AudioRouter/routing coordinator
Sessions                   -> existing SessionManager
Persistence/configuration  -> existing configuration/session persistence layer
Logging                    -> existing Logger
```

## Do NOT introduce

- `bluetoothctl` production calls;
- `wpctl` production calls;
- `pactl` production calls;
- shell parsing as backend state;
- a second DeviceRegistry;
- a second SessionManager;
- a second AudioRouter;
- production fake devices;
- QML-side session state machines;
- QML-side route state machines;
- QML-side persistence;
- arbitrary rewrites of already working Phase 0–6 code.

---

# 4. FIRST ACTION — AUDIT THE REAL REPOSITORY

Before changing anything:

```bash
pwd
git status --short
git branch --show-current
git log --oneline --decorate -n 30
```

Inspect repository layout:

```bash
find . -maxdepth 4 -type f | sort | sed -n '1,360p'
```

Search all Phase 7-facing code:

```bash
rg -n \
  "SessionsPage|DevicesPage|DashboardPage|AudioRoutingPage|DiagnosticsPage|SettingsPage|RoutePanel|DeviceRow|EndpointRow|SourceRow|StatusBadge|SessionManager|AudioRouter|RoutingCoordinator|DeviceRegistry|DiagnosticsLogModel|NotificationController|Q_PROPERTY|currentSourceId|currentMuted|groupVolume|currentRouteId|setRouteSource|setRouteDestinations|restoreLastSession|duplicate|UUID|servicesResolved|Clipboard|Copy visible|fileLogging" \
  apps src include ui tests docs
```

Read the existing:

```text
docs/PHASE_7_IMPLEMENTATION_AUDIT.md
```

if present.

Treat it as historical evidence only.

---

# 5. BASELINE BUILD BEFORE EDITING

If the local development machine has the required Qt/BlueZ/PipeWire toolchain, run:

```bash
rm -rf build

cmake -S . -B build -G Ninja

cmake --build build

ctest --test-dir build --output-on-failure
```

Record:

- configure result;
- compiler result;
- test count;
- failures;
- skipped tests;
- warnings.

If the baseline does not build, determine whether failure is:

```text
CODE DEFECT
ENVIRONMENT/DEPENDENCY
TEST ENVIRONMENT
HARDWARE-ONLY CONDITION
```

Do not conflate them.

Do not fix unrelated environment problems by changing production architecture.

---

# 6. KNOWN DEFECT GROUP A — SELECTED SESSION STATE IS NOT AUTHORITATIVE

## 6.1 Problem statement

The current Sessions UI can select one session while reading display properties from the globally active session.

A known pattern found during audit is conceptually:

```qml
property string selectedId

VolumeControl {
    value: sessions.groupVolume
    onValueChanged: sessions.setGroupVolume(root.selectedId, value)
}
```

where `SessionManager::groupVolume()` returns the active session's volume.

This means the screen can:

```text
DISPLAY Session A
MODIFY  Session B
```

for the same control.

That is a correctness failure.

## 6.2 Required correction

The selected session editor must display data for **the selected session ID**.

Choose the cleanest existing-compatible design.

Preferred architectural options:

### Option A — selected-session presentation model

Introduce or extend a presentation-facing object:

```text
SelectedSessionViewModel
SessionDetailsViewModel
SessionEditorModel
```

that has:

```text
sessionId
name
state
sourceId
sourceName
groupVolume
muted
recoveryPolicy
member devices
availability/degraded information
```

and updates when:

```text
selectedId changes
selected session is updated
selected session is deleted
source availability changes
device membership/state changes
```

### Option B — role/query helpers

If the current SessionListModel already exposes all required roles cleanly, use a safe lookup by stable session ID.

Avoid row-index-dependent state.

### Option C — SessionManager query API

Add narrow QML-safe accessors if needed:

```cpp
sessionGroupVolume(sessionId)
sessionMuted(sessionId)
sessionSourceId(sessionId)
sessionRecoveryPolicy(sessionId)
```

but prefer a reactive selected-session object/model rather than repeated imperative lookup calls.

## 6.3 Acceptance scenarios

Test this exact case:

```text
Session A:
    ACTIVE
    group volume 30%
    source Source-A
    recovery policy Policy-A
    muted false

Session B:
    INACTIVE
    group volume 80%
    source Source-B
    recovery policy Policy-B
    muted true
```

Select Session B.

The Sessions editor must show:

```text
80%
Source-B
Policy-B
Muted
```

Changing its controls must target Session B only.

Session A must remain unchanged.

Then select Session A and verify 30%/Source-A/Policy-A/unmuted.

This must be automated.

---

# 7. KNOWN DEFECT GROUP B — Q_PROPERTY NOTIFY CONTRACTS ARE WRONG/INCOMPLETE

## 7.1 Known source property issue

A known declaration resembles:

```cpp
Q_PROPERTY(QString currentSourceId
           READ currentSourceId
           NOTIFY currentSessionIdChanged)
```

while `setSource()` updates source state and emits a session update but not necessarily `currentSessionIdChanged()`.

A QML binding to:

```qml
sessions.currentSourceId
```

can therefore remain stale.

## 7.2 Known mute property issue

A known declaration resembles:

```cpp
Q_PROPERTY(bool currentMuted
           READ currentMuted
           NOTIFY groupVolumeChanged)
```

while changing mute does not necessarily emit the declared notify signal.

This violates Qt property semantics.

## 7.3 Required correction

Audit **every Q_PROPERTY exposed to QML**, not only these two.

For each property:

1. identify all code paths that can change the returned value;
2. ensure the NOTIFY signal is emitted whenever the property value changes;
3. ensure the signal is not emitted spuriously at an unreasonable rate;
4. ensure the signal has correct thread affinity;
5. add `QSignalSpy` regression tests.

Prefer dedicated signals where clearer:

```cpp
Q_PROPERTY(QString currentSourceId
           READ currentSourceId
           NOTIFY currentSourceIdChanged)

Q_PROPERTY(bool currentMuted
           READ currentMuted
           NOTIFY currentMutedChanged)
```

If changing public signals is too disruptive, ensure existing notify semantics are still strictly correct.

## 7.4 Required regression tests

At minimum:

```text
active session source changed
-> currentSourceId changes
-> notify signal emitted
-> QML-visible property observes new source

active session mute changed
-> currentMuted changes
-> notify signal emitted
-> Dashboard control updates
```

Also test when:

```text
active session changes
active session is deleted
session deactivates
```

---

# 8. KNOWN DEFECT GROUP C — AUDIO ROUTING ROUTE OWNERSHIP IS UNSAFE

## 8.1 Problem statement

The current route panel uses a property conceptually equivalent to:

```cpp
AudioRouter::currentRouteId()
{
    return routes_.isEmpty() ? QString() : routes_.front().id;
}
```

The GUI then mutates that route:

```text
setRouteSource
setRouteDestinations
activateRoute
deactivateRoute
setRouteVolume
setRouteMuted
```

Phase 6's RoutingCoordinator also creates routes for session members through the same AudioRouter.

This means the standalone Audio Routing page can accidentally edit a route owned by an active session.

That is unacceptable.

## 8.2 Required architecture

Establish explicit route ownership or isolation.

At minimum distinguish:

```text
MANUAL/USER ROUTE
SESSION-MANAGED ROUTE
```

Possible clean approaches:

### Approach A — route ownership metadata

Add route metadata:

```cpp
enum class RouteOwnerType {
    Manual,
    Session
};

QString ownerId;
RouteOwnerType ownerType;
```

The standalone routing planner edits only Manual routes.

The session engine edits only Session routes.

### Approach B — dedicated manual route ID

Create a persistent/manual route concept for the Audio Routing screen and never use `routes_.front()` as the UI target.

### Approach C — explicit selected route model

Expose all routes with:

```text
routeId
ownerType
ownerId
editable
```

The user may inspect session routes but cannot mutate a session-owned route from the manual editor unless such behavior is deliberately supported by SessionManager.

Use the smallest change compatible with existing design.

## 8.3 Forbidden behavior

Do not keep:

```text
"first route in vector" == "current route"
```

as the control target.

Vector/list order is not ownership.

## 8.4 Session route safety tests

Automate:

```text
Create Session S
Activate S
RoutingCoordinator creates session route R_session

Open/create manual route R_manual

Change manual source
Change manual destinations
Change manual volume
Mute manual route
Activate/deactivate manual route
```

Verify:

```text
R_session source unchanged
R_session destinations unchanged
R_session session ownership unchanged
Session S remains semantically intact
```

Then deactivate Session S and verify only its routes are removed/deactivated according to existing semantics.

## 8.5 Display behavior

Audio Routing should clearly label session-managed routes.

Examples:

```text
Living Room — Session route — Managed by session
Manual Route — Editable
```

If session routes are read-only on that page, visibly communicate this.

---

# 9. KNOWN DEFECT GROUP D — DEVICE DETAILS / SUPPORTED SERVICES ARE INCOMPLETE

## 9.1 Problem statement

The device list is substantially implemented, but the details UI does not expose the rich data already present in the backend.

A known current details panel primarily shows:

```text
raw BlueZ object path
audio status
```

The backend already contains richer device properties such as:

```text
name
alias
address
address type
RSSI
paired
connected
trusted
blocked
services resolved
icon
class
appearance
UUIDs/services
last seen
transport/audio mapping
```

The current `Services` action does not actually present the service list.

## 9.2 Required correction

Create a proper device detail panel/dialog/page.

Show meaningful available fields:

```text
Friendly name
Alias
Bluetooth address
Address type
Signal/RSSI
Paired
Trusted
Connected
Blocked
Services resolved
Device icon/type
Class
Appearance
Last seen
Mapped audio endpoint
Transport/profile if available
Supported services / UUIDs
```

Technical properties can be grouped under an expandable "Technical details" section.

The raw BlueZ object path can remain available in Diagnostics/technical details but must not be the main user-facing identity.

## 9.3 Services

The existing Services action must:

- open a service/technical details section; or
- remove the separate button and include services in Device Details.

Do not leave a button that changes only internal selection with no visible action.

## 9.4 Device disappearance safety

If the device disappears while its detail panel is open:

- do not dereference stale QObjects;
- show "device no longer available" or close gracefully;
- clear stale selected IDs safely.

## 9.5 Tests

Test:

```text
device with name + UUIDs
device with no name
device with no UUIDs
device RSSI update
servicesResolved change
device removal while details open/selected
```

---

# 10. KNOWN DEFECT GROUP E — SESSION DUPLICATE AND RESTORE

## 10.1 Roadmap requirement

The Sessions workflow requires:

```text
create
rename
add/remove devices
select source
route policy
activate/deactivate
save
restore
duplicate
delete
```

Current implementation is missing or incomplete for:

```text
Duplicate
Manual/user-facing Restore
```

## 10.2 Duplicate

Implement a proper duplication workflow through the session backend.

Expected behavior:

```text
Original:
    name
    devices
    source
    recovery/routing policy
    group volume
    mute state if persistable
```

New session:

```text
new stable session ID
copy of supported configuration
not accidentally treated as active
valid unique/default name
persisted normally
```

Possible naming:

```text
Living Room Copy
Living Room Copy 2
```

Use existing naming conventions and backend validation.

Do not clone runtime route IDs or stale live connection state.

## 10.3 Restore

Determine the Phase 6 persistence semantics before coding.

If `restoreLastSession()` means restoring/reopening the most recently active persisted session, expose a user-facing action where appropriate.

Possible UI:

```text
Restore last session
```

or session-level:

```text
Restore
```

depending on backend semantics.

Do not invent a second restore definition.

If the backend already restores all persisted session definitions at startup and `restoreLastSession()` means re-activating only the last active session, label the action clearly.

## 10.4 Tests

Duplicate:

```text
create session
configure devices/source/policy/volume
duplicate
verify new ID
verify copied config
verify original unchanged
delete copy
```

Restore:

```text
persist session
restart/reinitialize test manager
restore through same API as GUI
verify intended state
```

---

# 11. KNOWN DEFECT GROUP F — DIAGNOSTICS IS NOT YET DEEP ENOUGH

## 11.1 Required diagnostic coverage

Diagnostics should expose, based on existing backend capability:

### Bluetooth
- adapter state;
- powered/discovering;
- BlueZ service/connectivity state;
- current known devices;
- connected devices;
- per-device state;
- latest error.

### PipeWire
- manager readiness;
- endpoint count;
- active nodes if represented;
- active links/routes if represented;
- source/output relationship.

### Sessions
- active/current session;
- all session states if useful;
- members;
- degraded state;
- recovery/reconnect information exposed by Phase 6;
- route IDs/ownership.

### Routing
- route IDs;
- source;
- destinations;
- state;
- ownership;
- error.

### Application log
- timestamp;
- severity;
- subsystem;
- message;
- IDs/context where available;
- bounded retention;
- filters where already intended.

## 11.2 Do not shell out

Never populate Diagnostics by running:

```text
bluetoothctl
wpctl
pactl
```

Use the native backend state.

Manual shell commands are allowed only in the external validation process.

## 11.3 Copy Visible placeholder

The current UI contains or previously contained a `Copy visible` action that reports copying is not wired.

A user-visible placeholder action is not acceptable.

Choose one:

### Implement clipboard support

Using the appropriate Qt clipboard API.

Copy exactly the currently visible/filtered log rows.

### Or remove the action

If clipboard support is out of scope, remove the button.

Do not leave a fake button.

## 11.4 Recovery/reconnection diagnostics

If SessionManager exposes retry/recovery state, surface it.

If it does not expose attempt counters, do not invent fake numbers.

At minimum expose:

```text
Recovering
Device reconnect pending
Endpoint missing
Route restoration failed
```

using actual available state.

---

# 12. KNOWN DEFECT GROUP G — DARK THEME CONTRAST REGRESSIONS

## 12.1 Problem statement

The project now has a dark Theme, but several older QML components still use hard-coded light-theme colors or Qt Quick `Text` defaults.

Known examples include files such as:

```text
EndpointRow.qml
RoutePanel.qml
DeviceRow.qml
SourceRow.qml
```

with colors similar to:

```text
#444444
#666666
#777777
```

or no explicit text color.

On a dark surface such as:

```text
#0f1419
#1a222c
```

this can be unreadable.

## 12.2 Required correction

Audit every QML text/icon/control color.

Search:

```bash
rg -n \
  'color:\s*"#[0-9A-Fa-f]{6}"|Text\s*\{' \
  ui apps
```

Replace arbitrary legacy colors with semantic theme tokens where appropriate:

```text
Theme.text
Theme.textMuted
Theme.surface
Theme.surfaceElevated
Theme.border
Theme.success
Theme.warning
Theme.error
Theme.accent
```

Do not mechanically replace colors that represent deliberate semantic states without review.

## 12.3 Component audit

At minimum inspect:

```text
Dashboard
Devices
Sessions
Audio Routing
Diagnostics
Settings
dialogs
EndpointRow
RoutePanel
DeviceRow
SourceRow
SessionRow
StatusBadge
VolumeControl
EmptyState
ErrorBanner
Toast
```

## 12.4 Acceptance

No primary text should render black/dark-gray against dark application surfaces.

Disabled text must remain readable.

Selected/focused state must remain distinguishable.

Color must not be the sole status indicator.

---

# 13. KNOWN DEFECT GROUP H — SETTINGS FILE LOGGING RUNTIME SEMANTICS

## 13.1 Problem statement

The current Settings screen persists file-logging configuration but may not apply enable/disable changes to the running Logger.

A known implementation path writes through:

```text
ConfigurationManager::setFileLoggingEnabled()
```

while runtime Logger controls exist separately:

```text
Logger::enableFileLogging()
Logger::disableFileLogging()
```

The UI warns only that the log path requires restart.

This can mislead users.

## 13.2 Required correction

Inspect actual Logger and ConfigurationManager lifecycle.

Choose one correct behavior.

### Preferred if safe

Apply:

```text
Enable file logging -> runtime Logger enabled + setting persisted
Disable file logging -> runtime Logger disabled + setting persisted
```

If log-path change cannot safely apply live, mark only the path as restart-required.

### If runtime toggle is intentionally startup-only

Clearly label:

```text
File logging changes take effect after restart
```

and ensure all related controls reflect this.

Do not present immediate toggles that silently do nothing until restart.

## 13.3 Tests

Add tests for:

```text
settings model updates configuration
logger state changes if live behavior is supported
restart-required flag/text behavior if not
configuration failure surfaces to UI
```

---

# 14. DASHBOARD REVALIDATION

After fixing SessionManager notifications and selected-session logic, re-audit Dashboard.

Verify:

```text
active session name
active session state
active source
group volume
mute
member device summary
degraded state
recovering state
Bluetooth readiness
PipeWire readiness
```

All must update reactively.

Specific test:

```text
Activate Session A
Dashboard shows A

Change A source
Dashboard updates immediately

Mute A
Dashboard updates immediately

Disconnect member
Dashboard shows degraded/recovering as backend dictates

Reconnect member
Dashboard returns to active state
```

No page reload/navigation should be required.

---

# 15. DEVICES PAGE REVALIDATION

After details correction, verify:

```text
Start scan
Stop scan
per-device pending states
Pair
Pairing confirmation/passkey
Trust/untrust
Connect
Disconnect
Reconnect
Forget
Details
Services
Audio endpoint mapping
RSSI
adapter unavailable
no results
filtered no-results
```

Controls must be enabled/disabled from authoritative backend state.

No optimistic state changes.

---

# 16. SESSIONS PAGE REVALIDATION

Test at least three simultaneous stored sessions:

```text
Session A active
Session B inactive
Session C degraded/restored fixture where practical
```

Switch selection repeatedly.

Verify every editor control belongs to the selected session.

Test:

```text
rename
duplicate
delete
add member
remove member
select source
policy
volume
mute
activate
deactivate
restore
```

Verify destructive action confirmation.

Verify active session cannot be accidentally edited through a different selected session's displayed state.

---

# 17. AUDIO ROUTING PAGE REVALIDATION

After ownership correction:

Show all relevant routes with:

```text
route ID
source
destination(s)
state
owner
editable/read-only
```

Standalone manual control must target an explicit manual route.

Test with:

```text
0 sessions active
1 session active
multiple sessions stored
multiple routes
route insertion/removal/reordering
endpoint disappearance
source disappearance
```

Never rely on row 0/front() as identity.

---

# 18. DIAGNOSTICS REVALIDATION

Test live changes:

```text
start Bluetooth scan
device appears
connect
endpoint appears
session activates
route appears
member disconnects
session degrades
recovery begins
session restores
```

Diagnostics should update reactively.

No terminal refresh should be required.

Ensure log memory remains bounded.

---

# 19. SETTINGS REVALIDATION

Verify:

```text
settings load correctly
changes persist
invalid values handled
restart-required behavior is truthful
file logging behavior is truthful
reset behavior safe
```

Close and relaunch the app where necessary to confirm persistence.

---

# 20. ADD BEHAVIORAL GUI TESTS — CURRENT COVERAGE IS NOT ENOUGH

The existing QML component test coverage is insufficient if it only loads a simple component such as StatusBadge.

Add behavioral tests for the defects corrected in this pass.

At minimum implement automated coverage for:

## Session selection isolation

```text
two sessions with different volume/source/policy/mute
select each
verify editor representation
modify selected
verify only selected changed
```

## Q_PROPERTY notifications

```text
source change
mute change
active session change
```

## Route ownership

```text
session-owned route exists
manual route operation
session route unchanged
```

## Device details

```text
UUID/services displayed
friendly name/address fallback
device removal
```

## Duplicate

```text
new ID + copied configuration
```

## Restore

```text
persist + restore contract
```

## Dashboard reactivity

```text
active source/mute/degraded changes
```

## Diagnostics clipboard

If implemented:

```text
filtered log rows
copy visible
clipboard contains expected text
```

## Theme

Where feasible, test semantic properties rather than pixels.

At minimum instantiate every major QML page/component under the configured dark Theme and assert no fatal QML warnings.

---

# 21. QML RUNTIME WARNING GATE

Launch the application and scrutinize stderr/log output.

Treat these as failures if caused by Phase 7 code:

```text
QQmlApplicationEngine failed to load component
module not installed
type unavailable
ReferenceError
TypeError
binding loop
Cannot assign to non-existent property
Cannot read property of null/undefined
Unable to assign [undefined]
Connections target is not valid
```

Resolve warnings rather than suppressing them.

---

# 22. CLEAN ARCHITECTURE RULES FOR CORRECTIONS

Use:

```text
stable IDs
reactive Qt properties
QAbstractItemModel where appropriate
thin presentation adapters
signals
queued cross-thread delivery
```

Avoid:

```text
polling from QML
Timer-based state repair
global mutable JavaScript maps
row-index identity
front()/first() identity assumptions
duplicated configuration
duplicated session cache
duplicated routing state
```

---

# 23. THREAD-SAFETY AUDIT

Because Auralis integrates D-Bus and PipeWire, inspect all new and existing UI-facing callbacks.

For presentation models:

- QObject/model mutations must occur on their owning Qt thread;
- PipeWire callbacks must marshal safely;
- backend callbacks must not directly mutate QML models from worker threads;
- diagnostics/log sink must be thread-safe;
- application shutdown must not leave callbacks targeting destroyed UI models.

Add focused tests where feasible.

---

# 24. MODEL IDENTITY AUDIT

For:

```text
devices
sessions
sources
endpoints
routes
logs
```

ensure:

- row index is not treated as stable identity;
- updates preserve IDs;
- removal clears selected IDs;
- sorting/filtering does not retarget operations;
- route selection does not become another route when model order changes.

This is especially important for the Audio Routing fix.

---

# 25. DESTRUCTIVE ACTION AUDIT

Confirm dialogs for:

```text
Forget device
Delete session
Reset settings
```

and any other destructive operation.

Ensure the dialog action captures stable identity.

Do not rely on a delegate's stale `index`.

---

# 26. CLEAN BUILD AND FULL TEST GATE

After implementation:

```bash
rm -rf build

cmake -S . -B build -G Ninja

cmake --build build

ctest --test-dir build --output-on-failure
```

Run:

```bash
ctest --test-dir build -N
```

Record exact test count.

If Qt's QML lint tools are available/configured, run appropriate project-compatible checks such as:

```bash
qmllint <new/modified qml files>
```

Do not invent unsupported commands.

---

# 27. MANUAL GUI ACCEPTANCE MATRIX

Perform on the actual Linux development machine where services/hardware are available.

Use exactly:

```text
PASS
FAIL
NOT RUN
```

Never mark unexecuted hardware tests PASS.

## Shell

- [ ] application launches
- [ ] six pages load
- [ ] no fatal QML warnings
- [ ] navigation works
- [ ] resize works
- [ ] keyboard focus usable

## Devices

- [ ] adapter status
- [ ] start scan
- [ ] stop scan
- [ ] discovered device appears
- [ ] details show friendly/technical data
- [ ] services/UUIDs visible
- [ ] pair
- [ ] trust/untrust
- [ ] connect
- [ ] audio endpoint appears
- [ ] disconnect
- [ ] reconnect
- [ ] forget with confirmation

## Sessions

- [ ] create A
- [ ] create B
- [ ] configure A differently from B
- [ ] select A -> correct values
- [ ] select B -> correct values
- [ ] changing B does not change A
- [ ] rename
- [ ] duplicate
- [ ] delete copy
- [ ] add/remove device
- [ ] select source
- [ ] policy
- [ ] volume
- [ ] mute
- [ ] activate
- [ ] deactivate
- [ ] restore

## Dashboard

- [ ] active session correct
- [ ] source reacts immediately
- [ ] volume reacts
- [ ] mute reacts
- [ ] degraded state reacts
- [ ] recovery reacts

## Routing

- [ ] manual route created/selected explicitly
- [ ] manual route source changes
- [ ] manual route destinations change
- [ ] manual route volume/mute works
- [ ] session route remains unchanged
- [ ] session route labeled managed/read-only if intended
- [ ] route model reorder does not retarget editor

## Diagnostics

- [ ] Bluetooth state live
- [ ] device state live
- [ ] PipeWire state live
- [ ] endpoint/node state useful
- [ ] route ownership/state visible
- [ ] session/degraded/recovery visible
- [ ] logs live
- [ ] copy action genuinely works or is absent

## Settings

- [ ] settings load
- [ ] persist
- [ ] file logging behavior truthful
- [ ] restart requirement truthful
- [ ] reset safe

---

# 28. HARDWARE-INTEGRATION TESTING

Use existing Phase 2–6 test environment contracts.

If the repository has opt-in variables such as:

```text
AURALIS_RUN_BLUETOOTH_INTEGRATION
AURALIS_EXPECT_DEVICE_ADDRESS
```

use their documented semantics exactly.

Do not type shell placeholders such as:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

because shell interprets `<...>` as redirection.

Use a real quoted Bluetooth address, e.g.:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

only when a real test device is intentionally used.

Do not invent a device address in the audit.

---

# 29. UPDATE THE PHASE 7 AUDIT ONLY AFTER TESTING

Update/create:

```text
docs/PHASE_7_IMPLEMENTATION_AUDIT.md
```

It must contain:

## Executive summary

State whether the correction pass is complete.

## Original audit findings

List each previously known defect:

```text
selected-session binding
Q_PROPERTY notifications
route ownership
device details/services
session duplicate
session restore
diagnostics completeness
copy placeholder
theme contrast
file logging semantics
GUI test coverage
```

For each:

```text
RESOLVED
PARTIALLY RESOLVED
NOT RESOLVED
```

with evidence.

## Files changed

Exact list.

## Tests added

Exact test names.

## Clean build results

Exact commands and output summary.

## Manual results

Exact PASS/FAIL/NOT RUN table.

## Hardware results

Separate from unit/GUI tests.

## Known limitations

Only genuine remaining limitations.

## Exit gate

Exactly one:

```text
PHASE 7 EXIT GATE: PASSED
```

or

```text
PHASE 7 EXIT GATE: FAILED
```

Do not write PASSED while any known blocker remains.

---

# 30. REQUIRED EXIT CONDITIONS

Phase 7 may be declared complete only if:

- [ ] selected-session UI cannot display one session while editing another;
- [ ] Q_PROPERTY notify semantics are correct;
- [ ] Dashboard source/mute state updates reactively;
- [ ] manual routing cannot mutate session-owned routes unintentionally;
- [ ] route identity is explicit and stable;
- [ ] device details show supported backend data;
- [ ] services/UUIDs are actually visible;
- [ ] Duplicate is functional;
- [ ] Restore semantics are functional/user-accessible per backend contract;
- [ ] Diagnostics is meaningfully complete;
- [ ] no fake Copy-visible button remains;
- [ ] dark-theme text is readable across all six pages;
- [ ] logging setting behavior is truthful and tested;
- [ ] regression tests exist for the corrected bugs;
- [ ] clean build succeeds;
- [ ] full CTest suite passes;
- [ ] QML loads without fatal warnings;
- [ ] manual workflow passes to practical hardware extent;
- [ ] Phase 0–6 regressions remain green.

---

# 31. IMPLEMENTATION ORDER

Use this order:

## Step 1
Baseline repository and tests.

## Step 2
Fix SessionManager property notification contracts.

## Step 3
Fix selected-session view/state architecture.

## Step 4
Add automated session-selection and notification tests.

## Step 5
Fix route identity/ownership architecture.

## Step 6
Add route ownership tests.

## Step 7
Complete device details and services.

## Step 8
Implement Duplicate and Restore workflow.

## Step 9
Expand Diagnostics and remove/implement placeholders.

## Step 10
Fix theme/contrast across all pages/components.

## Step 11
Correct logging-setting semantics.

## Step 12
Expand QML/GUI behavioral tests.

## Step 13
Run clean build/full suite.

## Step 14
Run manual Linux GUI/hardware matrix.

## Step 15
Update Phase 7 audit from evidence.

Do not postpone testing until the end.

---

# 32. FINAL RESPONSE REQUIRED FROM CURSOR

When all work is done, respond with:

```text
AURALIS PHASE 7 CORRECTION PASS

Status: PASSED / FAILED

Resolved:
- selected-session state isolation
- property notification semantics
- route ownership
- device details/services
- duplicate/restore
- diagnostics
- theme contrast
- file logging semantics
- GUI behavioral coverage

Clean verification:
- Configure: PASS/FAIL
- Build: PASS/FAIL
- CTest: N/N PASS
- QML runtime warning gate: PASS/FAIL

Hardware/manual:
- summary

Audit:
- docs/PHASE_7_IMPLEMENTATION_AUDIT.md updated

Remaining blockers:
- none
```

If any required item is not resolved, use:

```text
Status: FAILED
```

and keep working on code-correctness issues.

---

# 33. MASTER DIRECTIVE

Correct the current Phase 7 implementation **without destroying the strong work already present**.

The target is not cosmetic perfection.

The target is trustworthy desktop behavior:

> **Every action targets the object the user selected, every displayed state belongs to the object being shown, all reactive QML properties actually notify, manual routes cannot corrupt session-managed routes, technical device/session/routing state is genuinely inspectable, no visible action is a placeholder, and automated tests prove the corrected failure modes stay fixed.**
