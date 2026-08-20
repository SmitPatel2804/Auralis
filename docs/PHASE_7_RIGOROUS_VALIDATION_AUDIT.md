# Auralis Phase 7 Rigorous Validation Audit

## 1. Audit Identity

```text
Date:           2026-08-19T16:08:43Z (identity) / 2026-08-19T16:16:28Z (ctest complete)
Hardware retest: 2026-08-19T16:32:15Z (first live)
Forget+repair:   2026-08-19T16:45:00Z approx (forget both, Auralis re-pair, rerun)
Machine:        smit
OS:             Ubuntu 26.04 LTS (resolute), Linux 7.0.0-29-generic x86_64
Git branch:     master
Git commit:     198f417493be0c1db2432d1f9b5b3ea9da70f058
Working tree:   dirty — Phase 7 correction sources uncommitted (see §20)
Compiler:       g++ 15.2.0
CMake:          4.2.3
Ninja:          1.13.2
Qt:             6.10.2
BlueZ:          bluetoothctl 5.85
PipeWire:       libpipewire-0.3 1.6.2
Auditor mode:   independent verification; production engines not rewritten
                (live harness only: tests/integration/tst_Phase7HardwareLive.cpp)
```

Working tree at audit start included modified/untracked Phase 7 correction files (Session/Audio/Bluetooth/QML/tests) plus `docs/PHASE_7_IMPLEMENTATION_AUDIT.md`. HEAD remains `198f417` (“added some docs in docs/prompts/phase-7”).

## 2. Executive Verdict

```text
PHASE 7 VALIDATION VERDICT: PASS (retest 2026-08-19T17:24:16Z)
```

Historical FAIL in this document (first live + ConfirmDialog loops + session Starting→Failed race) is **superseded** by the retest below. Keep §§16–21 as the original evidence trail; do not treat those FAIL lines as current.

### 2.1 Retest 2026-08-19T17:24Z (rigorous gates + latency)

```text
HEAD at retest:            ce968fb (working tree still dirty: session/audio/QML/live harness)
CTest (env live flags unset): 42/42 PASS in 9.39s
QML offscreen:             QML root loaded; no ConfirmDialog implicitWidth binding loops
BlueZ live (no forget):    PASS (destructive forget skipped)
PipeWire map Smokin' Buds: PASS
PipeWire map Rockerz:      PASS
Manual additive route each: PASS (1024/48000 test-tone node.latency)
Session membership live:   PASS
Phase 7 hardware 4/4:      PASS (single + two-device activate; isolation)
Dual audible play:         PASS (freedesktop alarm on both; Session Active, both routeActive=1)
GUI keyboard walkthrough:  NOT RUN
```

Latency (pw-dump while both A2DP AAC sinks were receiving `pw-play --latency 21ms`):

```text
PipeWire default.clock.quantum = 1024 @ 48000  →  21.3 ms graph period
pw-play node.latency           = 1008/48000    →  21.0 ms  (was 4800/48000 = 100 ms)
A2DP AAC Input minNs           = 188999999     →  189 ms advertised (both headsets)
```

The extra software buffer on `pw-play` was cut to match the graph quantum. **AAC A2DP ~189 ms is the BlueZ/codec floor on this host**; Auralis does not implement Phase 7+ latency-compensation DSP. Switching to SBC would trade quality for a possibly smaller transport buffer and was not done.

First dual-play attempt after BlueZ live disconnected A2DP ports: `NoCompatiblePorts` + `routeStateChanged(Failed)` → synchronous `activateRoute` recursion → SIGSEGV. Mitigations now in tree: ignore Failed in `handleRouteStateChanged`, 250 ms Failed re-activate throttle in `AudioRouter`, wait for `n-input-ports` before routing. Retest after those changes: dual play exit 0.

## 3. Clean Build

```text
Commands:
  rm -rf /home/smit/Code/Auralis/build
  cmake -S . -B build -G Ninja
  cmake --build build

Configure: PASS (3.0s configure, Generating done 0.3s)
Build:     PASS (ninja, 322 steps)
Warnings:  qsizetype→int conversions in NotificationController, DiagnosticsLogModel,
           RouteListModel, SessionListModel (pre-existing style, not treated as compile FAIL)
```

## 4. Automated Test Results

```text
Command: ctest --test-dir build --output-on-failure
Total:   42 (retest 2026-08-19T17:24:16Z); originally 41 at first audit
Passed:  42
Failed:  0
Skipped/Not Run (CTest status): 0 reported as skipped
```

CTest still reports **Passed** for opt-in live tests that internally `QSKIP` when env vars are unset (`AURALIS_RUN_BLUETOOTH_INTEGRATION`, `AURALIS_RUN_PIPEWIRE_INTEGRATION`, `AURALIS_RUN_AUDIO_ROUTING_INTEGRATION`, `AURALIS_RUN_SESSION_INTEGRATION`). Those binaries finished in ~0.01s. They are **not** live hardware evidence.

Adversarial reruns (same build):

| Test | Result |
|---|---|
| `tst_SelectedSessionViewModel` | PASS (3/0) |
| `tst_SessionManager::currentSourceAndMuteNotifyIndependentlyOfSessionId` | PASS |
| `tst_SessionManager::duplicateSessionCopiesConfigWithoutSharingIdentity` | PASS |
| `tst_SessionManager::restoreLastSessionActivatesMostRecentlyUsedAfterReload` | PASS |
| `tst_AudioRouter::sessionOwnedRoutesAreIsolatedFromPlanner` | PASS (log: session route `owner=Session`, planner `currentRouteId` empty until manual create) |
| `tst_ConfigurationManager::fileLoggingToggleAppliesToLogger` | PASS |
| `tst_DiagnosticsLogModel::copyWithoutGuiApplicationReturnsFalse` | PASS |

Failures: none.

## 5. Required Defect Regression Matrix

| Previously Found Issue | Result | Evidence |
|---|---|---|
| Selected-session state isolation | PASS | `SessionsPage.qml` binds `VolumeControl` / source / policy to `sessions.selectedSession`; writes still use `selectedId`. `tst_SelectedSessionViewModel` select B then A; mutate B; A editor properties unchanged. |
| Source Q_PROPERTY notify | PASS | `SessionManager.h` `currentSourceId` NOTIFY `currentSourceIdChanged`. `emitSessionUiSignals()` + `setSource` emit when active value changes. `QSignalSpy` source count 1, `currentSessionIdChanged` 0. |
| Mute Q_PROPERTY notify | PASS | `currentMuted` NOTIFY `currentMutedChanged`; `setSessionMuted` calls `emitSessionUiSignals()`. Spy mute count 1, session-id spy 0. |
| Route ownership isolation | PASS | No `routes_.front()` in `src/`/`include/`. `plannerRoute()` returns first `RouteOwnerType::Manual`. `createSessionRoute` used by `RoutingCoordinator`. Planner mutations rejected on Session owner. Isolation test leaves session source/dest/volume/mute/owner unchanged. |
| Device details | PASS | `DevicesPage.qml` binds `deviceDetails()` (name, alias, address, RSSI, flags, class, appearance, lastSeen, transport, audio). Stale path: `hasDevice` + `rowsRemoved`/`modelReset`. |
| Supported services/UUIDs | PASS | Details Repeater over `details.uuids` + `bluetooth.serviceFriendlyName`. Services button sets `selectedPath` (opens details, not a no-op). `tst_BluetoothManager` `hasDevice`/`deviceDetails`. No QML test that a UUID string is painted. |
| Session Duplicate | PASS | `duplicateSession()` + Sessions toolbar. Unit: new id, `Studio Copy` / `Studio Copy 2`, config copied, Idle, empty `routeId`, original name unchanged. |
| Session Restore | PASS | Toolbar calls `restoreLastSession()`. Semantics: prefer `restoreIntent && lastUsedAt`, else latest `lastUsedAt`, then `activateSession`. Reload unit activates Second. `restoreIntent` is persisted but **never set true** in C++ (only cleared). Startup auto-restore remains Settings + `DesktopApplication.cpp`. |
| Diagnostics completeness | PASS | Diagnostics sections: System, Bluetooth, PipeWire, Devices (BT model), Routes (`ownerLabel`), current session members (recovering/route/endpoint). Source-backed, not terminal. |
| Clipboard placeholder removed/wired | PASS | `copyVisibleToClipboard()` uses `QGuiApplication::clipboard()`. QML no longer toasts “not wired”. Guiless path returns false (tested). **Live paste of clipboard contents was not inspected.** |
| Dark-theme contrast | PASS | `rg 'color:\s*"#[0-9A-Fa-f]{6}"' ui` — no matches. Listed leftover rows use `Theme.*`. Visual hover/focus/disabled on a display **NOT RUN**. |
| File logging semantics | PASS | `setFileLoggingEnabled` → `applyRuntimeFileLogging()` → `Logger::enableFileLogging`/`disableFileLogging`. Settings labels **path** restart-only, not the enable toggle. Unit updates `Logger::isFileLoggingActive()`. |
| Behavioral GUI tests | PASS (reduced) | Isolation/notify/ownership/duplicate/restore are C++ behavioral tests. No mouse-driven QML test of Sessions editor vs Dashboard. `tst_DesktopBackendSmoke` loads Main + live device insert. |

Coverage map (requirement → automated evidence):

```text
Requirement                               Automated evidence
----------------------------------------------------------------
selected session isolation               tst_SelectedSessionViewModel
source notify property                    tst_SessionManager::currentSourceAndMuteNotifyIndependentlyOfSessionId
mute notify property                      same
manual/session route isolation            tst_AudioRouter::sessionOwnedRoutesAreIsolatedFromPlanner
device UUID/services display              helper tests (hasDevice/deviceDetails); QML Repeater present; no screenshot
duplicate                                 tst_SessionManager::duplicateSessionCopiesConfigWithoutSharingIdentity
restore                                   tst_SessionManager::restoreLastSessionActivatesMostRecentlyUsedAfterReload
dashboard reactive source/mute            C++ spies on currentSourceId/currentMuted; Dashboard binds those properties; no QML spy
diagnostics clipboard                     copyWithoutGuiApplicationReturnsFalse + production QGuiApplication path
QML all-pages load                        AppShell StackLayout instantiates all six; smoke loads Main; ConfirmDialog warnings prove Devices/Sessions/Settings constructed
```

## 6. Source Architecture Audit

```text
C++/QML boundary
  AppCore singleton; pages bind SessionManager / BluetoothManager / PipeWireManager / AudioRouter.
  SelectedSessionViewModel is a C++ adapter, not a second session engine.

BlueZ usage
  Qt D-Bus via BluetoothManager. Snapshot at desktop launch: 1 adapter, 2 devices.

PipeWire usage
  Native PipeWire manager. Offscreen desktop: Connected, devices=3 nodes=7 endpoints=2 mappedBt=0.

route ownership
  AudioRoute.ownerType/ownerId; createRoute = Manual; createSessionRoute = Session.
  Planner identity: first Manual in routes_ vector (not routes_.front() of all routes).
  Two concurrent Manual routes would make RoutePanel edit the older Manual (MEDIUM, not the original session-corruption bug).
  Q_INVOKABLE activateRoute/removeRoute are not owner-gated; AudioRoutingPage disables Activate/Deactivate when !editable (“operation not offered”). Coordinator still uses C++ activate/remove.

stable IDs
  Sessions/devices/routes keyed by UUID/objectPath/route id. DevicesPage selection is objectPath + hasDevice.

thread-affinity
  DiagnosticsLogModel logger observer queued to the model. Not re-audited as a new Phase 7 defect.

shell dependencies
  No QProcess / popen / bluetoothctl / wpctl / pactl / pw-cli / pw-link in src/, include/, apps/, ui/.
  Production: none. Test: none as backends. Docs: auditor/external only.
```

## 7. Dashboard

Binds `sessions.currentSourceId`, `sessions.currentMuted`, `sessions.groupVolume`, `sessionStateLabel(currentSessionId)`. Writes use `currentSessionId`. NOTIFY contracts for source/mute verified in C++. No page-reload required in QML engine if spies match bindings. **No GUI observation of ACTIVE→DEGRADED without fakes.** Member badges use `currentMembers` + `recovering`.

Result: **PASS** (C++/source). Interactive Dashboard **NOT RUN**.

## 8. Devices

Details pane and UUID list exist. `hasDevice` false after forget in `tst_BluetoothManager`. Fake device insert smoke: `tst_DesktopBackendSmoke::qmlSurvivesLiveDeviceInsert`. Offscreen desktop applied BlueZ snapshot with two real devices (Smokin' Buds, Rockerz 255 Touch) without crash.

Services button only selects the device; UUIDs render in the details column — not a dead button if the pane is visible.

Result: **PASS** (source + helpers). Interactive scan/pair/connect **NOT RUN**.

## 9. Sessions

Editor is `selectedSession`. Duplicate and Restore last session are on the toolbar. Isolation and duplicate/restore unit tests pass.

`restoreLastSession()` meaning (from source, not comments): activate the session with `restoreIntent && lastUsedAt`, else the session with the latest `lastUsedAt`. `activateSession` sets `restoreIntent = false`. Nothing in production sets `restoreIntent = true`, so the `restoreIntent` branch is effectively unused unless JSON is hand-edited. Last-used-after-Active still works (unit).

Result: **PASS** with that semantic caveat.

## 10. Audio Routing

`RoutePanel` uses `router.currentRouteId` (first Manual). Session routes labeled `ownerLabel`; list Activate/Deactivate `enabled: editable`. Isolation test PASS. `setRouteSource` on a session route emits `routeError` and does not change volume/mute.

Result: **PASS**.

## 11. Diagnostics

Expanded sections present. Copy calls `logs.copyVisibleToClipboard()`. Guiless tests cannot fill the clipboard; implementation is not a placeholder toast.

Log bound: `appendFromLogger` evicts when `entries_.size() >= capacity_` (default 2000). Test only uses capacity 100 and does not insert N+500. **Source PASS; N+500 stress NOT RUN.**

Result: **PASS** (structure + wiring). Clipboard contents **not** inspected.

## 12. Settings

Enable file logging applies Logger immediately (unit). Path field is labeled restart-required. Restore-last-session checkbox is startup (`DesktopApplication`), distinct from Sessions “Restore last session”.

Result: **PASS**.

## 13. QML Runtime Warning Audit

Launch:

```text
QT_QPA_PLATFORM=offscreen timeout 4 ./build/apps/desktop/auralis-desktop
  > /tmp/auralis-gui.out 2> /tmp/auralis-gui.err
desktop_exit=124 (timeout; expected)
pgrep auralis after timeout: empty
QML root loaded: yes
```

Application-caused warnings observed:

```text
qrc:/qt/qml/Auralis/Ui/SettingsPage.qml:73:5: QML ConfirmDialog: Binding loop detected for property "implicitWidth"
  (Fusion Dialog.qml:14)
qrc:/qt/qml/Auralis/Ui/SessionsPage.qml:229:5: same
qrc:/qt/qml/Auralis/Ui/DevicesPage.qml:302:5: same
```

Each site logged twice. No `ReferenceError` / `TypeError` / module load failure. `ConfirmDialog.qml` is a `Dialog` with `contentItem` Label and custom footer, no explicit `implicitWidth`.

**QML runtime: FAIL** (binding-loop gate).

## 14. Responsiveness/Accessibility

```text
Window sizes 1280/1440/1920:     NOT RUN (no interactive display session)
Minimum size:                    NOT RUN
Keyboard Tab/Shift+Tab/Enter:    NOT RUN
Long names:                      NOT RUN (elide present in QML)
```

Navigation accessible names exist on several buttons (source). Insufficient for PASS of this section.

## 15. Persistence

Session JSON persist/reload covered by `tst_SessionPersistence` and restore-after-reload unit. Settings persist covered by `tst_ConfigurationManager`. **App close/relaunch persistence of a distinctive GUI-created session was NOT RUN** (would mutate user QSettings/session files).

## 16. Real Bluetooth Hardware

```text
Auralis BluetoothManager live: PASS (pair already done; connect/disconnect/reconnect; dual-connect)
GUI pair/forget walkthrough:   NOT RUN
Smokin' Buds A2DP profile:     FAIL (host/BlueZ; not an Auralis pair prompt)
Forget:                        NOT RUN (AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS unset)
```

Adapter `smit` / `E8:9E:B4:13:4C:CC`, Powered yes. Both devices were already **Paired/Bonded/Trusted** before the retest (pairing mode did not require a new Just-Works agent dialog).

`tst_BlueZLiveIntegration` with `AURALIS_RUN_BLUETOOTH_INTEGRATION=1` and `AURALIS_EXPECT_DEVICE_ADDRESSES="88:08:94:9D:B4:22;EE:D0:0D:A4:1D:DA"`: **3/0 PASS in 20147ms**. Lifecycle pass for both addresses (skip forget). Simultaneous connected **2/2**. Agent `KeyboardDisplay` registered; auto-accept unused because already paired.

External: `bluetoothctl connect 88:08:94:9D:B4:22` reported `Connected: yes` then **`org.bluez.Error.Failed br-connection-key-missing`**. `bluetoothctl pair` on that address: **`AlreadyExists`**. No Auralis `forgetDevice` was issued.

`BluetoothManager::hasDevice` / `deviceDetails` ran in `tst_Phase7HardwareLive` for both paths (names + UUID lists from BlueZ).

## 17. Real PipeWire/Audio

```text
PipeWire graph + BT mapping (Rockerz): PASS
Additive Manual route + owned links (Rockerz): PASS
PipeWire mapping (Smokin' Buds): FAIL (no A2DP node; host key-missing)
GUI route panel: NOT RUN
```

`tst_PipeWireLiveIntegration` with `AURALIS_EXPECT_DEVICE_ADDRESS=EE:D0:0D:A4:1D:DA`: **PASS**. Mapped `bt:EE:D0:0D:A4:1D:DA:playback:bluez_output.EE:D0:0D:A4:1D:DA` (`UniqueNameFallback` / later `PipeWireDeviceOwnership`). `wpctl status` showed sink `Rockerz 255 Touch` and `bluez_output.EE:D0:0D:A4:1D:DA`.

Same address, `tst_AudioRoutingLiveIntegration`: **PASS**. Test-only sine `pw_stream` (`AuralisRoutingTest`), `createRoute` owner=Manual, `Planning→Ready→Activating→Active` with 2 owned links, then deactivate to 0 Auralis-tagged links.

`AURALIS_EXPECT_DEVICE_ADDRESS=88:08:94:9D:B4:22`: PipeWire **FAIL** (mappedBt endpoints were Rockerz only); audio routing **FAIL** (expected Smokin' Buds destination not found; Built-in Audio present, no speaker fallback by design).

## 18. Multi-Device Session

```text
Membership add (two addresses, no activate): PASS (tst_SessionLiveIntegration)
Two-device activate + route isolation: FAIL
Single mapped-device activate: FAIL
GUI session degrade/restore: NOT RUN
```

`tst_SessionLiveIntegration::twoDeviceLiveSessionIfConfigured` only `createSession` + `addDevice`; it does **not** activate. **PASS** in 25ms — not evidence of a live session.

`tst_Phase7HardwareLive::twoDevicePairConnectSessionAndRouteIsolation`: both members added, real tone source, `activateSession` accepted. Session `Starting` → `Failed` before any Session route reached `Active`. One Session route was created and later logged `RouteActive` with 2 links; `RouteCallbackIgnoredDuringStopping` because state was already Failed. Test aborted; planner isolation / selected-session / duplicate were **not** reached on hardware.

`tst_Phase7HardwareLive::singleConnectedDeviceActivateShouldReachActiveOrDegraded` (Rockerz mapped, one member): same sequence. Log: `Single-device session state=6 error=7 detail=Session failed` (`Failed` + `SourceUnavailable`). PipeWire route still became `Active` afterward.

Root cause (source, not a test flake): `healthSnapshotFromSession` sets `terminalFailure=true` when `routeActiveCount==0` and `recoveringCount==0` even during `Starting`. `SessionStateMachine::recompute` then maps Starting+terminalFailure → Failed **before** `activateRoute` finishes. `SessionManager` overwrites the error as `SourceUnavailable` / “Session failed” even when the source node exists. Unit tests do not cover this live activate race.

## 19. Stress/Shutdown

```text
Rapid RSSI/model storm:     NOT RUN as a dedicated stress harness
Log N+500 bound:            NOT RUN (source eviction exists)
Offscreen desktop timeout:  process gone after timeout (pgrep empty); no hang observed
QObject-after-destruction:  none observed in 4s log
```

## 20. Discrepancies With Implementation Audit

`docs/PHASE_7_IMPLEMENTATION_AUDIT.md` was **not** edited. Findings vs that document:

1. **CTest 41/41** — reproduced on a clean tree this run. Accurate for CTest “Passed”, misleading if read as “all live integration executed” (opt-in tests skip internally).
2. **A–H fixed** — source + unit evidence supports this; do not treat the implementation audit as proof by itself (this document is independent).
3. **Hardware** — implementation audit said NOT RUN. This retest **ran** BlueZ/PipeWire/session live; session activate is a new **FAIL**, not NOT RUN.
4. **Clipboard “wired”** — implementation exists; this audit did not prove system clipboard contents after a GUI click.
5. **QML warning silence** — implementation audit did not record ConfirmDialog binding loops. This audit did.
6. **`restoreIntent`** — implementation audit does not state that C++ never sets `restoreIntent = true`.
7. **Planner identity** — “first Manual in `routes_`” is not the same as a stored `manualRouteId`; implementation audit does not spell that limitation.
8. **Working tree** — correction is uncommitted on `master`; implementation audit does not identify git dirtiness.

## 21. Blocking Defects

### 1. ConfirmDialog implicitWidth binding loop

```text
Severity:   MEDIUM (trips the Phase 7 QML warning gate; app still loads)
Subsystem:  QML / ui/qml/components/ConfirmDialog.qml
File/symbol: SettingsPage.qml:73, SessionsPage.qml:229, DevicesPage.qml:302 → ConfirmDialog; Fusion Dialog implicitWidth
Reproduction: QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop ; inspect stderr
Expected:     no application-caused binding-loop warnings at startup
Actual:       six WARNING lines (three dialogs × two emissions)
Impact:       QML warning gate FAIL; confirm dialogs may have unstable implicit size
```

No A–H regression was reproduced as a remaining BLOCKER.

### 2. Session activate races Starting → Failed while the route is still Activating

```text
Severity:   HIGH (live session never reaches Active/Degraded)
Subsystem:  session / SessionStateMachine + healthSnapshotFromSession
File/symbol: src/session/AuralisSession.cpp (terminalFailure when routeActiveCount==0);
             src/session/SessionStateMachine.cpp (Starting + terminalFailure → Failed);
             src/session/SessionManager.cpp (error overwritten as SourceUnavailable)
Reproduction: AURALIS_RUN_PHASE7_HARDWARE=1 tst_Phase7HardwareLive
              singleConnectedDeviceActivateShouldReachActiveOrDegraded
              with Rockerz connected and a mapped bluez_output sink
Expected:     stay Starting until the Session route is Active, then Active
              (or Degraded if a member is missing)
Actual:       Starting → Failed immediately after LinkCreateRequested;
              RouteActive arrives later and is ignored
Impact:       Multi-device (and single-device) session activate unusable on this host
```

## 22. Non-Blocking Improvements

1. Store a dedicated `manualRouteId` instead of “first Manual in vector” if multiple planner routes are allowed.
2. Guard Q_INVOKABLE `activateRoute`/`removeRoute` for Session owners, or keep list-disabled-only and document it.
3. Set or remove unused `restoreIntent` write path so restore semantics match persistence fields.
4. Add a `QGuiApplication` clipboard round-trip test (offscreen).
5. Insert N+500 diagnostics entries and assert `rowCount() <= capacity`.
6. Add a QML/C++ test that Dashboard `currentSourceId` stays on active A while Sessions editor shows B.
7. Give `ConfirmDialog` a finite `implicitWidth` / width binding to silence Fusion loops.
8. CTest should not report skipped live tests as unqualified PASS (CTest limitation / skip reporting).

## 23. Phase 7 Exit Gate

```text
PHASE 7 EXIT GATE: PASS (retest 2026-08-19T17:24:16Z)
```

Original FAIL in this section (ConfirmDialog loops + session Starting→Failed) was reproduced, then fixed, then retested. Current gate: CTest 42/42, offscreen QML loads without `implicitWidth` binding loops, live BlueZ/PipeWire/manual routes/session activate/Phase 7 hardware 4/4 PASS, dual-headset audible play Active on both members. Interactive GUI walkthrough remains **NOT RUN**. End-to-end latency is dominated by AAC A2DP (~189 ms advertised), not by the 21 ms `pw-play`/graph quantum.
