# Phase 6 Validation — Multi-Device Session Engine

Validated: **2026-08-19** (software closure).  
Closure evidence: [phase-6-final-closure.md](phase-6-final-closure.md). Earlier live-machine audit: [phase-6-audit.md](phase-6-audit.md).

Phase 6 adds a multi-device session layer on Phase 5 `AudioRouter`: one route per Bluetooth member, authoritative recovery policies, JSON persistence, generation-safe cancel, and degraded keep-alive.

## Clean build and tests

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "tst_Session|tst_RoutingCoordinator|tst_VolumeCoordinator" --output-on-failure
```

## Session-focused coverage

| Target | Role |
|---|---|
| `tst_SessionStateMachine` | Pure state transitions |
| `tst_SessionPersistence` | Round-trip, parent mkpath, corrupt JSON |
| `tst_RoutingCoordinator` | One route per member, idempotent reconcile |
| `tst_VolumeCoordinator` | Group × trim, mute, reapply |
| `tst_SessionManager` | CRUD, policies None/Restore/Reconnect, Failed quiescence, generation, volume validation, persistence errors |
| `tst_SessionLifecycleIntegration` | Two-device degraded stop; member return with new endpoint id |
| `tst_SessionLiveIntegration` | Opt-in live init + optional two-address membership smoke |

## Live commands

```bash
AURALIS_RUN_SESSION_INTEGRATION=1 \
ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure

AURALIS_RUN_SESSION_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="AA:BB:CC:DD:EE:FF;11:22:33:44:55:66" \
ctest --test-dir build -R tst_SessionLiveIntegration --output-on-failure
```

Default `ctest` skips live session tests.

## Verdict

| Gate | Status |
|---|---|
| Software (clean build + full default ctest) | PASS |
| Live two-device hardware playthrough | NOT RUN / opt-in |

See [phase-6-final-closure.md](phase-6-final-closure.md) for defect list and ownership model.
