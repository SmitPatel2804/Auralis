# Auralis — PHASE 2 Final Snapshot Reconciliation Fix Prompt

**Project:** Auralis  
**Phase:** 2 — Bluetooth Device Discovery  
**Task type:** Final targeted correction  
**Scope:** One remaining snapshot reconciliation edge case only  
**Goal:** Ensure a refreshed BlueZ Adapter1 snapshot with `Discovering=false` correctly reconciles Auralis local discovery ownership/state.

---

# 0. IMPORTANT CONTEXT

Phase 2 is already substantially implemented and has already undergone remediation.

Do **not** rewrite the Bluetooth subsystem.

Do **not** redesign:

```text
BlueZDbusClient
AdapterManager
DiscoveryManager
DeviceRegistry
BluetoothDeviceListModel
BluetoothManager
QML
```

The current Phase 2 architecture is correct and must be preserved.

The following remediation work is already present and must remain intact:

```text
desiredScanning
ownsDiscovery
pending Start/Stop operation
serialized StartDiscovery / StopDiscovery
BlueZ generation protection
adapter-path tracking
stale error recovery
snapshotFailed propagation
system-bus error handling
parser warning diagnostics
deferred FakeBlueZClient operations
live BlueZ integration test
local scan ownership vs global Adapter1.Discovering
```

This prompt addresses **one small remaining reconciliation hole**.

---

# 1. THE REMAINING BUG

The application already processes real-time:

```text
org.freedesktop.DBus.Properties.PropertiesChanged
```

updates for:

```text
org.bluez.Adapter1.Discovering
```

and uses those updates to reconcile Auralis local discovery state.

However, after a full BlueZ snapshot refresh through:

```text
org.freedesktop.DBus.ObjectManager.GetManagedObjects
```

the current snapshot handler only forwards the selected adapter's discovery state when:

```text
Discovering == true
```

Conceptually, the current logic is equivalent to:

```cpp
discovery_->onSelectedAdapterChanged();

if (adapters_->hasAdapter() &&
    adapters_->selected().discovering) {

    discovery_->onAdapterDiscoveringPropertyChanged(true);
}
```

The problem is that:

```text
Discovering=false
```

from the authoritative refreshed snapshot is not forwarded.

---

# 2. WHY THIS MATTERS

This can leave Auralis local state stale in an edge case.

Example:

```text
Auralis starts discovery successfully
    |
    v
DiscoveryManager:
ownsDiscovery = true
scanning = true

BlueZ later stops global discovery
    |
    v
Adapter1 Discovering=false

For any reason the live PropertiesChanged(false) event
is missed or local state becomes stale

User presses Refresh
    |
    v
GetManagedObjects returns:
Adapter1 Discovering=false

AdapterManager correctly stores:
discovering=false

BUT DiscoveryManager is not told about false

Result:
Auralis may still report:
scanning=true

while:
adapterDiscovering=false
```

This violates the intended purpose of Refresh.

A fresh `GetManagedObjects` snapshot must be treated as authoritative reconciliation data.

---

# 3. REQUIRED CORRECTION

Locate the Phase 2 snapshot reconciliation code, expected in:

```text
src/bluetooth/BluetoothManager.cpp
```

inside or near:

```cpp
BluetoothManager::handleSnapshot(...)
```

After adapters are reconciled and the selected adapter is known, always pass the selected adapter's actual `Discovering` value to DiscoveryManager.

Change logic equivalent to:

```cpp
if (adapters_->hasAdapter() &&
    adapters_->selected().discovering) {

    discovery_->onAdapterDiscoveringPropertyChanged(true);
}
```

to logic equivalent to:

```cpp
if (adapters_->hasAdapter()) {
    discovery_->onAdapterDiscoveringPropertyChanged(
        adapters_->selected().discovering);
}
```

The exact syntax may differ based on the current classes.

The required semantic is:

```text
snapshot Discovering=true
    -> DiscoveryManager receives true

snapshot Discovering=false
    -> DiscoveryManager receives false
```

Do not special-case only `true`.

---

# 4. PRESERVE LOCAL VS GLOBAL DISCOVERY SEMANTICS

This correction must **not** collapse these two concepts:

```text
Auralis local discovery ownership
```

and:

```text
BlueZ global Adapter1.Discovering
```

They remain distinct.

The system must still support:

```text
adapterDiscovering = true
scanning = false
```

when another BlueZ client owns discovery.

Do not implement:

```cpp
scanning = adapter.discovering;
```

That would be wrong.

Instead, continue using the existing:

```text
DiscoveryManager::onAdapterDiscoveringPropertyChanged(...)
```

reconciliation logic.

The snapshot handler's responsibility is only to forward the authoritative refreshed property value.

---

# 5. EXPECTED FALSE RECONCILIATION BEHAVIOR

If Auralis currently believes:

```text
ownsDiscovery = true
scanning = true
```

and the refreshed BlueZ snapshot says:

```text
Adapter1 Discovering=false
```

then the existing DiscoveryManager reconciliation logic should transition Auralis out of stale ownership.

Expected result:

```text
ownsDiscovery = false
scanning = false
```

State should become an appropriate non-scanning state such as:

```text
Idle
```

or the existing designed recovery/error state.

Do not leave Auralis indefinitely claiming it is scanning.

---

# 6. EXPECTED TRUE RECONCILIATION BEHAVIOR

If the refreshed snapshot says:

```text
Adapter1 Discovering=true
```

continue forwarding:

```cpp
onAdapterDiscoveringPropertyChanged(true);
```

However, remember:

```text
global Discovering=true
```

does **not** automatically mean:

```text
Auralis owns discovery
```

If:

```text
ownsDiscovery=false
```

because another client is scanning, Auralis must continue reporting:

```text
scanning=false
adapterDiscovering=true
```

Preserve existing tests covering this distinction.

---

# 7. REQUIRED REGRESSION TEST

Add a dedicated test that fails against the old snapshot behavior.

The test should use the existing fake BlueZ infrastructure.

Suggested test name:

```text
snapshotDiscoveringFalseReconcilesLocalOwnership
```

or:

```text
refreshSnapshotDiscoveringFalseStopsStaleLocalScan
```

Use the naming convention already present in the repository.

---

# 8. REQUIRED TEST FLOW

Implement a test with behavior equivalent to:

## Step 1 — Initialize valid Bluetooth state

Provide:

```text
system bus connected
BlueZ available
powered adapter
Adapter1 Discovering=false
```

Verify:

```text
manager.canStartScan() == true
```

## Step 2 — Start Auralis discovery

Call:

```cpp
manager.startScan();
```

Complete the fake StartDiscovery successfully.

Verify:

```text
manager.scanning() == true
```

and local discovery ownership is active according to the public/test-visible state.

## Step 3 — Simulate authoritative refreshed snapshot

Without first sending:

```text
PropertiesChanged Discovering=false
```

send a new:

```text
GetManagedObjects / snapshotReceived
```

containing the selected Adapter1 with:

```text
Powered=true
Discovering=false
```

This is critical.

The test must prove that the **snapshot itself** reconciles the state.

## Step 4 — Verify reconciliation

Assert:

```cpp
QVERIFY(!manager.scanning());
```

Also verify, where appropriate:

```text
manager.adapterDiscovering() == false
```

and:

```text
manager.canStartScan() == true
```

provided prerequisites remain valid.

The application must no longer claim ownership of a scan that the authoritative snapshot says is not globally active.

---

# 9. OPTIONAL COMPLEMENTARY TEST

If not already covered, retain/add a complementary test for:

```text
snapshot Discovering=true
while Auralis ownsDiscovery=false
```

Expected:

```text
adapterDiscovering=true
scanning=false
```

This proves the fix does not accidentally redefine local scanning as the raw Adapter1 property.

If this behavior is already covered by an existing `PropertiesChanged` test, do not duplicate it unnecessarily.

---

# 10. REFRESH PATH VALIDATION

The production behavior being fixed is specifically relevant to:

```text
BluetoothManager::refresh()
```

or the equivalent snapshot request path.

Required semantics after correction:

```text
Refresh
    |
    v
request GetManagedObjects
    |
    v
snapshotReceived
    |
    +--> reconcile adapters
    +--> reconcile devices
    +--> forward selected Adapter1 Discovering
    |
    v
DiscoveryManager reconciles local state
```

Refresh must be able to repair stale local discovery state even if a live D-Bus property-change event was missed.

---

# 11. DO NOT INTRODUCE THESE REGRESSIONS

Do not:

```text
rewrite DiscoveryManager
remove desiredScanning
remove ownsDiscovery
remove pending-operation serialization
treat Adapter1.Discovering as local ownership
restart discovery automatically on every snapshot
call StartDiscovery from handleSnapshot()
call StopDiscovery directly from handleSnapshot()
reset the entire device model
change DeviceRegistry identity rules
change BlueZ InterfacesAdded handling
replace QtDBus
add bluetoothctl/QProcess fallbacks
```

This is a reconciliation input fix, not a Bluetooth architecture rewrite.

---

# 12. ERROR-STATE INTERACTION

When:

```text
snapshot Discovering=false
```

reconciles stale local ownership, preserve the existing DiscoveryManager error policy.

Do not invent a new fatal error simply because the snapshot says discovery is no longer active.

If the current DiscoveryManager intentionally surfaces an unexpected-discovery-stop error, preserve that behavior.

If it cleanly returns to:

```text
Idle
```

that is also acceptable if already established by design.

The key requirement is:

```text
scanning must not stay true
```

after authoritative snapshot reconciliation says otherwise.

---

# 13. TESTS THAT MUST CONTINUE TO PASS

Do not break existing Phase 2 tests for:

```text
Start -> immediate Stop
Start -> Stop -> Start
duplicate Start
Stop pending -> Start
Start failure
Stop failure
BlueZ restart / stale callback
NoAdapter recovery
AdapterPoweredOff recovery
BlueZUnavailable recovery
snapshotFailed recovery
NoSystemBus
other client scanning
Adapter1 Discovering=false PropertiesChanged reconciliation
parser warning propagation
DeviceRegistry deduplication
model row updates
live integration-test build
```

The new test supplements those; it does not replace them.

---

# 14. BUILD AND TEST PROCEDURE

After the code change:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

If CMake files were not changed, a full reconfigure is not strictly necessary during iteration.

Before final completion run:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Do not disable or skip existing unit tests to obtain a green build.

---

# 15. LIVE BLUEZ REGRESSION VALIDATION

After normal tests pass, run the existing opt-in Bluetooth integration test on the actual laptop:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
ctest --test-dir build \
-R tst_BlueZLiveIntegration \
--output-on-failure
```

If validating against a known discoverable test device, optionally use:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF \
ctest --test-dir build \
-R tst_BlueZLiveIntegration \
--output-on-failure
```

Replace the address with the actual test device address.

Do not include angle brackets around the value.

---

# 16. MANUAL REFRESH VALIDATION

Launch:

```bash
./build/apps/desktop/auralis-desktop
```

Perform:

```text
1. Verify Ready to scan.
2. Click Start Scan.
3. Verify Auralis shows scanning.
4. Allow real nearby device activity.
5. Click Refresh during normal operation.
6. Confirm UI state remains coherent.
7. Click Stop Scan.
8. Confirm local scanning becomes false.
9. Click Refresh again.
10. Confirm scanning remains false.
11. Confirm no duplicate rows.
12. Confirm no stale scan indicator.
```

The edge condition in the new unit test is the authoritative proof for the missed-event scenario; normal UI validation ensures no regression.

---

# 17. CODE QUALITY REQUIREMENT

Make the smallest correct production change.

Expected production diff should be very small.

The bulk of the work should be:

```text
one snapshot reconciliation correction
+
one focused regression test
```

Do not use this task as an opportunity for unrelated refactoring.

No new compiler warnings.

Follow existing style.

---

# 18. DEFINITION OF DONE

This correction is complete only when:

```text
[ ] refreshed Adapter1 Discovering=true is forwarded
[ ] refreshed Adapter1 Discovering=false is forwarded
[ ] local scanning ownership remains distinct from global discovering
[ ] stale local scanning is cleared by a false snapshot
[ ] new regression test fails on old behavior
[ ] new regression test passes after fix
[ ] existing unit tests still pass
[ ] clean build passes
[ ] normal CTest passes
[ ] live integration test still passes on target laptop
[ ] no Phase 3 functionality was added
[ ] no shell-command Bluetooth backend was added
```

---

# 19. REQUIRED FINAL REPORT

After implementation, provide:

## A. Production Change

Show the exact logical change made in the snapshot handler.

## B. Regression Test

State:

```text
test name
old behavior
expected failure before fix
passing behavior after fix
```

## C. Test Result

Report:

```bash
ctest --test-dir build --output-on-failure
```

## D. Clean Build Result

Report:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## E. Live Integration Result

Report:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
ctest --test-dir build \
-R tst_BlueZLiveIntegration \
--output-on-failure
```

## F. Final Gate

If every Phase 2 test and hardware validation succeeds, end with:

```text
PHASE 2 FINAL SNAPSHOT RECONCILIATION FIX: PASSED
PHASE 2 EXIT GATE: PASSED
```

Otherwise end with:

```text
PHASE 2 FINAL SNAPSHOT RECONCILIATION FIX: NOT PASSED
PHASE 2 EXIT GATE: NOT PASSED
```

and list the blocker.

---

# 20. FINAL INSTRUCTION

Make **only this final Phase 2 correction**.

The required behavior is simple:

```text
GetManagedObjects snapshot says Discovering=true
    -> forward true

GetManagedObjects snapshot says Discovering=false
    -> forward false
```

The snapshot must be authoritative enough to repair stale local discovery state.

Do not begin Phase 3 until this regression test, the clean build, normal tests, and the live BlueZ integration test all pass.
