# Phase 6 Independent Audit

Location: `docs/validation/phase-6-audit.md`  
Date: **2026-08-19**  
Git: **e77fc0d5f2eb03c01b7fe091a60624cf6fe36983**  
Host: Ubuntu 26.04 LTS, Qt 6.10.2, PipeWire 1.6.2, BlueZ 5.85, kernel 7.0.0-29-generic

## Scope

Independent validation of Phase 6 (multi-device session engine) on the Auralis development machine. Proof-only: build, test, static analysis, prerequisite live checks, limited session live test, desktop smoke, persistence via unit tests. No production session code was modified during this pass.

Phase 7 Sessions UI is out of scope.

## Environment evidence

Captured in `audit-phase6/preflight.txt` and `audit-phase6/preflight-services.txt`:

```text
Linux smit 7.0.0-29-generic ... x86_64 GNU/Linux
Ubuntu 26.04 LTS (resolute)
cmake 4.2.3, ninja 1.13.2, Qt 6.10.2, pw-cli/libpipewire 1.6.2, bluetoothctl 5.85
pipewire / pipewire-pulse / wireplumber: active
Connected: Device 88:08:94:9D:B4:22 Smokin' Buds
```

## Build evidence

```bash
rm -rf build
cmake -S . -B build -G Ninja   # PASS — audit-phase6/cmake-configure.log
cmake --build build            # PASS — 248/248 — audit-phase6/build.log
```

## Test matrix (default + session + live)

### Default ctest (no live flags)

```bash
ctest --test-dir build --output-on-failure
```

| Metric | Result |
|---|---|
| Total | 34 |
| Passed | 34 |
| Failed | 0 |

Phase 5 baseline was 28 tests; Phase 6 adds 6 session-related targets (+ coordinators in default suite).

Live slots skipped by default (verified by direct run of `tst_BlueZLiveIntegration` without flag — `QSKIP`).

### Session automated tests

| Target | Result | Evidence |
|---|---|---|
| tst_SessionManager | PASS | `audit-phase6/tst_SessionManager-verbose.txt` — 8 slots |
| tst_SessionStateMachine | PASS | default ctest #25 |
| tst_SessionPersistence | PASS | default ctest #26 |
| tst_RoutingCoordinator | PASS | default ctest #27 |
| tst_VolumeCoordinator | PASS | default ctest #28 |
| tst_SessionLifecycleIntegration | PASS | `audit-phase6/tst_SessionLifecycleIntegration-verbose.txt` |
| tst_SessionLiveIntegration | SKIP (default) / PASS init (live flag) | `audit-phase6/session-live-verbose.txt` |

### Prerequisite live tests

| Test | Result | Log |
|---|---|---|
| tst_BlueZLiveIntegration + `AURALIS_EXPECT_DEVICE_ADDRESS` | PASS | `bluez-live.txt` |
| tst_PipeWireLiveIntegration (generic) | PASS | `pipewire-live-generic.txt` |
| tst_PipeWireLiveIntegration + exact address | PASS | `pipewire-live-mapped.txt` |
| tst_AudioRoutingLiveIntegration (generic) | PASS | `audio-routing-live-generic.txt` |
| tst_AudioRoutingLiveIntegration + exact address | FAIL (1st batched run) / PASS (retry) | `audio-routing-live-exact.txt`, `audio-routing-live-exact-retry.txt` |

**Exact routing first-run failure:** PipeWire logged `mappedBt=0 unmappedBt=2` at initial sync before BlueZ snapshot completed; endpoint showed `addr=` empty. Retry after snapshot: `mappedBt=2`, route activated/deactivated with zero tagged links. Classified **MINOR** flaky timing in live test ordering.

### Session live

| Check | Result |
|---|---|
| Init/shutdown | PASS |
| `AURALIS_EXPECT_DEVICE_ADDRESSES` | NOT IMPLEMENTED |
| Live multi-device session | NOT COVERED |

## Static analysis

`audit-phase6/grep-session.txt`:

| Check | Result |
|---|---|
| `pw-link` / `wpctl` / `pactl` / `bluetoothctl` in `src/session` | **none** |
| Direct BlueZ client headers in `src/session` | **none** |
| `auralis-audio` + `auralis-bluetooth` in session CMake | **present** (`src/session/CMakeLists.txt:24-25`) |
| `SessionManager` in desktop wiring | **present** (`DesktopApplication.cpp:35`) |
| `ApplicationCore` sessions shutdown before pipeWire/bluetooth | **present** (`ApplicationCore.cpp:174-184`) |

## Architecture compliance

| Item | Status | Evidence |
|---|---|---|
| One `AudioRoute` per session member | PASS | `RoutingCoordinator.cpp:111-121` — `createRoute` per member |
| Member identity = Bluetooth address | PASS | `SessionPersistence.cpp:42-47` — `deviceId` normalized address |
| Source identity = Phase 5 `AudioSource.id` | PASS | `SessionManager::setSource`, routing uses `session.sourceId` |
| Single active session policy | PASS | `SessionManager.cpp:409-411` — `AlreadyActive` |
| Auto-restore off; `restoreLastSession()` explicit | PASS | `SessionPersistence` loads Active as Idle; `restoreLastSession()` API |
| Volume `clamp(groupVolume * memberTrim, 0, 1)` | PASS | `VolumeCoordinator.cpp:15-21` |
| Recovery via `BluetoothManager::reconnectDevice` only | PASS | `SessionManager.cpp:916` |
| Generation token cancels stale callbacks | PASS | `SessionManager.cpp:417-418`, `generations_` hash |
| Shutdown order sessions → pipeWire → bluetooth | PASS | `ApplicationCore::rollbackInitializedServices` |
| No shell audio tools in session production code | PASS | grep empty |
| Phase 5 RoutePanel when no session active | PASS | desktop smoke; no session routes created |

## Manual/hardware evidence

| Activity | Result |
|---|---|
| Desktop launch + developer status | PASS — `audit-phase6/desktop.log` |
| BT endpoint mapping after snapshot | PASS — `EndpointResolver Mapped ... 88:08:94:9D:B4:22:playback:...` |
| Manual session API via QML | **PARTIAL** — not exercised (no Sessions UI) |
| Hardware 2-device session Active | **NOT RUN** — single BT device |
| Hardware degraded keep-alive | **NOT RUN** — requires multi-member live session |
| Hardware recovery | **NOT RUN** |
| On-disk `sessions.json` from desktop | **NOT CREATED** — persistence proven in unit tests only |

## Regressions vs Phase 0–5

| Area | Result |
|---|---|
| Default ctest (34 tests) | PASS — no Phase 5 regressions |
| tst_AudioRouter | PASS (default #21) |
| tst_DesktopBackendSmoke | PASS (default #29) |
| Phase 5 live routing (retry) | PASS |
| Phase 5 exact BT routing (first batched run) | FAIL (timing); retry PASS |

No new production regressions identified in session layer static review.

## Degraded / recovery scenario matrix

| # | Scenario | Automated | Manual/HW | Result |
|---|---|---|---|---|
| 1 | Activate 2 devices → Active | `tst_SessionManager::twoDeviceActivation`, `tst_SessionLifecycleIntegration` | NOT RUN (1 BT device) | PASS (fake backend) |
| 2 | 2nd unavailable at start → Degraded | `tst_SessionManager::oneUnavailableMemberStartsDegraded` | NOT RUN | PASS (fake) |
| 3 | Lose member during Active; survivor keeps route | `tst_SessionLifecycleIntegration` (disconnect member B) | NOT RUN | PASS (fake) |
| 4 | Member returns → Recovering/Active | — | NOT RUN | NOT RUN |
| 5 | Stop while Recovering → Idle | `tst_SessionLifecycleIntegration` (deactivate during recovery) | NOT RUN | PASS (fake) |
| 6 | Second activate while one active | `SessionManager.cpp:409-411` (code); no dedicated unit slot | NOT RUN | PASS (by inspection + policy) |
| 7 | Delete active session stops routes | `SessionManager.cpp:237-238` `stopSession` before remove | NOT RUN | PASS (by inspection) |
| 8 | Rapid activate/deactivate | partial via lifecycle test | NOT RUN | PARTIAL |
| 9 | App shutdown with active session | — | NOT RUN | NOT RUN |

## Defects

| ID | Severity | Summary | Evidence |
|---|---|---|---|
| D1 | MINOR | `tst_AudioRoutingLiveIntegration` exact-address run can fail if BlueZ snapshot completes after PipeWire initial sync | `audio-routing-live-exact.txt` FAIL; retry PASS with `mappedBt=2` |
| D2 | MINOR | `tst_SessionLiveIntegration` is init-only; no live session activate/deactivate | `tst_SessionLiveIntegration.cpp` (40 lines) |
| D3 | MINOR | `AURALIS_EXPECT_DEVICE_ADDRESSES` not implemented | grep: only in prompt docs |
| D4 | MINOR | README says "Phase 6+: Not implemented" | `README.md:16` |
| D5 | MINOR | `ApplicationCore` log still says "Session service skeleton initialized" | `ApplicationCore.cpp:100` |
| D6 | NOTE | No Phase 7 Sessions UI — manual validation requires QML console or future UI | no `AppCore.sessions` in `ui/` |
| D7 | NOTE | Single BT device limits hardware multi-member proof | preflight inventory |

No **BLOCKER** or **MAJOR** production behavior defects found in automated validation. Live session routing on hardware remains unproven.

## Coverage gaps

1. Live multi-device session create → activate → volume → deactivate on real PipeWire
2. Hardware degraded/recovery with partial member loss
3. Persistence round-trip via desktop `AppDataLocation/sessions.json`
4. RoutePanel vs active session interference observation
5. `AlreadyActive`, delete-active-session, shutdown-with-active-session — no dedicated automated slots (policy in source only)

## Final verdict: PHASE 6: NOT COMPLETE

Phase 6 **implementation and automated validation are in good shape** (34/34 default ctest, session unit/integration harnesses, architecture compliance, prerequisite live tests pass on retry). However, the live-machine validation prompt requires proof that session behavior works on **real hardware** — partial member loss, degraded keep-alive, recovery — which was **not demonstrated** on this machine due to a single connected BT audio device, init-only session live test, and no manual `AppCore.sessions` API exercise.

**To reach COMPLETE:** extend `tst_SessionLiveIntegration` (or manual procedure) to activate a session against live endpoints; exercise degraded/recovery with two BT devices or simulated disconnect; optionally add BlueZ-snapshot wait to exact routing live test to eliminate D1 flake.

Runbook: [phase-6.md](phase-6.md)
