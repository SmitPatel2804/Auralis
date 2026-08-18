# Auralis — PHASE 3 Final Closure Prompt
## Preserve Reconnect Intent Across BlueZ Restart, Harden Registry Safety, and Lock PHASE 3

**Purpose:** Copy/paste this document into the AI coding IDE/agent working inside the current Auralis repository.

**Project:** Auralis  
**Target:** Ubuntu Linux laptop  
**Current status:** PHASE 0, PHASE 1, and PHASE 2 are complete. PHASE 3 is almost complete. The major defects from the previous remediation pass have been fixed, but **PHASE 3 must not yet be marked complete** until the final production-path recovery issue and validation requirements below are addressed.

---

# 1. YOUR ROLE

You are the senior C++/Qt/Linux Bluetooth engineer completing the **final closure pass for PHASE 3 — Bluetooth Device Management**.

This is **not** a rewrite.

Do not redesign working PHASE 3 architecture.

Do not replace the current lifecycle, pairing-agent, reconnect, registry, or D-Bus abstractions unless a narrowly scoped correction is necessary.

The current repository already contains working implementations for:

```text
DeviceLifecycleManager
ReconnectPolicy
BlueZAgent
BlueZAgentAdaptor
AgentCapability
PairingRequest
BluetoothDbusError
DeviceOperation
UuidCatalog
DeviceRegistry
BluetoothManager
QML Bluetooth lifecycle controls
Pairing prompt UI
Phase 3 tests
BlueZ live integration test
```

The following previous blockers have already been addressed and must remain working:

```text
Pairing -> CancellingPairing
actual Device1.CancelPairing() invocation
duplicate cancel protection
stale Pair callback protection
automatic reconnect after failed attempt
retryable vs non-retryable reconnect errors
bounded retry/backoff
ReconnectPolicy::resumeAll()
BlueZ recovery lifecycle
configurable Agent capability
backend PIN/passkey validation
expanded Phase 3 lifecycle tests
live Trust + Reconnect validation
destructive Forget opt-in protection
```

Your task is to close the remaining gap safely and prove PHASE 3 completion.

---

# 2. FINAL KNOWN BLOCKER

The remaining significant problem is:

> A user’s explicit Disconnect intent may be lost when BlueZ disappears because `BluetoothManager` clears the live `DeviceRegistry` before the lifecycle subsystem finishes BlueZ-loss handling.

This can cause an intentionally disconnected device to become automatically reconnect-eligible after BlueZ restarts.

That behavior violates PHASE 3 reconnect policy.

---

# 3. CURRENT PRODUCTION-PATH PROBLEM

The current production path behaves approximately like this.

The user explicitly disconnects:

```text
Connected device
   |
   | User clicks Disconnect
   v
DeviceLifecycleManager
   |
   +-- marks:
       userDisconnectRequested = true
   |
   +-- calls Device1.Disconnect()
   |
   v
BlueZ Connected=false
```

At this point, automatic reconnect is correctly suppressed.

However, when BlueZ becomes unavailable, `BluetoothManager` currently performs logic equivalent to:

```cpp
if (!available && registry_ != nullptr) {
    registry_->clear();
}

if (!available && adapters_ != nullptr) {
    adapters_->clear();
}

lifecycle_->onBlueZAvailabilityChanged(false);
```

The live device record is destroyed before the lifecycle manager receives the full recovery transition.

The current `BluetoothDeviceData` defaults include values equivalent to:

```cpp
bool userDisconnectRequested = false;
bool autoReconnectEnabled = true;
```

When BlueZ later returns, a new object-manager snapshot recreates the device.

The recreated device therefore has no memory of the user’s previous explicit Disconnect unless that Auralis-owned metadata is retained separately.

The later reconnect candidate evaluation sees:

```text
paired == true
connected == false
autoReconnectEnabled == true
userDisconnectRequested == false
```

and may schedule an automatic reconnect.

That is incorrect.

---

# 4. REQUIRED BEHAVIOR

An explicit user Disconnect must continue suppressing application-level automatic reconnect across:

```text
BlueZ daemon disappearance
BlueZ daemon restart
ObjectManager snapshot rebuild
Device1 object recreation
Adapter recreation
temporary system-bus service loss
```

The desired flow is:

```text
CONNECTED
   |
   | user Disconnect
   v
DISCONNECTED
userDisconnectRequested = true
   |
   | BlueZ disappears
   v
live BlueZ registry cleared
   |
   | BlueZ returns
   v
device recreated from ObjectManager
   |
   | restore Auralis-owned reconnect intent
   v
userDisconnectRequested = true
   |
   | evaluate reconnect eligibility
   v
DO NOT RECONNECT
```

---

# 5. OWNERSHIP BOUNDARY

Treat these two categories differently.

## 5.1 BlueZ-owned state

BlueZ is authoritative for:

```text
Paired
Connected
Trusted
ServicesResolved
UUIDs
RSSI
Device1 object path
Adapter1 object path
BlueZ discovery presence
```

This state may legitimately disappear and be rebuilt when `bluetoothd` restarts.

## 5.2 Auralis-owned state

Auralis owns:

```text
user explicitly disconnected this device
auto-reconnect enabled/disabled preference
reconnect suppression policy
future user-owned Bluetooth preferences
```

This state must not disappear merely because BlueZ recreated a D-Bus object.

Do not store Auralis-owned policy only inside a disposable BlueZ snapshot record.

---

# 6. REQUIRED FIX — RETAIN RECONNECT INTENT

Implement a small retained metadata layer for reconnect-related Auralis state.

The simplest acceptable design is an in-memory map owned by an appropriate long-lived manager.

For example:

```cpp
struct DeviceReconnectMetadata
{
    bool userDisconnectRequested = false;
    bool autoReconnectEnabled = true;
};
```

Stored using a stable device identity.

Conceptually:

```cpp
QHash<DeviceIdentity, DeviceReconnectMetadata> reconnectMetadata_;
```

You may choose another representation that fits the repository better.

Do not introduce a large persistence subsystem solely for this fix.

---

# 7. STABLE DEVICE IDENTITY

Do not key this retained metadata by:

```text
display name
alias
QML row index
temporary object pointer
```

Use the strongest existing identity strategy already established in PHASE 2 / PHASE 3.

Candidates may include:

```text
Bluetooth address
identity address
adapter identity + device address
stable internal Auralis device ID
```

Be careful with LE privacy/random addresses.

Reuse current identity/deduplication logic.

Do not invent a second identity system.

---

# 8. METADATA LIFETIME

The retained reconnect metadata should survive:

```text
BlueZ service restart
DeviceRegistry::clear()
Adapter registry clear
ObjectManager snapshot rebuild
Device1 recreation
```

It does not necessarily need to survive application restart unless the current architecture already supports that.

The minimum PHASE 3 requirement is:

```text
survive BlueZ restart during the same Auralis process
```

If the existing settings/persistence layer already has an appropriate place, reuse it.

---

# 9. METADATA UPDATE RULES

When the user explicitly chooses Disconnect:

```text
metadata.userDisconnectRequested = true
```

When the user explicitly chooses Connect / Reconnect:

```text
metadata.userDisconnectRequested = false
```

before or as part of the intentional connect request.

When the device connects successfully:

```text
metadata.userDisconnectRequested = false
```

unless current product semantics require otherwise.

When the user chooses Forget:

```text
remove retained metadata for that device
```

or reset it according to the current persistence policy.

When `autoReconnectEnabled` changes:

```text
retain that preference across BlueZ restart
```

if that property is already user-controlled.

---

# 10. SNAPSHOT RESTORATION

When BlueZ returns and `DeviceRegistry` is rebuilt:

```text
parse BlueZ Device1
   |
   v
derive stable identity
   |
   v
look up retained Auralis reconnect metadata
   |
   v
apply:
    userDisconnectRequested
    autoReconnectEnabled
   |
   v
insert/update DeviceRegistry
```

This metadata restoration must occur **before** reconnect candidates are evaluated.

Required ordering:

```text
BlueZ service returns
   |
   v
ObjectManager snapshot requested
   |
   v
adapters reconstructed
   |
   v
devices reconstructed
   |
   v
Auralis reconnect metadata restored
   |
   v
snapshot considered applied
   |
   v
ReconnectPolicy resumed
   |
   v
reevaluateReconnectCandidates()
```

Do not evaluate reconnect eligibility while the reconstructed devices still contain default reconnect metadata.

---

# 11. DO NOT PERSIST BLUEZ BOND MATERIAL

Do not store:

```text
link keys
LTKs
PIN codes
passkeys
BlueZ bond database
```

Do not touch:

```text
/var/lib/bluetooth
```

Auralis should retain only its own policy metadata.

---

# 12. TEST THE ACTUAL PRODUCTION PATH

A lifecycle-only test is not sufficient.

The previous test that directly calls:

```cpp
lifecycle.onBlueZAvailabilityChanged(false);
lifecycle.onBlueZAvailabilityChanged(true);
lifecycle.onSnapshotApplied();
```

does not reproduce the real application path because it bypasses `BluetoothManager::handleBlueZAvailable()` and therefore bypasses:

```text
DeviceRegistry::clear()
AdapterRegistry::clear()
```

Add a test that goes through the actual BluetoothManager-level recovery path.

---

# 13. REQUIRED TEST — EXPLICIT DISCONNECT SURVIVES BLUEZ RESTART

Add a production-path component/integration-style unit test.

Suggested name:

```text
explicitDisconnectSurvivesBlueZRestart
```

Exact name may follow current test conventions.

Test sequence:

```text
1. Construct BluetoothManager with fake/mock BlueZ dependencies.

2. Inject initial ObjectManager snapshot containing:
   - adapter
   - paired device
   - connected device

3. Confirm device exists in DeviceRegistry.

4. Issue explicit user Disconnect through the same public lifecycle API
   used by production/QML.

5. Simulate successful disconnect:
   Connected=false.

6. Confirm:
   userDisconnectRequested == true

7. Simulate org.bluez disappearance using the actual
   BluetoothManager availability path.

8. Confirm production registry clear occurs.

9. Simulate org.bluez return.

10. Inject rebuilt ObjectManager snapshot containing the same device:
    Paired=true
    Connected=false

11. Complete snapshot application/recovery.

12. Advance fake timers / event loop beyond initial reconnect delay.

13. Assert:
    Device1.Connect() was NOT called.

14. Confirm restored device metadata still indicates intentional
    disconnect suppression.
```

This test is mandatory.

---

# 14. REQUIRED OPPOSITE TEST — UNEXPECTED DISCONNECT RECOVERS

Also prove that preserving intentional Disconnect does not accidentally disable legitimate reconnect behavior.

Suggested test:

```text
unexpectedDisconnectReconnectsAfterBlueZRestart
```

Sequence:

```text
1. Paired + connected device exists.
2. No userDisconnectRequested flag.
3. Device disconnects unexpectedly.
4. BlueZ disappears before reconnect completes.
5. BlueZ registry is cleared.
6. BlueZ returns.
7. Same paired/disconnected device recreated.
8. ReconnectPolicy resumes.
9. Device remains eligible.
10. Reconnect attempt is eventually issued.
```

This proves the retained metadata logic does not suppress valid automatic recovery.

---

# 15. REQUIRED TEST — FORGET CLEARS RETAINED METADATA

Add a test verifying:

```text
explicit disconnect
   |
   v
metadata retained
   |
   v
Forget
   |
   v
Device removed
   |
   v
retained reconnect metadata removed
```

If the forgotten device is discovered later as a fresh unpaired device:

```text
userDisconnectRequested
```

must not leak from the previously forgotten identity unless product semantics explicitly require it.

For PHASE 3, reset/remove it.

---

# 16. REQUIRED TEST — MANUAL CONNECT CLEARS SUPPRESSION

Sequence:

```text
explicit Disconnect
   |
   v
userDisconnectRequested = true
   |
   | user clicks Connect/Reconnect
   v
userDisconnectRequested = false
   |
   v
Connect()
```

Test that subsequent unexpected disconnect is once again eligible for auto-reconnect according to policy.

---

# 17. REGISTRY POINTER / REFERENCE SAFETY FIX

There is also a defensive correctness issue in `DeviceLifecycleManager::onDevicePropertiesChanged()`.

The current logic obtains a device pointer/reference from `DeviceRegistry`, then calls a function that can mutate the registry, and then continues using the previously obtained pointer.

Conceptually:

```cpp
const BluetoothDeviceData* device =
    registry_->findByObjectPath(objectPath);

checkPropertyCompletion(objectPath, *device);

// registry may have been mutated here

// stale pointer/reference may still be used
```

`checkPropertyCompletion()` may call paths such as:

```text
finishOperation()
   |
   v
clearPending()
   |
   v
registry_->clearOperation()
   |
   v
registry mutation
```

Depending on container behavior and implementation changes, a pointer/reference into a mutated Qt hash/container should not be assumed stable.

---

# 18. REQUIRED REGISTRY SAFETY CHANGE

After any function that can mutate `DeviceRegistry`, reacquire the device.

Example pattern:

```cpp
const BluetoothDeviceData* device =
    registry_->findByObjectPath(objectPath);

if (device == nullptr) {
    return;
}

checkPropertyCompletion(objectPath, *device);

device = registry_->findByObjectPath(objectPath);

if (device == nullptr) {
    return;
}

// continue using the refreshed pointer
```

Or copy the required data by value before mutation if that is cleaner.

Do not depend on undocumented pointer/reference stability across registry writes.

Keep the fix minimal.

---

# 19. ADD REGISTRY-MUTATION SAFETY TEST

Add a test where a property update causes operation completion and registry mutation during the same signal handling path.

Prove:

```text
no crash
no stale pointer use
correct final operation state
correct reconnect decision
```

If sanitizers are already available in the project, this path should also be compatible with them.

---

# 20. REVERIFY PREVIOUS CANCEL-PAIRING FIX

Do not change the working Pairing -> CancellingPairing fix.

Re-run and preserve tests equivalent to:

```text
cancelPairingTransitionsAndCallsBlueZ
duplicateCancelPairingIsSafe
cancelPairingInvalidatesPendingRequest
cancelPairingRejectedOutsidePairing
latePairCallbackAfterCancelIsIgnored
```

The final closure pass must not regress this.

---

# 21. REVERIFY RECONNECT RETRY FIX

Preserve and re-run tests equivalent to:

```text
reconnectFailureSchedulesNextAttempt
reconnectNonRetryableErrorStopsRetries
reconnectSuccessResetsAttempts
bounded max attempts
backoff progression
explicit Disconnect suppresses reconnect
Forget suppresses reconnect
```

The new BlueZ-restart metadata fix must integrate with—not replace—this logic.

---

# 22. REVERIFY BLUEZ PAUSE / RESUME

Preserve:

```text
ReconnectPolicy::pauseAll()
ReconnectPolicy::resumeAll()
```

and verify:

```text
BlueZ loss
   -> pause

BlueZ recovery + valid snapshot
   -> resume
```

Do not call `resumeAll()` before registry metadata has been restored.

The ordering matters.

---

# 23. REVERIFY AGENT CAPABILITY

Preserve the existing `AgentCapability` abstraction.

Expected supported values:

```text
NoInputNoOutput
DisplayOnly
DisplayYesNo
KeyboardOnly
KeyboardDisplay
```

Ensure `KeyboardDisplay` remains the desktop default unless configured otherwise.

Preserve configuration support, including the current mechanism such as:

```text
AURALIS_BLUETOOTH_AGENT_CAPABILITY
```

if that is the actual repository interface.

Re-run capability mapping tests.

---

# 24. REVERIFY PIN / PASSKEY VALIDATION

Preserve backend validation.

PIN:

```text
must match pending request
must not be stale
must not be empty when required
must stay within reasonable protocol limits
must not contain invalid embedded null input
must not be logged
```

Passkey:

```text
must match request type
must not be stale
must be numeric
must remain within Bluetooth passkey range
must not be logged
```

Re-run the existing tests.

---

# 25. BLUEZ SERVICE-LOSS ORDERING

Audit the production ordering in:

```text
BluetoothManager::handleBlueZAvailable(...)
```

The desired conceptual sequence on loss is:

```text
notify subsystems BlueZ is going away
preserve Auralis-owned metadata
cancel/invalidate pending D-Bus operations
pause reconnect
cancel pairing prompts
invalidate agent registration
clear live BlueZ-derived registry objects
clear adapters
```

The exact ordering may differ based on current architecture.

The crucial requirement is:

> No Auralis-owned reconnect intent may be destroyed as a side effect of clearing BlueZ-derived live state.

On return:

```text
service available
fetch snapshot
rebuild live state
restore Auralis metadata
mark snapshot applied
register/re-register agent
resume reconnect
evaluate eligible devices
```

Document the final actual ordering.

---

# 26. DO NOT OVER-CORRECT BLUEZ RESTART

Do not keep stale BlueZ object paths alive across daemon restart.

Retain only stable Auralis metadata.

These should be discarded/recreated:

```text
Device1 object path state
Adapter1 object presence
temporary D-Bus proxy state
pending D-Bus calls
agent registration assumption
RSSI
ServicesResolved snapshot
```

These may be retained:

```text
userDisconnectRequested
autoReconnectEnabled
stable internal identity
future Auralis user preferences
```

---

# 27. PHASE 3 LIVE VALIDATION

The live integration test already now covers most of the full PHASE 3 gate.

Preserve and run:

```text
Discover
Pair
Trust
Connect
Disconnect
Reconnect
Forget
```

Forget must remain destructive-opt-in only.

The final live flow should prove:

```text
1. Start discovery.
2. Find expected real test device.
3. Pair.
4. Confirm Paired=true.
5. Trust.
6. Confirm Trusted=true.
7. Connect.
8. Confirm Connected=true.
9. Disconnect.
10. Confirm Connected=false.
11. Reconnect.
12. Confirm Connected=true.
13. Disconnect if required.
14. Forget only with explicit destructive-test flag.
15. Confirm Device1/registry removal.
```

---

# 28. MANUAL BLUEZ RESTART VALIDATION

In addition to automated tests, perform one manual development validation if practical.

Scenario:

```text
1. Pair/trust device.
2. Connect device.
3. Explicitly Disconnect from Auralis.
4. Confirm it stays disconnected.
5. Restart bluetoothd / simulate BlueZ daemon restart using the normal
   system development method.
6. Let Auralis recover.
7. Confirm device is rediscovered/reconstructed as paired and disconnected.
8. Confirm Auralis DOES NOT automatically reconnect it.
9. Click Reconnect manually.
10. Confirm connection succeeds.
```

Do not add daemon-control commands to production code.

Manual developer commands are acceptable in documentation only.

---

# 29. SECOND MANUAL RESTART SCENARIO

Validate legitimate recovery:

```text
1. Auto-reconnect enabled.
2. Device was not intentionally disconnected.
3. BlueZ disappears/restarts.
4. Device returns as paired + disconnected.
5. Auralis resumes reconnect policy.
6. Auralis attempts reconnection according to bounded policy.
```

This proves both sides of the policy.

---

# 30. CLEAN BUILD GATE

Run from a clean tree/build directory.

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Do not rely only on incremental builds.

Do not disable tests to make the suite green.

---

# 31. APPLICATION LAUNCH GATE

Launch the existing desktop executable.

Use the actual repository path.

Expected to be equivalent to:

```bash
./build/apps/desktop/auralis-desktop
```

Verify:

```text
application launches
no startup crash
BlueZ status appears
device discovery works
pairing UI still works
connection controls still work
```

---

# 32. PHASE 2 REGRESSION

Verify that the final Phase 3 closure does not damage Phase 2.

Required:

```text
Start discovery
Receive nearby devices
Stop discovery
Restart discovery
No duplicate live device objects
InterfacesAdded works
InterfacesRemoved works
PropertiesChanged works
BlueZ restart rebuilds registry
```

---

# 33. PHASE 3 FINAL ACCEPTANCE CHECKLIST

PHASE 3 may be locked only if every mandatory item passes.

## Cancel Pairing

- [ ] Pair -> CancellingPairing works.
- [ ] `Device1.CancelPairing()` is invoked.
- [ ] duplicate Cancel is safe.
- [ ] pending pairing request invalidated.
- [ ] stale Pair callback safe.

## Trust

- [ ] Trust works.
- [ ] Untrust works.
- [ ] BlueZ `Trusted` is authoritative.

## Connection

- [ ] Connect works.
- [ ] Disconnect works.
- [ ] BlueZ `Connected` is authoritative.

## Reconnect

- [ ] Manual Reconnect works.
- [ ] Unexpected disconnect can auto-reconnect.
- [ ] Failed retryable attempt schedules next attempt.
- [ ] Backoff works.
- [ ] maxAttempts works.
- [ ] non-retryable failure stops.
- [ ] success resets retry state.

## Intent suppression

- [ ] Explicit Disconnect suppresses automatic reconnect.
- [ ] Suppression survives BlueZ restart.
- [ ] Manual Connect/Reconnect clears suppression appropriately.
- [ ] Forget clears retained reconnect metadata.
- [ ] No stale reconnect timer survives Forget.

## BlueZ recovery

- [ ] reconnect pauses on BlueZ loss.
- [ ] agent registration state invalidated.
- [ ] live BlueZ registry safely cleared.
- [ ] Auralis-owned metadata survives.
- [ ] snapshot rebuild works.
- [ ] metadata restored before reconnect evaluation.
- [ ] reconnect resumes after snapshot application.
- [ ] intentionally disconnected device stays disconnected.
- [ ] legitimately eligible device reconnects.

## Registry safety

- [ ] no stale pointer/reference is used after registry mutation.
- [ ] property completion path safely reacquires/copies device data.
- [ ] test covers mutation during property handling.

## Pairing agent

- [ ] configurable `AgentCapability`.
- [ ] RegisterAgent receives configured capability.
- [ ] PIN validation.
- [ ] passkey validation.
- [ ] stale request rejection.
- [ ] duplicate response rejection.
- [ ] no secret logging.

## Tests

- [ ] clean configure passes.
- [ ] clean build passes.
- [ ] all ordinary `ctest` targets pass.
- [ ] manager-level BlueZ restart tests pass.
- [ ] reconnect lifecycle tests pass.
- [ ] agent tests pass.
- [ ] cancel pairing tests pass.

## Live

- [ ] Discover validated.
- [ ] Pair validated.
- [ ] Trust validated.
- [ ] Connect validated.
- [ ] Disconnect validated.
- [ ] Reconnect validated.
- [ ] Forget validated with destructive opt-in.
- [ ] explicit Disconnect across BlueZ restart manually or integration-tested.
- [ ] eligible reconnect across BlueZ restart validated.

---

# 34. PROHIBITED SHORTCUTS

Do not solve the issue by:

```text
disabling auto-reconnect entirely
never clearing DeviceRegistry
keeping stale Device1 object paths
hardcoding a specific Bluetooth address
using display name as identity
calling bluetoothctl
making every recovered device userDisconnectRequested=true
making every recovered device autoReconnectEnabled=false
reconnecting every paired device after restart
ignoring BlueZ restart
removing the manager-level production-path test
```

The policy must distinguish:

```text
intentional disconnect
```

from:

```text
unexpected disconnect
```

across BlueZ restart.

---

# 35. DOCUMENTATION UPDATE

Update the Phase 3 validation/remediation documentation.

Add:

```text
Auralis-owned reconnect metadata design
stable identity used for metadata
BlueZ-loss handling order
snapshot restoration order
explicit-disconnect persistence across daemon restart
unexpected-disconnect recovery behavior
manager-level recovery tests
registry pointer-safety fix
```

Only mark PHASE 3 complete after actual verification.

---

# 36. REQUIRED FINAL ENGINEERING REPORT

When all changes are complete, return a detailed report.

## A. Root cause

Explain why explicit Disconnect state was lost across BlueZ restart.

## B. Metadata ownership

Explain:

```text
what BlueZ owns
what Auralis owns
where reconnect metadata is retained
which stable identity keys it
```

## C. Production recovery sequence

Show the final exact sequence for:

```text
BlueZ loss
BlueZ return
ObjectManager snapshot
metadata restoration
ReconnectPolicy resume
candidate evaluation
```

## D. Registry safety

Explain the pointer/reference invalidation risk and the final fix.

## E. Files changed

For each:

```text
path
change
reason
```

## F. Tests added/updated

Explicitly list:

```text
explicitDisconnectSurvivesBlueZRestart
unexpectedDisconnectReconnectsAfterBlueZRestart
Forget clears retained metadata
manual reconnect clears suppression
registry mutation safety
```

or the actual equivalent test names.

## G. Regression tests

List all previous PHASE 3 tests executed.

## H. Build commands

Show actual commands.

## I. Test results

Provide:

```text
tests discovered
passed
failed
skipped
```

Do not invent results.

## J. Live hardware results

Document:

```text
device address/identifier
Pair
Trust
Connect
Disconnect
Reconnect
Forget
BlueZ restart scenarios
```

Redact anything sensitive if appropriate.

## K. Remaining limitations

Separate:

```text
true Phase 3 limitations
hardware/environment limitations
future Phase 4+ work
```

## L. Final verdict

Return exactly one of:

```text
PHASE 3: COMPLETE
```

or:

```text
PHASE 3: NOT COMPLETE
```

If NOT COMPLETE, list the exact remaining failed acceptance criteria.

---

# 37. START NOW

Work from the current repository.

Do not rewrite PHASE 3.

Implement only the final closure work:

```text
1. retain Auralis reconnect intent outside disposable BlueZ live state
2. restore it during snapshot rebuild
3. test actual BluetoothManager BlueZ restart path
4. prove intentional Disconnect remains suppressed
5. prove unexpected disconnect remains reconnect-eligible
6. clear/reset metadata correctly on Forget/manual reconnect
7. fix registry pointer/reference safety
8. run full clean build
9. run all tests
10. run live Phase 3 gate
11. provide evidence-based final verdict
```

Preserve all existing successful PHASE 3 fixes.

Do not begin PHASE 4.

The only acceptable end state is a verified, production-coherent Bluetooth lifecycle subsystem ready to support PHASE 4 — PipeWire Audio Integration.

Final target:

```text
PHASE 0: COMPLETE
PHASE 1: COMPLETE
PHASE 2: COMPLETE
PHASE 3: COMPLETE
```

Only claim the final line after the complete automated and live validation gates actually pass.
