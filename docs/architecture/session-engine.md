# Phase 6 Session Engine

Part of the [as-built architecture](README.md). See [overview.md](overview.md) for the full module map. Validation: [phase-6.md](../validation/phase-6.md).

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

## Recovery ownership

| Layer | Owns |
|---|---|
| `SessionManager` | Session recovery intent, policy, route restore eligibility, generation cancel |
| `BluetoothManager` / `ReconnectPolicy` | Bluetooth reconnect timing, backoff, attempt counting (`requestManagedReconnect` / `cancelManagedReconnect`) |
| `RoutingCoordinator` | Desired vs actual session-owned routes |
| `VolumeCoordinator` | Reapply group × trim / mute after restore |

SessionManager does **not** run a competing Bluetooth retry loop.

## Identity rules

- **Session id:** stable UUID persisted across restarts.
- **Member id:** normalized Bluetooth address (`AA:BB:CC:DD:EE:FF`), not BlueZ object path or PipeWire node id.
- **Source id:** Phase 5 stable `AudioSource.id` (`src:{serial|nodeName}:{mediaClass}`).
- **Runtime only:** endpoint ids, route ids, connection/recovery flags.

## Session states

`Idle`, `Starting`, `Active`, `Degraded`, `Recovering`, `Stopping`, `Failed`

- **Failed / Idle / Stopping:** no new session-owned routes.
- **Source loss** while Active/Degraded → Recovering; source return restores routes.
- **Activate with unavailable source** → Failed; source return alone does not create routes; `retrySession()` required.
- Stop cancels recovery intent and removes session-owned routes.

## Recovery policies

| Policy | Bluetooth reconnect | Auto recreate route when endpoint returns |
|---|---|---|
| `None` | no | no (retry / stop-start / explicit activate only) |
| `RestoreRoutesOnly` | no | yes |
| `ReconnectAndRestore` | yes (delegated to Phase 3) | yes |

## Other policies

- **Single active session:** only one session may be Starting/Active/Degraded/Recovering/Stopping/Failed until deactivated.
- **Auto-restore:** off by default; persisted sessions load as `Idle`. Use `restoreLastSession()` explicitly.
- **Volume:** `effective = clamp(groupVolume * memberTrim, 0, 1)`. Non-finite values rejected (`InvalidArgument`).
- **`lastUsedAt`:** updated on Active, or Degraded after Starting; not on activate request or Failed.
- **Persistence failures:** Model B — mutation may remain in memory; API returns `PersistenceFailure` and emits `sessionError`.

## Persistence

File: `QStandardPaths::AppDataLocation/sessions.json`

Schema version `1`, parent directory created if missing, atomic `QSaveFile` write. Invalid records skipped. Runtime PipeWire ids are never persistence identity.

## Build and test

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "Session|session|RoutingCoordinator|VolumeCoordinator" --output-on-failure
```

Optional live:

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure

AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="AA:BB:CC:DD:EE:FF;11:22:33:44:55:66" \
ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure
```

## Known limitations

- Phase 5 `RoutePanel` manual routing and an active session should not be mixed.
- No latency/drift compensation.
- Session UI is minimal (`AppCore.sessions`); full Sessions screen is Phase 7.
- Two-device live hardware proof is opt-in and machine-dependent.
