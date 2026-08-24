# Phase 9 Linux RC1 qualification audit

## Audit identity

Qualification window: 2026-08-24 18:52 – 2026-08-25 00:10 IST (Asia/Kolkata)  
Reference checkout: `9f8bd8a244a68ba205b5e428f36d5f2a2778ae9c`  
Branch: `master`  
Initial working tree: dirty only because `docs/prompts/QC/` was untracked; it
was preserved. Phase 9 later added the validation artifacts recorded below.

| Item | Actual value |
|---|---|
| OS | Ubuntu 26.04 LTS (`resolute`) |
| Kernel | `7.0.0-30-generic` x86_64 |
| Compiler | g++ 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Qt | 6.10.2 from `/usr` |
| BlueZ | 5.85 |
| PipeWire | 1.6.2 |
| D-Bus | 1.16.2 |
| WirePlumber | 0.5.13; user service active |
| Bluetooth adapter | One powered default controller; address redacted; USB modalias recorded locally |

Raw evidence is under the gitignored `audit-phase9-linux-rc/` directory. The
files named below use the 2026-08-24 and 2026-08-25 run IDs and are not committed.

## Architecture confirmation

The checkout preserves the Phase 8 ownership boundaries:

- `DesktopApplication` composes one BlueZ `BluetoothManager`, one PipeWire
  manager/router stack, one `SessionManager`, and one `RecoveryManager`.
- `BluetoothButtonControlManager` alone owns address-correlated evdev probing
  and `EVIOCGRAB`; QML invokes model APIs and does not open input devices.
- `RoutingCoordinator` and `SessionManager` own higher-level session intent;
  `AudioRouter` owns Auralis PipeWire links and permits output fan-out while
  retaining destination-input exclusivity.
- `RecoveryManager` remains the central service/suspend reconciliation layer.
- The persistent/app-lifetime virtual output uses the stable Auralis System
  Audio identity documented in `docs/architecture/LINUX_VIRTUAL_AUDIO_OUTPUT.md`.

No competing recovery, Bluetooth reconnect, route owner, or button engine was
introduced.

## Baseline regression

Fresh tree: `build-phase9-baseline` (`RelWithDebInfo`, Ninja, one build job).

| Check | Result | Evidence |
|---|---|---|
| Configure | PASS | `builds/baseline-configure-20260824-185457.log` |
| Full build (391 steps) | PASS | `builds/baseline-build-20260824-185457.log` |
| Default CTest | PASS — 52/52, 0 failed, 13.56 s | `ctest/default-20260824-185457.log` |
| Stress default (100 iterations) | PASS — 1/1, 0.05 s | `ctest/stress-default-20260824-185457.log` |
| Stress extended | PASS — 1,000 iterations, 0.35 s | `ctest/stress-extended-1000-20260824-185457.log` |
| Stress bounded larger run | PASS — 2,500 iterations, 0.89 s | `ctest/stress-bounded-2500-20260824-185457.log` |

After the routing and recovery fixes, the final rebuilt baseline again passed
52/52 in 9.50 s. The focused regressions and the 2,500-iteration stress label
also passed. Evidence: `baseline/ctest-after-logind-lsan-fix-20260824-212527.log`
and `stress/phase8-2500-post-recovery-fixes-20260824-211812.log`.

The default suite includes opt-in live executables. With their opt-in variables
unset, Qt skips their live test cases even though CTest reports the executable
as Passed. The 52/52 result is therefore the mandatory hardware-independent
regression result, not a claim that Bluetooth hardware ran.

Configure/build warnings were retained in evidence. They include distro-Qt
static QML plugin-target warnings, existing `qsizetype` to `int` conversion
warnings in list models, and PipeWire event-struct missing-field warnings. No
warning was hidden with a warnings-as-errors workaround, and no warning was
linked to a reproduced Phase 9 defect.

## Sanitizer results

Fresh tree: `build-phase9-asan` (`RelWithDebInfo`,
`AURALIS_ENABLE_SANITIZERS=ON`, one build job).

| Check | Result |
|---|---|
| Configure/build | PASS, 391 steps |
| ASan | PASS — no finding in 52/52 CTest |
| UBSan | PASS — no finding in 52/52 CTest |
| LeakSanitizer | PASS — exercised with `detect_leaks=1` |
| CTest | PASS — 52/52, 0 failed, 44.86 s in the final run |

The run used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Evidence:
`sanitizer/ctest-after-logind-lsan-fix-20260824-212321.log`.

An intermediate post-recovery run correctly failed on a 181-byte QtDBus
dynamic-meta-object lifetime allocation caused by Auralis constructing a
redundant `QDBusInterface` probe. After replacing that probe with the durable,
service-qualified D-Bus signal match, the targeted test and final 52/52 suite
passed under the same leak and undefined-behavior settings with no diagnostic.

## Hardware inventory and current availability

| Alias | Model | Current Phase 9 state |
|---|---|---|
| Device A | Smokin' Buds | Paired, bonded, trusted, connected during live tests; 90–100% battery observed; address redacted |
| Device B | Rockerz 255 Touch | Paired, bonded, trusted, connected during live tests; 90% battery; address redacted |

Initial preflight found both devices unavailable. When the operator later put
them in pairing mode, Device B reconnected normally. Device A exposed a stale
BlueZ bond (`br-connection-key-missing`). After explicit destructive approval,
only Device A's record was removed, rediscovered, paired, trusted, and
reconnected. Both devices then remained connected simultaneously and exposed
distinct A2DP playback/capture endpoints. No other bond was removed.

## Live PipeWire and routing evidence

The safe live tests ran against the actual user PipeWire daemon from the final
RelWithDebInfo baseline build.

| Check | Result | Evidence |
|---|---|---|
| `tst_PipeWireLiveIntegration` opt-in path | PASS — actual daemon, no skipped case | `pipewire/live-20260824-204644.log` |
| PipeWire initial graph sync | PASS — Connected, graph enumerated | same log |
| Runtime virtual output | PASS — `VirtualOutputReady`, not selected, non-persistent fallback | same log |
| `tst_AudioRoutingLiveIntegration` opt-in path | PASS — actual daemon and expected Bluetooth sink | `routing/live-20260824-204644.log` |
| Stereo route lifecycle | PASS — Active with 2 links; both owned links destroyed on deactivate | same log |

The final live route used the expected Device A Bluetooth sink. It reached
Active with two stereo links and removed both owned links on deactivation.

## Two-device connection and fan-out evidence

Current-run technical result: **PASS**. Run `20260824-204644` passed all seven
live stages. BlueZ and session two-address tests passed, and Phase 7 hardware
finished 4 passed, 0 failed, 0 skipped. Both devices were simultaneously
connected, mapped to distinct endpoints, and active in one compensated fan-out
session. The final deactivation removed the Auralis-owned links.

The dedicated playback helper additionally proved both direct application
capture and the virtual system-audio topology with two active member routes.
Raw evidence: `routing/phase7-hardware-20260824-204644.log` and
`routing/system-audio-play-both-20260824.log`.

## System-audio evidence

The real daemon created the runtime virtual sink/source pair. The dedicated
helper sent deterministic local media to `auralis_virtual_output`, selected
`src:auralis-system-audio`, reached Session Active with two active Bluetooth
members, and recorded both AAC sinks at the same advertised 189 ms input
latency. Machine topology result: **PASS**. The operator confirmed the alarm was
audible on both Device A and Device B. Acoustic sample alignment was not
measured or claimed.

## Button policy evidence

Software policy tests passed in baseline, Release, and ASan/UBSan suites. They
cover default ALLOW, per-address persistence, exact endpoint match, suppression,
failed-grab state, release, reconnect behavior, and per-device isolation.

With both devices connected, Linux created separate AVRCP event nodes named for
Device A and Device B. Both had empty `Uniq` fields and a `Phys` value containing
only the local adapter address. That is insufficient for the implementation's
safe per-headset address correlation. No arbitrary input node was opened or
grabbed. The correct effective state for both targets is Unsupported; physical
ALLOW/DISALLOW suppression therefore remains **NOT EXERCISED**.

An isolated-XDG live policy probe set DISALLOW on each target while the
two-device system-audio session remained Active. Both returned
`controllable=0` and `Unsupported on this transport`; the probe restored ALLOW
and left no playback child behind. Evidence:
`buttons/unsupported-policy-live-20260824.log`.

| Item | Device A | Device B |
|---|---|---|
| Safely correlated input path found | No — AVRCP identity not address-correlatable | No — AVRCP identity not address-correlatable |
| Effective ALLOW physical result | NOT EXERCISED | NOT EXERCISED |
| Effective DISALLOW physical result | NOT EXERCISED | NOT EXERCISED |
| Audio/route unaffected | PASS — Active during isolated DISALLOW probe | PASS — Active during isolated DISALLOW probe |
| Reconnect persistence | NOT EXERCISED | NOT EXERCISED |
| Application restart persistence | NOT EXERCISED | NOT EXERCISED |

## Recovery evidence

Software recovery tests passed, including the real-manager harness, new backend
outage regression, Bluetooth error-classification cases, and stress. The
operator authorized both user-service disruptions and the adapter power toggle.
Each final run began with a two-device Active session using stable
`src:auralis-system-audio`, observed the
real graph loss, returned to Active with two connected/available/routed members
and exactly two owned links for a one-second stability window, and exited with
no Auralis or playback child left behind.

| Scenario | Result |
|---|---|
| Device A connect/disconnect/rejoin | PASS — 10/10 bounded cycles |
| Device B connect/disconnect/rejoin | PASS — 10/10 bounded cycles |
| Healthy peer survives member loss | NOT EXERCISED |
| PipeWire restart while Auralis is open | PASS — core reconnected, both devices/routes restored, no duplicate/stale nodes or links |
| WirePlumber restart | PASS — graph churn observed, two-device session restored, no duplicate/stale nodes or links |
| Adapter power off/on | PASS — adapter loss/return observed; both bonds and trust preserved; session restored Active with two members/two links |
| BlueZ daemon restart | PENDING OPERATOR ACTION |

Final evidence: `recovery/pipewire-restart-final-20260824-211630.log` and
`recovery/wireplumber-restart-20260824-211730.log`, plus
`recovery/adapter-power-cycle-final-20260825-001002.log` and its paired Auralis
log. The first adapter attempt is retained as inconclusive: Device B reported
0% battery and could not rejoin. After both devices were charged to 100%, Device
A's stale bond was repaired under the existing device-specific destructive
approval, and the final adapter run passed. Earlier failing or precondition
attempts are retained beside the final logs and are not presented as passes.

## Suspend/resume evidence

The packaged and baseline applications successfully subscribed to logind
`PrepareForSleep`, and deterministic idempotency/restore-policy tests passed.
No real suspend cycle was performed. Two normal cycles and one
`restoreOnResume=false` cycle remain **PENDING OPERATOR ACTION**.

## Persistence and hardware stress evidence

Automated session, configuration, recovery-preference, logger, and button-policy
persistence tests passed. Ten connect/disconnect cycles per target passed with
zero failures. Physical clean exit/restart with a persisted two-device session,
restart with one absent member, stale-grab checks, session/button churn, and
long-running resource sampling remain **NOT EXERCISED**.

## Soak evidence

Planned minimum: 2 hours continuous two-device playback.  
Actual duration: 0 hours — bounded functional playback only.  
Result: **NOT EXERCISED**. No dropout, RSS delta, FD delta, or log-rotation claim
is made. Gate F remains pending.

## Package evidence

Fresh final tree: `build-phase9-release-final` (Release, Ninja, two build jobs).

| Check | Result |
|---|---|
| Release configure/build | PASS, 391 steps |
| Release CTest | PASS — 52/52, 0 failed, 11.87 s |
| Non-root staged install | PASS |
| CPack | PASS — DEB and TGZ |
| DEB | `auralis_0.1.0_amd64.deb`, 1,332,688 bytes |
| Metadata | PASS — amd64, 0.1.0, sound, truthful `Auralis (contact pending)` |
| Contents | PASS — binary, desktop, AppStream, SVG icon, LICENSE, PipeWire drop-in |
| Extracted dependency resolution | PASS — no `not found`, no build-tree library path |
| Extracted offscreen launch | PASS — timeout 124 after 5 s stable event loop |
| Launch milestones | QML root, BlueZ snapshot, PipeWire initial sync, logind subscription, virtual output ready |
| Normal interactive packaged launch | NOT EXERCISED |
| Root at runtime | Not required |

Artifacts and raw inspection live below
`audit-phase9-linux-rc/package/artifacts-20260824-212646/`. The extracted smoke
used the DEB tree directly and did not execute the source/build-tree binary.

Because this host intentionally uses distro Qt rather than bundling it, the DEB
declares Qt/QML dependencies including the exact Ubuntu private ABI packages.
This is appropriate for this internal reference-host package but lowers
cross-distro portability; second-host testing remains outstanding.

## Defects found/fixed

One Auralis core routing race was reproduced on actual hardware. During a new
session route's Activating window, volume reconciliation called
`syncSessionDestinations()`. `replanIfEnabled()` then replaced two pending
ownership tokens before PipeWire published their global IDs, requested the same
port pairs again, and PipeWire rejected the duplicate links with `EEXIST`.

The fix prevents replanning while a route is Planning, Ready, or Activating;
normal graph callbacks remain responsible for completing or failing the
in-flight activation. `sessionSyncDoesNotDuplicatePendingActivation()` recreates
the exact delayed-bind sequence and asserts two create calls, retained tokens,
no backend-owned leak, and eventual Active state. Exact hardware retest moved
the former single-device failure to Active, and the full two-device suite passed.

Phase 9 also added or hardened validation support:

- `scripts/validation/run-phase9-linux-rc.sh`: fresh safe orchestration,
  opt-in live execution, raw evidence, package extraction, and explicit result
  summaries;
- `docs/validation/phase-9-linux-rc.md`: operator reproduction procedure;
- this audit and documentation index/status updates.

Actual PipeWire restart then exposed a second core defect: synchronous route
callbacks replanned against a dead graph and moved an otherwise recoverable
session to Failed. Session reconciliation now holds the session in Recovering
while the audio backend is disconnected or its graph is not ready. The
`backendOutageDoesNotMakeRecoverableSessionTerminal()` regression verifies no
stale-graph link creation, followed by Active restoration with two links.

The first real retry after service return also showed that BlueZ reports
transient transport races as generic `org.bluez.Error.Failed` reasons such as
`br-connection-unknown`. Those explicitly enumerated transport reasons now use
the existing five-attempt exponential-backoff budget; bond/authentication
failures such as `br-connection-key-missing` remain terminal. Unit coverage
locks both sides of that classification.

The local audible helper gained `--system-audio` and
`--probe-service-recovery` modes, explicit post-disruption assertions, scoped
signal connections, and process-group cleanup after an escaped `pw-play` child
and a probe-lifetime crash were reproduced during qualification.

LeakSanitizer also identified a redundant dynamic logind `QDBusInterface`
probe. Auralis now registers the service-qualified `PrepareForSleep` signal
match directly, avoiding Qt's process-lifetime meta-object cache while
remaining valid across logind restarts.

The runner itself was corrected during review to query `wireplumber --version`
instead of the unsupported `wpctl --version` form. This did not affect any
application or validation result.

## Required result matrix

| Area | Scenario | Mandatory? | Result | Evidence |
|---|---|---:|---|---|
| Build | Fresh RelWithDebInfo configure/build | Yes | PASS | baseline logs |
| Tests | Full default CTest | Yes | PASS | 52/52 |
| Tests | Extended stress label | Yes | PASS | 1,000 + 2,500 iterations |
| Sanitizer | ASan/UBSan CTest | Yes | PASS | 52/52, leaks enabled |
| BlueZ | Adapter/live discovery | Yes | PASS | final live run, no skips |
| BlueZ | Device A lifecycle | Yes | PASS | re-pair, map, route, 10/10 churn |
| BlueZ | Device B lifecycle | Yes | PASS | connect, map, route, 10/10 churn |
| BlueZ | Two simultaneous connections | Yes | PASS | distinct A2DP devices/sinks |
| PipeWire | Live graph sync | Yes | PASS | live test, no skips |
| Routing | Device A route | Yes | PASS | single hardware route Active |
| Routing | Device B route | Yes | PASS | direct playback helper member Active |
| Routing | Shared-source A+B fan-out | Yes | PASS | two member routes Active |
| System audio | Auralis virtual system output | Yes | PASS | topology PASS; operator heard deterministic alarm on both devices |
| Buttons | Device A ALLOW | If supported | NOT EXERCISED | target path Unsupported |
| Buttons | Device A DISALLOW | If supported | NOT EXERCISED | target path Unsupported |
| Buttons | Device B ALLOW | If supported | NOT EXERCISED | target path Unsupported |
| Buttons | Device B DISALLOW | If supported | NOT EXERCISED | target path Unsupported |
| Buttons | Per-device isolation | If supported | NOT EXERCISED | both AVRCP nodes lack per-device address identity |
| Buttons | Restart persistence | Yes | NOT EXERCISED | physical lifecycle not run |
| Recovery | Power off/on Device A | Yes | NOT EXERCISED | physical power loss not initiated |
| Recovery | Power off/on Device B | Yes | NOT EXERCISED | physical power loss not initiated |
| Recovery | Connect/disconnect Device A | Additional | PASS | 10/10 bounded cycles |
| Recovery | Connect/disconnect Device B | Additional | PASS | 10/10 bounded cycles |
| Recovery | PipeWire restart | Yes | PASS | real loss/reconnect; Active with two members/two links |
| Recovery | WirePlumber restart | Yes | PASS | real graph churn; Active with two members/two links |
| Recovery | Adapter off/on | When safe | PASS | real power loss/return; bonds/trust preserved; Active with two members/two links |
| Recovery | BlueZ daemon restart | Operator-approved | PENDING OPERATOR ACTION | privileged/disruptive |
| Power | Real suspend/resume | Yes | PENDING OPERATOR ACTION | real laptop suspend not initiated |
| Persistence | App restart with healthy session | Yes | NOT EXERCISED | persisted restart scenario not run |
| Persistence | App restart with one device absent | Yes | NOT EXERCISED | absent-member restart scenario not run |
| Stress | Repeated hardware churn | Yes | PASS | 10 connect/disconnect cycles per target, zero failures |
| Soak | >=2h two-device playback | Yes | NOT EXERCISED | 0 h actual |
| Package | Release build | Yes | PASS | build + 52/52 |
| Package | DEB build | Yes | PASS | DEB + TGZ |
| Package | DEB inspect | Yes | PASS | metadata/contents logs |
| Package | Extracted-tree launch | Yes | PASS | 5 s stable timeout |
| Package | Normal packaged launch | Preferred | NOT EXERCISED | no interactive manual run |
| Scaling | 3-device preflight | Optional | NOT EXERCISED | hardware unavailable |
| Scaling | 4-device preflight | Optional | NOT EXERCISED | hardware unavailable |
| Portability | Second clean machine | Optional/next gate | NOT EXERCISED | host unavailable |

## Known limitations

- public license terms and maintainer contact remain owner-controlled and pending;
- physical button suppression is unsupported on these address-uncorrelatable AVRCP nodes;
- physical device-loss and BlueZ-daemon recovery are pending; adapter recovery passes;
- real suspend/resume and lifecycle checks are pending;
- the two-hour soak is pending;
- normal interactive packaged and second-clean-machine launches are pending;
- 3/4-device scaling is not exercised;
- acoustic synchronization is not measured and is a later phase.

## Exit gates

### Gate A — Software regression

**PASS.** Fresh build, 52/52 default tests, extended/bounded stress, and 52/52
ASan/UBSan/LeakSanitizer all passed.

### Gate B — Phase 8 hardware closure

**PASS.** Live BlueZ, PipeWire, two actual devices, fan-out, and the virtual
system-audio topology all pass. The operator confirmed the deterministic alarm
was audible on both devices. This independently closes the prior Phase 8
hardware evidence gap on 2026-08-24.

### Gate C — Button policy

**BLOCKED for this RC target.** Software policy passes and Unsupported is
truthful, but neither actual device exposes a safely address-correlatable evdev
path. Physical DISALLOW suppression and persistence cannot be validated.

### Gate D — Recovery

**BLOCKED.** Software recovery plus actual PipeWire, WirePlumber, and Bluetooth
adapter off/on recovery pass. Physical device power-off/rejoin with healthy-peer
survival, BlueZ-daemon recovery, and suspend/resume remain outstanding.

### Gate E — Persistence/lifecycle

**BLOCKED.** Automated persistence passes, but the mandatory two-device restart,
absent-member restart, and stale-grab lifecycle were not physically exercised.

### Gate F — Stress/soak

**BLOCKED.** Software stress and 10/10 actual connect/disconnect cycles per
device pass, but the minimum two-hour two-device soak did not run.

### Gate G — Packaging

**PASS.** Release build/test, non-root staging, DEB/TGZ creation, metadata and
contents inspection, dependency resolution, and extracted-tree smoke passed.

### Gate H — Technical Linux RC1

**BLOCKED.** Gates C–F are not passed. No internal RC1 technical candidate is
declared by this audit.

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PASS
PHASE 8 OVERALL: PASS

PHASE 9 SOFTWARE/REGRESSION GATE: PASS
PHASE 9 LIVE HARDWARE GATE: PASS
PHASE 9 BUTTON POLICY GATE: BLOCKED
PHASE 9 RECOVERY GATE: BLOCKED
PHASE 9 PERSISTENCE/LIFECYCLE GATE: BLOCKED
PHASE 9 STRESS/SOAK GATE: BLOCKED
PHASE 9 PACKAGING GATE: PASS

PHASE 9 LINUX RC1 TECHNICAL EXIT GATE: PENDING
PUBLIC DISTRIBUTION READINESS: BLOCKED — OWNER LICENSE/CONTACT METADATA PENDING
```

The next work is the remaining physical-device and BlueZ-daemon recovery, suspend/resume,
persistence/lifecycle, and two-hour soak sequence in
`docs/validation/phase-9-linux-rc.md`. Phase 10 scaling or latency work must not
start from this audit as if Phase 9 had passed.
