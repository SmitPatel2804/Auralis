# Auralis Phase 5 Final Closure Audit

Location: `docs/validation/phase-5-audit.md` (moved from
`docs/PHASE_5_FINAL_CLOSURE_AUDIT.md` during documentation reorganization).

## 1. Executive Verdict

PHASE 5: COMPLETE

## 2. Scope

Proof-only closure of Phase 5 (additive native PipeWire routing). No Phase 6 session/sync/DSP work. Production routing types were not redesigned. Remaining work was regression tests, a one-line QML `enabled` binding fix found during desktop smoke, clean validation, live routing, and this audit.

Host: Ubuntu 26.04, Qt 6.10.2, PipeWire 1.6.2, BlueZ 5.85. Date: 2026-08-19.

## 3. Final Architecture Status

`AudioRouter` is owned by `PipeWireManager`. `RoutePlanner` is a pure graph transform. `LinkManager` talks to `IPipeWireLinkBackend`; production backend is `PipeWireConnection` (`link-factory` create, token destroy, `SPA_PROP_volume` / `SPA_PROP_mute`). Destinations are Phase 4 `AudioEndpoint` objects. `SessionManager` is unchanged. QML uses `AppCore.audio.router` only.

Policies still locked: **AdditiveRouting**, **RebindOnGraphReplacement**, atomic activate with rollback.

## 4. Previously Identified Blockers

| Previous Issue | Resolution | Evidence | Status |
|---|---|---|---|
| Disconnect left route Active | Active → Degraded + `PipeWireDisconnected` | `disconnectKeepsEnabledAndReplansAfterSync` | PASS |
| Pending proxy had no destroyable ownership | `ownershipToken` at create; `globalId` may be 0 | `pendingCreateThenFailDestroysPendingToken` | PASS |
| Same-port foreign link could be treated as owned | token → `ownedLinkGlobalId` | `foreignIdenticalPortLinkNeverOperational` | PASS |
| Partial pending rollback leak | destroy by token | `pendingCreateThenFailDestroysPendingToken` | PASS |
| Source loss stale ownership | `invalidateOwnedLinks` on Degraded | `sourceLossDegradesEnabledRoute` | PASS |
| Destination loss stale ownership | `invalidateOwnedLinks` on Degraded | `destinationLossDegradesEnabledRoute` | PASS |
| Unsafe PipeWire proxy lookup | lock, lookup, use, unlock | `PipeWireConnection.cpp` audit | PASS |
| Volume capability too broad | writable `SPA_PARAM_Props` | `volumeSupported()` + `tst_VolumeController` | PASS |
| Shared activation timer | per-route `QTimer` + generation | `twoRoutesHaveIndependentTimeouts` | PASS |
| BT expected address fallback | fail if address missing | negative live run | PASS |
| Router-only stale-link check | store `auralis.route.id` count | live deactivate asserts | PASS |
| Q_PROPERTY NOTIFY misuse | dedicated NOTIFY signals | `qmlPropertyNotifyFiresOnActivate` | PASS |
| Owned link Error left Active | Degraded + `LinkEnteredErrorState` | `ownedLinkErrorDegradesRoute` | PASS |
| Remove while activating resurrected | generation + destroy tokens | `removeRouteWhileActivatingCannotResurrect` | PASS |
| Stale timeout rolled back newer gen | generation guard + timer stop | `staleActivationTimeoutGenerationIsIgnored` | PASS |
| Shutdown left timers running | `stopAllActivationTimeouts` | `shutdownInvalidatesAllActivationTimers` | PASS |
| Activate button bound `undefined` to bool | explicit ComboBox value check | desktop relaunch, no QML warning | PASS |

## 5. Final Regression Tests Added

Added in `tests/unit/audio/tst_AudioRouter.cpp`:

- `ownedLinkErrorDegradesRoute`
- `removeRouteWhileActivatingCannotResurrect` (includes timeout invalidation)
- `deactivateWhileActivatingCannotResurrect` (pending tokens + late bind)
- `staleActivationTimeoutGenerationIsIgnored`
- `shutdownInvalidatesAllActivationTimers`
- `qmlPropertyNotifyFiresOnActivate`
- `volumeWriteFailureDoesNotTearDownRoute`

`twoRoutesHaveIndependentTimeouts` now completes route A while B times out.

## 6. Ownership and Link-Lifecycle Verification

Create returns a non-zero token immediately. Pending links are destroyed by token before bind. `linksOperational()` requires the token’s own global id in the store; foreign same-port ACTIVE links are ignored. Rollback of a two-link stereo plan with the second create failing destroys the first pending token.

## 7. Route State-Machine Verification

Activate: Planning → Ready → Activating → Active when owned links are ACTIVE/PAUSED. Disconnect: enabled stays true, state Degraded. Source/dest loss: Degraded with structured error and tokens destroyed. Owned ERROR: Degraded, not Active, foreign replacement ignored. Remove/deactivate during Activating cannot resurrect. Reconnect without `initialSyncComplete` does not create links.

## 8. Thread-Safety Verification

`grep` of `BoundProxy` / `ownedByToken` / `pendingCreated` / `proxies`: public `destroyOwnedLink`, `ownedLinkGlobalId`, `volumeSupported`, and `applyNodeProps` take `pw_thread_loop_lock`, use the pointer, then unlock. No unlock/race/re-lock dereference window found. `createdLinkIds` is an internal bound-id set, not topology ownership inference.

Port-pair matches in `RoutePlanner` / `LinkManager::createLink` / replan pair comparison are allowed (planning and diagnostics), not ownership.

## 9. Volume Capability Verification

Production: node proxy exists **and** `propsWritable` from writable `SPA_PARAM_Props`. Fake `unsupportedVolumeNodes` maps to `VolumeControlUnsupported`. `failVolumeNodes` maps to `VolumeControlFailed`. Volume failure does not tear down an Active route.

## 10. Activation Timeout Verification

Per-route single-shot `QTimer` armed with the attempt generation. Callback no-ops if generation differs or the route is gone/not Activating. A’s success does not cancel B’s timeout. Shutdown and `removeRoute` call `stopActivationTimeout`.

## 11. Bluetooth Integration Verification

Requested address `88:08:94:9D:B4:22` (Smokin' Buds, connected). Live test mapped `bt:88:08:94:9D:B4:22:playback:bluez_output.88:08:94:9D:B4:22`, activated two owned links, deactivated with zero tagged leftovers.

Negative: `AURALIS_EXPECT_DEVICE_ADDRESS="00:00:00:00:00:01"` failed with `Expected Bluetooth destination ... was not found. No speaker fallback.` and dumped Built-in Audio plus Smokin' Buds. Not part of default ctest.

## 12. Stale-Link Cleanup Verification

Live deactivate: `ownedLinkCount()==0` and `auralisTaggedLinkCount==0` after globals 97 and 95 were destroyed (`auralis.route.id` tags gone from the object store).

## 13. QML/UI Verification

`AudioRouter` properties use dedicated NOTIFY signals. `QSignalSpy` on `currentRouteIdChanged` / `routeStateTextChanged` / `routeEnabledChanged` passed. Desktop launch: PipeWire Connected, registry sync, endpoints=4 mappedBt=2. First launch warned `RoutePanel.qml:122 Unable to assign [undefined] to bool`; Activate `enabled` now checks `currentValue !== undefined && !== ""`. Relaunch: no QML assignment warning.

## 14. Build and Test Evidence

Commands actually run (fresh empty tree `build-phase5-closure`; `rm -rf build` was not executed because recursive delete of the existing `build/` tree was blocked):

```bash
cmake -S . -B build-phase5-closure -G Ninja
cmake --build build-phase5-closure
ctest --test-dir build-phase5-closure -N
ctest --test-dir build-phase5-closure --output-on-failure
ctest --test-dir build-phase5-closure -R "tst_AudioRoute|tst_AudioSources|tst_RoutePlanner|tst_AudioRouter|tst_VolumeController" --output-on-failure
./build-phase5-closure/tests/unit/audio/tst_AudioRouter -v2
```

Configure: exit 0. Found libpipewire-0.3 1.6.2. Build: 218/218 then incremental QML rebuild: exit 0.

`ctest -N` Phase 5 names: `tst_AudioRoute`, `tst_AudioSources`, `tst_RoutePlanner`, `tst_AudioRouter`, `tst_VolumeController`, `tst_AudioRoutingLiveIntegration`.

Default ctest: Total 28, Passed 28, Failed 0. Focused Phase 5 unit: 5/5 PASS. `tst_AudioRouter -v2`: all slots PASS including the new ones.

Static greps: no `pw-link`/`wpctl`/`pactl`/`pacmd` in `src include apps ui`; no `QProcess` there.

## 15. Manual Smoke-Test Evidence

```bash
./build-phase5-closure/apps/desktop/auralis-desktop
```

Launched on `DISPLAY=:0`. PipeWire Connected, initial sync complete, QML root loaded. After the ComboBox binding fix, no `Unable to assign` warning. Process stopped after 6s (`timeout`); no crash. Full click-through of Activate in the GUI was not performed; the live integration binary exercised create/activate/deactivate on the same PipeWire session.

## 16. Phase 0–4 Regression Status

Default suite includes core, Bluetooth, PipeWire manager/properties/endpoints/resolver, DeviceManager, SessionManager, desktop backend smoke, and skipped-by-default live BlueZ/PipeWire tests. All 28 default tests passed. SessionManager sources were not modified.

## 17. Known Environmental Limitations

None that block closure. Live PipeWire and exact Bluetooth routing both ran on this host. Interactive GUI route clicks were not fully walked; engine coverage is the live test.

## 18. Remaining Issues

None.

Future work belongs to Phase 6 and is outside this audit.

## 19. Phase 5 Completion Checklist

- [x] Immediate ownership token exists for every successful created link
- [x] Pending links are destroyable before global ID assignment
- [x] Rollback is atomic
- [x] Foreign links are never treated as owned
- [x] PipeWire disconnect removes Active state
- [x] Source loss cleans runtime ownership
- [x] Destination loss cleans runtime ownership
- [x] Reconnect waits for graph sync
- [x] Fresh runtime IDs are used after graph replacement
- [x] PipeWire backend accesses are synchronized
- [x] Volume capability reflects writable properties
- [x] Activation timeout is per route/attempt
- [x] Stale generations cannot mutate current activation
- [x] Route removal cannot be resurrected by late callbacks
- [x] Shutdown invalidates all activation timers
- [x] QML properties have correct notifications
- [x] Expected Bluetooth address is strict
- [x] Deactivation proves zero tagged Auralis links in graph
- [x] Default test suite passes
- [x] Focused Phase 5 tests pass
- [x] Live PipeWire test passes when environment is available
- [x] Desktop smoke test passes
- [x] No Phase 0–4 regression

## 20. Final Sign-Off

PHASE 5: COMPLETE

The Auralis Phase 5 Audio Routing Engine is implemented, regression-tested,
ownership-safe, rollback-safe, graph-churn-aware, thread-safe under the
defined PipeWire locking model, and verified not to leave stale Auralis-owned
links after route deactivation.

Phase 5 is formally locked.

The repository is ready to begin Phase 6.
