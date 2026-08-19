# Auralis — Phase 6 Independent Rigorous Test, Stress, Race & Final Audit Prompt
## Adversarial Verification of the Multi-Device Session Engine After Final Fixes

**Project:** Auralis  
**Purpose:** Independently prove or disprove that Phase 6 is truly complete  
**When to run:** Only after the final Phase 6 correction prompt has been implemented  
**Role:** Independent verification agent / QA engineer / adversarial code auditor  
**Rule:** Do not trust previous implementation reports, comments, documentation, or self-reported pass counts without reproducing them.

---

# 1. Mission

Perform a rigorous independent validation of the complete Phase 6 Multi-Device Session Engine.

You are not primarily here to write features.

You are here to:

```text
inspect
build
test
stress
break
verify
audit
report
```

The implementation should survive:

- normal operation;
- partial failure;
- repeated events;
- source loss;
- endpoint churn;
- Bluetooth reconnect exhaustion;
- policy changes;
- rapid user operations;
- synchronous callbacks;
- stale asynchronous callbacks;
- persistence failures;
- corrupted persistence;
- restart;
- application shutdown;
- regression testing of Phases 0–5.

A green happy-path test suite alone is not sufficient.

---

# 2. Verification Philosophy

Assume bugs may exist in:

- state transitions;
- event ordering;
- duplicate signals;
- retry ownership;
- cancellation;
- persistence;
- route ownership;
- callbacks;
- QObject lifetime;
- model synchronization;
- stale errors;
- test doubles themselves.

Try to falsify Phase 6.

Do not merely confirm expected behavior.

---

# 3. Do Not Trust Existing Audit Claims

Repository files such as:

```text
docs/validation/phase-6-*.md
```

are context only.

Do not accept:

```text
34/34 passed
Phase 6 complete
```

until you run the tests yourself.

If raw prior logs are absent, state that.

---

# 4. Environment Audit

Record:

```bash
uname -a
lsb_release -a 2>/dev/null || true

gcc --version | head -1
g++ --version | head -1
cmake --version | head -1
ninja --version
qmake6 --version 2>/dev/null || true

pkg-config --modversion Qt6Core 2>/dev/null || true
pkg-config --modversion Qt6Bluetooth 2>/dev/null || true
pkg-config --modversion Qt6Multimedia 2>/dev/null || true
pkg-config --modversion libpipewire-0.3 2>/dev/null || true
pkg-config --modversion dbus-1 2>/dev/null || true
bluetoothctl --version 2>/dev/null || true
```

Record whether:

- Qt development packages are present;
- BlueZ is present;
- PipeWire is running;
- a graphical session exists;
- hardware is available.

---

# 5. Repository Integrity Audit

Run:

```bash
git status --short
git branch --show-current
git log --oneline -n 30
git diff --stat
```

Check for:

- generated files accidentally committed;
- build directories;
- temporary logs;
- disabled tests;
- commented-out assertions;
- `QSKIP` misuse;
- `EXPECT_TRUE(true)`-style fake coverage.

Search:

```bash
grep -R "QSKIP\|SKIP\|TODO\|FIXME\|HACK\|XXX" -n tests src include docs 2>/dev/null
```

Review every Phase 6 skip.

---

# 6. Clean Build

Do not rely on an existing build directory.

```bash
rm -rf build-rigorous

cmake -S . -B build-rigorous -G Ninja
cmake --build build-rigorous
```

Fail the audit if:

- clean configure fails due to repository issue;
- clean compile fails;
- new warnings indicate Phase 6 defects.

Environment-missing dependencies are not repository defects, but must be reported.

---

# 7. Full Test Suite

Run:

```bash
ctest --test-dir build-rigorous --output-on-failure
```

Then:

```bash
ctest --test-dir build-rigorous -N
```

Record:

```text
total tests
passed
failed
skipped
```

Do not rely only on CTest summary.

Inspect Phase 6 test names.

---

# 8. Repeat the Full Test Suite

Race-sensitive bugs can pass once.

Run the full suite multiple times:

```bash
for i in $(seq 1 20); do
  echo "===== RUN $i ====="
  ctest --test-dir build-rigorous --output-on-failure || exit 1
done
```

If total runtime is excessive, run Phase 6 tests 50–100 times instead.

Example:

```bash
for i in $(seq 1 100); do
  ctest --test-dir build-rigorous \
    -R "Session|session" \
    --output-on-failure || exit 1
done
```

Any flaky failure is a Phase 6 issue.

---

# 9. Parallel Test Execution

If tests are isolation-safe:

```bash
ctest --test-dir build-rigorous \
  -j "$(nproc)" \
  --output-on-failure
```

Parallel execution can expose shared-state bugs.

Compare with serial execution.

---

# 10. Targeted Phase 6 Inventory

Locate all Phase 6 tests.

```bash
ctest --test-dir build-rigorous -N | grep -i session
```

Map tests to requirements:

| Requirement | Test |
|---|---|
| CRUD | |
| State machine | |
| Routing | |
| Volume | |
| Persistence | |
| Recovery None | |
| RestoreRoutesOnly | |
| ReconnectAndRestore | |
| Exhaustion | |
| Source loss | |
| Reentrancy | |
| Generation race | |
| Restart | |

Any blank mandatory row is a coverage gap.

---

# 11. State Machine Exhaustive Audit

Review `SessionStateMachine`.

Verify states:

```text
IDLE
STARTING
ACTIVE
DEGRADED
RECOVERING
STOPPING
FAILED
```

Build a transition matrix from code and compare it against tests.

Test every meaningful edge.

Forbidden examples:

```text
IDLE -> ACTIVE without STARTING
FAILED -> ACTIVE without explicit retry
STOPPING -> RECOVERING
STOPPING -> ACTIVE
IDLE with active routes
FAILED with newly-created active routes
```

---

# 12. Invariant Testing

Create or verify assertions for:

```text
IDLE => zero session-owned active routes
STOPPING => no route creation
FAILED => no automatic recovery
removed member => zero owned routes
disabled member => zero owned routes
ACTIVE => all required enabled members healthy
DEGRADED => at least one required member unhealthy
```

If production code does not expose enough observability, inspect internal fake/service call logs.

---

# 13. Managed Reconnect De-duplication Test

This is mandatory.

Use the production event chain:

```text
Fake BlueZ
BluetoothManager
DeviceLifecycleManager
ReconnectPolicy
DeviceRegistry
SessionManager
```

Simulate one:

```text
Connected true -> false
```

Assert:

```text
ReconnectPolicy attempt increments once
one pending retry timer
one retry schedule
```

Then emit duplicate disconnect notifications.

Assert retry state does not double-increment.

---

# 14. Reconnect Exhaustion Test

Configure reconnect policy:

```text
maxAttempts = small deterministic value
```

Example:

```text
2
```

Simulate all attempts failing.

Assert:

```text
ReconnectPolicy emits exhaustion
BluetoothManager propagates exhaustion
SessionManager clears recovery request
member.recovering = false
session leaves RECOVERING
sessionError = RecoveryExhausted
```

If another member is healthy:

```text
DEGRADED
```

If no member is healthy and no recovery remains:

```text
FAILED
```

---

# 15. RecoveryPolicy::None

Test both failure shapes.

## A. Endpoint removed

```text
ACTIVE
policy None
remove member endpoint
return endpoint
```

Assert:

```text
no automatic new route
```

## B. Existing route becomes inactive

Do not remove endpoint.

Force:

```text
RouteState::Inactive
```

or:

```text
Failed
```

Assert:

```text
no automatic activateRoute call
```

Only explicit retry/start may restore.

---

# 16. RestoreRoutesOnly

Test:

```text
ACTIVE
member disconnects
```

Assert:

```text
no Bluetooth reconnect request
```

Then externally restore connection/endpoint.

Assert:

```text
route restored
volume restored
mute restored
ACTIVE
```

---

# 17. ReconnectAndRestore

Test:

```text
ACTIVE
member disconnects
```

Assert:

```text
managed reconnect requested
lower layer owns retries
no session fixed retry timer
```

After reconnect:

```text
new runtime endpoint ID
route recreated
state ACTIVE
```

---

# 18. New Endpoint ID Recovery

Explicitly test:

```text
old endpoint ID = 42
disconnect
new endpoint ID = 97
```

Assert:

```text
route targets 97
no stale 42 reference remains
```

Search session runtime metadata and route ownership maps.

---

# 19. Healthy Peer Preservation

Two-device session:

```text
A
B
```

Activate both.

Remove B.

Assert A:

```text
route stays active
route ID unchanged
no unnecessary route recreation
audio path logically preserved
```

This is a core Phase 6 requirement.

---

# 20. Mid-Session Source Loss

Start:

```text
ACTIVE
```

Remove source.

Verify documented state:

```text
RECOVERING
```

or defined terminal state.

Then restore source with a different runtime node ID.

Assert exact intended behavior.

If recoverable:

```text
routes restored
ACTIVE
```

If terminal:

```text
no auto route creation
explicit retry required
```

---

# 21. Start With Missing Source

Test:

```text
source configured but unavailable
activate
```

Expected:

```text
FAILED
```

or documented state.

Then source appears.

Assert no contradiction such as:

```text
FAILED + active routes
```

Explicit retry must work.

---

# 22. Stop During Recovery

Test:

```text
member lost
RECOVERING
stop
```

Assert:

```text
STOPPING
reconnect cancelled
route recovery cancelled
all routes removed
IDLE
```

Then deliver late:

```text
deviceConnected
endpointAdded
route completion
reconnect completion
```

Assert no resurrection.

---

# 23. Rapid Start-Stop-Start

Repeat:

```text
start
stop
start
stop
start
```

with delayed fake callbacks from previous generations.

Assert only the latest generation may mutate state.

Run this 1000 iterations if test runtime permits.

---

# 24. Stale Generation Test

Explicitly capture:

```text
generation N
```

Schedule recovery.

Move session to:

```text
generation N+2
```

Fire old callback.

Assert:

```text
ignored
```

No:

- reconnect;
- route;
- state change;
- error overwrite.

---

# 25. Stop Reentrancy Test

This is mandatory.

Use a router double that emits synchronously inside:

```cpp
deactivateRoute()
```

Signals:

```text
Deactivating
Inactive
routeRemoved
```

Then stop.

Assert:

```text
no recursive stop
no route recreation
no duplicate remove
IDLE
```

Run under ASan/UBSan.

---

# 26. Member Disable During Recovery

```text
B recovering
set enabled=false
```

Then:

```text
B reconnects
endpoint appears
```

Assert:

```text
no route
no reconnect
not recovering
```

---

# 27. Member Removal During Recovery

Same pattern.

Assert:

```text
no stale callback references removed member
no route restored
no crash
```

---

# 28. Policy Change During Recovery

## To None

```text
recovering
set policy None
```

Assert lower reconnect cancellation.

## To RestoreRoutesOnly

Assert:

```text
Bluetooth reconnect cancelled
route-return monitoring still allowed
```

## To ReconnectAndRestore

Assert managed reconnect may begin once.

---

# 29. AutoReconnect Toggle

```text
recovering
set autoReconnect=false
```

Assert:

```text
lower managed reconnect cancelled
```

If route-only restoration policy still allows endpoint restoration, verify that distinction.

---

# 30. Multi-Session Race

If single active session policy:

```text
Session A ACTIVE
A enters RECOVERING
activate Session B
```

After B becomes active, deliver late A callbacks.

Assert:

```text
A cannot recreate route
A cannot reconnect on behalf of active session
B remains authoritative
```

---

# 31. Session Delete Race

```text
session recovering
delete session
```

Then fire all pending callbacks.

Assert:

```text
no use-after-free
no signal referencing nonexistent session
no route recreation
```

Run sanitizer.

---

# 32. Persistence First-Run Test

Use nonexistent nested directory.

Assert:

```text
save creates directory
file exists
load succeeds
```

---

# 33. Persistence Failure Tests

Test failures at:

```text
directory creation
file open
write
commit
```

where practical.

Assert:

```text
public operation reports PersistenceFailure
sessionError emitted
```

Verify documented in-memory rollback/dirty semantics.

---

# 34. createSession Failure

Specifically fail persistence during create.

Assert:

```text
create API does not pretend full success
```

If rollback model:

```text
session absent
```

If dirty-state model:

```text
explicit failure result
session marked dirty
```

---

# 35. Corrupted JSON

Test:

```text
empty
truncated
invalid syntax
wrong root type
```

No crash.

Verify log/error.

---

# 36. Schema Validation

Test:

```text
supported current version
older supported version
future unknown version
```

If migration exists, verify output.

Future unknown version must fail safely.

---

# 37. Duplicate Session IDs

Persistence file contains two sessions with same ID.

Verify documented behavior:

```text
skip duplicate
or reject file
```

No silent overwrite unless explicitly intended.

---

# 38. Duplicate Member IDs

Same device twice in one persisted session.

Verify safe normalization or rejection.

---

# 39. Stable Source Restoration

Persist logical source.

Restart simulation:

```text
old runtime node ID != new runtime node ID
```

Assert source resolves by stable descriptor and routes can activate.

---

# 40. Stable Device Restoration

Persist stable Bluetooth device identity.

Simulate BlueZ object path changing after restart.

Assert member still resolves through stable identity if architecture supports that.

---

# 41. Runtime IDs Not Persisted As Authority

Inspect persistence file.

Ensure it does not treat:

```text
PipeWire object ID
PipeWire link ID
transient BlueZ object path
```

as the sole persistent identity.

---

# 42. Volume Input Validation

Test:

```text
NaN
+Inf
-Inf
```

must be rejected.

For finite out-of-range:

```text
-0.5
1.5
```

verify documented clamp/reject semantics.

---

# 43. Group Volume Recovery

Set:

```text
group = 0.37
```

Disconnect B.

Change:

```text
group = 0.68
```

Reconnect B.

Assert B gets current:

```text
0.68
```

not stale `0.37`.

---

# 44. Per-Device Trim Recovery

Example:

```text
group 0.8
B trim 0.5
```

Reconnect B.

Assert effective volume matches expected semantics.

---

# 45. Mute Recovery

Mute B while absent.

Reconnect.

Assert B returns muted.

Then unmute group/member as appropriate.

---

# 46. Error Clearing

Force B route failure.

Assert:

```text
member lastError set
sessionError emitted
```

Recover B.

Assert stale recoverable error is cleared.

---

# 47. lastUsedAt

Test:

```text
failed activation
```

does not update last-used timestamp.

Successful:

```text
ACTIVE
```

does update.

If degraded activation counts as use, verify documented rule.

---

# 48. Event Idempotency

Repeat identical events:

```text
deviceUpdated
endpointAdded
endpointRemoved
routeStateChanged
```

multiple times.

Assert:

- no duplicate routes;
- no duplicate reconnect scheduling;
- no incorrect attempt count;
- no state oscillation.

---

# 49. Endpoint Churn Stress

Loop:

```text
add endpoint
remove endpoint
add new endpoint ID
remove
```

hundreds of times while session is recovering.

Assert no leak/crash/stale mapping.

---

# 50. Bluetooth Churn Stress

Loop device:

```text
connected
disconnected
connected
disconnected
```

with deterministic fake timing.

Verify:

- retry ownership;
- cancellation;
- attempt reset semantics;
- state convergence.

---

# 51. Route Churn Stress

Repeatedly force route:

```text
Active
Failed
Inactive
Active
```

under each recovery policy.

Verify policy correctness.

---

# 52. Session CRUD Stress

Create and delete many sessions.

Example:

```text
1000 sessions
```

Add/remove devices.

Save/reload.

Check:

- unique IDs;
- no leaks;
- deterministic persistence;
- no stale route ownership.

---

# 53. Shutdown Audit

Start active/recovering session.

Trigger application shutdown.

Assert order:

```text
SessionManager stops recovery
routes cleaned
persistence flushed
then router/endpoint/Bluetooth services destroyed
```

No callback reaches destroyed dependency.

---

# 54. QObject Lifetime Audit

Inspect all Phase 6 signal connections.

Check:

- context objects supplied to lambdas;
- parent ownership;
- queued connections;
- disconnected timers;
- deleteLater behavior.

Any lambda capturing raw `this` without QObject context deserves scrutiny.

---

# 55. Container/Reference Safety

Search code manipulating route/member/session collections while signals may fire synchronously.

Look for:

```text
QVector erase/remove
std::vector erase
QHash removal
```

while holding references/iterators.

Reentrancy can invalidate them.

Run sanitizer tests around these paths.

---

# 56. AddressSanitizer

If supported:

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"

cmake --build build-asan

ASAN_OPTIONS=detect_leaks=1 \
ctest --test-dir build-asan --output-on-failure
```

Adapt to project toolchain.

Do not permanently modify production flags merely for audit.

---

# 57. UndefinedBehaviorSanitizer

If supported:

```bash
cmake -S . -B build-ubsan -G Ninja \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"

cmake --build build-ubsan

ctest --test-dir build-ubsan --output-on-failure
```

Pay special attention to:

- const-cast;
- invalid references;
- enum conversion;
- integer overflow;
- lifetime.

---

# 58. ThreadSanitizer

Only if project/test environment supports it.

Use cautiously because Qt/DBus/PipeWire can generate noise.

If used, report suppressions explicitly.

---

# 59. Valgrind

If available and meaningful:

```bash
valgrind --leak-check=full ...
```

Use on focused session tests rather than the entire desktop stack if needed.

---

# 60. Compiler Warnings

Build with configured warnings.

If feasible, temporarily test stronger warnings:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wshadow
```

Do not make permanent unrelated warning-policy changes.

---

# 61. Static Search for Dangerous Patterns

Search:

```bash
grep -R "const_cast" -n src/session include/auralis/session
grep -R "reinterpret_cast" -n src/session include/auralis/session
grep -R "\.data()" -n src/session include/auralis/session
grep -R "new QTimer" -n src/session include/auralis/session
grep -R "singleShot" -n src/session include/auralis/session
```

Inspect each occurrence.

---

# 62. Search for Production Shell Commands

Phase 6 must not shell out for production routing/recovery.

Search:

```bash
grep -R "bluetoothctl\|wpctl\|pactl\|pw-link\|pw-cli" -n src include apps
```

Diagnostic scripts/docs are acceptable.

Production C++ calls are not.

---

# 63. Search for Parallel Subsystems

Look for accidental duplicates:

```text
another SessionManager
another routing engine
another reconnect scheduler
another persistence store
```

Phase 6 must reuse existing subsystem boundaries.

---

# 64. Full Desktop Launch

If graphical environment allows:

```bash
./build-rigorous/apps/desktop/auralis-desktop
```

Observe:

- startup;
- session loading;
- no warnings/crash;
- shutdown.

If no GUI environment, report unavailable.

---

# 65. Manual Two-Device Validation

If two compatible Bluetooth audio devices are available:

1. connect A;
2. connect B;
3. verify endpoints;
4. create session;
5. add both;
6. select source;
7. activate;
8. verify both receive audio;
9. change group volume;
10. disconnect B;
11. confirm A remains;
12. confirm session degraded/recovering;
13. reconnect B;
14. confirm route restoration;
15. stop;
16. confirm clean teardown.

Record actual device identities carefully but avoid publishing private data unnecessarily.

---

# 66. Manual Recovery Exhaustion

If feasible, configure a device that cannot reconnect.

Observe:

```text
attempts
backoff
exhaustion
session transition
```

Verify no infinite RECOVERING.

---

# 67. Manual Restart Persistence

Create session.

Exit.

Restart.

Verify:

- session restored;
- runtime IDs freshly resolved;
- no stale route IDs;
- activation works.

---

# 68. Live Hardware Test Must Be Honest

If hardware is unavailable:

```text
LIVE HARDWARE VALIDATION: NOT RUN
```

Do not mark PASS.

If one device only:

```text
MULTI-DEVICE LIVE VALIDATION: NOT RUN
```

---

# 69. Test Independence Audit

Inspect fake implementations.

Ensure fakes are not written to mirror implementation bugs.

Examples of weak tests:

```text
fake SessionManager
fake state machine
```

when testing SessionManager.

Prefer real subject + fake external dependencies.

---

# 70. Assertions Quality Audit

Look for tests that only check:

```text
no crash
signal count >= 1
```

when exact semantics can be asserted.

Strengthen:

```text
exact state
exact reconnect count
exact route count
exact route target
exact error
```

---

# 71. Timing Determinism

Avoid tests dependent on arbitrary long sleeps.

Prefer:

- fake timers;
- Qt event-loop pumping;
- short bounded signal waits;
- deterministic test hooks.

Flag flaky timing-based tests.

---

# 72. Final Requirements Matrix

Build a final matrix:

| Phase 6 Requirement | Code | Unit Test | Integration Test | Live Test |
|---|---|---|---|---|
| Multi-device session | | | | |
| Group volume | | | | |
| Degraded operation | | | | |
| Reconnect policy None | | | | |
| RestoreRoutesOnly | | | | |
| ReconnectAndRestore | | | | |
| Reconnect exhaustion | | | | |
| Source loss | | | | |
| Member return/new endpoint | | | | |
| Persistence | | | | |
| Restart | | | | |
| Stop race | | | | |
| Generation race | | | | |

No mandatory software requirement should lack deterministic automated coverage.

---

# 73. Produce Raw Evidence

Save logs to:

```text
audit-phase6-final/
```

Suggested files:

```text
environment.txt
git-status.txt
configure.log
build.log
ctest-full.log
ctest-repeat.log
ctest-parallel.log
ctest-session-repeat.log
asan.log
ubsan.log
desktop-launch.log
hardware-test.log
requirements-matrix.md
```

Do not commit huge logs unless project policy wants them.

---

# 74. Final Independent Audit Document

Create:

```text
docs/validation/phase-6-independent-rigorous-audit.md
```

Sections:

1. Executive summary
2. Environment
3. Repository state
4. Build result
5. Full automated test result
6. Repeated/flaky test result
7. Recovery policy verification
8. Reconnect ownership verification
9. Reconnect exhaustion
10. State invariants
11. Route reentrancy
12. Source recovery
13. Persistence
14. Volume/mute
15. Race/stale callback tests
16. Sanitizer/static checks
17. Live hardware validation
18. Remaining findings
19. Final verdict

---

# 75. Severity Model

Classify findings:

```text
BLOCKER
MAJOR
MEDIUM
LOW
INFO
```

A Phase 6 `COMPLETE` verdict requires:

```text
0 BLOCKER
0 MAJOR
```

Medium/low findings may remain only if they do not violate Phase 6 acceptance criteria and are documented.

---

# 76. Final Verdict Rules

Use:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
```

only if:

- clean build passes;
- full tests pass;
- repeated tests show no flake;
- all mandatory recovery policies pass;
- exhaustion works;
- stop reentrancy is safe;
- no stale generation recovery;
- persistence works;
- no major sanitizer finding;
- Phase 0–5 regressions pass.

If official roadmap requires physical two-device proof and it was not run:

```text
PHASE 6 SOFTWARE STATUS: COMPLETE
PHASE 6 HARDWARE VALIDATION: NOT COMPLETE
PHASE 6 OVERALL STATUS: NOT COMPLETE
```

Otherwise, if software is sufficient for Phase gate:

```text
PHASE 6 STATUS: COMPLETE
```

Do not hide hardware limitations.

---

# 77. Required Final Console Report

At end, output:

```text
=== PHASE 6 INDEPENDENT RIGOROUS AUDIT ===

Clean build:
Full CTest:
Repeated CTest:
Parallel CTest:
Phase 6 focused repetition:
ASan:
UBSan:
Desktop launch:
Two-device hardware:
Reconnect exhaustion:
Stop reentrancy:
Persistence:
Source recovery:
Generation races:

BLOCKERS:
MAJOR:
MEDIUM:
LOW:

PHASE 6 STATUS:
```

Fill with actual results only.

---

# 78. Core Principle

The goal is not to make the test suite green.

The goal is to prove that the Multi-Device Session Engine remains correct under real asynchronous failure and recovery.

Try hard to break it.

If you cannot break it after all mandatory checks, document the evidence and mark Phase 6 complete.
