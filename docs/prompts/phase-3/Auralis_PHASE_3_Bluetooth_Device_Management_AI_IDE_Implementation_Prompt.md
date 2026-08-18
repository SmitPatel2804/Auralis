# Auralis — PHASE 3 Implementation Prompt
## Bluetooth Device Management

**Purpose:** Copy/paste this document into an AI coding IDE/agent working inside the existing Auralis repository.

**Project:** Auralis  
**Target platform:** Ubuntu Linux laptop  
**Phase:** PHASE 3 — Bluetooth Device Management  
**Prerequisite:** PHASE 0, PHASE 1, and PHASE 2 are already implemented and must remain working.  
**Primary stack:** C++ / Qt 6 / QML / CMake / Ninja / BlueZ over system D-Bus  
**Phase 3 milestone:** Device pairing and connection lifecycle works end-to-end from Auralis without requiring terminal commands.

---

# 1. YOUR ROLE

You are the senior C++/Qt/Linux Bluetooth engineer responsible for implementing **PHASE 3 — Bluetooth Device Management** in the existing Auralis codebase.

Do not create a replacement project.

Do not reimplement PHASE 0–2 from scratch.

Do not redesign working Phase 2 discovery architecture unless a narrowly scoped refactor is genuinely required to support Phase 3.

Your job is to **inspect the current repository first**, understand the architecture and conventions already established, and extend that implementation cleanly.

You must preserve all existing working behavior while adding production-quality Bluetooth device lifecycle management.

The result must be maintainable, asynchronous, testable, and suitable as the foundation for later PipeWire audio integration.

---

# 2. SOURCE OF TRUTH AND PHASE DEFINITION

For this repository, **PHASE 3 means Bluetooth Device Management**.

It does **not** mean multi-device audio routing, synchronization, PipeWire endpoint mapping, or playback.

The Phase 3 objective is:

> Implement the complete lifecycle of a Bluetooth device inside Auralis.

The required user operations are:

- Pair
- Cancel pairing
- Trust
- Untrust
- Connect
- Disconnect
- Remove / Forget
- Reconnect
- View supported services

The required logical lifecycle includes:

```text
UNKNOWN
   |
   v
DISCOVERED
   |
   v
PAIRING
   |
   v
PAIRED
   |
   v
TRUSTED
   |
   v
CONNECTING
   |
   v
CONNECTED
```

and recovery / terminal states such as:

```text
DISCONNECTED
RECONNECTING
FAILED
REMOVED
```

The Phase 3 exit gate is:

```text
Discover
Pair
Trust
Connect
Disconnect
Forget
Reconnect
```

with every state change correctly reflected in the Auralis application model and UI.

---

# 3. NON-NEGOTIABLE ARCHITECTURAL PRINCIPLES

The following rules are mandatory.

## 3.1 No production shell-command control

Production code must **not** invoke or parse:

```text
bluetoothctl
btmgmt
busctl
dbus-send
gdbus
pactl
wpctl
```

These tools may be mentioned only as optional manual diagnostics.

Bluetooth lifecycle operations must use **BlueZ D-Bus APIs directly**.

## 3.2 Preserve separation of concerns

Do not merge UI, D-Bus transport, domain state, persistence, and policy into one class.

Retain or evolve the repository toward clear ownership:

```text
BluetoothManager
    |
    +-- BlueZ D-Bus communication
    +-- device lifecycle orchestration
    +-- connection operations

DeviceRegistry
    |
    +-- stable BluetoothDevice objects
    +-- device lookup/deduplication
    +-- model update propagation

BlueZAgent
    |
    +-- org.bluez.Agent1 implementation
    +-- pairing prompts / authorization

PersistenceManager or equivalent
    |
    +-- user-owned Auralis metadata
    +-- no duplication of BlueZ's bond database

Qt/QML presentation
    |
    +-- display state
    +-- invoke backend actions
    +-- never fabricate Bluetooth state
```

Adapt class names to the existing codebase where necessary.

Do **not** force these exact names if Phase 2 already established equivalent abstractions.

## 3.3 BlueZ is authoritative for Bluetooth state

The UI must never decide that a device is paired or connected merely because an operation was requested.

For authoritative Bluetooth properties such as:

- `Paired`
- `Connected`
- `Trusted`
- `Blocked`
- `ServicesResolved`
- `UUIDs`
- `Alias`

the source of truth must be BlueZ D-Bus properties and asynchronous property-change events.

Command completion and property state are related but not identical.

## 3.4 Do not begin PHASE 4

PHASE 4 is PipeWire audio integration.

Do not implement:

- PipeWire graph enumeration
- Bluetooth-to-PipeWire endpoint mapping
- AudioEndpointRegistry
- audio stream creation
- route creation
- playback routing
- multi-output audio
- latency/synchronization engine

It is acceptable for a successfully connected Bluetooth audio device to cause PipeWire/WirePlumber to create objects naturally, but Auralis must **not** manage those objects yet.

---

# 4. FIRST ACTION: REPOSITORY AUDIT

Before editing code, inspect the complete repository.

At minimum inspect:

```text
CMakeLists.txt
apps/
src/
include/
ui/
tests/
config/
docs/
```

Identify the actual current implementation of:

- `ApplicationCore`
- `BluetoothManager`
- `BlueZDbusClient` or equivalent
- adapter management
- discovery management
- `BluetoothDevice`
- `DeviceRegistry`
- Qt model exposed to QML
- current Phase 2 QML
- configuration system
- logging system
- unit-test framework
- integration-test framework
- live BlueZ integration test(s)
- CMake target structure

Look specifically for existing Phase 2 behavior around:

- `org.freedesktop.DBus.ObjectManager`
- `InterfacesAdded`
- `InterfacesRemoved`
- `PropertiesChanged`
- `org.bluez.Adapter1`
- `org.bluez.Device1`
- object path tracking
- discovery start/stop
- adapter availability
- device identity/deduplication
- BlueZ service-owner changes
- live test environment variables

There has already been Phase 2 validation work around a BlueZ live integration test. Preserve the existing opt-in hardware-test philosophy and naming conventions instead of introducing a conflicting mechanism.

After the audit, make the smallest architecture-compatible set of changes required for Phase 3.

---

# 5. REQUIRED BLUEZ D-BUS SURFACE

Use the system bus and service:

```text
org.bluez
```

Phase 3 will primarily interact with the following interfaces.

## 5.1 `org.bluez.Device1`

Use the BlueZ device object already discovered and tracked by Phase 2.

Required methods:

```text
Pair()
CancelPairing()
Connect()
Disconnect()
```

Potentially useful later but not mandatory for the core Phase 3 gate:

```text
ConnectProfile(uuid)
DisconnectProfile(uuid)
```

Do not use profile-specific connection as the default unless the current device/architecture requires it.

The generic `Connect()` path should remain the ordinary Phase 3 operation.

Important properties include:

```text
Address
AddressType
Name
Alias
UUIDs
Paired
Connected
Trusted
Blocked
ServicesResolved
Adapter
RSSI
Icon
Class
Appearance
```

The existing `BluetoothDevice` model from Phase 2 should already cover many of these.

Extend it only where Phase 3 needs additional lifecycle/presentation state.

## 5.2 `org.freedesktop.DBus.Properties`

Use this interface for writable BlueZ properties.

At minimum Phase 3 needs:

```text
org.bluez.Device1.Trusted
```

Operations:

```text
Trust   => set Trusted = true
Untrust => set Trusted = false
```

Do not maintain a fake local `trusted` value that diverges from BlueZ.

## 5.3 `org.bluez.Adapter1`

Use the owning adapter object for:

```text
RemoveDevice(deviceObjectPath)
```

This is the Phase 3 **Forget** operation.

Remember:

- Forget is not merely Disconnect.
- Forget removes BlueZ's stored device/pairing information.
- BlueZ may remove the corresponding `Device1` object.
- Phase 2's `InterfacesRemoved` handling must therefore cooperate with Phase 3.

## 5.4 `org.bluez.AgentManager1`

Auralis should be capable of registering its own pairing agent when appropriate.

Expected operations:

```text
RegisterAgent(agentPath, capability)
UnregisterAgent(agentPath)
```

Use `RequestDefaultAgent(agentPath)` only if the product behavior truly requires Auralis to become the system-wide default agent.

Do **not** seize global default-agent ownership unnecessarily.

A pairing initiated by Auralis should use the application agent when BlueZ associates the registered agent with the initiating application.

## 5.5 `org.bluez.Agent1`

Implement an exported D-Bus object for pairing interactions.

Support the BlueZ agent methods required for robust pairing, including the relevant subset of:

```text
Release()
RequestPinCode(device)
DisplayPinCode(device, pincode)
RequestPasskey(device)
DisplayPasskey(device, passkey, entered)
RequestConfirmation(device, passkey)
RequestAuthorization(device)
AuthorizeService(device, uuid)
Cancel()
```

Implement the complete interface shape expected by BlueZ even if some methods are uncommon with the initial target hardware.

Each method must have deterministic behavior and appropriate D-Bus success/error replies.

---

# 6. PAIRING AGENT DESIGN

Create or complete a dedicated pairing-agent component.

Example conceptual structure:

```text
BlueZAgent
|
+-- lifecycle
|   +-- registerAgent()
|   +-- unregisterAgent()
|   +-- registered
|
+-- capability
|
+-- pairing requests
|   +-- PIN request
|   +-- passkey request
|   +-- passkey display
|   +-- confirmation
|   +-- authorization
|   +-- service authorization
|
+-- cancellation
|
+-- signals to application/UI
```

Use the project's existing Qt D-Bus style.

## 6.1 Capability

Support a configurable BlueZ IO capability abstraction.

At minimum represent:

```text
NoInputNoOutput
DisplayOnly
DisplayYesNo
KeyboardOnly
KeyboardDisplay
```

Choose a sensible default for the current Auralis laptop UI.

A desktop application capable of displaying information and receiving user confirmation can generally support a richer interaction than `NoInputNoOutput`.

However:

- do not hardcode hearing devices to a pairing behavior they may not use;
- keep capability policy configurable/extensible;
- support Just Works devices cleanly;
- do not assume every Bluetooth audio device displays a PIN.

## 6.2 User-interaction model

Design a clean asynchronous pairing-prompt model.

For example, the backend may expose a pending pairing request object containing:

```text
requestId
deviceId
deviceObjectPath
requestType
pinCode
passkey
enteredDigits
serviceUuid
message
```

Possible request types:

```text
DisplayPin
EnterPin
DisplayPasskey
EnterPasskey
ConfirmPasskey
AuthorizePairing
AuthorizeService
```

Do not block the Qt GUI thread waiting for modal input.

Expose an asynchronous response path such as:

```text
acceptPairingRequest(requestId)
rejectPairingRequest(requestId)
submitPinCode(requestId, value)
submitPasskey(requestId, value)
```

Use whatever shape best fits the existing repository.

## 6.3 D-Bus error behavior

Map user rejection or cancellation to the appropriate BlueZ-compatible D-Bus agent error semantics.

Do not crash, hang, or leave the agent call unresolved.

Every incoming Agent1 request must eventually receive exactly one reply or error.

## 6.4 Agent lifecycle

Register the agent only after:

- system D-Bus is available;
- BlueZ is available;
- the object has been exported successfully.

Unregister it during orderly shutdown when registered.

Handle:

- BlueZ daemon restart;
- service disappearance;
- service reappearance;
- stale registration assumptions;
- duplicate registration attempts.

If BlueZ restarts, local `registered=true` cannot be blindly trusted.

Re-establish state safely when the service returns.

---

# 7. DEVICE LIFECYCLE STATE MODEL

Do not overload BlueZ's raw properties with application-operation state.

There are two categories of state.

## 7.1 Authoritative device facts

These come directly from BlueZ:

```text
paired
trusted
connected
servicesResolved
blocked
```

## 7.2 Transient Auralis operation state

These represent what Auralis is currently doing:

```text
Idle
Pairing
CancellingPairing
Trusting
Untrusting
Connecting
Disconnecting
Forgetting
Reconnecting
```

and failure metadata:

```text
lastOperation
lastErrorCode
lastErrorName
lastErrorMessage
lastErrorTimestamp
```

Build the higher-level UI state from these two sources.

Example:

```text
if operation == Pairing:
    logicalState = PAIRING
else if operation == Connecting:
    logicalState = CONNECTING
else if operation == Reconnecting:
    logicalState = RECONNECTING
else if connected:
    logicalState = CONNECTED
else if trusted:
    logicalState = TRUSTED
else if paired:
    logicalState = PAIRED
else if device exists:
    logicalState = DISCOVERED
else:
    logicalState = REMOVED
```

Treat this as conceptual guidance, not mandatory exact code.

Do not discard useful distinctions.

---

# 8. VALID STATE TRANSITIONS

Define and test legal transitions.

A representative flow:

```text
DISCOVERED
    |
    | Pair
    v
PAIRING
    |
    | BlueZ Paired=true
    v
PAIRED
    |
    | Trust
    v
TRUSTED
    |
    | Connect
    v
CONNECTING
    |
    | BlueZ Connected=true
    v
CONNECTED
    |
    | Disconnect
    v
DISCONNECTED
```

Forget path:

```text
PAIRED/TRUSTED/CONNECTED/DISCONNECTED
    |
    | Forget
    v
FORGETTING
    |
    | Adapter1.RemoveDevice()
    | + InterfacesRemoved
    v
REMOVED
```

Reconnect path:

```text
DISCONNECTED
    |
    | Reconnect
    v
RECONNECTING
    |
    | Device1.Connect()
    | + Connected=true
    v
CONNECTED
```

Failure path:

```text
ANY OPERATIONAL STATE
    |
    | D-Bus failure / timeout / BlueZ loss
    v
FAILED or stable BlueZ-derived state + error metadata
```

Prefer preserving the real BlueZ-derived device state plus an operation error rather than permanently trapping a device in an artificial `FAILED` state.

---

# 9. REQUIRED BACKEND OPERATIONS

Expose Phase 3 actions at the appropriate service layer.

The exact API should follow current code conventions.

Conceptually:

```cpp
pairDevice(deviceId)
cancelPairing(deviceId)

trustDevice(deviceId)
untrustDevice(deviceId)

connectDevice(deviceId)
disconnectDevice(deviceId)

forgetDevice(deviceId)

reconnectDevice(deviceId)

supportedServices(deviceId)
```

If the project uses stable internal IDs, prefer them at the application boundary and resolve to BlueZ object paths internally.

Do not make QML responsible for constructing BlueZ object paths.

---

# 10. PAIR OPERATION

Implement pairing through:

```text
org.bluez.Device1.Pair()
```

Requirements:

1. Resolve device from `DeviceRegistry`.
2. Validate device still has a live BlueZ object path.
3. Reject or no-op appropriately if already paired.
4. Mark Auralis transient operation as `Pairing`.
5. Invoke `Pair()` asynchronously.
6. Never block the GUI event loop.
7. Observe BlueZ `Paired` changes.
8. Observe `ServicesResolved`/`UUIDs` as they become available.
9. Clear operation state on success/failure.
10. Expose structured errors.
11. Handle agent interaction if required.
12. Handle device removal during pairing.
13. Handle BlueZ daemon loss during pairing.

Do not infer success only from the D-Bus method return.

The final model must reconcile with `Paired`.

---

# 11. CANCEL PAIRING OPERATION

Use:

```text
org.bluez.Device1.CancelPairing()
```

Requirements:

- only act on an existing device;
- handle the case where no pairing is active;
- ensure any pending Auralis agent UI request is invalidated;
- clear transient pairing state correctly;
- propagate a meaningful result to QML;
- do not leave a stale prompt after cancellation;
- handle BlueZ's own `Agent1.Cancel()` callback.

---

# 12. TRUST / UNTRUST OPERATIONS

Implement by setting:

```text
org.bluez.Device1
property:
Trusted = true / false
```

Requirements:

- asynchronous property set;
- authoritative state confirmed from BlueZ;
- avoid duplicate writes if state already matches;
- structured failure reporting;
- UI action enablement based on current state.

Important:

`Trusted` is not the same thing as `Paired` and not the same thing as `Connected`.

Do not collapse them into one boolean.

---

# 13. CONNECT OPERATION

Use:

```text
org.bluez.Device1.Connect()
```

Requirements:

1. Resolve a live device.
2. Validate adapter/BlueZ availability.
3. Prevent duplicate concurrent connect attempts.
4. Set transient operation to `Connecting`.
5. Invoke asynchronously.
6. Observe `Connected`.
7. Observe `ServicesResolved` if relevant.
8. Clear operation state deterministically.
9. Handle already-connected state gracefully.
10. Handle `InProgress` without producing broken state.
11. Handle device disappearance.
12. Handle failure/rejection/timeouts.
13. Preserve reconnect eligibility.

Do not use `ConnectProfile()` as the default shortcut for Phase 3.

---

# 14. DISCONNECT OPERATION

Use:

```text
org.bluez.Device1.Disconnect()
```

Requirements:

- asynchronous;
- graceful idempotency;
- transient `Disconnecting` state;
- final state driven by `Connected=false`;
- handle already-disconnected behavior;
- do not automatically forget the device;
- preserve paired/trusted information;
- cancel any pending reconnect timer/policy when the disconnect was explicitly requested by the user.

This last rule is critical:

> An intentional user disconnect must not immediately trigger automatic reconnection.

Track whether disconnection was expected/user-initiated.

---

# 15. FORGET / REMOVE OPERATION

Implement through the owning adapter:

```text
org.bluez.Adapter1.RemoveDevice(deviceObjectPath)
```

Requirements:

1. Determine the correct adapter path.
2. Confirm that the `Device1` object belongs to that adapter.
3. Stop/cancel outstanding operations for that device.
4. Cancel pending reconnect behavior.
5. Optionally disconnect first only if required by existing architecture; do not unnecessarily serialize operations if BlueZ can remove directly.
6. Invoke `RemoveDevice`.
7. Expect `InterfacesRemoved`.
8. Ensure `DeviceRegistry` removes the correct object.
9. Remove or update Auralis-owned persistence.
10. Clear pairing-agent requests associated with the device.
11. QML must reflect removal immediately when registry change is authoritative.

Do not simulate forgetting by only setting `Trusted=false`.

---

# 16. SUPPORTED SERVICES

Expose a device's supported services/capabilities based on BlueZ `UUIDs`.

Requirements:

- show raw canonical UUIDs where appropriate for diagnostics;
- optionally map common UUIDs to friendly names in a utility layer;
- do not hardcode the product to only one hearing-device profile;
- maintain unknown UUIDs instead of discarding them;
- update when `UUIDs` changes after pairing/service discovery.

Useful user-facing representation may contain:

```text
Service name
UUID
known/unknown
```

Do not make Phase 3 depend on PipeWire to determine service support.

---

# 17. RECONNECT ARCHITECTURE

Phase 3 requires reconnect support.

Implement reconnect in two layers:

## 17.1 Manual reconnect

A user action that calls the connection path for a previously paired/trusted disconnected device.

This is mandatory.

## 17.2 Bounded automatic reconnect policy

Implement a conservative application-level auto-reconnect capability if it fits the existing architecture.

Do not create an uncontrolled infinite reconnect loop.

Recommended policy model:

```text
enabled
maxAttempts
initialDelay
maxDelay
backoffMultiplier
```

Example behavior:

```text
attempt 1 -> short delay
attempt 2 -> longer delay
attempt 3 -> longer delay
...
stop after configured maximum
```

Use Qt timers/event-loop mechanisms, not sleeps.

Default values should be explicit in configuration or constants with documentation.

### Automatic reconnect should be eligible only when:

- the device remains known;
- it is paired;
- policy allows it;
- disconnection was unexpected;
- BlueZ is available;
- adapter is available/powered;
- no explicit disconnect/forget is in progress.

### Automatic reconnect must stop when:

- user selects Disconnect;
- user selects Forget;
- device is removed;
- application shuts down;
- retry budget is exhausted;
- policy is disabled;
- a non-retryable error is detected.

A successful connection resets the retry counter.

---

# 18. OPERATION SERIALIZATION AND CONCURRENCY

Bluetooth is asynchronous.

Protect the system from contradictory operations.

Examples that must be handled:

```text
Connect clicked twice
Pair clicked twice
Forget while Pair is active
Disconnect while Connect is pending
Connect while Forget is pending
Reconnect timer fires during manual Disconnect
Device disappears during an operation
BlueZ restarts during an operation
```

Use an explicit per-device operation model or equivalent.

Do not rely on button disabling alone for correctness.

Backend correctness must survive repeated or racing calls from any client.

Prefer:

- per-device operation token/generation ID;
- per-device pending call tracking;
- clear operation completion;
- stale callback rejection;
- deterministic cancellation.

Avoid:

- arbitrary sleeps;
- busy waiting;
- nested event loops;
- blocking system-bus calls on the GUI thread where they may stall the UI;
- detached threads with unmanaged Qt object lifetimes.

---

# 19. D-BUS ASYNC CALL WRAPPER

If Phase 2 already has a reusable D-Bus client abstraction, extend it.

Do not bypass it with one-off QDBus calls scattered through UI-facing classes.

A useful abstraction may centralize:

```text
service
object path
interface
method
arguments
timeout
reply
error mapping
logging
```

Create structured error data, for example:

```text
BluetoothError
|
+-- operation
+-- deviceId
+-- dbusErrorName
+-- message
+-- category
+-- retryable
```

Potential categories:

```text
BlueZUnavailable
AdapterUnavailable
DeviceUnavailable
NotReady
InProgress
AlreadyConnected
NotConnected
AuthenticationCanceled
AuthenticationFailed
AuthenticationRejected
AuthenticationTimeout
ConnectionFailed
NotSupported
InvalidArguments
TimedOut
PermissionDenied
Unknown
```

Do not make UI code parse raw error strings.

Preserve the original D-Bus error name/message for diagnostics.

---

# 20. BLUEZ SERVICE RESTART / OWNER CHANGE

Phase 3 must not assume `bluetoothd` lives forever.

Reuse Phase 2 service-owner monitoring if it already exists.

When `org.bluez` disappears:

- mark backend unavailable;
- cancel/invalidate pending D-Bus operations;
- cancel pending agent requests;
- invalidate local agent registration state;
- pause reconnect attempts;
- keep safe Auralis-owned persisted metadata;
- avoid crashes or use-after-free;
- reflect backend-unavailable state in UI.

When `org.bluez` returns:

- rebuild/reconcile the managed object snapshot;
- restore adapter/device registry state through the Phase 2 pipeline;
- re-register the pairing agent where appropriate;
- restart eligible reconnect policy only after the registry is valid.

Do not duplicate BlueZ object-manager reconstruction logic if Phase 2 already handles it.

---

# 21. DEVICE OBJECT REMOVAL

Phase 2 already handles D-Bus object removal.

Phase 3 must integrate lifecycle state with it.

When a `Device1` interface disappears:

- terminate pending operations for that path safely;
- invalidate pairing requests;
- cancel timers;
- remove stale object references;
- let `DeviceRegistry` remove/update the stable device object according to existing design;
- do not dereference deleted QObjects from asynchronous callbacks.

When the disappearance is the expected consequence of `Forget`, treat it as success rather than an unexpected failure.

---

# 22. DEVICE IDENTITY

Continue Phase 2's identity strategy.

Do not regress to using display name as identity.

Preferred identifiers:

```text
internal stable Auralis ID
BlueZ object path
Bluetooth address / identity address
adapter identity
```

Be careful with LE privacy/random addresses.

Do not invent address normalization rules that contradict Phase 2 behavior.

If the current registry already handles identity changes or deduplication, preserve it.

---

# 23. PERSISTENCE SCOPE

Phase 3 may add Auralis-owned persistent device metadata.

Conceptual record:

```text
DeviceRecord
|
+-- internalId
+-- address
+-- alias/userLabel
+-- preferredRole
+-- lastConnected
+-- autoReconnect
+-- reconnect policy metadata
+-- future sessionMembership
```

Important boundaries:

- BlueZ owns Bluetooth bonding/pairing keys.
- Auralis must **not** copy or manage BlueZ key material.
- Never persist PIN/passkey data.
- Never log secret pairing credentials.
- Do not manipulate `/var/lib/bluetooth` directly.
- Do not require root access.
- Do not edit BlueZ's storage files.

The `trusted` value can be cached for presentation if the architecture demands it, but BlueZ remains authoritative whenever available.

Do not implement Phase 6 session membership behavior beyond harmless schema placeholders already present in the roadmap.

---

# 24. UI / QML REQUIREMENTS

Phase 3 does not need the final Phase 7 visual design.

It **does** need a functional control surface for validating every lifecycle operation.

Extend the existing Phase 2 device list rather than replacing it with an unrelated UI.

Each device row/card should expose enough state to understand what is happening.

Suggested information:

```text
Name / Alias
Address
Address type
RSSI (when available)
Paired
Trusted
Connected
Services resolved
Lifecycle/operation state
Last operation error
```

Suggested actions, enabled only when meaningful:

```text
Pair
Cancel Pairing
Trust
Untrust
Connect
Disconnect
Reconnect
Forget
Services
```

## 24.1 Busy-state behavior

For a pending operation, show clear state such as:

```text
Pairing...
Connecting...
Disconnecting...
Forgetting...
Reconnecting (attempt 2/5)...
```

Prevent accidental duplicate UI submissions but retain backend-level safeguards.

## 24.2 Pairing prompts

Provide minimal QML handling for:

- confirmation prompt;
- PIN/passkey entry;
- displayed PIN/passkey;
- authorization;
- cancel/reject.

Do not depend on terminal input.

## 24.3 Error display

Show user-readable operation errors without exposing only cryptic D-Bus names.

For diagnostics, preserve access to technical details.

A simple development-phase error area/dialog is acceptable.

## 24.4 State truth

Never optimistically show:

```text
Connected
Paired
Trusted
```

until authoritative state says so.

It is fine to show:

```text
Connecting...
Pairing...
Trusting...
```

while awaiting the result.

---

# 25. APPLICATION/API SIGNALS

Expose clean signals suitable for QML and tests.

Adapt names to the current codebase.

Potential examples:

```text
deviceOperationStarted(deviceId, operation)
deviceOperationFinished(deviceId, operation)
deviceOperationFailed(deviceId, operation, error)

pairingRequestAdded(...)
pairingRequestUpdated(...)
pairingRequestRemoved(...)

agentRegisteredChanged(...)
blueZAvailableChanged(...)
```

Do not emit redundant signals if existing `BluetoothDevice` property notifications already cover the required state.

Prefer one coherent model over duplicate state channels.

---

# 26. C++ / QT ENGINEERING REQUIREMENTS

Follow modern, safe project conventions.

## 26.1 Object ownership

Use explicit Qt/C++ ownership.

Avoid:

- raw owning pointers;
- unmanaged detached work;
- dangling callbacks;
- QObject deletion races.

Use:

- parent ownership where appropriate;
- RAII for non-QObject resources;
- guarded pointers where asynchronous lifetime requires them;
- context-bound Qt signal connections.

## 26.2 Threading

Keep the design event-driven.

D-Bus calls should integrate with the Qt event loop.

Do not add threads unless there is a demonstrated requirement.

Do not use `sleep()` to sequence Bluetooth state.

## 26.3 Logging

Use the existing Auralis logging system.

Log at useful boundaries:

```text
operation requested
D-Bus call issued
D-Bus reply success/failure
authoritative BlueZ property transition
pairing-agent request
pairing-agent response
reconnect attempt
BlueZ service loss/recovery
device removal
```

Include stable device identifiers.

Redact:

- PIN codes;
- passkeys;
- secret credentials.

Do not spam high-frequency RSSI updates at high log severity.

## 26.4 No silent error swallowing

Every D-Bus failure must either:

- be handled as an expected benign/idempotent case; or
- become structured diagnostic state/log output.

---

# 27. CMAKE / BUILD REQUIREMENTS

Integrate Phase 3 cleanly into existing targets.

Do not create duplicate Bluetooth libraries unless repository structure genuinely requires it.

Update:

```text
source lists
headers
Qt D-Bus dependencies
test targets
QML resources/modules
```

only as necessary.

Preserve build commands:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The desktop application must continue to launch as:

```bash
./build/apps/desktop/auralis-desktop
```

or the exact existing Phase 1/2 path if it differs.

A clean build from scratch must work.

---

# 28. TEST STRATEGY

Testing is part of implementation, not optional follow-up work.

Implement three layers where appropriate.

---

# 29. UNIT TESTS — HARDWARE INDEPENDENT

Unit tests must run without a Bluetooth adapter or BlueZ daemon wherever practical.

Create fakes/interfaces around external D-Bus effects as needed.

Test at minimum:

## 29.1 State derivation

Cases such as:

```text
discovered
pairing
paired
trusted
connecting
connected
disconnecting
disconnected
reconnecting
forgetting
removed
failed operation
```

## 29.2 Operation validity

Examples:

- Pair on unpaired device.
- Pair on already paired device.
- Cancel pairing while pairing.
- Connect on connected device.
- Disconnect on disconnected device.
- Forget during another operation.
- Trust/untrust idempotency.
- Duplicate requests.
- stale async callback ignored.

## 29.3 Reconnect policy

Test:

- bounded attempt count;
- backoff progression;
- reset after success;
- cancellation on user disconnect;
- cancellation on forget;
- pause on BlueZ loss;
- no reconnect for intentional disconnect;
- no reconnect after device removal.

Use deterministic/fake timing where possible.

## 29.4 Error mapping

Map representative D-Bus error names to structured error categories.

Do not assert only free-form log text.

## 29.5 Agent request model

Test:

- each request type;
- acceptance;
- rejection;
- cancellation;
- invalid/expired request ID;
- duplicate response protection;
- secret value lifetime where practical.

---

# 30. D-BUS CONTRACT / COMPONENT TESTS

Use a mock/fake D-Bus backend or abstract client to verify exact operations.

Validate:

```text
Pair -> Device1.Pair
CancelPairing -> Device1.CancelPairing
Connect -> Device1.Connect
Disconnect -> Device1.Disconnect
Trust -> Properties.Set(Device1, Trusted, true)
Untrust -> Properties.Set(Device1, Trusted, false)
Forget -> Adapter1.RemoveDevice(devicePath)
```

Validate agent registration:

```text
RegisterAgent(path, capability)
UnregisterAgent(path)
```

Validate that Auralis exports the Agent1 interface at the expected object path.

Test malformed replies and D-Bus errors.

---

# 31. INTEGRATION TESTS — BLUEZ WITHOUT REQUIRED PHYSICAL DEVICE

Where possible, add tests that validate:

- system-bus/BlueZ availability detection;
- object-manager parsing;
- agent object export;
- registration behavior when BlueZ exists;
- graceful skip when live environment prerequisites are not enabled.

Hardware/environment-dependent tests must not make ordinary `ctest` fail on CI or on machines without the expected Bluetooth setup.

Use the project's existing opt-in live-test mechanism.

---

# 32. LIVE HARDWARE INTEGRATION TEST

Extend the existing opt-in BlueZ live integration strategy.

The live test must require an explicit environment flag before it performs real pairing/connection mutation.

Use existing Phase 2 environment variable conventions when available rather than inventing a parallel standard.

The test device address must be a **real environment value**, not a shell placeholder such as:

```text
<TEST_DEVICE_ADDRESS>
```

which the shell interprets incorrectly.

A conceptual invocation may look like:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF \
ctest --test-dir build \
-R <phase3-live-test-name> \
--output-on-failure
```

Choose the actual test name to fit the repository.

## 32.1 Safety of live tests

Never automatically forget an arbitrary user's paired Bluetooth device in default testing.

Destructive operations must require an additional explicit opt-in if tested live.

For example:

```text
AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1
```

or an equivalent repository-consistent mechanism.

Without destructive opt-in:

- discover;
- inspect;
- optionally pair/connect/disconnect if explicitly enabled;
- do not forget existing devices.

The test should print clear skip reasons.

---

# 33. LIVE PHASE 3 VALIDATION MATRIX

Manual or opt-in automated validation should cover:

## Scenario A — clean pairing

```text
device unpaired
scan
select device
pair
respond to agent prompt if necessary
observe Paired=true
```

## Scenario B — trust

```text
paired device
Trust
observe Trusted=true
Untrust
observe Trusted=false
Trust again if desired
```

## Scenario C — connect

```text
paired device
Connect
observe Connecting
observe Connected=true
```

## Scenario D — disconnect

```text
connected device
Disconnect
observe Disconnecting
observe Connected=false
ensure no unintended auto-reconnect
```

## Scenario E — manual reconnect

```text
disconnected paired device
Reconnect
observe Reconnecting
observe Connected=true
```

## Scenario F — forget

```text
explicit destructive test device
Forget
observe Device1 removed / registry updated
scan again
device may reappear as an unpaired discovered object
```

## Scenario G — failure

Test at least:

- device switched off/out of range;
- connect failure;
- pairing canceled;
- BlueZ unavailable/restarted where practical.

Auralis must remain responsive.

---

# 34. EDGE CASES THAT MUST BE HANDLED

At minimum reason about and implement safe behavior for:

```text
BlueZ unavailable at startup
BlueZ exits while operation is pending
BlueZ returns later
adapter missing
adapter removed
adapter powered off
device disappears
device reappears
device already paired
device already connected
connect already in progress
pair already in progress
authentication rejected
authentication timeout
pairing canceled from remote side
pairing canceled from Auralis
services resolve after Pair reply
device has empty/missing optional name
device has unknown service UUIDs
random/private LE address
forget causes InterfacesRemoved before/around method reply
late D-Bus callback after object removal
user double-clicks operation
user requests Disconnect during reconnect delay
application exits with pending agent request
```

Do not add brittle special cases for one specific device unless isolated behind a capability/policy abstraction.

---

# 35. SECURITY / PRIVACY REQUIREMENTS

Bluetooth pairing is security-sensitive.

Mandatory rules:

- never log PIN/passkey secrets;
- never persist PIN/passkey secrets;
- clear transient credential data after response;
- reject invalid input ranges/types;
- do not silently auto-confirm all pairing requests;
- do not automatically authorize unknown services without an explicit documented policy;
- do not run the entire application as root;
- do not loosen system D-Bus policy globally;
- do not edit BlueZ system configuration as part of normal Phase 3 operation;
- do not read/write BlueZ bond keys directly.

If a permission problem occurs, report it clearly rather than weakening machine security.

---

# 36. UX POLICY FOR PAIRING AUTHORIZATION

For a development-quality Phase 3 UI:

- Display-only information can be shown directly.
- Confirmation requests require explicit user confirmation unless the selected capability/protocol guarantees a Just Works path.
- PIN/passkey requests require explicit input.
- Unknown service authorization should not be invisibly accepted by a blanket rule without explanation.

Keep the policy layer separable so future hearing-device-specific behavior can be added without rewriting D-Bus plumbing.

---

# 37. DO NOT OVERFIT TO A2DP

Phase 3 is Bluetooth lifecycle management.

A discovered hearing device may expose:

- Bluetooth Classic services;
- A2DP;
- HFP/HSP;
- BLE GATT;
- LE Audio-related services;
- proprietary services;
- combinations of these.

Do not require an A2DP UUID merely to pair/trust/manage a device.

Service-capability interpretation should remain extensible.

PipeWire/audio-profile selection comes later.

---

# 38. REQUIRED DOCUMENTATION CHANGES

Update project documentation as part of Phase 3.

At minimum document:

- Phase 3 architecture;
- pairing agent behavior;
- supported operations;
- transient vs authoritative state;
- reconnect policy;
- live integration test instructions;
- environment flags;
- any limitations;
- manual validation procedure.

If there is an existing phase-status document, mark Phase 3 complete **only after all exit criteria pass**.

Do not prematurely change roadmap status simply because code compiles.

---

# 39. SUGGESTED IMPLEMENTATION ORDER

Implement in small verified increments.

## Increment 1 — Audit and interfaces

- inspect existing Phase 2 architecture;
- define lifecycle operation enum/state;
- define structured error model;
- extend `BluetoothDevice` only where necessary;
- create test seams for D-Bus actions.

Gate:

```text
existing Phase 2 tests still pass
new hardware-independent state tests pass
```

## Increment 2 — Basic device operations

Implement:

```text
Trust
Untrust
Connect
Disconnect
Forget
```

with asynchronous D-Bus calls and error mapping.

Gate:

```text
mock D-Bus contract tests pass
UI can trigger operations
state comes from BlueZ properties
```

## Increment 3 — Pairing agent

Implement/export:

```text
org.bluez.Agent1
```

and register with:

```text
org.bluez.AgentManager1
```

Gate:

```text
agent unit/component tests pass
pairing requests can reach UI without blocking
```

## Increment 4 — Pair / cancel pairing

Implement `Pair()` and `CancelPairing()` around the agent.

Gate:

```text
pairing state is coherent
agent cancellation is coherent
Paired property is authoritative
```

## Increment 5 — Reconnect

Implement:

```text
manual reconnect
bounded auto-reconnect policy
```

Gate:

```text
explicit Disconnect never causes immediate reconnect
retry limits/backoff are tested
```

## Increment 6 — QML lifecycle UI

Expose all Phase 3 operations and prompts.

Gate:

```text
no terminal UI is needed for ordinary device management
```

## Increment 7 — integration/live validation

Add/extend opt-in integration tests.

Gate:

```text
clean build
all normal ctest tests pass
live test passes on a real target device
```

---

# 40. PROHIBITED SHORTCUTS

Do not solve Phase 3 by:

- calling `bluetoothctl` from `QProcess`;
- parsing terminal output;
- executing shell scripts from production code;
- using blocking sleeps;
- marking a device connected immediately after button click;
- storing pairing secrets;
- manipulating `/var/lib/bluetooth`;
- requiring root;
- disabling security;
- hardcoding one Bluetooth MAC address;
- hardcoding one hearing-device name;
- inventing fake test devices in the production registry;
- replacing Phase 2 discovery with a new unrelated Bluetooth framework;
- implementing PipeWire/audio routing early;
- making ordinary tests depend on physical hardware;
- swallowing D-Bus failures;
- adding an infinite auto-reconnect loop;
- automatically reconnecting after an explicit user Disconnect;
- automatically forgetting user devices during normal tests.

---

# 41. CODE QUALITY BAR

The implementation should be suitable for continued production development.

Require:

- clear module ownership;
- focused classes;
- const-correctness where appropriate;
- no avoidable duplicated D-Bus strings;
- central interface/method/property constants where useful;
- structured errors;
- deterministic asynchronous completion;
- testable policy logic;
- safe QObject lifetime management;
- minimal public API surface;
- clear naming;
- documentation for non-obvious state transitions;
- no dead code;
- no placeholder TODOs for Phase 3 exit-gate functionality.

A TODO is acceptable only for explicitly later-phase behavior such as PipeWire endpoint mapping.

---

# 42. BUILD AND REGRESSION GATE

Before claiming completion, perform a clean build.

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

Validate that Phase 2 discovery still works:

```text
start scan
receive nearby devices
stop scan
registry remains coherent
UI remains responsive
```

Then validate Phase 3.

No pre-existing test may be disabled merely to make Phase 3 green unless the test is demonstrably obsolete and is replaced with equivalent or stronger coverage.

---

# 43. PHASE 3 ACCEPTANCE CRITERIA

PHASE 3 is complete only when **all** of the following are true.

## Build

- [ ] Clean CMake configuration succeeds.
- [ ] Ninja build succeeds.
- [ ] No new critical warnings/errors.
- [ ] Desktop application launches.

## Regression

- [ ] Phase 1 core behavior remains functional.
- [ ] Phase 2 discovery remains functional.
- [ ] DeviceRegistry still receives live BlueZ changes.
- [ ] Existing tests pass.

## Pairing

- [ ] Auralis can initiate pairing through `Device1.Pair()`.
- [ ] Auralis can cancel its pairing attempt.
- [ ] Agent interaction works when required.
- [ ] Pairing does not require terminal interaction.
- [ ] `Paired` state is sourced from BlueZ.

## Agent

- [ ] `Agent1` is exported correctly.
- [ ] Agent registration works.
- [ ] Capability is explicit/configurable.
- [ ] confirmation flow works.
- [ ] PIN/passkey flow is safely supported.
- [ ] rejection/cancel flow works.
- [ ] secrets are not logged/persisted.
- [ ] daemon restart does not leave stale registration state.

## Trust

- [ ] Auralis can set `Trusted=true`.
- [ ] Auralis can set `Trusted=false`.
- [ ] UI reflects authoritative BlueZ property changes.

## Connection

- [ ] Auralis can connect a paired device.
- [ ] Auralis can disconnect a connected device.
- [ ] operation-in-progress state is visible.
- [ ] final `Connected` state comes from BlueZ.
- [ ] expected idempotent cases are handled.
- [ ] failures are structured and visible.

## Forget

- [ ] Auralis can invoke `Adapter1.RemoveDevice`.
- [ ] DeviceRegistry handles resulting removal.
- [ ] stale state/timers/agent prompts are cleared.
- [ ] destructive live test is separately opt-in.

## Reconnect

- [ ] Manual Reconnect works.
- [ ] reconnect state is visible.
- [ ] auto-reconnect, if enabled, is bounded.
- [ ] explicit Disconnect suppresses reconnect.
- [ ] Forget suppresses reconnect.
- [ ] successful reconnect resets retries.

## Services

- [ ] Supported BlueZ UUIDs can be viewed.
- [ ] unknown UUIDs are retained.
- [ ] service data updates after pairing/resolution.

## Reliability

- [ ] BlueZ absence does not crash Auralis.
- [ ] adapter absence does not crash Auralis.
- [ ] device disappearance during an operation is safe.
- [ ] stale async callbacks do not mutate deleted/replaced devices.
- [ ] repeated button clicks do not issue uncontrolled contradictory calls.
- [ ] shutdown with pending operations is safe.

## Tests

- [ ] hardware-independent unit tests cover lifecycle logic.
- [ ] D-Bus operation contract tests exist.
- [ ] reconnect tests exist.
- [ ] pairing-agent tests exist.
- [ ] ordinary `ctest` does not require real Bluetooth hardware.
- [ ] opt-in live test mechanism is documented.

## Architectural boundary

- [ ] No production shell-command dependency was added.
- [ ] No root requirement was added.
- [ ] No PipeWire Phase 4 implementation was smuggled into Phase 3.
- [ ] No BlueZ bond files are directly manipulated.

---

# 44. PHASE 3 EXIT DEMONSTRATION

The final development demonstration should be possible entirely through Auralis:

```text
1. Launch Auralis.
2. Start discovery.
3. Find a real nearby Bluetooth device.
4. Select Pair.
5. Complete any required pairing-agent interaction.
6. Observe Paired.
7. Select Trust.
8. Observe Trusted.
9. Select Connect.
10. Observe Connecting.
11. Observe Connected.
12. View advertised/resolved services.
13. Select Disconnect.
14. Observe Disconnected.
15. Select Reconnect.
16. Observe Connected again.
17. Select Disconnect.
18. With explicit destructive-test intent, select Forget.
19. Observe the device removed from the BlueZ-backed registry.
20. Scan again and verify it can appear as an unpaired device when discoverable.
```

At no point should the standard workflow require:

```text
bluetoothctl
```

---

# 45. EXPECTED FINAL DELIVERABLES

At the end of your implementation, the repository should contain the Phase 3 equivalent of the following, adapted to the existing architecture:

```text
Bluetooth lifecycle service / manager
BlueZ pairing Agent1 implementation
AgentManager1 registration lifecycle
Pair / cancel implementation
Trust / untrust implementation
Connect / disconnect implementation
Forget implementation
Manual reconnect implementation
Bounded reconnect policy
Structured Bluetooth operation errors
Supported-service presentation
QML lifecycle controls
QML pairing prompts
Unit tests
D-Bus contract/component tests
Opt-in live BlueZ integration test additions
Documentation
```

Do not create duplicate classes if equivalent Phase 2 structures already exist.

---

# 46. REQUIRED FINAL RESPONSE FROM THE AI IDE

When implementation is complete, do not merely say "done."

Return a structured engineering report containing:

## A. Repository audit

List the Phase 2 components reused and explain any required refactors.

## B. Files changed

For each file:

```text
path
reason
major change
```

## C. Architecture

Explain:

- lifecycle operation ownership;
- authoritative vs transient state;
- Agent1 design;
- reconnect design;
- error design;
- QML/backend interaction.

## D. BlueZ calls implemented

Explicitly list the D-Bus interfaces/methods/properties used.

## E. Tests added

List every new/modified test and what it proves.

## F. Commands executed

Show:

```text
cmake configure
build
ctest
any live test command
```

## G. Results

Provide actual pass/fail counts.

Do not invent test results.

## H. Hardware validation

State exactly what was and was not validated on real hardware.

If no real pairing device was available, say so clearly.

Do not claim the live gate passed without executing it.

## I. Remaining limitations

Only list genuine remaining limitations.

Separate:

```text
Phase 3 limitation
future Phase 4+ work
hardware-dependent validation
```

## J. Phase 3 gate

Conclude with one of:

```text
PHASE 3: COMPLETE
```

or:

```text
PHASE 3: NOT COMPLETE
```

If not complete, list the exact blocking acceptance criteria.

---

# 47. IMPORTANT IMPLEMENTATION PHILOSOPHY

The objective is not merely to make buttons call D-Bus methods.

The objective is to create a reliable Bluetooth lifecycle subsystem whose behavior remains coherent when:

- operations are asynchronous;
- devices disappear;
- pairing requires user interaction;
- BlueZ rejects an operation;
- BlueZ restarts;
- a device disconnects unexpectedly;
- the user intentionally disconnects;
- reconnect is pending;
- the application is closing.

Auralis must treat the Bluetooth stack as an external asynchronous system.

The application model should converge on actual BlueZ state rather than pretending commands always succeed.

That discipline is essential before Phase 4 introduces PipeWire audio endpoints and before later phases introduce multi-device sessions.

---

# 48. START NOW

Begin by auditing the current repository.

Preserve the completed Phase 0–2 implementation.

Then implement **PHASE 3 — Bluetooth Device Management** incrementally, with tests at each step.

Do not move into PHASE 4.

Do not stop at scaffolding.

Continue until every hardware-independent Phase 3 acceptance criterion is implemented and verified, and clearly identify any final live-hardware validation that could not be executed in the current environment.

The final target is:

```text
Auralis
  |
  +-- Discover existing Phase 2 devices
  |
  +-- Pair
  +-- Cancel Pairing
  +-- Trust / Untrust
  +-- Connect
  +-- Disconnect
  +-- Forget
  +-- Reconnect
  +-- View Services
  |
  +-- Correct asynchronous BlueZ state
  +-- Pairing Agent
  +-- Structured errors
  +-- Tests
  +-- Functional QML controls
```

**PHASE 3 must end with a dependable Bluetooth device-management foundation ready for PHASE 4 — PipeWire Audio Integration.**
