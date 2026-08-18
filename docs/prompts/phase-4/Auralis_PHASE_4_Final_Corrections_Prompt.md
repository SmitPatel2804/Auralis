# Auralis — PHASE 4 Final Corrections Prompt
## Restore Phase 3 Live Test Registration + Fix PipeWire Startup Failure Semantics

Use this document as the implementation prompt inside the AI coding IDE.

---

# 0. ROLE

You are working inside the existing **Auralis** C++/Qt/QML repository on Linux.

Phases 0, 1, 2, and 3 are already complete and working.

Phase 4 is substantially implemented. Do **not** redesign Phase 4 and do **not** reimplement the PipeWire subsystem.

Your job is to make the small remaining corrections identified by the Phase 4 audit, preserve all completed work, rebuild from a clean state, and prove that the repository still passes the full regression suite.

Target:

```text
PHASE 4: READY FOR FINAL AUDIT
```

Do not claim `PHASE 4: COMPLETE` unless every acceptance criterion in this prompt passes.

---

# 1. KNOWN REMAINING ISSUES

Two concrete issues were identified.

## ISSUE 1 — Phase 3 BlueZ live integration test is not registered in CTest

The repository contains:

```text
tests/integration/tst_BlueZLiveIntegration.cpp
```

and existing Phase 3 validation expects:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" \
ctest --test-dir build \
  -R '^tst_BlueZLiveIntegration$' \
  --output-on-failure
```

However, the previous audit found that `tests/integration/CMakeLists.txt` no longer registers `tst_BlueZLiveIntegration`.

This is a regression. Phase 4 must preserve the Phase 0–3 regression suite.

### Required correction

Inspect:

```text
tests/integration/CMakeLists.txt
tests/integration/tst_BlueZLiveIntegration.cpp
```

Also inspect:

- the project's test helper CMake functions;
- neighboring integration-test registrations;
- target dependencies required by the test.

Restore the BlueZ live integration test to CMake/CTest using the repository's established conventions.

A likely shape may resemble:

```cmake
auralis_add_test(
    tst_BlueZLiveIntegration
    tst_BlueZLiveIntegration.cpp
)

target_link_libraries(
    tst_BlueZLiveIntegration
    PRIVATE
        auralis-bluetooth
)
```

Do **not** blindly paste this. Use the actual target/helper names in the repository.

After configuring the project:

```bash
ctest --test-dir build -N | grep -F tst_BlueZLiveIntegration
```

must find the test.

The live test should stay environment-gated at runtime. It must not be removed from CTest merely because it needs real Bluetooth hardware.

---

## ISSUE 2 — PipeWire startup failure paths report success

Inspect:

```text
src/audio/PipeWireConnection.cpp
```

The previous audit found immediate startup-failure branches around native PipeWire setup, including paths involving operations such as:

```text
pw_context_connect(...)
pw_core_get_registry(...)
```

At least some failure paths emit/log an error, clean up resources, and then return:

```cpp
return true;
```

That gives inconsistent behavior:

```text
PipeWire startup failed
resources cleaned up
error emitted
start() reports success
```

### Required correction

Audit **every return path** in the PipeWire startup function.

Establish and enforce this contract:

```text
start() returns true:
    synchronous initialization succeeded and the PipeWire subsystem
    was successfully launched/prepared for normal asynchronous operation.

start() returns false:
    a synchronous fatal initialization failure prevented startup.
```

Review at minimum:

```text
thread-loop creation
context creation
core connection
registry acquisition
registry/listener initialization
thread-loop start
other synchronous fatal setup
```

Do not mechanically change unrelated asynchronous runtime handling.

A later runtime error should continue to use the existing state/error signal path.

If a failure branch destroys the resources required for normal operation, it must not report successful startup.

---

# 2. DO NOT REIMPLEMENT PHASE 4

The current repository already contains the main Phase 4 architecture.

Do not replace or redesign:

```text
PipeWireConnection
PipeWireManager
PipeWire object store
AudioEndpoint
AudioEndpointRegistry
EndpointResolver
Qt/QML endpoint exposure
Phase 4 unit tests
PipeWire live integration tests
```

Make the smallest correct changes.

---

# 3. DO NOT IMPLEMENT PHASE 5

Do not add:

```text
PipeWire link creation
pw_stream routing
audio duplication
source selection
multi-endpoint playback
group routing
latency compensation
session routing
route creation UI
```

Phase 5 remains future work.

---

# 4. FIRST: AUDIT THE CURRENT REPOSITORY

Before editing:

1. Inspect Git status.
2. Inspect `tests/integration/CMakeLists.txt`.
3. Inspect `tst_BlueZLiveIntegration.cpp`.
4. Inspect test helper CMake functions/macros.
5. Inspect `src/audio/PipeWireConnection.cpp`.
6. Identify every startup return path.
7. Inspect PipeWire connection/manager tests.
8. Inspect README/docs for live-test commands.
9. Record the baseline test list with `ctest -N` after configuration.

Do not invent file names or target names if the actual repository differs.

---

# 5. RESTORE BLUEZ LIVE TEST REGISTRATION

The final CTest tree must contain the existing Phase 3 live regression test.

Verify:

```bash
ctest --test-dir build -N
```

and specifically:

```bash
ctest --test-dir build -N | grep -F tst_BlueZLiveIntegration
```

Also verify the Phase 4 live PipeWire test remains registered:

```bash
ctest --test-dir build -N | grep -F tst_PipeWireLiveIntegration
```

Use exact actual test names if the project names differ.

Do not disable one test while restoring the other.

---

# 6. PRESERVE LIVE TEST ENVIRONMENT GATING

Real Bluetooth hardware tests may require:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

Real PipeWire tests may require:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1
```

Use the actual test source as authority.

If environment variables are absent, a hardware-dependent test may report SKIP according to the existing convention.

If the opt-in variable is explicitly enabled, the test must actually attempt validation rather than silently skipping.

---

# 7. CORRECT SHELL EXAMPLES

Never document:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

because angle brackets are shell redirection syntax.

Use:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

---

# 8. PIPEWIRE START FAILURE SEMANTICS

After correction, startup semantics must be internally consistent.

Example:

```cpp
if (!context) {
    reportError(...);
    cleanup();
    return false;
}
```

and similarly for immediate fatal core/registry/start failures.

Do not change a healthy asynchronous design into a blocking startup sequence.

Do not duplicate error reporting.

Do not return failure merely because initial graph enumeration has not yet completed if enumeration is intentionally asynchronous.

---

# 9. TEST FAILURE SEMANTICS WHERE PRACTICAL

If the existing architecture makes failure injection straightforward, add/adapt tests for:

```text
synchronous connection/setup failure -> start() false
error state/message emitted consistently
cleanup remains safe
repeated stop/shutdown remains safe
```

Do not introduce a large mock framework solely for this correction.

If low-level PipeWire failure injection is impractical, document the limitation and validate through source-level review plus existing manager tests.

---

# 10. CLEAN BUILD

After modifications:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build
```

Do not accept an incremental-only build.

---

# 11. CTEST ENUMERATION

Run:

```bash
ctest --test-dir build -N
```

Confirm Phase 0–4 tests are still registered.

Explicitly verify:

```text
tst_BlueZLiveIntegration
tst_PipeWireLiveIntegration
```

or their actual equivalent names.

A source file existing without a registered CTest target is not sufficient.

---

# 12. FULL HARDWARE-INDEPENDENT SUITE

Run:

```bash
ctest --test-dir build --output-on-failure
```

Requirements:

```text
all normal tests pass
no Phase 0–3 test silently disappears
no Phase 4 unit test is disabled
hardware-dependent tests skip only through their intended opt-in gate
```

Do not weaken tests to get green output.

---

# 13. PHASE 3 LIVE BLUEZ REGRESSION TEST

With a real test device available, use its actual address:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" \
ctest --test-dir build \
  -R '^tst_BlueZLiveIntegration$' \
  --output-on-failure
```

If the actual test uses different environment variables, follow its source.

Expected:

```text
test is registered
test executes
test passes, or reports a genuine actionable environment/device failure
```

Do not mark unavailable hardware as PASS.

---

# 14. PHASE 4 LIVE PIPEWIRE TEST

Run:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build \
  -R '^tst_PipeWireLiveIntegration$' \
  --output-on-failure
```

Expected minimum:

```text
native PipeWire connection succeeds
registry populates
audio endpoint(s) appear
clean shutdown
```

If the live test supports Bluetooth mapping:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" \
ctest --test-dir build \
  -R '^tst_PipeWireLiveIntegration$' \
  --output-on-failure
```

Use the actual source code as authority.

---

# 15. DESKTOP SMOKE TEST

Run:

```bash
./build/apps/desktop/auralis-desktop
```

or the actual built executable path.

Verify:

```text
application launches
Phase 2/3 Bluetooth UI still works
PipeWire status is healthy
Audio Endpoints view/model appears
built-in endpoints appear
no immediate crash
no repeated error storm
```

With the test Bluetooth audio device:

```text
connect via existing Auralis UI
Bluetooth Connected=true
PipeWire endpoint appears
endpoint maps to correct Phase 3 device
disconnect removes/unavailable-marks endpoint
reconnect remaps correctly
```

No Phase 5 routing action is required.

---

# 16. REGRESSION CHECK

Verify at minimum:

```text
Phase 1 foundation/core tests                  PASS
Phase 2 discovery tests                       PASS
Phase 3 device-management tests               PASS
Phase 3 BlueZ live test registered            PASS
Phase 4 PipeWire/property tests               PASS
Phase 4 endpoint classifier tests             PASS
Phase 4 endpoint registry tests               PASS
Phase 4 EndpointResolver tests                PASS
Phase 4 object-store/event-order tests        PASS
Phase 4 PipeWire live test registered         PASS
```

Do not declare readiness if a previously existing test is missing from CTest.

---

# 17. PHASE 5 BOUNDARY CHECK

Search:

```bash
grep -RIn   "pw_link\|pw_filter\|pw_stream\|create.*link\|route.*audio"   include src apps tests || true
```

Interpret matches manually.

Comments, type declarations, and future stubs are not automatically violations.

Confirm that Phase 4 does **not** actively create user-selected audio routes.

---

# 18. TEST-CHEATING PROHIBITIONS

Do not:

```text
disable tst_BlueZLiveIntegration
rename it to avoid expected commands
mark failing tests WILL_FAIL
make live tests always skip
weaken assertions
remove regression tests
ignore failed command exit codes
fake endpoint data in production
```

---

# 19. DOCUMENTATION UPDATE

Only update documentation where needed to match actual test registration/commands.

Verify live-test examples use:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"
```

and that Phase 4 is still described as endpoint integration, while Phase 5 remains routing.

---

# 20. REQUIRED FINAL REPORT

Return a structured implementation report with:

## A. Repository audit
- relevant files inspected;
- current test architecture;
- startup contract found.

## B. Files changed
For each:

```text
path
reason
key change
```

## C. BlueZ live-test restoration
Show:
- CMake registration;
- `ctest -N` evidence.

## D. PipeWire startup correction
List:
- every corrected failure branch;
- previous behavior;
- new behavior.

## E. Clean build
Show results of:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## F. Full CTest
Show:
- total;
- passed;
- failed;
- skipped;
- unexpected omissions.

## G. Live validation
Report separately:

```text
BlueZ live
PipeWire live
Bluetooth↔PipeWire endpoint mapping
desktop smoke test
disconnect/reconnect
```

If hardware is unavailable, say `NOT EXECUTED` with the reason.

## H. Remaining issues
List only real remaining Phase 4 issues.

## I. Verdict
Return exactly one:

```text
PHASE 4 CORRECTIONS: COMPLETE
READY FOR FINAL PHASE 4 AUDIT
```

or:

```text
PHASE 4 CORRECTIONS: INCOMPLETE
```

---

# 21. ACCEPTANCE CRITERIA

All required:

- [ ] `tst_BlueZLiveIntegration.cpp` remains in repository.
- [ ] `tst_BlueZLiveIntegration` is registered with CTest.
- [ ] Phase 3 live-test gating remains valid.
- [ ] `tst_PipeWireLiveIntegration` remains registered.
- [ ] Synchronous fatal PipeWire startup failures return failure.
- [ ] Asynchronous runtime failures retain state/error handling.
- [ ] Clean configure passes.
- [ ] Clean build passes.
- [ ] Normal CTest suite passes.
- [ ] Phase 0–3 tests remain registered.
- [ ] Phase 4 tests remain registered.
- [ ] Real BlueZ live validation is run when hardware is available.
- [ ] Real PipeWire live validation is run.
- [ ] Bluetooth endpoint mapping is verified when hardware is available.
- [ ] Desktop app launches.
- [ ] No Phase 5 routing implementation is introduced.

---

# 22. FINAL INSTRUCTION

Make the **smallest correct corrections**.

Do not rewrite working Phase 4 code.

Restore the dropped Phase 3 regression gate.

Correct the PipeWire startup-return contract.

Build from clean state.

Run the complete regression suite.

Run the real live tests on the Linux machine.

Leave the repository ready for an independent final Phase 4 audit.
