# AURALIS — PHASE 7 COMPLETE GUI IMPLEMENTATION MASTER PROMPT

## Purpose

This document is a **master implementation prompt for Cursor AI IDE** to execute **Phase 7 — Complete Auralis GUI** for the Auralis Linux desktop application.

Phase 0, Phase 1, Phase 2, Phase 3, Phase 4, Phase 5, and Phase 6 are considered **COMPLETED BASELINE WORK**.

Phase 7 must therefore **not re-implement, replace, bypass, or destabilize** the Bluetooth, PipeWire, audio-routing, device-management, or multi-device session engines that already exist. The job of this phase is to build the complete production-oriented desktop interaction layer over the existing backend.

The authoritative Phase 7 goal is:

> Build the full Auralis desktop control surface using Qt Quick + QML, with the core application remaining primarily C++, so that the complete primary Auralis workflow can be performed without requiring a terminal.

The required primary areas are:

1. Dashboard
2. Devices
3. Sessions
4. Audio Routing
5. Diagnostics
6. Settings

This prompt intentionally expands that roadmap item into a rigorous implementation specification.

---

# 1. YOUR ROLE

You are acting as the senior software engineer responsible for **implementing Phase 7 of Auralis to production-quality completion**.

You are working directly inside the existing Auralis repository.

Your responsibilities are to:

- inspect and understand the current Phase 0–6 code before editing;
- preserve working architecture and public contracts wherever possible;
- implement a coherent Qt Quick/QML presentation architecture;
- expose existing C++ backend state safely and reactively to QML;
- build all primary screens and workflows;
- ensure user actions are asynchronous and state-driven rather than optimistic;
- add appropriate GUI-facing models, view-models/facades, adapters, and command objects where needed;
- avoid duplicating backend/domain logic inside QML;
- add GUI-oriented unit/integration/component tests;
- maintain all prior tests;
- ensure the application builds from a clean tree;
- launch and exercise the application;
- resolve Phase 7 defects rather than merely documenting them;
- leave Phase 8 production hardening work out of scope except where minimum defensive behavior is required for a correct Phase 7 GUI;
- produce a final implementation/audit report when complete.

You must treat this as a real engineering phase, **not a visual mock-up exercise**.

A screen that renders static cards but does not control real backend behavior is **NOT** a completed Phase 7 screen.

---

# 2. NON-NEGOTIABLE PHASE BOUNDARY

## 2.1 Completed baseline

Assume the following capabilities already exist from prior phases and should be reused:

### Phase 1 — Project Foundation
- CMake/Ninja build structure
- desktop application target
- Qt/QML shell
- logging
- configuration
- testing infrastructure
- core service ownership

### Phase 2 — Bluetooth Device Discovery
- BlueZ D-Bus integration
- adapter/discovery state
- Bluetooth device registry
- reactive device updates

### Phase 3 — Bluetooth Device Management
- pairing
- trust/untrust where supported
- connect
- disconnect
- forget/remove
- reconnect
- state tracking
- BlueZ pairing agent behavior

### Phase 4 — PipeWire Audio Integration
- PipeWire registry/state monitoring
- audio endpoint discovery
- endpoint model
- Bluetooth-device-to-audio-endpoint mapping

### Phase 5 — Audio Routing Engine
- source selection
- endpoint selection
- route planning
- route activation/deactivation
- volume/mute controls as implemented
- route status

### Phase 6 — Multi-Device Session Engine
- session creation
- session device membership
- source assignment
- activation/deactivation
- group control
- persistence/restoration
- degraded/recovery behavior as implemented

Phase 7 is a **presentation and interaction phase** over these capabilities.

## 2.2 Do not perform a destructive rewrite

DO NOT:

- replace working BlueZ D-Bus code with shell commands;
- replace native PipeWire integration with `wpctl`, `pactl`, or shell parsing;
- use `bluetoothctl` as application logic;
- create a second competing DeviceRegistry;
- create a second competing SessionManager;
- create a separate fake routing engine for the UI;
- hide backend failures by pretending UI actions succeeded;
- hard-code fake devices, fake sessions, fake endpoints, or fake routes in production code;
- move core domain rules into QML JavaScript;
- redesign the entire repository merely for aesthetic preference;
- begin Phase 8 packaging/recovery architecture unless required to keep Phase 7 behavior correct.

Debug tooling may invoke shell utilities manually during verification, but the production application must not depend on them.

---

# 3. FIRST ACTION: REPOSITORY RECONNAISSANCE

Before writing implementation code, perform a disciplined audit of the current repository.

Do not assume filenames from this prompt exist.

## 3.1 Inspect the repository

At minimum inspect:

```bash
pwd
git status --short
git branch --show-current
git log --oneline --decorate -n 30

find . -maxdepth 3 -type f \
  | sort \
  | sed -n '1,260p'
```

Inspect:

- root `CMakeLists.txt`
- `apps/desktop`
- current `main.cpp`
- `ui/`
- QML module declarations
- resource/QML packaging
- `src/core`
- `src/bluetooth`
- `src/devices`
- `src/audio`
- `src/session`
- public headers
- existing QObject/QAbstractItemModel classes
- service construction/lifetime
- `ApplicationCore`
- current QML context/property/singleton registration
- tests and test registration
- any existing facade/controller/view-model classes

Search broadly:

```bash
rg -n \
  "QObject|QAbstractListModel|QAbstractItemModel|Q_PROPERTY|Q_INVOKABLE|qmlRegister|QQml|setContextProperty|BluetoothManager|DeviceRegistry|PipeWireManager|AudioRouter|SessionManager|Recovery|Persistence|Configuration|Logger" \
  apps src include ui tests CMakeLists.txt
```

Also inspect all current test names:

```bash
ctest --test-dir build -N || true
```

## 3.2 Build baseline before editing

If a build directory exists, do not trust it as proof.

Perform or preserve a baseline test result before Phase 7 changes.

Preferred clean verification:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If the repository requires documented CMake options, use the repository’s actual documented configuration rather than inventing new ones.

## 3.3 Establish the actual Phase 0–6 public surface

Before coding, explicitly determine:

- how devices are represented;
- how discovered/known/connected devices are exposed;
- how commands are invoked;
- how asynchronous operation state is reported;
- how PipeWire endpoints are represented;
- how sources are represented;
- how routes are represented;
- how sessions are represented;
- how session states are encoded;
- how group volume is changed;
- how persistence is performed;
- how errors are represented;
- which signals are emitted;
- which code already has Qt-facing properties/models.

Write a short internal mapping such as:

```text
UI concern                  Existing backend owner
--------------------------------------------------------------
Bluetooth adapter state     <actual class>
Device discovery            <actual class>
Device list                 <actual class/model>
Pair/connect/etc.           <actual class>
Audio endpoints             <actual class/model>
Audio sources               <actual class/model>
Route activation            <actual class>
Route list/state            <actual class/model>
Session CRUD                <actual class>
Session activation          <actual class>
Group volume                <actual class>
Persistence                 <actual class>
Logs/diagnostics            <actual class>
```

Do not proceed with large-scale QML construction until this mapping is understood.

---

# 4. PHASE 7 SUCCESS DEFINITION

Phase 7 is complete only when a normal user can launch `auralis-desktop` and, through the GUI alone:

1. see overall Auralis system state;
2. see Bluetooth adapter/discovery state;
3. start and stop device discovery;
4. inspect discovered and known devices;
5. pair a supported device;
6. trust/untrust where exposed by backend policy;
7. connect a device;
8. disconnect a device;
9. forget/remove a device;
10. see the associated audio endpoint state;
11. inspect available audio sources/endpoints;
12. create a session;
13. rename a session;
14. add/remove multiple devices;
15. select a source;
16. configure the route/session selection exposed by Phase 5/6;
17. activate a session;
18. observe ACTIVE/DEGRADED/RECOVERING/etc. state;
19. adjust group volume;
20. adjust per-device volume if supported by the existing backend;
21. mute/unmute where supported;
22. deactivate a session;
23. save/restore/reopen session state through the existing persistence model;
24. inspect live routing state;
25. inspect diagnostics and logs;
26. change supported application settings;
27. understand actionable failures without opening a terminal.

The GUI must reflect **authoritative backend state**, not merely button-click intent.

---

# 5. ARCHITECTURAL TARGET

Use Qt Quick + QML for the presentation layer while keeping core application logic in C++.

Recommended logical structure:

```text
+----------------------------------------------------------+
|                      QML PRESENTATION                    |
|                                                          |
|  Screens | Components | Dialogs | Navigation | Theme     |
+------------------------------+---------------------------+
                               |
                               v
+----------------------------------------------------------+
|            GUI / PRESENTATION ADAPTER LAYER              |
|                                                          |
| AppFacade / ViewModels / Qt Models / Action Controllers  |
+------------------------------+---------------------------+
                               |
                               v
+----------------------------------------------------------+
|                  EXISTING APP SERVICES                   |
|                                                          |
| Bluetooth | Devices | PipeWire | Router | Sessions       |
+------------------------------+---------------------------+
                               |
                               v
+----------------------------------------------------------+
|                    EXISTING PLATFORM                     |
|              BlueZ / D-Bus / PipeWire / Linux            |
+----------------------------------------------------------+
```

## 5.1 Presentation adapter layer

If existing services are already safe and clean to expose directly to QML, reuse them.

If not, introduce a **thin presentation adapter layer**.

Possible names, adapted to repository style:

```text
AppFacade
UiController
ApplicationViewModel
DashboardViewModel
DeviceListModel
SessionListModel
AudioSourceListModel
EndpointListModel
RouteListModel
DiagnosticsModel
SettingsViewModel
```

Do not create unnecessary wrappers just to match these names.

A wrapper is justified when it:

- flattens multiple backend services into UI-ready state;
- provides QML-friendly role names;
- converts enums to display metadata;
- owns transient UI command state;
- coalesces signals;
- prevents QML from reaching deep into service internals;
- exposes safe asynchronous actions;
- improves testability.

## 5.2 C++ owns domain behavior

C++ should remain responsible for:

- Bluetooth/device lifecycle;
- session rules;
- routing rules;
- persistence;
- source/endpoint identity;
- backend validation;
- operation results;
- domain state transitions.

QML should own:

- visual structure;
- navigation;
- selection/highlighting;
- dialogs;
- visual feedback;
- presentation formatting;
- local ephemeral interaction state.

## 5.3 No business logic in JavaScript

Small QML expressions are acceptable.

Avoid large JavaScript functions implementing:

- device state machines;
- session state machines;
- route planning;
- retry logic;
- persistence logic;
- backend polling;
- identifier matching;
- model deduplication.

---

# 6. GUI INFORMATION ARCHITECTURE

Implement these primary destinations:

```text
Auralis
|
+-- Dashboard
+-- Devices
+-- Sessions
+-- Audio Routing
+-- Diagnostics
+-- Settings
```

Use a persistent desktop navigation pattern suitable for a Linux desktop application.

A recommended structure is:

```text
+------------------+---------------------------------------+
| AURALIS          | Top bar / current page / status       |
|                  +---------------------------------------+
| Dashboard        |                                       |
| Devices          |                                       |
| Sessions         |          PAGE CONTENT                 |
| Audio Routing    |                                       |
| Diagnostics      |                                       |
| Settings         |                                       |
|                  |                                       |
| System health    |                                       |
+------------------+---------------------------------------+
```

On narrower window sizes, navigation may collapse to icons/drawer behavior if implemented cleanly.

Do not optimize only for a single hard-coded resolution.

---

# 7. APPLICATION SHELL REQUIREMENTS

Implement or refine the root application shell.

## 7.1 Root window

The main window should have:

- application title;
- sensible default size;
- sensible minimum size;
- layout that remains usable when resized;
- central navigation;
- page title;
- global system-status indicator;
- global notifications/toasts;
- modal dialog host;
- busy/operation feedback;
- optional developer diagnostics shortcut only if appropriate.

Do not hard-code geometry assuming the development laptop’s exact screen.

## 7.2 Navigation behavior

Navigation must:

- clearly indicate current destination;
- preserve expected page-local state when switching pages;
- not duplicate screen instances uncontrollably;
- support keyboard activation;
- expose tooltips for icon-only navigation;
- avoid destructive side effects on page changes.

## 7.3 Global status

At minimum provide compact top-level visibility into:

- Bluetooth readiness;
- PipeWire/audio readiness;
- current/active session;
- active warning/error state.

This should be based on current backend capability.

## 7.4 Global notifications

Implement a reusable, consistent notification system for:

- action success when useful;
- action failure;
- warning;
- informational state.

Do not show a success toast for every trivial slider update.

Error notifications must not silently disappear before the user can understand them.

---

# 8. VISUAL/DESIGN SYSTEM

The objective is a professional, coherent desktop UI—not a collection of unrelated QML Controls.

Create a lightweight design system using reusable tokens/components.

## 8.1 Design tokens

Centralize:

- spacing scale;
- corner radii;
- typography roles;
- icon sizes;
- control heights;
- semantic colors;
- content/background/surface colors;
- focus ring;
- separators;
- status colors;
- disabled opacity;
- animation durations if used.

Do not duplicate magic numbers across dozens of QML files.

Possible files:

```text
ui/qml/theme/Theme.qml
ui/qml/theme/Metrics.qml
ui/qml/theme/Typography.qml
```

Adapt to the existing QML module structure.

## 8.2 Semantic status styles

Define consistent visual treatment for at least:

```text
Ready / Connected / Active
Neutral / Idle
Scanning / Connecting / Starting / Recovering
Warning / Degraded
Error / Failed / Unavailable
Disabled
```

Never rely on color alone.

Pair color with:

- label;
- icon;
- shape;
- text.

## 8.3 Reusable components

Create only components that actually reduce duplication.

Likely useful components include:

```text
PageHeader
StatusBadge
SectionCard
EmptyState
ErrorBanner
LoadingState
DeviceCard / DeviceRow
SessionCard / SessionRow
SourceSelector
EndpointRow
VolumeControl
ConfirmDialog
FormDialog
ToastHost
KeyValueRow
DiagnosticSection
SearchField
```

Do not over-engineer a generic component framework.

## 8.4 Icons

Use a consistent icon source available to the project.

If adding assets:

- keep them in the repository;
- respect licensing;
- package them correctly;
- ensure resources work from the installed/built application rather than local absolute paths.

Never depend on developer-machine absolute asset paths.

---

# 9. DASHBOARD — COMPLETE SPECIFICATION

The Dashboard is the operational overview.

It should answer:

> Is Auralis ready, what is active, which devices are participating, and is anything wrong?

## 9.1 Required sections

### A. System readiness

Show:

- Bluetooth adapter state;
- discovery state if relevant;
- PipeWire/audio state;
- number of connected devices;
- warnings/errors.

### B. Active/current session

If a session is active, show:

- session name;
- session state;
- selected source;
- member device count;
- connected vs expected device count;
- route/routing state if exposed;
- group volume;
- mute state if supported.

If no session is active, show a purposeful empty state and quick action to open/create a session.

### C. Session device summary

For each active-session device show useful state such as:

- user-visible name;
- connected/disconnected/reconnecting;
- audio endpoint available/unavailable;
- per-device volume if exposed;
- warning if expected but missing.

### D. Quick actions

Provide only meaningful actions, e.g.:

- open Devices;
- scan for devices;
- create session;
- activate/deactivate current session;
- open Diagnostics when a system problem exists.

### E. Warnings

Examples:

```text
Bluetooth adapter unavailable
PipeWire unavailable
Session degraded: 1 of 2 devices connected
Selected source unavailable
Route inactive
```

Use actual available backend states.

## 9.2 Dashboard correctness

Do not aggregate state by guessing.

If the backend exposes explicit session state, show it.

Do not display “Active” merely because an Activate button was clicked.

---

# 10. DEVICES SCREEN — COMPLETE SPECIFICATION

This screen provides the complete Bluetooth device workflow.

## 10.1 Primary device categories

The UI should clearly differentiate, based on backend data:

- discovered/nearby devices;
- known/paired devices;
- connected devices;
- optionally all devices in a unified list with filters.

Choose the structure that best fits existing models.

Do not duplicate the same logical device into confusing independent cards.

## 10.2 Toolbar

Include relevant controls:

- Start Scan / Stop Scan;
- refresh/resync if the backend provides a meaningful operation;
- search/filter;
- device count;
- adapter status.

Button behavior must track real discovery state:

```text
Not discovering -> Scan
Discovering     -> Stop Scan / Scanning…
Unavailable     -> disabled + explanation
```

## 10.3 Device row/card fields

Use available fields, prioritizing:

- alias/name;
- address;
- address type if useful;
- RSSI/signal strength;
- paired;
- trusted;
- connected;
- services resolved;
- device type/icon;
- audio capability;
- audio endpoint availability;
- session membership where useful.

Do not expose raw internal IDs as the primary identifier to the user.

## 10.4 Device actions

Conditionally expose appropriate actions:

```text
Pair
Cancel pairing
Trust / Untrust
Connect
Disconnect
Reconnect
Forget / Remove
Details
```

Do not display contradictory actions simultaneously.

Examples:

- a connected device should not present “Connect” as the primary action;
- an unpaired device should not pretend Forget is meaningful unless backend semantics support it;
- pair/connect controls should disable during an in-flight operation for that device where appropriate.

## 10.5 Asynchronous operation states

The UI needs per-device action feedback.

Examples:

```text
Pairing…
Connecting…
Disconnecting…
Removing…
Reconnecting…
```

The state must be driven by backend state/operation state.

Prevent accidental repeated submissions while an operation is already in progress.

## 10.6 Device details

A device detail panel/dialog/page should display available technical details without overwhelming the main list.

Useful information:

```text
Name
Alias
Address
Address type
RSSI
Paired
Trusted
Connected
Services resolved
Class
Appearance
UUID/services
Mapped audio endpoint
Endpoint/profile
Last seen
Session membership
```

Only display values the backend actually exposes.

## 10.7 Pairing interaction

If the BlueZ agent requires user confirmation, PIN/passkey display, entry, or authorization, Phase 7 must surface it correctly.

Implement the UI-facing handling supported by the existing Phase 3 pairing agent.

Potential cases:

- display PIN;
- enter PIN/passkey;
- confirm passkey;
- authorize service;
- pairing cancellation;
- timeout;
- failure.

Never auto-accept security-sensitive confirmation if Phase 3 intentionally requires user confirmation.

## 10.8 Forget/remove confirmation

For destructive actions, use confirmation.

The confirmation should clearly identify the device.

After confirmed removal, the UI must reflect authoritative registry/BlueZ state.

## 10.9 Empty states

Handle:

- Bluetooth adapter unavailable;
- scan not started;
- scanning with no results yet;
- no devices found;
- no known devices;
- filter matches none.

These are different states and should not all display the same generic message.

---

# 11. SESSIONS SCREEN — COMPLETE SPECIFICATION

This is a central Auralis workflow.

## 11.1 Session list

Display existing sessions with:

- name;
- state;
- source;
- member count;
- connected/available member count;
- last-used information if exposed;
- active marker;
- degraded/warning state.

## 11.2 Session operations

Support all Phase 6 operations available through the backend:

```text
Create
Rename
Duplicate
Delete
Add devices
Remove devices
Select source
Choose route policy if implemented
Activate
Deactivate
Save/update
Restore/open
```

If a roadmap operation is not present in the Phase 6 backend despite Phase 6 being marked complete, do not silently fake it in QML. Inspect whether an existing API is simply not exposed to QML. Add a proper thin backend adapter if necessary.

## 11.3 Create session flow

Provide a complete create flow.

At minimum:

1. choose/name the session;
2. choose devices;
3. choose source if required by existing backend semantics;
4. create/save;
5. land in the new session’s detail/editor view.

Validate:

- empty names;
- duplicate naming behavior according to backend policy;
- no eligible devices;
- unavailable source.

Do not introduce UI-only rules that conflict with backend validation.

## 11.4 Session editor/detail

A session detail/editor view should expose:

### Identity
- name;
- state;
- saved status if meaningful.

### Source
- selected source;
- availability;
- source selector.

### Devices
- member list;
- connection state;
- endpoint availability;
- add/remove;
- per-device state.

### Group controls
- activate/deactivate;
- group volume;
- mute if supported.

### Routing/session policy
- route/recovery policy fields only if the existing backend exposes user-configurable policy.

### Warnings
- missing device;
- disconnected member;
- endpoint unavailable;
- source unavailable;
- route problem.

## 11.5 Session states

Represent Phase 6 state explicitly.

Expected conceptual states include:

```text
IDLE
STARTING
ACTIVE
DEGRADED
RECOVERING
STOPPING
FAILED
```

Use the repository’s actual enum names.

Provide clear user-facing labels.

Examples:

```text
IDLE        -> Inactive
STARTING    -> Starting…
ACTIVE      -> Active
DEGRADED    -> Degraded
RECOVERING  -> Recovering…
STOPPING    -> Stopping…
FAILED      -> Failed
```

## 11.6 Degraded mode

This state must be visible and useful.

Example:

```text
Living Room
Degraded

2 devices expected
1 connected

Hearing Aid Left     Connected
Hearing Aid Right    Disconnected
```

Do not collapse a degraded session into a generic “error.”

If the backend is automatically recovering, show that.

## 11.7 Session activation

When Activate is requested:

- disable/guard duplicate activation commands as appropriate;
- show transient STARTING state if backend exposes it;
- update from actual SessionManager state;
- surface failures;
- do not blindly mark ACTIVE.

## 11.8 Session deletion

Deleting a session must require confirmation if destructive.

If deleting the active session is not permitted by backend semantics, reflect that clearly.

If the backend supports automatic deactivation-before-delete, do not reproduce that logic independently in QML.

---

# 12. AUDIO ROUTING SCREEN — COMPLETE SPECIFICATION

This screen should make Phase 5 behavior understandable.

The roadmap concept is:

```text
Sources
   |
   v
Routes
   |
   v
Endpoints
```

A full node graph is optional. Correctness and clarity are mandatory.

## 12.1 Source section

Show available sources using backend state.

For each source:

- name;
- type/media class if meaningful;
- availability;
- selected/current state.

## 12.2 Destination/endpoints section

Show available output endpoints:

- friendly name;
- physical/Bluetooth device association;
- transport/profile where useful;
- availability;
- selected state;
- active route state.

## 12.3 Route section

Show:

- route/source;
- destinations;
- active/inactive state;
- errors;
- session association if relevant;
- route identity only as secondary diagnostic detail.

## 12.4 User interaction

Depending on current backend architecture, support:

- select source;
- select one or multiple destinations;
- activate route;
- deactivate route;
- update route selection;
- navigate to affected device/session.

Do not create a GUI concept that is incompatible with the existing `AudioRouter`.

## 12.5 Route state feedback

The UI must handle:

```text
Inactive
Activating
Active
Partially active / degraded if represented
Stopping
Failed
Endpoint unavailable
Source unavailable
```

Use actual backend states.

## 12.6 Volume controls

If Phase 5/6 exposes:

- source volume;
- destination volume;
- group volume;
- per-device volume;
- mute;

connect the correct control to the correct backend owner.

Avoid fighting multiple volume owners.

For example, do not have both SessionManager and AudioRouter independently receive each slider movement if one is authoritative.

## 12.7 Slider behavior

Avoid flooding the backend with an unreasonable rate of commands.

Depending on API behavior:

- bind continuously if the backend is designed for it;
- throttle/coalesce;
- commit on release.

Use the approach that matches existing APIs and yields responsive behavior.

---

# 13. DIAGNOSTICS SCREEN — COMPLETE SPECIFICATION

Diagnostics is explicitly required and is important for Auralis development/support.

It must be useful without becoming a second implementation path.

## 13.1 System section

Show:

- application/core status;
- Bluetooth adapter state;
- BlueZ connection/service state if available;
- PipeWire state;
- active session state;
- counts for devices/endpoints/routes.

## 13.2 Bluetooth section

Show useful backend-observed information:

- adapter;
- powered;
- discovering;
- device count;
- connected device count;
- per-device summary;
- latest Bluetooth error if available.

## 13.3 PipeWire section

Show:

- manager/core state;
- endpoints;
- nodes if backend exposes them;
- links/routes if exposed;
- active source/output relationships.

Do not run `wpctl` from the application to populate this page.

## 13.4 Session/routing section

Show:

- current session;
- state;
- device membership;
- route IDs/statuses;
- recovery attempts if Phase 6 exposes them;
- failures.

## 13.5 Application log viewer

If the existing Logger exposes a safe way to consume recent log events, implement a bounded diagnostics log model.

Requirements:

- do not retain unbounded logs in memory;
- severity;
- timestamp;
- subsystem;
- event/message;
- relevant device/session ID when available;
- filter by severity/subsystem if practical;
- copy selected entry or all visible entries if easy to support.

If the Logger only writes to output/file and there is no reasonable observer interface, add a small, thread-safe bounded log sink/observer in C++ rather than tailing a log file from QML.

Avoid turning Phase 7 into a complete logging subsystem rewrite.

## 13.6 Refresh

Diagnostics should primarily be reactive.

Do not require repeated polling if the backend already emits signals.

A manual refresh/resync button may exist only where it invokes an existing meaningful backend refresh operation.

---

# 14. SETTINGS SCREEN — COMPLETE SPECIFICATION

Use the existing ConfigurationManager/persistence design.

Do not create a parallel JSON/INI settings store in QML if C++ already owns configuration.

## 14.1 Expose only meaningful settings

Examples, only when supported:

- startup behavior;
- auto-reconnect preference;
- last-session restoration preference;
- logging level;
- diagnostics verbosity;
- UI preference/theme if deliberately implemented;
- default recovery behavior;
- default routing preferences.

Do not invent dozens of speculative options.

## 14.2 Persistence behavior

Settings must:

- load current values;
- update through the backend/configuration owner;
- persist according to existing config rules;
- report write errors;
- not require restart unless actually necessary.

If a setting requires restart, label it.

## 14.3 Reset behavior

If adding “Reset to defaults,” require confirmation if it changes multiple values.

Use ConfigurationManager defaults, not duplicated QML literals.

---

# 15. UI-FACING MODEL REQUIREMENTS

Where list data is dynamic, prefer real Qt models over ad-hoc QVariant blobs.

## 15.1 `QAbstractListModel` / model roles

For dynamic collections, expose stable roles.

Examples:

### Device roles

```text
id
name
alias
address
addressType
rssi
paired
trusted
connected
servicesResolved
deviceType
audioCapable
endpointAvailable
operationState
errorText
```

### Session roles

```text
id
name
state
sourceName
deviceCount
connectedDeviceCount
active
degraded
groupVolume
lastUsed
```

### Endpoint roles

```text
id
name
description
direction
transport
profile
available
bluetoothDeviceId
selected
active
```

### Route roles

```text
id
sourceName
destinationCount
active
state
errorText
sessionId
```

Use actual backend fields and enum structures.

## 15.2 Stable identity

Model identity must not depend on row index.

When underlying registries reorder, UI actions must still target the correct domain object.

Use stable backend IDs.

## 15.3 Minimal resets

Avoid unnecessary `beginResetModel()` for every property change.

Use:

- insert/remove signals;
- `dataChanged`;
- row moves if required.

This preserves QML delegate state and improves performance.

## 15.4 Thread affinity

All QML-facing QObject/model updates must obey Qt thread rules.

If backend callbacks occur on worker/PipeWire threads, marshal presentation state to the appropriate Qt thread.

Do not mutate QML-facing models from arbitrary backend threads.

---

# 16. ASYNCHRONOUS ACTION DESIGN

Bluetooth, PipeWire, and session actions are asynchronous.

The UI must represent this.

## 16.1 Command states

Use a clear pattern, whether centralized or per-item:

```text
Idle
Pending
Succeeded
Failed
```

or backend-derived domain states such as CONNECTING/CONNECTED/FAILED.

Avoid inventing redundant command state if the backend state machine already gives enough information.

## 16.2 No optimistic lies

Forbidden pattern:

```qml
onClicked: {
    connected = true
    backend.connectDevice(id)
}
```

Correct pattern:

```text
User requests connect
        |
        v
Backend command invoked
        |
        v
UI shows connecting/pending
        |
        v
BlueZ/backend state changes
        |
        v
Model emits update
        |
        v
UI shows connected or failed
```

## 16.3 Error context

Errors should identify:

- what failed;
- which device/session/route;
- a useful human-readable message;
- retry/action if appropriate.

Do not dump raw D-Bus or PipeWire internals as the only user-facing error.

The raw detail can appear in Diagnostics.

---

# 17. DIALOGS AND CONFIRMATIONS

Implement reusable dialogs for workflows that need them.

Likely cases:

- create/rename session;
- choose/add devices;
- choose source;
- confirm forget device;
- confirm delete session;
- pairing confirmation/passkey if needed;
- destructive settings reset.

Requirements:

- keyboard accessible;
- Enter/Escape behavior appropriate;
- initial focus meaningful;
- validation shown inline;
- destructive actions styled consistently;
- no accidental dismissal during critical pairing confirmation if it causes unclear behavior.

---

# 18. EMPTY, LOADING, DEGRADED, ERROR STATES

Every major screen must deliberately handle state variations.

## 18.1 Loading/initializing

At application startup, do not flash misleading “No devices” content before models are initialized if an initializing state exists.

## 18.2 Empty

Examples:

- no sessions;
- no known devices;
- no nearby devices;
- no audio endpoints;
- no routes;
- no logs.

Each should offer the next useful action where possible.

## 18.3 Degraded

Use warnings when the application remains usable.

Examples:

- one session member disconnected;
- one endpoint unavailable;
- Bluetooth discovery unavailable while current connected devices remain usable.

## 18.4 Fatal/unavailable

If a subsystem required for a screen is unavailable, explain:

- what is unavailable;
- what features are affected;
- whether retry/restart is available through existing backend behavior;
- where to inspect diagnostics.

Do not block the entire application for a non-fatal page-level issue.

---

# 19. RESPONSIVENESS AND WINDOW RESIZING

A desktop GUI must handle realistic resizing.

## 19.1 Minimum target behavior

Verify at several window sizes such as:

```text
1280x800
1440x900
1920x1080
```

Also test near the declared minimum size.

The precise values can be adjusted to the existing UI.

## 19.2 Layout requirements

Avoid:

- clipped primary buttons;
- overlapping text;
- horizontal scrollbars for normal content;
- fixed pixel widths that break immediately;
- off-screen dialogs;
- cards requiring enormous width for simple information.

Use:

- anchors/layouts;
- `RowLayout`, `ColumnLayout`, `GridLayout`;
- sensible `Layout.fillWidth`;
- content wrapping/elision;
- ScrollView where appropriate.

## 19.3 Long text

Test:

- long device names;
- long session names;
- long error messages;
- unknown/missing names;
- IPv6-like? Not relevant; Bluetooth addresses should fit;
- technical UUID lists in detail panels.

Use elision and tooltips where needed.

---

# 20. ACCESSIBILITY AND KEYBOARD UX

Phase 7 should be usable without requiring a mouse for every operation.

## 20.1 Keyboard

Ensure:

- Tab/Shift+Tab traversal;
- Enter/Space activation;
- Escape closes dismissible dialogs;
- arrow-key behavior for lists where standard controls provide it;
- visible focus indicator.

## 20.2 Accessible naming

Controls with icons must have accessible labels/tooltips.

Provide meaningful names for:

- scan;
- connect;
- disconnect;
- activate session;
- volume;
- mute;
- delete/forget.

## 20.3 Contrast and semantics

Do not encode state by color alone.

Check that disabled text remains legible.

## 20.4 Motion

Keep animations restrained.

State changes should not depend on animation to be understood.

---

# 21. PERFORMANCE REQUIREMENTS

The GUI must remain responsive during active backend changes.

## 21.1 Do not block the GUI thread

Never perform:

- synchronous D-Bus waits on user action if avoidable;
- long PipeWire operations;
- file scanning;
- heavy diagnostics parsing;

on the GUI thread.

Reuse existing async backend architecture.

## 21.2 Signal storms

Bluetooth RSSI and PipeWire changes can update frequently.

Do not cause full-screen rebuilds or full model resets for every signal.

## 21.3 Logging

The diagnostics viewer must be bounded.

Example policy:

```text
retain last 1,000–5,000 in-memory events
```

Choose a reasonable limit based on existing logger design.

## 21.4 Volume changes

Avoid excessive command traffic as already described.

---

# 22. QML ENGINEERING STANDARDS

Follow repository conventions and modern Qt 6 QML practices.

## 22.1 QML modules

Package QML correctly using the project’s existing approach.

If using `qt_add_qml_module`, keep URIs/versions/import paths coherent.

Do not rely on execution from the repository root for resource lookup.

## 22.2 Required properties

Use `required property` for delegate dependencies where appropriate.

## 22.3 IDs and aliases

Avoid deeply coupling screens by reaching through arbitrary object IDs.

Prefer explicit properties/signals/navigation contracts.

## 22.4 Bindings

Avoid binding loops.

Do not break bindings by assigning imperatively to properties that should remain model-driven.

## 22.5 Singletons

Use QML singletons sparingly.

Theme/design tokens are reasonable.

Business services should follow the repository’s chosen dependency-exposure mechanism.

## 22.6 Dynamic object creation

Avoid unnecessary `Qt.createComponent()`/manual object lifetime if standard StackView/Loader/delegates can do the job safely.

---

# 23. C++ / QML BOUNDARY STANDARDS

## 23.1 Safe ownership

Every QObject exposed to QML must have clear lifetime ownership.

The application should not expose raw pointers that can disappear unexpectedly.

## 23.2 Enum exposure

Expose enums to QML in a strongly typed way where practical.

Do not compare magic integer state codes in QML.

## 23.3 Error types

Prefer structured error information in C++ and user-facing formatting in the presentation layer.

## 23.4 Q_INVOKABLE / slots

Expose only necessary actions.

Avoid turning every backend method into a QML-callable API.

A GUI facade should be intentionally narrow.

---

# 24. PROPOSED QML FILE ORGANIZATION

Adapt to current repository rather than forcing this exact tree.

A good target may resemble:

```text
ui/
└── qml/
    ├── Main.qml
    ├── Auralis/
    │   ├── AppShell.qml
    │   ├── navigation/
    │   │   ├── NavigationRail.qml
    │   │   └── TopBar.qml
    │   ├── theme/
    │   │   ├── Theme.qml
    │   │   └── Metrics.qml
    │   ├── components/
    │   │   ├── StatusBadge.qml
    │   │   ├── SectionCard.qml
    │   │   ├── EmptyState.qml
    │   │   ├── ErrorBanner.qml
    │   │   ├── VolumeControl.qml
    │   │   ├── DeviceRow.qml
    │   │   ├── SessionRow.qml
    │   │   └── ToastHost.qml
    │   ├── dialogs/
    │   │   ├── ConfirmDialog.qml
    │   │   ├── SessionEditorDialog.qml
    │   │   ├── DevicePickerDialog.qml
    │   │   └── PairingDialog.qml
    │   └── pages/
    │       ├── DashboardPage.qml
    │       ├── DevicesPage.qml
    │       ├── SessionsPage.qml
    │       ├── AudioRoutingPage.qml
    │       ├── DiagnosticsPage.qml
    │       └── SettingsPage.qml
```

This is illustrative.

If the current project already has a clean QML organization, extend it instead of moving everything for cosmetic reasons.

---

# 25. POSSIBLE C++ PRESENTATION LAYER ORGANIZATION

Only introduce what is needed.

Illustrative:

```text
include/auralis/ui/
├── AppFacade.hpp
├── DeviceListModel.hpp
├── SessionListModel.hpp
├── AudioSourceListModel.hpp
├── AudioEndpointListModel.hpp
├── RouteListModel.hpp
├── DiagnosticsModel.hpp
└── SettingsViewModel.hpp

src/ui/
├── AppFacade.cpp
├── DeviceListModel.cpp
├── SessionListModel.cpp
├── AudioSourceListModel.cpp
├── AudioEndpointListModel.cpp
├── RouteListModel.cpp
├── DiagnosticsModel.cpp
└── SettingsViewModel.cpp
```

Before adding a class, prove an existing model/facade does not already satisfy the role.

---

# 26. APPLICATION FACADE — RECOMMENDED RESPONSIBILITIES

If appropriate, expose a high-level `AppFacade`/equivalent to QML.

It may coordinate references to:

```text
BluetoothManager
DeviceRegistry
PipeWireManager
AudioEndpointRegistry
AudioRouter
SessionManager
ConfigurationManager
Logger
```

It should expose high-level readiness and navigation-relevant state, not duplicate the domain.

Possible properties:

```text
bluetoothAvailable
bluetoothPowered
discovering
pipeWireAvailable
audioReady
activeSessionId
activeSessionName
activeSessionState
warningCount
errorCount
```

Possible actions:

```text
startDiscovery()
stopDiscovery()
open/activate session actions through subordinate models/controllers
```

Do not create one giant God object containing every row-level operation if narrower models/controllers are cleaner.

---

# 27. TESTING STRATEGY

Phase 7 requires tests.

The UI cannot be accepted solely because it “looks good.”

Tests should be layered.

---

# 28. TEST LAYER A — C++ PRESENTATION MODEL UNIT TESTS

For each newly introduced GUI-facing C++ model/view-model, add unit tests.

Examples:

## 28.1 Device list model

Test:

- initial empty state;
- insertion;
- update;
- removal;
- stable ID;
- role values;
- paired change;
- connected change;
- RSSI change;
- endpoint availability change;
- operation/error state;
- no unnecessary full reset.

## 28.2 Session list model

Test:

- creation reflected;
- rename reflected;
- state transitions;
- active status;
- member counts;
- degraded state;
- deletion;
- persistence reload representation.

## 28.3 Audio endpoint/source models

Test:

- add/remove/update;
- availability;
- mapping;
- source selection;
- endpoint selection.

## 28.4 Diagnostics model

Test:

- bounded log retention;
- role values;
- filtering if implemented;
- thread-safe queued update behavior if relevant.

## 28.5 Settings view-model

Test:

- initial values from configuration;
- update;
- error handling;
- defaults/reset if implemented.

Use fake/mock dependencies where the repository already supports them.

Do not require physical Bluetooth hardware for pure presentation model unit tests.

---

# 29. TEST LAYER B — QML COMPONENT TESTS

If the repository supports Qt Quick Test or can reasonably add it, test reusable QML components/screens.

Useful tests:

- StatusBadge mapping;
- button visibility/enabled state by model status;
- empty state rendering;
- confirmation dialog behavior;
- session state label mapping;
- device operation state rendering;
- navigation selection;
- volume control basic behavior.

Do not create brittle pixel-perfect tests.

Test behavior and exposed properties.

---

# 30. TEST LAYER C — GUI INTEGRATION TESTS

Add integration tests around the presentation facade with fake/in-memory backend data when practical.

Scenarios:

### Scenario 1 — device discovery flow

```text
adapter ready
-> start scan
-> discovering true
-> device inserted
-> device visible
-> stop scan
-> discovering false
```

### Scenario 2 — connect flow

```text
device discovered
-> Connect command
-> CONNECTING
-> CONNECTED
-> UI/model reflects state
```

### Scenario 3 — failure flow

```text
device discovered
-> Connect command
-> operation fails
-> FAILED / disconnected
-> error exposed
```

### Scenario 4 — session active

```text
session exists
-> activate
-> STARTING
-> ACTIVE
-> dashboard reflects active session
```

### Scenario 5 — degraded session

```text
session ACTIVE
-> one device disconnects
-> session DEGRADED
-> dashboard + session page reflect missing device
```

### Scenario 6 — recovery

```text
session DEGRADED
-> device reconnects
-> session RECOVERING or ACTIVE according to backend
-> UI updates without page reload
```

### Scenario 7 — persistence

```text
saved sessions loaded
-> session list populated
-> current selection resolves safely
```

---

# 31. TEST LAYER D — HEADLESS/SMOKE STARTUP

Add or maintain an application startup smoke test where practical.

At minimum verify QML loads without fatal errors.

Possible approaches:

- Qt Quick Test;
- launch with an offscreen/minimal platform and terminate after readiness;
- repository’s existing smoke-test mechanism.

Do not make CI depend on a physical display when an offscreen test is sufficient.

Watch for:

```text
QQmlApplicationEngine failed to load component
module not installed
type unavailable
binding loop
ReferenceError
Cannot assign to non-existent property
qrc path failure
```

Treat QML runtime warnings seriously.

---

# 32. TEST LAYER E — EXISTING REGRESSION SUITE

All prior Phase 1–6 tests must still pass.

Required:

```bash
ctest --test-dir build --output-on-failure
```

If Phase 0–6 hardware/integration tests are opt-in through environment variables, preserve that design.

Do not make normal unit tests fail merely because hardware is absent.

---

# 33. TEST LAYER F — MANUAL GUI WORKFLOW

Perform a deliberate manual GUI workflow on the development machine when runtime services are available.

At minimum:

## Application shell
- launch application;
- switch through all six pages;
- resize window;
- verify no fatal QML warnings.

## Devices
- observe adapter;
- scan;
- stop scan;
- inspect device;
- perform available connection lifecycle operations using a real test device if safe/available;
- verify state is reactive.

## Sessions
- create test session;
- rename;
- add devices;
- choose source;
- activate;
- change group volume;
- deactivate;
- reload/restore;
- delete test session if appropriate.

## Routing
- inspect sources/endpoints;
- activate/deactivate route through supported workflow;
- verify displayed route state.

## Diagnostics
- observe changes while scanning/connecting;
- verify logs update;
- verify no application shell command dependency is introduced.

## Settings
- change a safe setting;
- verify persistence;
- restore original value.

Do not fabricate PASS for steps that cannot be run.

Mark hardware-dependent steps as `NOT RUN` with exact reason if hardware/runtime conditions make them impossible.

---

# 34. REAL HARDWARE TEST SAFETY

When exercising real devices:

- do not forget/remove a user’s personal device unless the test plan requires it and the device can be re-paired;
- do not alter unrelated system audio configuration permanently;
- restore changed session/settings state after testing;
- do not run destructive D-Bus commands outside Auralis merely to make the UI appear successful;
- distinguish application correctness from environment limitations.

---

# 35. BUILD/CMAKE REQUIREMENTS

Update CMake only as needed for Phase 7.

Ensure:

- new C++ presentation sources are included;
- new public/private headers are included appropriately;
- QML files are packaged;
- assets are packaged;
- Qt modules are linked explicitly;
- tests are registered;
- install/runtime QML lookup is correct.

Do not “fix” missing resources by copying random files into the build directory manually.

A clean build must reproduce the application.

---

# 36. CLEAN BUILD GATE

Before declaring success, run from a clean build:

```bash
rm -rf build

cmake -S . -B build -G Ninja

cmake --build build

ctest --test-dir build --output-on-failure
```

Then launch using the project’s actual output location, expected conceptually as:

```bash
./build/apps/desktop/auralis-desktop
```

If the actual repository output path differs, use the real path.

A stale previous build is not acceptable proof.

---

# 37. STATIC/QUALITY CHECKS

Use project-supported tools if present.

Examples:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

If configured:

```bash
clang-tidy
qmllint
qmllint
qmlformat --check
```

Do not add a large tooling migration just for Phase 7.

At minimum, run the Qt/QML linter available in the installed Qt stack against new QML where practical.

Resolve meaningful warnings.

---

# 38. ERROR-HANDLING RULES

## 38.1 Backend error

A backend operation failure should:

1. leave authoritative state intact;
2. expose a user-facing message;
3. log technical detail;
4. enable retry if safe.

## 38.2 Missing subsystem

If Bluetooth or PipeWire is unavailable:

- disable impossible actions;
- explain why;
- keep other usable screens available.

## 38.3 Race conditions

Examples:

- device disappears while detail dialog is open;
- session deleted while selected;
- endpoint disappears during selection;
- source disappears during activation.

The GUI must fail safely:

- close or invalidate stale selection;
- avoid null dereference;
- avoid invoking actions on deleted IDs;
- update to the new backend state.

---

# 39. STATE CONSISTENCY RULES

## 39.1 One source of truth

For each concept, identify the backend owner.

Examples:

```text
Bluetooth connected state -> BlueZ/device backend
Endpoint availability     -> PipeWire/audio backend
Route active state        -> AudioRouter
Session state             -> SessionManager
Persisted setting         -> ConfigurationManager
```

The UI must not maintain a second truth.

## 39.2 Selection vs state

Local UI selection is allowed:

```text
selectedDeviceId
selectedSessionId
currentPage
filterText
```

But connectivity/session/routing status must be backend-derived.

## 39.3 Stale selections

If a selected item disappears:

- clear selection;
- navigate to safe state;
- do not crash.

---

# 40. UX DETAILS THAT MUST NOT BE SKIPPED

Implement the small details that separate a complete GUI from a prototype:

- consistent spacing;
- no raw enum names with underscores where a friendly label is needed;
- meaningful empty states;
- disabled controls with explanation/tooltips;
- progress/busy state;
- confirmation for destructive actions;
- text elision;
- detail views for technical data;
- obvious active session;
- obvious degraded session;
- focus indicators;
- keyboard activation;
- error banner/toast;
- proper scroll behavior;
- status badges;
- human-readable timestamps;
- consistent percentage volume display;
- safe slider bounds;
- no “undefined”/“null” rendered as visible UI text;
- no raw `QVariant(...)` strings;
- no addresses displayed as the only device name if a friendly name exists;
- fallback to address when no name exists;
- no buttons wired to placeholder handlers.

---

# 41. GUI TEST DATA / DEVELOPMENT MODE

If a mock/demo data source is useful for QML development, it must be explicitly isolated.

Allowed:

- test-only fake services;
- unit-test fixtures;
- QML preview/test data that cannot be active in production by accident.

Forbidden:

- production runtime falling back to fake devices when BlueZ is unavailable;
- fake sessions used to hide missing backend integration;
- mock route success in normal builds.

If adding a demo mode, it must be opt-in and clearly separated.

Prefer tests over adding a persistent demo mode unless genuinely useful.

---

# 42. LOGGING REQUIREMENTS FOR PHASE 7

Add useful UI-related log events through the existing Logger.

Examples:

```text
Gui.NavigationChanged
Gui.DeviceActionRequested
Gui.SessionActionRequested
Gui.RouteActionRequested
Gui.SettingsChanged
Gui.QmlLoadFailure
```

Do not spam logs on every animation frame or mouse hover.

Use device/session IDs where useful.

Do not log secret pairing codes beyond what is safely required.

---

# 43. SETTINGS / USER PREFERENCES OWNERSHIP

Do not introduce hidden QML state that users expect to persist.

If persisting GUI preferences such as:

- last page;
- window size;
- theme;
- last selected session;

decide deliberately whether this belongs in Phase 7.

Keep these preferences separate from core session/domain persistence.

Do not let UI preferences corrupt domain/session data.

---

# 44. THEME SUPPORT

A full custom theme switcher is not a mandatory roadmap requirement.

Priority:

1. coherent default theme;
2. legible on the target desktop;
3. correct semantic states;
4. accessibility.

If the existing project already supports dark/light modes, integrate them correctly.

Do not spend major Phase 7 effort inventing advanced theming unless the core GUI is already complete.

---

# 45. DIAGNOSTIC VISUALIZATION SCOPE

The Audio Routing page may use a graph-like visualization if it is reliable and clear.

However, do not allow fancy graph rendering to delay core functionality.

A clear structured layout such as:

```text
Selected Source
    |
    +--> Endpoint A
    |
    +--> Endpoint B
```

is acceptable.

The required success criterion is control and observability, not a node-editor product.

---

# 46. SCREEN-BY-SCREEN ACCEPTANCE CHECKLIST

## Dashboard

- [ ] Bluetooth status visible.
- [ ] PipeWire/audio status visible.
- [ ] active/current session visible.
- [ ] selected source visible when applicable.
- [ ] member/connected device summary visible.
- [ ] group volume visible and controllable when session allows.
- [ ] degraded/warning state visible.
- [ ] useful quick actions exist.
- [ ] no fake state.

## Devices

- [ ] scan start/stop works.
- [ ] discovery state visible.
- [ ] real discovered devices visible.
- [ ] known/paired state visible.
- [ ] pair works.
- [ ] pairing UX handles agent interaction where needed.
- [ ] trust/untrust supported where backend exposes it.
- [ ] connect works.
- [ ] disconnect works.
- [ ] reconnect state visible.
- [ ] forget/remove works with confirmation.
- [ ] RSSI/signal shown when available.
- [ ] device details accessible.
- [ ] audio endpoint state visible.
- [ ] no terminal dependency.

## Sessions

- [ ] list sessions.
- [ ] create.
- [ ] rename.
- [ ] duplicate if backend exposes it.
- [ ] delete.
- [ ] add devices.
- [ ] remove devices.
- [ ] select source.
- [ ] activate.
- [ ] deactivate.
- [ ] group volume.
- [ ] per-device volume if supported.
- [ ] save.
- [ ] restore.
- [ ] state transitions visible.
- [ ] degraded state visible.
- [ ] recovery state visible.
- [ ] backend is source of truth.

## Audio Routing

- [ ] available sources visible.
- [ ] available endpoints visible.
- [ ] selected source/destinations visible.
- [ ] active routes visible.
- [ ] activation/deactivation supported through backend.
- [ ] route failure visible.
- [ ] endpoint/source disappearance handled.
- [ ] volume controls map to correct owner.

## Diagnostics

- [ ] Bluetooth status.
- [ ] adapter details.
- [ ] PipeWire status.
- [ ] device summary.
- [ ] endpoint summary.
- [ ] route/link summary where available.
- [ ] session state.
- [ ] bounded log viewer if feasible.
- [ ] errors understandable.
- [ ] no shell-command production dependency.

## Settings

- [ ] current config values load.
- [ ] supported settings editable.
- [ ] writes persist through ConfigurationManager.
- [ ] invalid changes rejected safely.
- [ ] errors shown.
- [ ] reset behavior, if provided, is safe.

---

# 47. ARCHITECTURAL ACCEPTANCE CHECKLIST

- [ ] Qt Quick + QML is the desktop presentation technology.
- [ ] core remains primarily C++.
- [ ] BlueZ remains managed through existing D-Bus backend.
- [ ] PipeWire remains managed through existing native backend.
- [ ] QML does not invoke `bluetoothctl`.
- [ ] QML does not invoke `wpctl`.
- [ ] QML does not invoke `pactl`.
- [ ] no duplicate DeviceRegistry.
- [ ] no duplicate AudioRouter.
- [ ] no duplicate SessionManager.
- [ ] dynamic lists use stable identities.
- [ ] QML-facing models are thread-safe with correct affinity.
- [ ] async commands do not pretend to succeed.
- [ ] destructive actions require confirmation.
- [ ] reusable components reduce duplication.
- [ ] design tokens prevent scattered magic style values.
- [ ] QML resources work from clean build.
- [ ] no developer absolute paths.

---

# 48. TEST ACCEPTANCE CHECKLIST

- [ ] clean CMake configure passes.
- [ ] clean Ninja build passes.
- [ ] all old tests pass.
- [ ] new C++ UI/presentation tests pass.
- [ ] QML/component tests pass where added.
- [ ] QML loads without fatal warnings.
- [ ] startup smoke test passes where implemented.
- [ ] application manually launches.
- [ ] all six pages load.
- [ ] resize test passes.
- [ ] device UI workflow tested to practical extent.
- [ ] session workflow tested to practical extent.
- [ ] audio routing page reflects real backend state.
- [ ] diagnostics updates.
- [ ] settings persist.
- [ ] no regression in Phase 0–6 functionality.

---

# 49. FORBIDDEN SHORTCUTS

Do not declare Phase 7 complete using any of these shortcuts:

## Shortcut A — Static mock UI

A beautiful screen filled with sample devices is not completion.

## Shortcut B — Button stubs

```qml
onClicked: console.log("Connect")
```

is not implementation.

## Shortcut C — QML-only fake state

Locally toggling `connected`, `active`, or `paired` without backend authority is forbidden.

## Shortcut D — Shelling out

Do not execute:

```text
bluetoothctl
wpctl
pactl
```

as production GUI actions.

## Shortcut E — Ignoring errors

A backend failure must not result in a permanently spinning button or silent reset.

## Shortcut F — Disabling tests

Do not remove/falsify prior tests to make the suite pass.

## Shortcut G — Replacing previous phases

Do not rewrite working Phase 2–6 subsystems merely because UI integration is inconvenient.

Create thin adapters instead.

## Shortcut H — Marking unrun tests as passed

If hardware/manual testing cannot be performed, report `NOT RUN`.

---

# 50. IMPLEMENTATION ORDER

Execute Phase 7 in controlled increments.

## Increment 7.0 — Baseline audit

- inspect repository;
- map Phase 0–6 services;
- clean build/test;
- document actual GUI gaps.

## Increment 7.1 — Presentation foundation

- establish facade/models where needed;
- design tokens/theme;
- app shell;
- navigation;
- global status;
- notifications.

Gate:

```text
application launches
navigation works
backend readiness reaches QML
```

## Increment 7.2 — Devices

- device model binding;
- scan;
- lifecycle actions;
- detail UI;
- pairing interaction;
- endpoint state.

Gate:

```text
complete device lifecycle can be controlled through UI
```

## Increment 7.3 — Sessions

- session list;
- create/edit/delete;
- device membership;
- source selection;
- activation/deactivation;
- group controls;
- degraded/recovery UI.

Gate:

```text
multi-device session can be built and operated through UI
```

## Increment 7.4 — Dashboard

- aggregate readiness;
- active session;
- device summary;
- group controls;
- warnings;
- quick actions.

Gate:

```text
dashboard accurately represents current operation
```

## Increment 7.5 — Audio Routing

- source list;
- endpoint list;
- route state;
- activation/deactivation controls as supported.

Gate:

```text
route topology/state is understandable and controllable
```

## Increment 7.6 — Diagnostics

- system state;
- Bluetooth;
- PipeWire;
- routes;
- sessions;
- logs.

Gate:

```text
technical state can be inspected without terminal for normal diagnosis
```

## Increment 7.7 — Settings

- expose supported settings;
- persistence;
- validation.

Gate:

```text
settings page is functional, not placeholder
```

## Increment 7.8 — UX/accessibility/responsiveness

- resizing;
- keyboard;
- focus;
- long names;
- empty/error/loading;
- final component consistency.

## Increment 7.9 — Full automated/manual verification

- clean build;
- test suite;
- GUI smoke;
- real-machine validation to practical extent;
- final audit.

Do not wait until the very end to run tests.

Build and test after meaningful increments.

---

# 51. GIT DISCIPLINE

Do not destroy existing working user changes.

Before modifying:

```bash
git status --short
```

Do not blindly run destructive reset/clean commands against uncommitted work.

Suggested logical commits if the user wants commits:

```text
phase-7: add QML application shell and presentation models
phase-7: implement complete device management UI
phase-7: implement session management UI
phase-7: add dashboard and active session controls
phase-7: implement audio routing interface
phase-7: add diagnostics and settings interfaces
phase-7: add GUI tests and phase gate verification
```

Do not create commits unless repository/user workflow expects Cursor to do so.

---

# 52. CODE REVIEW QUESTIONS TO APPLY TO YOURSELF

Before considering each feature done, answer:

1. Does this UI reflect backend truth?
2. What happens if the backend action fails?
3. What happens if the object disappears while selected?
4. What happens if this signal occurs from another thread?
5. Is this logic duplicated from a backend state machine?
6. Can this be unit tested without hardware?
7. Does this work after a clean build?
8. Does QML resource lookup work from the built executable?
9. Is there a visible loading/empty/error state?
10. Can a keyboard user operate it?
11. Does resizing break it?
12. Does the action remain understandable when a session is degraded?
13. Have prior Phase 1–6 tests still passed?

---

# 53. DEFINITION OF “100% PHASE 7”

Do not use a vague percentage.

Phase 7 is **100% complete** only if all of the following are true:

## Functional

- every required primary screen exists;
- every required screen is connected to real backend data;
- the main user workflow is possible without terminal intervention;
- no major Phase 7 button is placeholder;
- backend actions reflect real state transitions;
- errors are surfaced.

## Architectural

- C++ remains domain owner;
- QML remains presentation owner;
- no shell dependency has entered production paths;
- no Phase 0–6 subsystem has been duplicated.

## Quality

- coherent design system;
- consistent status visuals;
- clear empty/error/loading states;
- keyboard/focus behavior;
- resize usability;
- no obvious QML warnings/binding errors.

## Testing

- clean configure/build passes;
- all automated tests pass;
- newly added presentation tests pass;
- application launches;
- manual workflow tested to practical extent;
- any unexecuted hardware-only test is clearly documented.

## Exit gate

The complete primary Auralis workflow can be performed through the GUI.

If any of those are false, report Phase 7 as incomplete and continue fixing.

---

# 54. FINAL VERIFICATION COMMANDS

Use the repository’s actual configuration, but the final sequence should be equivalent to:

```bash
cd <AURALIS_REPOSITORY>

git status --short

rm -rf build

cmake -S . -B build -G Ninja

cmake --build build

ctest --test-dir build --output-on-failure

./build/apps/desktop/auralis-desktop
```

If GUI testing needs environment variables, document them.

Run optional integration/hardware tests only according to the existing test contracts.

Do not introduce magic `<PLACEHOLDER>` values into commands that users are expected to execute literally.

---

# 55. FINAL DELIVERABLES

At completion, provide all code changes plus a Phase 7 audit document.

Create:

```text
docs/PHASE_7_IMPLEMENTATION_AUDIT.md
```

or follow the repository’s existing documentation naming convention.

The audit must include:

# Phase 7 Implementation Audit

## 1. Executive Summary
- completion status;
- what was implemented;
- remaining limitations.

## 2. Repository Changes
- files added;
- files modified;
- architecture changes.

## 3. Presentation Architecture
- facade/models;
- QML module;
- backend binding strategy.

## 4. Screens
### Dashboard
### Devices
### Sessions
### Audio Routing
### Diagnostics
### Settings

For each:
- implemented behavior;
- backend owner;
- relevant tests.

## 5. Async/Error Behavior
- operation state handling;
- errors;
- confirmations.

## 6. Accessibility/Responsiveness
- keyboard;
- focus;
- resize results.

## 7. Automated Test Results

Include exact commands and results.

Example:

```text
Configure: PASS
Build: PASS
CTest:  <N>/<N> PASS
QML tests: PASS
```

Never invent counts.

## 8. Manual Test Matrix

Use:

```text
Test                                     Result
------------------------------------------------
Application launch                       PASS/FAIL/NOT RUN
Dashboard backend status                 PASS/FAIL/NOT RUN
Device scan                              PASS/FAIL/NOT RUN
Pair                                     PASS/FAIL/NOT RUN
Connect                                  PASS/FAIL/NOT RUN
Disconnect                               PASS/FAIL/NOT RUN
Forget                                   PASS/FAIL/NOT RUN
Create session                           PASS/FAIL/NOT RUN
Add multiple devices                     PASS/FAIL/NOT RUN
Select source                            PASS/FAIL/NOT RUN
Activate session                         PASS/FAIL/NOT RUN
Group volume                             PASS/FAIL/NOT RUN
Degraded state visualization             PASS/FAIL/NOT RUN
Deactivate session                       PASS/FAIL/NOT RUN
Restore saved session                    PASS/FAIL/NOT RUN
Audio routing screen                     PASS/FAIL/NOT RUN
Diagnostics                              PASS/FAIL/NOT RUN
Settings persistence                     PASS/FAIL/NOT RUN
Window resize                            PASS/FAIL/NOT RUN
Keyboard navigation                      PASS/FAIL/NOT RUN
```

For every `FAIL` or `NOT RUN`, provide the exact reason.

## 9. Regression Results
Document whether Phase 1–6 tests remain passing.

## 10. Known Limitations
Only genuine limitations.

## 11. Phase 7 Exit Gate
Conclude exactly one:

```text
PHASE 7 EXIT GATE: PASSED
```

or

```text
PHASE 7 EXIT GATE: FAILED
```

If FAILED, list blocking issues.

---

# 56. FINAL RESPONSE FORMAT FROM CURSOR

When your implementation work is complete, respond with a concise engineering summary containing:

```text
PHASE 7 — COMPLETE AURALIS GUI

Status: PASSED / FAILED

Implemented:
- ...
- ...

Architecture:
- ...

Automated Verification:
- Configure: ...
- Build: ...
- Tests: ...

Manual Verification:
- ...

Files/Areas Added:
- ...

Remaining Issues:
- none
```

If tests fail, do not hide them.

Continue fixing Phase 7 issues until all code-correctness failures within scope are resolved.

Environmental/hardware limitations must be separated from code defects.

---

# 57. IMPORTANT INTERPRETATION RULES

When this prompt and the existing repository differ:

1. Existing working Phase 0–6 behavior is the baseline.
2. The authoritative Auralis roadmap defines the Phase 7 objective.
3. This prompt defines the required engineering rigor and GUI completeness.
4. Existing class/file names take precedence over illustrative names in this prompt.
5. Do not invent APIs if an equivalent already exists.
6. Do not silently reduce required UI functionality.
7. Do not rewrite unrelated components without evidence.
8. Maintain backward compatibility with existing tests/contracts.
9. Any needed backend-facing change in Phase 7 should be the smallest clean adapter/exposure necessary for GUI integration.
10. Report any genuine architectural blocker rather than masking it with fake GUI state.

---

# 58. PHASE 7 MASTER TASK

Implement **Phase 7 — Complete Auralis GUI** now.

Start by auditing the actual current repository and running the existing clean build/test baseline.

Then implement the presentation architecture and complete all six required desktop screens:

```text
Dashboard
Devices
Sessions
Audio Routing
Diagnostics
Settings
```

Bind them to the real Phase 0–6 backend, preserve architectural boundaries, add meaningful automated tests, perform clean build and manual GUI verification, fix all in-scope defects you discover, and create the Phase 7 implementation audit.

Do not stop at scaffolding.

Do not stop at mock-ups.

Do not stop when the program merely compiles.

Do not claim success because QML renders.

The final condition is:

> **Auralis can be operated end-to-end through the desktop GUI, with real backend state, real device/session/routing controls, clear diagnostics, no production shell-command dependency, all prior regressions passing, and a clean Phase 7 exit-gate audit.**
