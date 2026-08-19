# Phase 6 Session Engine

Phase 6 adds a **multi-device session layer** on top of the Phase 5 additive routing engine. A session binds one stable audio source to N Bluetooth-identified playback members, coordinates one Phase 5 route per member, and exposes group/per-device volume with degraded operation and recovery.

## Architecture

```text
ApplicationCore
      |
SessionManager
      +-- SessionStateMachine (pure recompute)
      +-- SessionPersistence (versioned JSON)
      +-- RoutingCoordinator -> AudioRouter (one route per member)
      +-- VolumeCoordinator  -> destination volume APIs
      +-- DeviceRegistry / AudioEndpointRegistry (runtime resolution)
```

## Identity rules

- **Session id:** stable UUID persisted across restarts.
- **Member id:** normalized Bluetooth address (`AA:BB:CC:DD:EE:FF`), not BlueZ object path or PipeWire node id.
- **Source id:** Phase 5 stable `AudioSource.id` (for example `src:7:Stream/Output/Audio`).
- **Runtime only:** endpoint ids, route ids, connection state, recovery timers.

## Session states

`Idle`, `Starting`, `Active`, `Degraded`, `Recovering`, `Stopping`, `Failed`

Disconnect or partial member loss moves to **Degraded** / **Recovering** without tearing down healthy member routes. Stop cancels recovery and removes session-owned routes.

## Policies

- **Single active session:** only one session may be in an active lifecycle state at a time.
- **Auto-restore:** off by default; persisted sessions load as `Idle`. Use `restoreLastSession()` explicitly.
- **Recovery:** `ReconnectAndRestore` uses `BluetoothManager::reconnectDevice` then re-resolves endpoints and recreates missing routes.
- **Volume:** `effective = clamp(groupVolume * memberTrim, 0, 1)`; group changes do not erase per-member trim.

## Persistence

File: `QStandardPaths::AppDataLocation/sessions.json`

Schema version `1`, atomic `QSaveFile` write. Invalid records are skipped without crashing startup. Persisted `Active` is never trusted after restart.

## Build and test

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "Session|session" --output-on-failure
```

Optional hardware session integration (when added):

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure
```

## Known limitations

- Phase 5 `RoutePanel` manual routing and an active session should not be mixed in the same process.
- No latency/drift compensation (future specialized track).
- Session UI is minimal backend exposure only; full Sessions screen is Phase 7.
