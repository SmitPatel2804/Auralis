# Auralis — Phase 5 Final Closure Pass
## Remaining Regression Tests, Full Verification, and Formal Sign-Off Prompt

> **Use this prompt inside Cursor AI IDE on the latest Auralis repository.**
>
> **Repository state assumed:** Phases 0–4 are complete. Phase 5 is functionally implemented and the previously identified ownership, rollback, disconnect-state, foreign-link, thread-safety, volume-capability, per-route timeout, QML notification, Bluetooth selection, and graph-cleanup issues have already been corrected.
>
> **Purpose of this pass:** Do **not** redesign Phase 5. Complete only the remaining regression coverage, run the full clean validation matrix, and produce the mandatory final closure audit.
>
> **Final goal:** If and only if every mandatory check passes, write:
>
> ```text
> PHASE 5: COMPLETE
> ```
>
> and formally lock Phase 5 before moving to Phase 6.

---

# 1. Role

Act as a senior C++ / Qt 6 / PipeWire Linux audio systems engineer performing the **final closure audit** for Auralis Phase 5.

This is not a feature-development pass.

This is a **proof pass**.

Your job is to:

1. inspect the already-corrected Phase 5 implementation;
2. add the few remaining regression tests;
3. run a clean build;
4. run the full test suite;
5. run focused Phase 5 tests;
6. run live PipeWire routing tests where hardware/environment permit;
7. verify no stale Auralis links remain;
8. perform a desktop smoke test where practical;
9. generate `docs/PHASE_5_FINAL_CLOSURE_AUDIT.md`;
10. declare Phase 5 complete only if the evidence supports it.

---

# 2. Absolutely Do Not Rewrite Phase 5

The major implementation corrections are already present.

Do not replace or redesign:

- `AudioRoute`
- `AudioRouter`
- `RoutePlanner`
- `LinkManager`
- `VolumeController`
- `IPipeWireLinkBackend`
- `PipeWireConnection`
- ownership-token architecture
- current route state machine
- current PipeWire link-factory implementation
- current source/destination recovery approach
- current per-route activation timeout design
- existing QML routing panel
- current test infrastructure

Only make implementation changes if one of the new regression tests exposes a genuine remaining bug.

Prefer adding tests over refactoring working production code.

---

# 3. Phase Boundary

Do not implement Phase 6.

Do not add:

- `SessionManager`
- synchronization engine
- cross-device clock alignment
- latency compensation
- drift correction
- DSP delay pipelines
- persisted multi-device sessions
- device synchronization policies

Phase 5 remains:

```text
one logical source
        ->
one or more playback destinations
        ->
native PipeWire additive routing
```

---

# 4. First Step — Inspect Current State

Before editing, inspect:

```text
include/auralis/audio/AudioRoute.h
include/auralis/audio/AudioRouter.h
include/auralis/audio/IPipeWireLinkBackend.h
include/auralis/audio/LinkManager.h
include/auralis/audio/PipeWireConnection.h

src/audio/AudioRouter.cpp
src/audio/LinkManager.cpp
src/audio/PipeWireConnection.cpp
src/audio/RoutePlanner.cpp
src/audio/VolumeController.cpp

tests/unit/audio/tst_AudioRouter.cpp
tests/unit/audio/tst_AudioRoute.cpp
tests/unit/audio/tst_AudioSources.cpp
tests/unit/audio/tst_RoutePlanner.cpp
tests/unit/audio/tst_VolumeController.cpp
tests/integration/tst_AudioRoutingLiveIntegration.cpp

docs/phase-5-validation.md
```

Also inspect the fake/mock PipeWire backend used by `tst_AudioRouter`.

Confirm the following corrections already exist before changing anything:

- immediate non-zero ownership token for successful link creation;
- global ID may remain `0` while proxy is pending;
- rollback destroys by ownership token;
- `linksOperational()` resolves exact owned global ID by token;
- no same-port ownership inference;
- disconnect moves enabled route out of `Active`;
- source/destination disappearance cleans runtime ownership;
- PipeWire proxy/container access is synchronized;
- writable `SPA_PARAM_Props` drives volume capability;
- activation timeouts are per route/attempt;
- expected Bluetooth address is strict;
- live deactivation checks actual PipeWire graph tags;
- QML-facing properties have dedicated NOTIFY signals.

If any of these is missing in the current repository, stop treating this as only a test pass and fix the regression before continuing.

---

# 5. Remaining Required Regression Test 1
## Owned PipeWire Link Enters Error

Add a focused unit test to `tst_AudioRouter.cpp`.

Suggested test name:

```cpp
void ownedLinkErrorDegradesRoute();
```

Exact naming may follow repository conventions.

## Test scenario

1. Build a valid route.
2. Activate it successfully.
3. Ensure route reaches:

```text
RouteState::Active
```

4. Identify one exact Auralis-owned link using its ownership token/global ID.
5. Mutate the fake graph so that exact owned link enters:

```text
PipeWireLinkState::Error
```

6. Emit/trigger the same graph update path used in production.

## Required assertions

The route must:

- no longer be `Active`;
- become `Degraded` or the current defined failure state;
- expose:

```text
RouteError::LinkEnteredErrorState
```

- remain logically enabled if current recovery semantics retain route intent;
- not treat another foreign same-port link as a replacement;
- not silently return to `Active` without a fresh successful replan/rebind;
- clean stale runtime ownership as required by the current recovery design.

## Regression purpose

This test must fail if someone later changes:

```text
exact owned link is Error
```

into:

```text
route remains Active
```

or accidentally allows an unrelated foreign graph link to satisfy the route.

---

# 6. Remaining Required Regression Test 2
## Remove Route While Activation Is In Flight

Add a test such as:

```cpp
void removeRouteWhileActivatingCannotResurrect();
```

## Test scenario

1. Create a route.
2. Begin activation.
3. Fake backend successfully creates one or more ownership tokens.
4. Keep at least one created link pending/not operational so route remains:

```text
RouteState::Activating
```

5. Call:

```cpp
removeRoute(routeId)
```

before activation completes.
6. Simulate late backend/global-ID/graph callbacks that would normally complete activation.

## Required assertions

Immediately after removal:

- route no longer exists in router;
- every ownership token from the in-flight attempt has been destroyed;
- fake backend has zero Auralis-owned handles for that route;
- activation timeout for that route is invalidated;
- activation generation is invalidated.

After late callbacks:

- route is **not recreated**;
- no `Active` state is emitted for the removed route;
- no new ownership record appears;
- no stale timer callback causes mutation;
- no crash/use-after-free occurs.

## Regression purpose

This proves:

> stale asynchronous results cannot resurrect a removed route.

---

# 7. Remaining Required Regression Test 3
## Deactivate Route While Activation Is In Flight

If not already explicitly covered, add:

```cpp
void deactivateWhileActivatingCannotResurrect();
```

## Scenario

1. route starts activation;
2. backend returns ownership token(s);
3. route remains `Activating`;
4. call `deactivateRoute(routeId)`;
5. simulate delayed global-ID assignment and link-state callbacks.

## Required assertions

- route becomes `Inactive`;
- route is disabled;
- all pending/bound ownership tokens are destroyed;
- route owns zero links;
- stale callbacks do not move route back to:
  - `Ready`
  - `Activating`
  - `Active`
- stale timeout callback is harmless.

If equivalent coverage already exists and proves all of these points, document the existing test in the final audit rather than duplicating it.

---

# 8. Remaining Required Regression Test 4
## Stale Activation Timeout Generation

Add an explicit test for stale timeout generation.

Suggested name:

```cpp
void staleActivationTimeoutGenerationIsIgnored();
```

## Scenario

1. Begin activation generation N.
2. Capture or allow scheduling of its timeout.
3. Before timeout fires:
   - invalidate generation N by deactivation/replan/new activation.
4. Begin generation N+1.
5. Make N+1 valid/successful or leave it in a known valid state.
6. Trigger or wait for the old generation-N timeout callback.

## Required assertions

The old callback must not:

- roll back generation N+1;
- mark current route failed;
- clear N+1 ownership;
- overwrite error state;
- disable the current route.

This test should directly prove the generation guard works.

If current code uses per-route `QTimer` objects where the old timer is destroyed rather than callback-generation checked, prove the equivalent safety contract.

---

# 9. Remaining Required Regression Test 5
## Route Removal Invalidates Its Timeout

Add or confirm coverage for:

```cpp
void routeRemovalInvalidatesActivationTimeout();
```

Scenario:

```text
route A -> Activating
timer scheduled
removeRoute(A)
timer would otherwise expire
```

Required:

- no callback operates on removed route;
- no error/state signal refers to resurrected route;
- timer resources are cleaned;
- no crash.

This may be combined with the remove-while-activating test if the combined test clearly exercises the timeout path.

---

# 10. Remaining Required Regression Test 6
## Shutdown Invalidates All Activation Timers

Add:

```cpp
void shutdownInvalidatesAllActivationTimers();
```

or equivalent.

## Scenario

1. create route A;
2. create route B;
3. leave both `Activating`;
4. ensure both have active activation deadlines/timers;
5. call `AudioRouter::shutdown()`;
6. allow enough test time for any old timeout callbacks to have fired if not invalidated.

## Required assertions

- both route activations are cancelled;
- owned tokens are destroyed;
- timers are gone/inactive;
- no delayed timeout signals mutate router state;
- no stale error signals are emitted;
- router collections are clean.

Do not use a multi-second test if timeout duration can be injected or shortened for unit tests.

---

# 11. Multi-Route Timeout Independence

Confirm the existing test for independent activation timers remains present.

It must prove:

```text
Route A success
```

does not cancel:

```text
Route B timeout
```

and vice versa.

If the current test only checks one direction, extend it sufficiently.

The core invariant is:

> activation lifecycle for one route must never control timeout state for another route.

---

# 12. Foreign Same-Port Link Regression

Confirm the existing regression test remains strong.

It should set up:

```text
foreign link:
source port X -> destination port Y
```

and then an Auralis route planning the same:

```text
source port X -> destination port Y
```

Required:

- foreign global ID is never assigned to Auralis `OwnedLink`;
- foreign link does not make `linksOperational()` true;
- Auralis waits for its own ownership token to receive its own global ID;
- deactivation destroys only Auralis-created link;
- foreign link survives.

Do not weaken this test.

---

# 13. Pending-Link Rollback Regression

Confirm the existing test remains:

```text
first create succeeds
ownershipToken != 0
globalId == 0

second create fails

rollback
```

Required:

- first ownership token is destroyed;
- no pending fake backend handle remains;
- route does not become Active;
- `PartialActivationFailed` or correct structured error is reported;
- no stale ownership survives.

This test is mandatory for Phase 5 closure.

---

# 14. Disconnect Regression

Confirm the current disconnect regression verifies all of:

```text
Active
-> PipeWire Error
-> enabled remains true
-> state becomes Degraded
-> error PipeWireDisconnected
-> runtime ownership cleared
```

Then:

```text
Connected + initialSyncComplete=false
```

must not create links.

Then:

```text
Connected + initialSyncComplete=true
```

must:

- re-resolve runtime nodes/ports;
- create fresh ownership tokens;
- use fresh runtime global IDs;
- return to `Active` only after exact new links are operational.

---

# 15. Source/Endpoint Loss Regression

Confirm tests for both source and destination disappearance.

Each must prove:

- route leaves `Active`;
- route remains logically enabled where recovery policy requires;
- correct structured error:
  - `SourceRemoved`
  - `DestinationRemoved`
- owned runtime handles are destroyed;
- stale global IDs are cleared;
- reappearance may recover through fresh planning.

---

# 16. Volume Capability Regression

Confirm production logic remains based on writable PipeWire property support, not merely node existence.

Unit/fake coverage should distinguish:

```text
node exists + writable props
    -> supported
```

from:

```text
node exists + no writable SPA_PARAM_Props
    -> VolumeControlUnsupported
```

and:

```text
capability advertised
pw_node_set_param fails
    -> VolumeControlFailed
```

Volume failure must not tear down a healthy audio route.

---

# 17. Q_PROPERTY Notification Regression

Inspect QML-facing `AudioRouter` properties.

Confirm each has an appropriate notification signal, for example:

```text
currentRouteIdChanged
routeStateTextChanged
lastErrorTextChanged
routeEnabledChanged
routeVolumeChanged
routeMutedChanged
volumeCapableChanged
ownedLinkCountChanged
```

Ensure domain event signals remain available separately.

Where practical, add a `QSignalSpy` test proving a property signal is emitted when its observable property changes.

Avoid adding redundant tests for every property unless necessary.

---

# 18. Clean Static Search Gate

From repository root, inspect for prohibited production shell routing:

```bash
grep -RInE "\bpw-link\b|\bwpctl\b|\bpactl\b|\bpacmd\b" \
    src include apps ui
```

Expected:

```text
no production routing implementation uses these tools
```

Manual documentation/test mentions are acceptable if they are not production control paths.

Also inspect:

```bash
grep -RIn "QProcess" src include apps ui
```

Any match must be reviewed.

---

# 19. Ownership Guessing Search

Search for suspicious topology-based ownership inference:

```bash
grep -RInE \
"outputPort.*inputPort|inputPort.*outputPort|outputPortId.*inputPortId|inputPortId.*outputPortId" \
src/audio include/auralis/audio
```

Review all matches.

Allowed:

- `RoutePlanner` matching;
- comparison for diagnostics;
- test setup;
- channel pairing.

Forbidden:

```text
same output port + same input port
    =>
this foreign graph link is our Auralis-owned link
```

Ownership must come from:

```text
ownership token -> exact created proxy -> exact bound global ID
```

---

# 20. Thread-Safety Audit Search

Review every access to the backend structures containing:

- `BoundProxy*`
- proxy maps
- ownership-token maps
- global-ID maps
- pending created proxies
- writable capability state

Search:

```bash
grep -RIn \
"ownedByToken\|pendingCreated\|createdLink\|proxies\|BoundProxy" \
src/audio/PipeWireConnection.cpp \
include/auralis/audio/PipeWireConnection.h
```

Confirm no public method performs:

```text
lookup raw pointer
unlock/race window
later lock
dereference pointer
```

Confirm ownership lookup + destruction are serialized.

Confirm `volumeSupported()` is synchronized.

If all prior fixes remain correct, do not refactor further.

---

# 21. Configure From Clean State

Run exactly from repository root:

```bash
rm -rf build

cmake -S . -B build -G Ninja
```

Record:

- command;
- exit code;
- warnings/errors.

Configuration must succeed on the intended Linux development machine.

Do not use an old build directory as closure evidence.

---

# 22. Clean Build

Run:

```bash
cmake --build build
```

Required:

```text
PASS
```

Do not suppress warnings or weaken compiler settings.

If build fails:

- diagnose root cause;
- fix;
- delete/reconfigure if needed;
- rerun.

---

# 23. Discover Tests

Run:

```bash
ctest --test-dir build -N
```

Record the Phase 5 test names in the audit.

Do not assume target names if the repository differs.

---

# 24. Full Regression Suite

Run:

```bash
ctest --test-dir build --output-on-failure
```

This is mandatory.

All default tests from Phases 0–5 must pass.

A Phase 5 change is not acceptable if it breaks Phase 0–4.

Record:

```text
Total tests
Passed
Failed
Skipped/Not Run
```

and exact failing tests if any.

For true closure:

```text
Failed = 0
```

for the default test suite.

---

# 25. Focused Phase 5 Suite

Run the actual Phase 5 test targets.

For example:

```bash
ctest --test-dir build \
-R "tst_AudioRoute|tst_AudioSources|tst_RoutePlanner|tst_AudioRouter|tst_VolumeController" \
--output-on-failure
```

Use exact names reported by:

```bash
ctest --test-dir build -N
```

Required:

```text
PASS
```

---

# 26. Run AudioRouter Test Directly If Useful

For detailed QtTest output, locate the executable and run it directly.

Example:

```bash
./build/tests/unit/audio/tst_AudioRouter -v2
```

Use the repository's actual path.

This is particularly useful for documenting the new regression cases.

---

# 27. Live PipeWire Routing Test

If the Linux machine has a working PipeWire session, run:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
ctest --test-dir build \
-R tst_AudioRoutingLiveIntegration \
--output-on-failure
```

Required checks inside the test should include:

- PipeWire connected;
- initial sync complete;
- valid route source;
- valid playback endpoint;
- real Auralis link creation;
- route becomes `Active`;
- exact route links visible in graph;
- route deactivation;
- router-owned link count becomes zero;
- actual graph contains zero `auralis.route.id=<route-id>` links.

Record result.

---

# 28. Exact Bluetooth Routing Test

If the intended Bluetooth device is available, run:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="<REAL_TEST_DEVICE_ADDRESS>" \
ctest --test-dir build \
-R tst_AudioRoutingLiveIntegration \
--output-on-failure
```

Replace the placeholder with the actual connected device address.

Do not literally run:

```text
<REAL_TEST_DEVICE_ADDRESS>
```

as a shell token.

The integration test must fail if the requested Bluetooth endpoint does not exist.

It must not silently route to laptop speakers.

Record:

- requested address;
- resolved endpoint;
- route result;
- final stale-link result.

---

# 29. Bluetooth Negative Validation

Where practical, also validate the strict-selection behavior using an intentionally nonexistent test address.

For example:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="00:00:00:00:00:01" \
ctest --test-dir build \
-R tst_AudioRoutingLiveIntegration \
--output-on-failure
```

Expected:

```text
test fails specifically because requested Bluetooth endpoint is absent
```

This negative validation does not count as a regression-suite failure; run it separately and document that the failure was expected.

Do not include the intentionally failing negative run in the normal green `ctest` invocation.

---

# 30. Actual Graph Cleanup Evidence

The live test already checks route tags.

Confirm that after deactivation:

```text
router->ownedLinkCount() == 0
```

and:

```text
PipeWireObjectStore contains zero links with
auralis.route.id == deactivated route ID
```

If the live test only checks one of these, fix it.

Both are required.

---

# 31. Optional Manual PipeWire Inspection

During live testing, read-only diagnostics may be used.

Examples:

```bash
pw-cli ls Link
```

or:

```bash
pw-dump
```

or another installed inspection tool.

These commands are **diagnostic only**.

Do not modify production code to depend on them.

When manually inspecting, confirm:

- Auralis links appear during activation;
- Auralis route properties are visible where supported;
- Auralis links disappear after deactivation;
- unrelated links remain.

---

# 32. Desktop Smoke Test

Run:

```bash
./build/apps/desktop/auralis-desktop
```

Validate:

1. application launches;
2. no immediate crash;
3. PipeWire state reaches connected/synchronized;
4. source list populates;
5. playback endpoints populate;
6. a route can be created;
7. route can become active;
8. route state UI updates automatically;
9. volume controls respect capability;
10. mute/volume operations do not crash;
11. deactivation removes route links;
12. PipeWire disconnect/reconnect does not leave UI falsely showing active routing;
13. app closes cleanly.

Capture any warnings.

No new QML binding warnings should be introduced.

---

# 33. Phase 0–4 Regression Check

Do not focus only on routing.

The full test suite must demonstrate that these earlier systems remain intact:

- Bluetooth discovery
- Bluetooth connection
- BlueZ integration
- PipeWire lifecycle
- object store
- graph synchronization
- endpoint registry
- Bluetooth endpoint resolver
- desktop bootstrap

If any earlier-phase test fails because of Phase 5 changes, Phase 5 is not closed.

---

# 34. Update `docs/phase-5-validation.md`

After successful validation, update this document with:

- clean build commands;
- focused test commands;
- live integration command;
- exact Bluetooth command syntax;
- ownership token/global-ID invariant;
- disconnect behavior;
- per-route timer behavior;
- graph-level stale-link verification;
- date of final validation.

Do not turn it into a huge audit report.

The detailed evidence belongs in the final closure audit.

---

# 35. Mandatory Final Closure Audit

Create:

```text
docs/PHASE_5_FINAL_CLOSURE_AUDIT.md
```

This file is mandatory.

Do not finish the task without creating it.

---

# 36. Required Audit Structure

Use exactly this high-level structure:

```markdown
# Auralis Phase 5 Final Closure Audit

## 1. Executive Verdict
## 2. Scope
## 3. Final Architecture Status
## 4. Previously Identified Blockers
## 5. Final Regression Tests Added
## 6. Ownership and Link-Lifecycle Verification
## 7. Route State-Machine Verification
## 8. Thread-Safety Verification
## 9. Volume Capability Verification
## 10. Activation Timeout Verification
## 11. Bluetooth Integration Verification
## 12. Stale-Link Cleanup Verification
## 13. QML/UI Verification
## 14. Build and Test Evidence
## 15. Manual Smoke-Test Evidence
## 16. Phase 0–4 Regression Status
## 17. Known Environmental Limitations
## 18. Remaining Issues
## 19. Phase 5 Completion Checklist
## 20. Final Sign-Off
```

---

# 37. Audit — Executive Verdict

The audit must begin with exactly one of:

```text
PHASE 5: COMPLETE
```

or:

```text
PHASE 5: NOT COMPLETE
```

Do not use ambiguous wording such as:

```text
mostly complete
essentially complete
complete except...
```

If a mandatory gate is unresolved:

```text
PHASE 5: NOT COMPLETE
```

---

# 38. Audit — Previously Identified Blockers Table

Include a table like:

| Previous Issue | Resolution | Evidence | Status |
|---|---|---|---|
| Disconnect left route Active | Active -> Degraded | unit test | PASS |
| Pending proxy had no destroyable ownership | ownership token | unit test | PASS |
| Same-port foreign link could be treated as owned | token-based lookup | unit test | PASS |
| Partial pending rollback leak | destroy by token | unit test | PASS |
| Source loss stale ownership | cleanup on degradation | unit test | PASS |
| Destination loss stale ownership | cleanup on degradation | unit test | PASS |
| Unsafe PipeWire proxy lookup | synchronized lookup/use | code audit | PASS |
| Volume capability too broad | writable SPA_PARAM_Props | code/unit test | PASS |
| Shared activation timer | route-specific generation timer | unit test | PASS |
| BT expected address fallback | strict exact address | integration | PASS |
| Router-only stale-link check | actual graph tag check | integration | PASS |
| Q_PROPERTY NOTIFY misuse | dedicated NOTIFY signals | code/UI | PASS |

Add the final newly-added test items too.

---

# 39. Audit — Regression Test Table

Include every significant Phase 5 scenario.

Minimum:

| Scenario | Test | Expected | Result |
|---|---|---|---|
| Single route activation | unit | Active | PASS |
| Multi-destination route | unit | all expected links | PASS |
| Pending first link + second create failure | unit | full rollback | PASS |
| Foreign identical-port link | unit | ignored for ownership | PASS |
| Owned link enters Error | unit | route leaves Active | PASS |
| Source disappears | unit | Degraded + cleanup | PASS |
| Destination disappears | unit | Degraded + cleanup | PASS |
| PipeWire disconnect | unit | enabled + Degraded | PASS |
| Reconnect before sync | unit | no reactivation | PASS |
| Reconnect after sync | unit | fresh rebind | PASS |
| Deactivate while activating | unit | no resurrection | PASS |
| Remove while activating | unit | no resurrection | PASS |
| Stale timeout generation | unit | ignored | PASS |
| Two simultaneous activations | unit | independent timers | PASS |
| Shutdown with activating routes | unit | timers/links cleared | PASS |
| Unsupported volume | unit | structured unsupported | PASS |
| Live route activation | integration | real PipeWire links | PASS/NOT RUN |
| Exact Bluetooth target | integration | exact endpoint only | PASS/NOT RUN |
| Deactivation graph cleanup | integration | zero tagged links | PASS/NOT RUN |
| Full ctest regression | ctest | zero failures | PASS |

---

# 40. Audit — Commands Executed

Copy the exact commands actually run.

For example:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
...
```

Do not claim commands were run if they were not.

---

# 41. Audit — Environmental Limitations

If a hardware test cannot run, write exactly why.

Acceptable example:

```text
Exact Bluetooth live routing was NOT RUN because no Bluetooth A2DP
playback endpoint was connected to the test machine.
```

Do not label it PASS.

If all software/default tests pass but hardware is unavailable, determine whether project policy permits Phase 5 closure without hardware evidence.

For this project, prefer actual live validation before final sign-off when the hardware is available.

---

# 42. Audit — Remaining Issues

For a final successful closure:

```markdown
## 18. Remaining Issues

None.
```

Future Phase 6 work is not a Phase 5 issue.

You may add:

```text
Future work belongs to Phase 6 and is outside this audit.
```

But do not list unresolved Phase 5 defects under future work and still claim completion.

---

# 43. Completion Checklist

The audit must include:

```markdown
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
```

Use `[ ]` for anything not proven.

If any mandatory item remains `[ ]`, do not write `PHASE 5: COMPLETE`.

---

# 44. Required Final Sign-Off Language

If every gate is proven:

```markdown
## 20. Final Sign-Off

PHASE 5: COMPLETE

The Auralis Phase 5 Audio Routing Engine is implemented, regression-tested,
ownership-safe, rollback-safe, graph-churn-aware, thread-safe under the
defined PipeWire locking model, and verified not to leave stale Auralis-owned
links after route deactivation.

Phase 5 is formally locked.

The repository is ready to begin Phase 6.
```

If not:

```markdown
## 20. Final Sign-Off

PHASE 5: NOT COMPLETE

The following mandatory closure items remain unresolved:

- ...
```

---

# 45. Required IDE Final Summary

At the end of the Cursor task, print:

```text
AURALIS PHASE 5 FINAL CLOSURE PASS

New regression tests:
- owned link error handling: PASS/FAIL
- remove while activating: PASS/FAIL
- deactivate while activating: PASS/FAIL
- stale timeout generation: PASS/FAIL
- route removal timeout invalidation: PASS/FAIL
- shutdown timer invalidation: PASS/FAIL

Clean configure: PASS/FAIL
Clean build: PASS/FAIL
Full default ctest: PASS/FAIL
Focused Phase 5 tests: PASS/FAIL
Live PipeWire routing: PASS/FAIL/NOT RUN
Exact Bluetooth routing: PASS/FAIL/NOT RUN
Desktop smoke test: PASS/FAIL/NOT RUN

Final closure audit:
docs/PHASE_5_FINAL_CLOSURE_AUDIT.md

FINAL VERDICT:
PHASE 5: COMPLETE / NOT COMPLETE
```

---

# 46. Stop Condition

Do not stop because:

- the new tests compile;
- one test passes;
- default `ctest` is green but live test was never considered;
- the application launches once;
- README says Phase 5 implemented.

Stop only when the full closure process has been completed and the audit document exists.

---

# 47. Final Principle

The implementation has already crossed the hard architectural threshold.

This final pass exists to convert:

```text
"the code appears correct"
```

into:

```text
"the code is covered by explicit regression tests,
the complete project builds from clean state,
the whole regression suite passes,
the real graph cleanup is validated,
and the result is documented."
```

Do not broaden the scope.

Do not start Phase 6.

Finish Phase 5, prove it, document it, and lock it.
