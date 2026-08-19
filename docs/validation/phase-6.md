# Phase 6 Validation — Multi-Device Session Engine

Validated: **2026-08-19**  
Machine: **smit / Ubuntu 26.04 LTS (resolute), Linux 7.0.0-29-generic**  
Git: **e77fc0d5f2eb03c01b7fe091a60624cf6fe36983**  
Independent audit: [phase-6-audit.md](phase-6-audit.md)

Phase 6 adds a multi-device session layer on top of Phase 5 additive routing. This document records the live-machine validation run from the repository root after a clean configure/build.

## Environment

| Component | Value |
|---|---|
| GCC | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Qt | 6.10.2 |
| PipeWire | 1.6.2 |
| BlueZ | 5.85 |
| PipeWire user services | active (pipewire, pipewire-pulse, wireplumber) |
| Connected BT audio | Smokin' Buds `88:08:94:9D:B4:22` (single device; no second device for multi-member hardware tests) |

Shell variables used:

```bash
export AURALIS_DEVICE_A="88:08:94:9D:B4:22"
export AURALIS_DEVICE_B=""   # no second connected BT audio device
```

## Clean build

```bash
rm -rf build
cmake -S . -B build -G Ninja 2>&1 | tee audit-phase6/cmake-configure.log
cmake --build build 2>&1 | tee audit-phase6/build.log
```

| Step | Result |
|---|---|
| configure | **PASS** (libpipewire-0.3 1.6.2 found) |
| build | **PASS** (248/248 targets) |

Build tree: `build/` (clean `rm -rf build` succeeded).

## Default ctest

```bash
ctest --test-dir build --output-on-failure 2>&1 | tee audit-phase6/ctest-default.txt
ctest --test-dir build -N > audit-phase6/ctest-list.txt
```

| Metric | Value |
|---|---|
| Total | 34 |
| Passed | 34 |
| Failed | 0 |

Live integration targets are present and **skip their live slots** when env flags are unset (QtTest `QSKIP`; ctest still reports Passed):

- `tst_BlueZLiveIntegration` — skip message: `Set AURALIS_RUN_BLUETOOTH_INTEGRATION=1`
- `tst_PipeWireLiveIntegration` — skip message: `Set AURALIS_RUN_PIPEWIRE_INTEGRATION=1`
- `tst_AudioRoutingLiveIntegration` — skip message: `Set AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1`
- `tst_SessionLiveIntegration` — skip message: `Set AURALIS_RUN_SESSION_INTEGRATION=1`

Phase 0–5 regression remains green (includes `tst_AudioRouter`, `tst_DesktopBackendSmoke`, etc.).

## Session-focused ctest

```bash
ctest --test-dir build -R 'Session|session' --output-on-failure 2>&1 | tee audit-phase6/ctest-session.txt
./build/tests/unit/session/tst_SessionManager -v2 2>&1 | tee audit-phase6/tst_SessionManager-verbose.txt
./build/tests/integration/tst_SessionLifecycleIntegration -v2 2>&1 | tee audit-phase6/tst_SessionLifecycleIntegration-verbose.txt
```

| Target | Result |
|---|---|
| tst_SessionManager | PASS |
| tst_SessionStateMachine | PASS (default suite) |
| tst_SessionPersistence | PASS (default suite) |
| tst_RoutingCoordinator | PASS (default suite) |
| tst_VolumeCoordinator | PASS (default suite) |
| tst_SessionLifecycleIntegration | PASS |
| tst_SessionLiveIntegration | PASS (skipped live slot without flag) |

Session regex subset (`ctest -R 'Session|session'`): **5/5 PASS** (coordinator unit tests match by name but not regex; covered in full default suite).

## Prerequisite live tests

```bash
export AURALIS_DEVICE_A="88:08:94:9D:B4:22"

AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_DEVICE_A}" \
ctest --test-dir build -R '^tst_BlueZLiveIntegration$' --output-on-failure

AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build -R '^tst_PipeWireLiveIntegration$' --output-on-failure

AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_DEVICE_A}" \
ctest --test-dir build -R '^tst_PipeWireLiveIntegration$' --output-on-failure

AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
ctest --test-dir build -R '^tst_AudioRoutingLiveIntegration$' --output-on-failure

AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="${AURALIS_DEVICE_A}" \
ctest --test-dir build -R '^tst_AudioRoutingLiveIntegration$' --output-on-failure
```

| Test | Result | Notes |
|---|---|---|
| BlueZ live (exact address) | **PASS** | `audit-phase6/bluez-live.txt` |
| PipeWire live (generic) | **PASS** | `audit-phase6/pipewire-live-generic.txt` |
| PipeWire live (mapped BT) | **PASS** | `audit-phase6/pipewire-live-mapped.txt` |
| Audio routing live (generic) | **PASS** | `audit-phase6/audio-routing-live-generic.txt` |
| Audio routing live (exact BT) | **FAIL then PASS** | First batched ctest run failed (`mappedBt=0` before BlueZ snapshot); verbose retry **PASS** — see `audit-phase6/audio-routing-live-exact.txt` vs `audio-routing-live-exact-retry.txt` |

## Session live test

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
ctest --test-dir build -R '^tst_SessionLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/session-live-init.txt

AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="${AURALIS_DEVICE_A};${AURALIS_DEVICE_B}" \
ctest --test-dir build -R '^tst_SessionLiveIntegration$' --output-on-failure \
2>&1 | tee audit-phase6/session-live-multi.txt
```

| Check | Result |
|---|---|
| Init/shutdown with live BT + PipeWire | **PASS** (`session-live-verbose.txt`) |
| Multi-address env `AURALIS_EXPECT_DEVICE_ADDRESSES` | **NOT IMPLEMENTED** (no code reference; test ignores variable) |
| Multi-device activate on hardware | **NOT COVERED** (init-only live test) |

Current live test scope: `BluetoothManager + PipeWireManager + SessionManager` initialize and shut down cleanly with `sessionCount == 0`.

## Manual desktop validation

```bash
AURALIS_UI_SHOW_DEVELOPER_STATUS=1 \
timeout 8 ./build/apps/desktop/auralis-desktop 2>&1 | tee audit-phase6/desktop.log
```

| Check | Result |
|---|---|
| Bluetooth Ready | **PASS** |
| PipeWire Connected | **PASS** |
| Session manager initializes | **PASS** — log: `Session manager initialized sessions= 0` |
| QML root loads | **PASS** |
| AppCore.sessions API exercise (create/activate/deactivate) | **PARTIAL** — no Phase 7 Sessions UI; QML console not attached; lifecycle proven by automated harnesses only |
| RoutePanel overlap with active session | **NOT RUN** (no session activated from desktop) |

After BlueZ snapshot, desktop logs show `mappedBt=2 unmappedBt=0` and endpoint mapping for Smokin' Buds playback.

## Persistence

On-disk file `$XDG_DATA_HOME/Auralis/sessions.json` was **not created** during this validation (desktop smoke did not invoke session CRUD).

Persistence behavior verified via automated tests:

- `tst_SessionPersistence::roundTrip` — `schemaVersion` 1, BT address in `deviceId`, persisted `Active` loads as **Idle**, `groupVolume` survives
- `tst_SessionPersistence::invalidJsonDoesNotCrash` — corrupt file returns empty sessions
- `tst_SessionPersistence::skipsDuplicateDeviceIds` — duplicate members deduplicated
- `tst_SessionManager::persistenceReloadStartsIdle` — reload after save starts Idle with preserved `groupVolume`

## Known limitations confirmed

- No Sessions screen (Phase 7); `AppCore.sessions` exists but is unused in QML
- `tst_SessionLiveIntegration` is init-only; no live multi-device activate/deactivate
- `AURALIS_EXPECT_DEVICE_ADDRESSES` documented in prompts but not implemented in test code
- Single connected BT audio device on this machine — hardware multi-member and degraded/recovery scenarios not exercisable end-to-end
- Mixing RoutePanel manual routing with an active session is documented as unsupported (`docs/phase-6-session-engine.md`)
- README still lists Phase 6+ as "Not implemented" (docs drift — see audit)

## Commands reference

Full command log artifacts: `audit-phase6/` at repository root (gitignored).

```bash
# Default + session subset
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'Session|session' --output-on-failure

# Live prerequisites
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" \
  ctest --test-dir build -R '^tst_BlueZLiveIntegration$' --output-on-failure
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
  ctest --test-dir build -R '^tst_PipeWireLiveIntegration$' --output-on-failure
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" \
  ctest --test-dir build -R '^tst_AudioRoutingLiveIntegration$' --output-on-failure

# Session live (init only)
AURALIS_RUN_SESSION_INTEGRATION=1 \
  ctest --test-dir build -R '^tst_SessionLiveIntegration$' --output-on-failure

# Desktop smoke
AURALIS_UI_SHOW_DEVELOPER_STATUS=1 ./build/apps/desktop/auralis-desktop
```

## Verdict

**PHASE 6: NOT COMPLETE** on this machine.

Automated unit/integration coverage is green and architecture compliance checks pass, but the prompt’s hardware bar for multi-device session routing, degraded keep-alive, and recovery on real PipeWire/Bluetooth was **not proven** here: only one BT device, no live session activate test, and manual `AppCore.sessions` exercise was partial. See [phase-6-audit.md](phase-6-audit.md) for evidence and defect classification.
