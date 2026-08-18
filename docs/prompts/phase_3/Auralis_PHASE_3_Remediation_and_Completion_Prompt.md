# Auralis — PHASE 3 Remediation & Completion Prompt
## Fix Remaining Bluetooth Device Management Defects and Close the PHASE 3 Gate

**Purpose:** Give this prompt to an AI coding IDE/agent working inside the current Auralis repository.

**Project:** Auralis  
**Platform:** Ubuntu Linux laptop  
**Current status:** PHASE 0, PHASE 1, and PHASE 2 are complete. PHASE 3 is substantially implemented but **must not yet be marked complete** because several specific defects and validation gaps remain.  
**Goal:** Correct the existing PHASE 3 implementation with the smallest safe set of changes, add missing tests, validate the full PHASE 3 lifecycle, and produce evidence that the phase is truly complete.

---

# 1. IMPORTANT CONTEXT

Do **not** reimplement PHASE 3 from scratch.

The repository already contains a substantial Bluetooth Device Management implementation, including:

```text
DeviceLifecycleManager
BlueZAgent
BlueZAgentAdaptor
PairingRequest
ReconnectPolicy
BluetoothDbusError
DeviceOperation
UuidCatalog
QML lifecycle controls
Pairing prompt UI
Phase 3 unit tests
BlueZ live integration test
```

These existing components are valuable and must be preserved unless a narrowly scoped refactor is required to fix a real defect.

Your task is a **remediation pass**, not a rewrite.

The known remaining problems are:

```text
1. Pairing -> CancelPairing is functionally broken.
2. Automatic reconnect stops after the first failed reconnect attempt.
3. Automatic reconnect remains permanently paused after BlueZ disappears/restarts.
4. Agent capability is hardcoded instead of configurable/extensible.
5. Pairing PIN/passkey backend validation is weaker than required.
6. Tests do not fully cover the Phase 3 failure/reconnect/cancel flows.
7. Live/manual Phase 3 validation does not yet prove the full exit sequence.
```

The final Phase 3 exit sequence must be:

```text
Discover
Pair
Trust
Connect
Disconnect
Reconnect
Forget
```

with correct BlueZ-authoritative state at every step.

---

# 2. NON-NEGOTIABLE RULES

You MUST obey the following.

## 2.1 Preserve existing architecture

Do not replace working components with unrelated alternatives.

Keep the current architecture unless fixing a defect genuinely requires a local redesign.

Do not:

```text
replace DeviceLifecycleManager with shell scripts
replace BlueZAgent with bluetoothctl
bypass DeviceRegistry
move D-Bus operations into QML
rewrite discovery
introduce a second competing Bluetooth state model
```

## 2.2 No shell-command Bluetooth control

Production code must not invoke:

```text
bluetoothctl
btmgmt
busctl
gdbus
dbus-send
pactl
wpctl
```

Use BlueZ D-Bus directly.

## 2.3 Do not begin PHASE 4

Do not implement:

```text
PipeWire graph management
audio endpoint routing
multi-device playback
latency correction
synchronization
audio session routing
```

PHASE 3 must remain Bluetooth lifecycle management only.

## 2.4 BlueZ remains authoritative

These values must be derived from BlueZ state:

```text
Paired
Trusted
Connected
ServicesResolved
UUIDs
```

Transient Auralis operation state may represent:

```text
Pairing
CancellingPairing
Connecting
Disconnecting
Trusting
Untrusting
Forgetting
Reconnecting
```

Do not fake final success state before BlueZ confirms it.

---

# 3. FIRST STEP — RE-AUDIT THE CURRENT IMPLEMENTATION

Before editing code, inspect the current repository and confirm the exact implementation locations of:

```text
DeviceLifecycleManager
ReconnectPolicy
BlueZAgent
BlueZAgentAdaptor
BlueZDbusClient
BluetoothDevice
DeviceRegistry
BluetoothDbusError
DeviceOperation
QML lifecycle controls
PairingPrompt
Phase 3 tests
BlueZ live integration test
```

Pay special attention to:

```text
pending operation tracking
beginOperation(...)
finishOperation(...)
pairDevice(...)
cancelPairing(...)
handleConnectFinished(...)
unexpected disconnect handling
reconnectDue(...)
pauseAll(...)
BlueZ availability callbacks
Agent registration
agent capability selection
pairing credential submission
test seams/fakes
```

Do not assume filenames or line numbers from this prompt are exact.

Use the actual current repository.

---

# 4. CRITICAL FIX #1 — PAIRING CANCELLATION

## 4.1 Known defect

The current lifecycle manager tracks active operations per device.

Pairing starts approximately like:

```cpp
beginOperation(devicePath, DeviceOperation::Pairing);
client_->pairDevice(devicePath);
```

The operation is inserted into a pending-operation map.

Cancel pairing then attempts approximately:

```cpp
beginOperation(devicePath, DeviceOperation::CancellingPairing);
client_->cancelPairing(devicePath);
```

But `beginOperation()` rejects any operation when the device already exists in `pending_`.

Therefore:

```text
Pairing active
   ↓
pending_ already contains device
   ↓
Cancel requested
   ↓
beginOperation(CancellingPairing)
   ↓
rejected
   ↓
Device1.CancelPairing() never called
```

This is a Phase 3 blocker.

---

# 5. REQUIRED DESIGN FOR CANCEL PAIRING

Cancellation must be modeled as a valid transition from an active Pairing operation.

Do not solve this by globally allowing contradictory concurrent operations.

Implement an explicit exception/transition model.

Acceptable approaches include:

```text
A. Replace Pairing pending state with CancellingPairing atomically

or

B. Mark Pairing operation as cancellationRequested and issue CancelPairing

or

C. Allow a narrow Pairing -> CancellingPairing transition in operation arbitration
```

Whichever design you choose must guarantee:

```text
only Pairing can transition into CancellingPairing
only one CancelPairing D-Bus call is issued
duplicate Cancel clicks are harmless
stale Pair reply cannot resurrect Pairing state
pending agent prompt is invalidated
operation completes deterministically
```

Do not allow arbitrary concurrent state transitions such as:

```text
Connecting -> Pairing
Forgetting -> Connecting
Disconnecting -> Pairing
```

---

# 6. CANCEL PAIRING SUCCESS SEMANTICS

When the user requests cancellation:

```text
Pairing
   ↓
CancellingPairing
   ↓
Device1.CancelPairing()
```

Then reconcile with BlueZ.

Possible outcomes include:

```text
CancelPairing method succeeds
Pair() returns AuthenticationCanceled
Pair() returns Canceled
Agent1.Cancel() is invoked
Paired remains false
device disappears
BlueZ disappears
```

The operation must converge safely.

On successful cancellation:

```text
Paired == false
operation == Idle
pending pairing prompt removed
last error should not incorrectly show cancellation as an unexpected failure
```

If the remote device completed pairing before cancellation won the race:

```text
Paired == true
```

BlueZ's final state wins.

Do not forcibly lie to the model.

---

# 7. CANCEL PAIRING TESTS — REQUIRED

Add unit/component tests proving the real lifecycle behavior.

At minimum:

## Test A

```text
Given device is Pairing
When cancelPairing() is invoked
Then BlueZDbusClient.cancelPairing() is called exactly once
```

## Test B

```text
duplicate cancelPairing()
does not issue duplicate uncontrolled D-Bus calls
```

## Test C

```text
Pairing -> CancellingPairing
is accepted
```

## Test D

```text
Connecting -> CancellingPairing
is rejected
```

## Test E

```text
pending pairing request is invalidated when cancellation occurs
```

## Test F

```text
late Pair callback after cancellation cannot overwrite newer state
```

If the code already uses operation generations/tokens, use them.

---

# 8. CRITICAL FIX #2 — AUTOMATIC RECONNECT RETRIES

## 8.1 Known defect

The reconnect policy supports:

```text
maxAttempts
initialDelay
maxDelay
backoffMultiplier
```

but the lifecycle manager appears to schedule only the first reconnect.

Current behavior is effectively:

```text
unexpected disconnect
   ↓
schedule reconnect
   ↓
timer fires
   ↓
Connect()
   ↓
failure
   ↓
STOP
```

This violates the intended bounded retry policy.

---

# 9. REQUIRED RECONNECT FLOW

The reconnect lifecycle should behave like:

```text
unexpected disconnect
   ↓
eligible?
   ↓ yes
schedule attempt 1
   ↓
RECONNECTING
   ↓
Connect()
   ↓
success?
   ├── yes -> reset retry state
   └── no
        ↓
     retryable?
        ├── no -> stop
        └── yes
             ↓
        attempts remaining?
             ├── no -> stop with final error
             └── yes
                  ↓
             schedule next backoff
```

Do not recursively call connect immediately.

Use Qt timers/event-loop scheduling.

No sleeps.

---

# 10. RETRYABLE VS NON-RETRYABLE ERRORS

Use the existing structured Bluetooth error mapping.

Define/document which error categories are retryable.

Likely retryable examples:

```text
ConnectionFailed
TimedOut
DeviceUnavailable
NotReady
BlueZUnavailable
temporary transport failure
```

Potential non-retryable examples:

```text
AuthenticationRejected
AuthenticationFailed
PermissionDenied
InvalidArguments
NotSupported
device forgotten
explicit user cancellation
```

Do not blindly retry every error.

The exact mapping must follow current error model semantics.

Add tests for the retryability decision.

---

# 11. RECONNECT OPERATION COMPLETION

The lifecycle manager must know whether a failed `Connect()` was:

```text
manual Connect
manual Reconnect
automatic Reconnect
```

Do not use ambiguous logic that causes automatic retries after every manual connect failure.

Track operation origin explicitly if necessary.

Possible model:

```text
DeviceOperation::Connecting
DeviceOperation::Reconnecting
```

plus reconnect policy state.

When `DeviceOperation::Reconnecting` fails with a retryable error:

```text
finish current operation
schedule next reconnect attempt
```

When it succeeds:

```text
Connected=true
reset attempt counter
cancel pending retry timer
clear reconnect error state as appropriate
```

---

# 12. EXPLICIT DISCONNECT MUST CANCEL AUTO-RECONNECT

This behavior is mandatory.

Sequence:

```text
device connected
   ↓
user clicks Disconnect
   ↓
mark disconnect intentional
   ↓
cancel reconnect timer/state
   ↓
Device1.Disconnect()
   ↓
Connected=false
```

The resulting BlueZ `Connected=false` event must **not** trigger automatic reconnect.

Test this.

---

# 13. FORGET MUST CANCEL AUTO-RECONNECT

Sequence:

```text
device known
   ↓
Forget
   ↓
cancel reconnect timer
clear reconnect attempt state
mark removal expected
   ↓
Adapter1.RemoveDevice()
```

After `InterfacesRemoved`:

```text
no reconnect attempt
no stale timer
no stale device callback
```

Test this.

---

# 14. CRITICAL FIX #3 — RESUME RECONNECT AFTER BLUEZ RESTART

## 14.1 Known defect

Current reconnect policy contains logic equivalent to:

```cpp
pauseAll();
```

which sets:

```text
paused = true
```

and cancels timers.

There is no reliable resume path after BlueZ becomes available again.

Result:

```text
BlueZ disappears once
   ↓
ReconnectPolicy permanently disabled
```

This must be fixed.

---

# 15. REQUIRED RECONNECT PAUSE/RESUME API

Add a clear lifecycle such as:

```cpp
pauseAll();
resumeAll();
```

or:

```cpp
setPaused(bool);
```

Use whichever matches repository style.

Required semantics:

## pause

```text
paused = true
cancel active reconnect timers
preserve only safe metadata needed to decide future eligibility
```

## resume

```text
paused = false
re-evaluate known devices
restart reconnect only for eligible devices
do not reconnect devices explicitly disconnected by user
do not reconnect forgotten devices
do not reconnect unpaired devices
```

Do not simply restart every canceled timer blindly.

---

# 16. BLUEZ RESTART RECOVERY ORDER

When `org.bluez` returns:

```text
1. BlueZ service detected
2. object-manager snapshot rebuilt
3. adapters restored
4. devices restored in DeviceRegistry
5. agent re-registered
6. reconnect policy resumed
7. eligible devices evaluated
```

Do not resume reconnect before device paths/registry state are valid.

Reuse Phase 2 BlueZ recovery logic.

Do not duplicate object-manager reconstruction.

---

# 17. BLUEZ RESTART TESTS

Add tests proving:

```text
pauseAll() suppresses reconnect
resumeAll() allows reconnect again
BlueZ loss cancels current retry timers
BlueZ return re-enables policy
intentional disconnect remains suppressed after resume
forgotten device remains suppressed after resume
```

If the existing architecture emits a BlueZ availability signal, test the actual lifecycle wiring rather than only testing ReconnectPolicy in isolation.

---

# 18. FIX #4 — AGENT CAPABILITY CONFIGURATION

## 18.1 Current issue

The BlueZ agent appears to register with:

```text
KeyboardDisplay
```

directly.

This is functional but does not satisfy the desired configurable/extensible design.

---

# 19. REQUIRED CAPABILITY MODEL

Create a small explicit abstraction.

Example:

```cpp
enum class AgentCapability
{
    NoInputNoOutput,
    DisplayOnly,
    DisplayYesNo,
    KeyboardOnly,
    KeyboardDisplay
};
```

Map each to the exact BlueZ string:

```text
"NoInputNoOutput"
"DisplayOnly"
"DisplayYesNo"
"KeyboardOnly"
"KeyboardDisplay"
```

Keep `"KeyboardDisplay"` as the default if that is appropriate for the current desktop UI.

But do not hardcode the D-Bus registration call to one literal capability.

The capability should come from:

```text
application configuration
constructor parameter
Bluetooth settings structure
or equivalent dependency
```

Use the current repository's configuration style.

Do not create a huge settings subsystem just for this fix.

---

# 20. AGENT CAPABILITY TESTS

Test mapping for every supported enum value.

Test that `RegisterAgent()` receives the configured value.

Test invalid/unrecognized configuration fallback if applicable.

---

# 21. FIX #5 — PAIRING PIN/PASSKEY VALIDATION

Validate pairing credentials at the backend boundary.

Do not rely only on QML validation.

## PIN code

BlueZ PIN code semantics are string-based.

Validate at least:

```text
not empty when required
reasonable maximum length
no accidental embedded null
no stale request
correct request type
```

Do not log the submitted PIN.

## Passkey

Passkey must be a numeric Bluetooth passkey.

Validate:

```text
numeric
within valid uint32 / Bluetooth passkey range
correct pending request type
not stale
not already answered
```

Typical Bluetooth passkeys are displayed as six digits, but preserve correct numeric semantics rather than assuming UI formatting is the protocol type.

Do not log the secret.

---

# 22. PAIRING REQUEST LIFETIME

Ensure a pairing request cannot be answered twice.

Required behavior:

```text
pending
   ↓
accept/reject/submit
   ↓
resolved
   ↓
removed
```

Subsequent response:

```text
reject as stale/invalid
```

On:

```text
CancelPairing
Agent1.Cancel
device removal
BlueZ disappearance
application shutdown
```

all relevant pending requests must become invalid.

Test each important invalidation path.

---

# 23. STRUCTURED ERROR BEHAVIOR

Do not regress the current structured Bluetooth error model.

Preserve:

```text
operation
deviceId/path
D-Bus error name
message
category
retryable
```

Improve it if necessary for reconnect decisions.

Do not make QML parse raw BlueZ error strings.

Cancellation should be represented meaningfully.

For example:

```text
AuthenticationCanceled
UserCanceled
OperationCanceled
```

should not necessarily be shown as a frightening fatal failure.

Use current error categories where possible.

---

# 24. OPERATION ARBITRATION

Audit all operation-transition rules after fixing CancelPairing.

Required examples:

```text
Pair -> CancelPairing       allowed
Pair -> Connect             reject
Pair -> Forget              define deterministic policy
Connect -> Disconnect       define deterministic policy
Connect -> Forget           define deterministic policy
Reconnect -> Disconnect     allowed as explicit user override
Reconnect -> Forget         allowed as explicit user override
Forget -> Connect           reject
```

Do not broaden concurrency indiscriminately.

Implement explicit override/cancellation behavior for destructive or user-priority actions where appropriate.

Document the policy.

---

# 25. STALE CALLBACK PROTECTION

Async D-Bus calls may complete after:

```text
device removal
operation cancellation
new generation of same operation
BlueZ restart
object recreation
```

Use existing generation IDs/tokens if present.

A stale callback must not:

```text
clear a newer operation
set an error on a replacement device
restart reconnect unexpectedly
resurrect a canceled pairing state
```

Add or strengthen tests.

---

# 26. QML VALIDATION

Do not redesign the Phase 3 UI.

Keep current QML lifecycle controls.

Verify the UI correctly represents the corrected backend transitions.

Required examples:

## Pairing

```text
Pair
   ↓
Pairing...
   ↓
Cancel becomes available
```

## Cancel

```text
Cancel
   ↓
Cancelling...
   ↓
returns to discovered/unpaired state
```

## Reconnect

```text
Disconnected
   ↓
Reconnect
   ↓
Reconnecting...
```

For auto-reconnect, expose attempt metadata if already supported:

```text
Reconnecting (2/5)...
```

Do not fabricate attempt count in QML.

---

# 27. TEST REQUIREMENTS

Do not consider this remediation complete until automated tests cover the fixed defects.

At minimum extend/create tests for the following.

---

# 28. DEVICE LIFECYCLE TESTS

Required:

```text
Pair starts Pairing
CancelPairing transitions from Pairing
CancelPairing reaches D-Bus client
duplicate CancelPairing is safe
Pair callback after cancel is stale-safe
Connect starts Connecting
Disconnect suppresses reconnect
Forget suppresses reconnect
manual reconnect starts Reconnecting
automatic reconnect failure schedules next attempt
automatic reconnect success resets attempts
automatic reconnect stops at maxAttempts
non-retryable reconnect error stops retries
```

---

# 29. RECONNECT POLICY TESTS

Required:

```text
first delay
backoff progression
max delay cap
max attempt cap
success reset
cancel device
cancel all
pause
resume
paused scheduling suppressed
resume scheduling restored
```

Do not only test the policy class in isolation.

Also test DeviceLifecycleManager integration.

---

# 30. BLUEZ RESTART TESTS

Simulate:

```text
BlueZ available
unexpected disconnect
reconnect pending
BlueZ disappears
reconnect pauses
BlueZ returns
registry rebuilt
reconnect resumes if eligible
```

Also test:

```text
intentional disconnect before BlueZ restart
```

must remain intentionally disconnected afterward.

---

# 31. AGENT TESTS

Required:

```text
RegisterAgent uses configured capability
all supported capability mappings
PIN request
passkey request
confirmation request
authorization request
service authorization request
accept
reject
cancel
stale request rejection
duplicate response rejection
BlueZ restart invalidates/re-registers
```

---

# 32. D-BUS CONTRACT TESTS

Verify exact operations:

```text
Pair
    -> org.bluez.Device1.Pair

CancelPairing
    -> org.bluez.Device1.CancelPairing

Connect/Reconnect
    -> org.bluez.Device1.Connect

Disconnect
    -> org.bluez.Device1.Disconnect

Trust
    -> org.freedesktop.DBus.Properties.Set(
           "org.bluez.Device1",
           "Trusted",
           true)

Untrust
    -> same with false

Forget
    -> org.bluez.Adapter1.RemoveDevice(deviceObjectPath)

Agent registration
    -> org.bluez.AgentManager1.RegisterAgent(path, capability)
```

Do not replace contract tests with only UI tests.

---

# 33. LIVE INTEGRATION TEST SAFETY

Preserve the current opt-in test philosophy.

Normal:

```bash
ctest --test-dir build --output-on-failure
```

must not require real hardware.

Live Bluetooth tests must require explicit opt-in.

Use the repository's existing environment-variable names.

A real address must look like:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF
```

Do not document:

```bash
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS>
```

as a literal shell command because `<...>` is interpreted as shell redirection syntax.

---

# 34. EXPAND LIVE PHASE 3 VALIDATION

The live/manual validation must prove the full phase gate.

Required sequence:

```text
1. Discover device.
2. Pair device.
3. Confirm Paired=true.
4. Trust device.
5. Confirm Trusted=true.
6. Connect device.
7. Confirm Connected=true.
8. Disconnect device.
9. Confirm Connected=false.
10. Reconnect device.
11. Confirm Connected=true.
12. Disconnect if needed.
13. Forget device only with explicit destructive-test opt-in.
14. Confirm Device1 removal / registry update.
```

If automatic reconnect is tested live:

```text
unexpected disconnect
   ↓
attempt 1
   ↓ fail
attempt 2
```

must be observable, but do not make physical-device instability mandatory for ordinary CI.

---

# 35. DESTRUCTIVE TEST PROTECTION

Forget is destructive.

Do not forget a user's existing paired device during default test execution.

Require an extra explicit environment flag before live Forget.

If the repository already has one, keep it.

Otherwise introduce a clearly named flag consistent with current conventions, for example:

```text
AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1
```

The test should print:

```text
SKIPPED: destructive Bluetooth operations not enabled
```

instead of failing.

---

# 36. CLEAN BUILD REGRESSION GATE

After fixes:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Then launch:

```bash
./build/apps/desktop/auralis-desktop
```

or the exact existing application path.

Do not disable existing tests to obtain green results.

---

# 37. PHASE 2 REGRESSION CHECK

Verify discovery still works.

Required manual behavior:

```text
launch app
start discovery
nearby devices appear
stop discovery
device registry remains coherent
restart discovery
no duplicate/stale device objects
```

Also verify:

```text
BlueZ restart/recovery still rebuilds registry
```

Do not let Phase 3 reconnect changes break Phase 2 recovery.

---

# 38. CODE QUALITY REQUIREMENTS

Maintain the existing quality bar.

Use:

```text
Qt event loop
asynchronous D-Bus
QObject lifetime safety
RAII
explicit signals
structured errors
central constants
unit-test seams
```

Avoid:

```text
sleep()
busy wait
nested event loop
raw unmanaged owning pointers
global mutable Bluetooth state
string parsing of UI labels
duplicated BlueZ interface strings everywhere
```

---

# 39. LOGGING REQUIREMENTS

Add useful logs around corrected flows:

```text
Pair requested
CancelPairing requested
Pairing -> CancellingPairing transition
CancelPairing D-Bus call
Reconnect scheduled
Reconnect attempt number
Reconnect failed/retryable
Reconnect retry scheduled
Reconnect max attempts reached
Reconnect paused
Reconnect resumed
BlueZ unavailable
BlueZ restored
Agent registered with capability
```

Never log:

```text
PIN
passkey secret
private pairing credential
```

---

# 40. DOCUMENTATION UPDATE

Update Phase 3 validation documentation.

Document:

```text
fixed cancel-pairing transition
reconnect retry semantics
retryable/non-retryable error policy
BlueZ pause/resume behavior
agent capability configuration
full live validation sequence
destructive test opt-in
```

Do not mark Phase 3 complete in documentation until the acceptance gate below actually passes.

---

# 41. REQUIRED PHASE 3 ACCEPTANCE GATE

PHASE 3 may be marked COMPLETE only when every mandatory item below is satisfied.

## Pair cancellation

- [ ] Pairing can transition to CancellingPairing.
- [ ] `Device1.CancelPairing()` is actually invoked.
- [ ] duplicate Cancel is safe.
- [ ] pending pairing UI request is removed.
- [ ] stale Pair callback cannot corrupt state.
- [ ] automated test proves this.

## Reconnect retry

- [ ] first automatic reconnect attempt is scheduled.
- [ ] failed retryable attempt schedules the next attempt.
- [ ] delay backoff works.
- [ ] retry count is bounded.
- [ ] maxAttempts stops reconnect.
- [ ] successful reconnect resets attempt state.
- [ ] non-retryable failure stops retries.
- [ ] automated lifecycle test proves the full sequence.

## Reconnect suppression

- [ ] explicit Disconnect cancels/suppresses reconnect.
- [ ] Forget cancels/suppresses reconnect.
- [ ] removed device cannot reconnect.
- [ ] application shutdown cancels timers safely.

## BlueZ recovery

- [ ] BlueZ loss pauses reconnect.
- [ ] BlueZ return resumes reconnect policy.
- [ ] registry rebuild completes before reconnect evaluation.
- [ ] agent is re-registered where required.
- [ ] intentional disconnect remains suppressed after recovery.
- [ ] automated test proves recovery behavior.

## Agent

- [ ] capability is represented explicitly.
- [ ] configured capability is passed to RegisterAgent.
- [ ] KeyboardDisplay can remain default.
- [ ] capability mapping tests pass.
- [ ] agent prompts remain asynchronous.
- [ ] cancellation invalidates requests.

## Credential safety

- [ ] PIN validated.
- [ ] passkey validated.
- [ ] stale request rejected.
- [ ] duplicate response rejected.
- [ ] credentials not logged.
- [ ] credentials not persisted.

## Regression

- [ ] clean configure passes.
- [ ] clean build passes.
- [ ] all ordinary ctest targets pass.
- [ ] Phase 2 discovery still works.
- [ ] application launches.

## Live validation

- [ ] Discover proven.
- [ ] Pair proven.
- [ ] Trust proven.
- [ ] Connect proven.
- [ ] Disconnect proven.
- [ ] Reconnect proven.
- [ ] Forget proven with explicit destructive opt-in.
- [ ] actual test device/address documented.
- [ ] no terminal Bluetooth control required for standard workflow.

---

# 42. REQUIRED FINAL REPORT

When finished, return a structured report.

Do not simply say "done."

## A. Root causes

Explain the original defects:

```text
CancelPairing rejection
single-attempt reconnect
permanent reconnect pause
hardcoded agent capability
credential-validation gaps
test gaps
```

## B. Files changed

For each:

```text
path
what changed
why
```

## C. Cancel-pairing fix

Explain the new operation-transition semantics.

Show how duplicate/stale events are prevented.

## D. Reconnect fix

Explain:

```text
attempt scheduling
backoff
max attempts
retryability
manual vs automatic reconnect
explicit disconnect suppression
forget suppression
success reset
```

## E. BlueZ recovery

Explain:

```text
pause
service recovery
snapshot/registry rebuild
resume
eligibility re-evaluation
```

## F. Agent capability

Show the capability enum/config mapping.

State the default.

## G. Credential validation

Explain PIN/passkey validation and secret handling.

## H. Tests

List every test added/modified and the behavior it proves.

## I. Build/test commands

Show exact commands executed.

## J. Actual results

Provide actual:

```text
number of tests
passed
failed
skipped
```

Do not invent results.

## K. Live hardware validation

State the exact device used and which operations were actually executed.

If hardware was unavailable, say:

```text
hardware validation not executed
```

Do not falsely claim Phase 3 COMPLETE without the live gate if the project's completion policy requires it.

## L. Final verdict

Return exactly one:

```text
PHASE 3: COMPLETE
```

or:

```text
PHASE 3: NOT COMPLETE
```

If NOT COMPLETE, list the exact blockers.

---

# 43. IMPORTANT — DO NOT ACCEPT PARTIAL FIXES

The following are NOT sufficient:

```text
Cancel button exists
CancelPairing method exists somewhere
ReconnectPolicy has maxAttempts variables
ReconnectPolicy unit test manually calls scheduleReconnect repeatedly
BlueZ recovery works for discovery only
Agent capability is stored as one string constant
QML validates passkeys but backend does not
tests compile but are not executed
documentation claims success without hardware evidence
```

The real execution path must work end-to-end.

---

# 44. EXPECTED TARGET BEHAVIOR

After remediation:

```text
Auralis starts
   ↓
BlueZ available
   ↓
Agent registered using configured capability
   ↓
Discover device
   ↓
Pair
   ↓
Pairing...
   ├── Cancel -> CancelPairing really issued
   └── Success -> Paired=true
   ↓
Trust
   ↓
Trusted=true
   ↓
Connect
   ↓
Connected=true
   ↓
Unexpected disconnect
   ↓
bounded reconnect attempts with backoff
   ↓
Connected=true OR retry budget exhausted
```

And separately:

```text
user Disconnect
   ↓
no auto-reconnect
```

```text
Forget
   ↓
no auto-reconnect
device removed
```

```text
BlueZ disappears
   ↓
reconnect paused
   ↓
BlueZ returns
   ↓
registry rebuilt
agent restored
reconnect resumed
```

---

# 45. START NOW

Start with the existing repository.

Do not rewrite working Phase 3 architecture.

Fix the known defects in this order:

```text
1. Pairing -> CancellingPairing
2. CancelPairing tests
3. reconnect retry continuation
4. reconnect pause/resume after BlueZ recovery
5. reconnect lifecycle tests
6. agent capability abstraction/configuration
7. PIN/passkey backend validation
8. live/manual Phase 3 validation expansion
9. clean build + full ctest
10. final completion report
```

Preserve PHASE 0, PHASE 1, and PHASE 2.

Do not begin PHASE 4.

The objective is to convert the current state from:

```text
PHASE 3: substantially implemented
```

into:

```text
PHASE 3: COMPLETE
```

with code, tests, and validation evidence—not merely source-level presence.
