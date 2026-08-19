# Phase 6 Final Closure — Correction & Hardening

Date: **2026-08-19**  
Prompt: `docs/prompts/phase-6/Auralis_PHASE_6_Correction_Hardening_and_Final_Closure_Prompt.md`  
Baseline: Phase 6 substantially implemented; this pass corrects defects A–N and expands tests.

## Baseline

- Clean configure/build with Ninja.
- Default `ctest`: **34/34** pass (live targets SKIP by default).

## Identified defects addressed

| Id | Defect | Resolution |
|---|---|---|
| A | Recovery policy not authoritative | `autoRestoreAllowed` + `ReconcileMode`; None does not auto-recreate |
| B | Competing reconnect loops | Removed session `start(500)` timers; `requestManagedReconnect` / `cancelManagedReconnect` via Phase 3 `ReconnectPolicy` |
| C | Failed recreates routes | Failed/Idle/Stopping suppress create; missing source suppresses create |
| D | Persistence parent dir | `QDir::mkpath` before `QSaveFile` |
| E | Silent persist success | `SessionCommandResult::PersistenceFailure` + `sessionError` |
| F | Recovery survives policy changes | Cancel matrix on stop/delete/disable/None/autoReconnect/shutdown |
| G | Stale generation | Bump generation on activate/deactivate/delete/policy/autoReconnect; capture for reconcile |
| H | Member return test | Lifecycle + manager tests with new endpoint id + volume/mute |
| I | Source loss semantics | Mid-session → Recovering; activate without source → Failed until retry |
| J | `lastUsedAt` | Active or meaningful Degraded after Starting only |
| K | Stable source id | Confirmed Phase 5 `src:{serial\|nodeName}:…`; activate after node-id change |
| L | `sessionError` | Emit on persist/route/fail; clear member error on recover |
| M | Invalid volume | Reject NaN/±Inf; clamp finite out-of-range |
| N | Stale docs | README / architecture / docs hub / phase-6 docs updated |

## Recovery policy semantics

| Policy | BT reconnect | Auto route restore |
|---|---|---|
| None | no | no |
| RestoreRoutesOnly | no | yes |
| ReconnectAndRestore | yes (managed) | yes |

## Reconnect ownership

Session expresses intent once per loss episode. Phase 3 `ReconnectPolicy` owns backoff/attempts. Session cancels managed reconnect when intent is withdrawn.

## Tests added/expanded

- Policy None / RestoreRoutesOnly / ReconnectAndRestore
- Failed source quiescence + retry
- Disable member / policy flip cancel
- Stale stop generation
- Persistence mkpath + failure surface
- Invalid volume
- Source serial stable across node id change
- Member return with new endpoint id (integration)
- Live two-address membership smoke (SKIP unless env set)
- State machine Failed stays Failed without Starting intent

## Exact verification

```bash
rm -rf build && cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure
```

Result on closure machine: **34 tests, 0 failed** (live SKIP without flags).

## Hardware status

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 LIVE HARDWARE VALIDATION: NOT RUN — opt-in only
```

Two-device live playthrough remains optional via `AURALIS_RUN_SESSION_INTEGRATION` and `AURALIS_EXPECT_DEVICE_ADDRESSES`.

## Remaining limitations

- No Phase 7 Sessions UI
- No latency sync
- Do not mix RoutePanel activation with an active session
- Full live multi-device audio playthrough not executed in this closure pass

## Final verdict

```text
PHASE 6 STATUS: COMPLETE
```

Software gates from the correction prompt are met. Live two-device hardware proof is explicitly **not claimed**.
