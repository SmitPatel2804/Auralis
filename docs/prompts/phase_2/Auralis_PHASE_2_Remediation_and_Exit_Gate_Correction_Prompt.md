# Auralis — PHASE 2 Remediation, Hardening & Exit-Gate Correction Prompt

**Project:** Auralis  
**Target:** Ubuntu Linux desktop/laptop  
**Phase:** 2 — Bluetooth Device Discovery  
**Task type:** Targeted remediation of the existing Phase 2 implementation  
**Primary stack:** C++ / Qt 6 / QML / QtDBus / BlueZ  
**Baseline:** Phase 1 is complete. Phase 2 core architecture is already implemented and must be repaired/hardened rather than rewritten.  
**Goal:** Fix every confirmed Phase 2 audit issue, add missing regression coverage, and prove the real-device exit gate.

---

# 0. IMPORTANT CONTEXT

You are working in an existing Auralis repository where **Phase 2 is substantially implemented already**.

The following core Phase 2 architecture already exists:

```text
BlueZ
  |
  v
system D-Bus / QtDBus
  |
  v
BlueZDbusClient
  |
  +--> AdapterManager
  |
  +--> DiscoveryManager
  |
  v
DeviceRegistry
  |
  v
BluetoothDeviceListModel
  |
  v
BluetoothManager
  |
  v
QML
```

Existing relevant files include, among others:

```text
include/auralis/bluetooth/BlueZConstants.h
include/auralis/bluetooth/BlueZDbusClient.h
include/auralis/bluetooth/BlueZPropertyParser.h
include/auralis/bluetooth/BlueZTypes.h
include/auralis/bluetooth/BluetoothDeviceListModel.h
include/auralis/bluetooth/BluetoothError.h
include/auralis/bluetooth/BluetoothManager.h
include/auralis/bluetooth/DeviceRegistry.h
include/auralis/bluetooth/IBlueZClient.h

src/bluetooth/BlueZDbusClient.cpp
src/bluetooth/BlueZPropertyParser.cpp
src/bluetooth/BlueZTypes.cpp
src/bluetooth/BluetoothDeviceListModel.cpp
src/bluetooth/BluetoothError.cpp
src/bluetooth/BluetoothManager.cpp
src/bluetooth/DeviceRegistry.cpp

tests/unit/bluetooth/FakeBlueZClient.h
tests/unit/bluetooth/tst_BlueZPropertyParser.cpp
tests/unit/bluetooth/tst_BluetoothDeviceListModel.cpp
tests/unit/bluetooth/tst_BluetoothManager.cpp
tests/unit/bluetooth/tst_DeviceRegistry.cpp
tests/integration/tst_BlueZLiveIntegration.cpp

ui/qml/Main.qml
ui/components/DeviceRow.qml
ui/components/StatusRow.qml

docs/phase-2-validation.md
```

Do **not** throw away this architecture.

This is a correction/hardening task.

The desired outcome is:

> Keep the working BlueZ/QtDBus/registry/model/QML implementation, fix its remaining lifecycle and validation defects, add tests that reproduce those defects, and only then declare Phase 2 complete.

---

# 1. PHASE 2 CURRENT AUDIT VERDICT

The current implementation has genuine Phase 2 functionality and is **not a stub**.

The following are already present and should be preserved:

```text
[✓] Qt6::DBus integration
[✓] QDBusConnection::systemBus()
[✓] org.bluez service monitoring
[✓] ObjectManager.GetManagedObjects bootstrap
[✓] Adapter1 enumeration
[✓] Device1 parsing
[✓] InterfacesAdded handling
[✓] InterfacesRemoved handling
[✓] PropertiesChanged handling
[✓] asynchronous StartDiscovery
[✓] asynchronous StopDiscovery
[✓] AdapterManager
[✓] DiscoveryManager
[✓] DeviceRegistry
[✓] device deduplication by BlueZ object path
[✓] optional RSSI/property handling
[✓] QAbstractListModel exposure
[✓] QML Start / Stop / Refresh controls
[✓] QML device list
[✓] no production bluetoothctl backend
[✓] no QProcess shell-command discovery backend
[✓] no Phase 3 pairing/connection implementation
[✓] hardware-independent unit-test structure
[✓] opt-in BlueZ integration-test structure
```

However:

```text
PHASE 2 CORE IMPLEMENTATION: PRESENT
PHASE 2 EXIT GATE: NOT YET PASSED
```

This prompt exists to close that gap.

---

# 2. STRICT SCOPE

Implement **only corrections needed to finish Phase 2**.

Do not implement Phase 3.

Do not add:

```text
Pair()
CancelPairing()
Trusted=true administration
Connect()
Disconnect()
RemoveDevice()/Forget
Agent1
pairing dialogs
automatic reconnect
profile selection
```

Do not implement Phase 4:

```text
PipeWire discovery
audio endpoints
Bluetooth-to-PipeWire mapping
audio profiles
```

Do not implement Phase 5+:

```text
routing
multi-output audio
sessions
synchronization
latency compensation
```

Do not redesign the entire UI.

Do not rewrite BlueZ integration using `QBluetoothDeviceDiscoveryAgent`.

Do not replace QtDBus with another IPC library.

---

# 3. DO NOT “FIX” THIS VALID IMPLEMENTATION DETAIL

The current `BlueZDbusClient` uses a QtDBus signal connection conceptually like:

```cpp
SLOT(onInterfacesAdded(QDBusObjectPath,QDBusMessage))
```

and recovers the second D-Bus signal argument through:

```cpp
message.arguments()
```

This pattern is valid in QtDBus.

Do **not** rewrite it merely because `org.freedesktop.DBus.ObjectManager.InterfacesAdded`
normally has two D-Bus payload arguments.

Do not introduce an unnecessary regression here.

You may improve validation/logging for malformed arguments, but do not treat this slot shape itself as a bug.

---

# 4. CONFIRMED ISSUE 1 — STALE ERROR STATE AFTER RECOVERY

## Problem

`DiscoveryManager` currently sets errors such as:

```text
BlueZUnavailable
NoAdapter
AdapterPoweredOff
```

during temporary prerequisite states.

When a valid powered adapter later becomes available, the discovery state can return to:

```text
Idle
```

while `lastError_` remains an old error.

`BluetoothManager` updates `errorText_` only when `DiscoveryManager::errorChanged` fires.

This can produce an inconsistent UI such as:

```text
Status: Ready to scan
Error: Bluetooth adapter not available
```

or:

```text
Power: On
Error: Bluetooth is powered off
```

This is a real correctness bug.

### Current audited behavior

The problematic paths are around:

```text
DiscoveryManager::onBlueZAvailabilityChanged(...)
DiscoveryManager::onSelectedAdapterChanged(...)
```

The current implementation transitions to `Idle` after recovery but does not reliably call:

```cpp
setError(BluetoothError::None);
```

---

# 5. REQUIRED FIX 1 — ERROR RECOVERY MUST BE STATE-CONSISTENT

Define a clear invariant:

> If the Bluetooth prerequisites are currently valid and no active operation has failed, stale prerequisite errors must be cleared.

At minimum:

### BlueZ unavailable

```text
BlueZ disappears
    -> state Unavailable
    -> error BlueZUnavailable

BlueZ returns
    -> snapshot begins
    -> adapter is rediscovered
    -> powered adapter becomes selected
    -> state Idle
    -> stale BlueZUnavailable/NoAdapter error cleared
```

### No adapter

```text
BlueZ available
adapter absent
    -> error NoAdapter

adapter appears
powered=true
    -> state Idle
    -> error None
```

### Adapter powered off

```text
adapter powered=false
    -> error AdapterPoweredOff

Powered becomes true
    -> state Idle
    -> error None
```

### Discovery start failure

```text
StartDiscovery fails
    -> error DiscoveryStartFailed

user retries
successful StartDiscovery
    -> error None
```

### Discovery stop failure

A stop failure must not be cleared until the manager has actually returned to a valid/reconciled state.

---

# 6. ERROR-STATE DESIGN REQUIREMENT

Do not scatter arbitrary `setError(None)` calls without defining semantics.

Use a small coherent rule.

One acceptable pattern is:

```cpp
void DiscoveryManager::reconcilePrerequisites();
```

which evaluates:

```text
system bus
BlueZ availability
adapter existence
adapter power
```

and sets:

```text
state
lastError
```

together.

Another acceptable design is a helper such as:

```cpp
void clearRecoverablePrerequisiteError();
```

called only after prerequisites become valid.

Whichever design is chosen, write tests proving stale errors cannot survive recovery.

---

# 7. CONFIRMED ISSUE 2 — START → IMMEDIATE STOP RACE

## Problem

The current `DiscoveryManager::stopScan()` allows stop while:

```text
state == Starting
```

and immediately sends:

```text
StopDiscovery()
```

It also changes the state to:

```text
Stopping
```

Then when the pending StartDiscovery reply arrives, `handleStartFinished()` currently rejects it because the manager is no longer in `Starting`.

This creates a race:

```text
startScan()
    |
    +--> StartDiscovery request pending
    |
stopScan()
    |
    +--> state becomes Stopping
    +--> StopDiscovery sent immediately
    |
StartDiscovery reply arrives
    |
    +--> ignored because state != Starting
```

Possible consequences:

- local ownership diverges from BlueZ;
- stop may reach BlueZ before Auralis has acquired a discovery session;
- StartDiscovery success may be ignored;
- Auralis may leave a discovery session active while believing it is not;
- the UI can enter the wrong state;
- rapid button interactions are nondeterministic.

This violates the Phase 2 race-condition requirement.

---

# 8. REQUIRED FIX 2 — SERIALIZE DISCOVERY INTENT

Implement explicit user intent and pending-operation tracking.

Recommended internal state:

```cpp
bool desiredScanning_ = false;
bool ownsDiscovery_ = false;

enum class PendingOperation {
    None,
    Start,
    Stop
};

PendingOperation pendingOperation_ = PendingOperation::None;
QString operationAdapterPath_;
```

You may adapt names to the current project style.

The important distinction is:

```text
desiredScanning
```

means:

> What the Auralis user currently wants.

While:

```text
ownsDiscovery
```

means:

> Whether Auralis believes its BlueZ discovery session has actually been acquired.

And:

```text
pendingOperation
```

means:

> Which asynchronous BlueZ method call is currently unresolved.

Do not collapse these into one boolean.

---

# 9. REQUIRED DISCOVERY RECONCILIATION ALGORITHM

Implement a method conceptually equivalent to:

```cpp
void DiscoveryManager::reconcileDesiredState();
```

Rules:

## Want scan, do not own scan

```text
desiredScanning == true
ownsDiscovery == false
pendingOperation == None
prerequisites valid
```

Then:

```text
send StartDiscovery
pendingOperation = Start
state = Starting
```

## Do not want scan, currently own scan

```text
desiredScanning == false
ownsDiscovery == true
pendingOperation == None
```

Then:

```text
send StopDiscovery
pendingOperation = Stop
state = Stopping
```

## Operation pending

While a Start or Stop is pending:

```text
do not issue another D-Bus Start/Stop call
```

Update only:

```text
desiredScanning
```

Then reconcile again after the pending operation completes.

---

# 10. EXPECTED START/STOP RACE BEHAVIOR

### Case A — Start then immediate Stop before Start reply

Required:

```text
startScan()
    desiredScanning = true
    StartDiscovery sent

stopScan()
    desiredScanning = false
    NO StopDiscovery yet because Start is pending

StartDiscovery succeeds
    ownsDiscovery = true
    pending = None

reconcileDesiredState()
    desiredScanning = false
    ownsDiscovery = true
    -> send StopDiscovery

StopDiscovery succeeds
    ownsDiscovery = false
    state = Idle
```

Exactly one Start and one Stop should be sent.

### Case B — Start then immediate Stop, Start fails

Required:

```text
StartDiscovery fails
ownsDiscovery = false
desiredScanning = false
state returns Idle
NO StopDiscovery sent
```

### Case C — Start, Stop, Start while Start reply still pending

Required final user intent:

```text
desiredScanning = true
```

When Start succeeds:

```text
ownsDiscovery = true
desiredScanning = true
```

Therefore:

```text
do NOT send StopDiscovery
remain Discovering
```

### Case D — Stop then Start while Stop pending

Required:

```text
StopDiscovery allowed to complete
ownsDiscovery becomes false
desiredScanning is true
reconcile
StartDiscovery again
```

No overlapping requests.

---

# 11. ADAPTER PATH SAFETY

Every pending asynchronous operation must remember the adapter path it belongs to.

Do not use the newly selected adapter path to interpret a reply for an old adapter.

Recommended:

```text
operationAdapterPath_
```

When callbacks arrive:

```text
if callback adapter path != pending operation adapter path
    ignore/log stale callback safely
```

If the selected adapter disappears while an operation is pending:

- invalidate local ownership for that adapter;
- clear pending operation when safe;
- do not issue StopDiscovery against a different adapter accidentally;
- transition to Unavailable or select a new adapter;
- do not crash.

---

# 12. SERVICE GENERATION / STALE CALLBACK PROTECTION

BlueZ can disappear and return while an asynchronous operation is pending.

The current implementation already watches `org.bluez`.

Harden stale callback handling.

A simple generation counter is acceptable:

```cpp
quint64 bluezGeneration_ = 0;
```

Increment when the BlueZ service instance changes.

Capture generation when starting an async logical operation.

Ignore callbacks that belong to an obsolete generation.

Do not build a massive retry framework.

The requirement is simply:

> A late callback from the old BlueZ service instance must not corrupt the state of the newly initialized service instance.

If the existing QtDBus client architecture can guarantee equivalent safety another way, use that instead.

---

# 13. CONFIRMED ISSUE 3 — RAW Adapter1 `Discovering` IS NOT RECONCILED

## Problem

The implementation exposes:

```text
adapterDiscovering
```

from the selected Adapter1 object.

However, `DiscoveryManager` does not meaningfully reconcile its local ownership/state when the real BlueZ:

```text
Adapter1.Discovering
```

property changes.

This permits local state to become stale.

Example:

```text
Auralis owns discovery
state = Discovering

BlueZ reports:
Discovering = false
```

The UI can remain:

```text
scanning = true
```

indefinitely because local ownership was not reconciled.

The opposite case is also important:

```text
BlueZ Discovering = true
Auralis ownsDiscovery = false
```

This can happen because another client is scanning.

Auralis must **not** claim that it owns scanning merely because the adapter is globally discovering.

---

# 14. REQUIRED FIX 3 — LOCAL OWNERSHIP VS GLOBAL DISCOVERING

Preserve two concepts:

```text
Auralis local discovery ownership
BlueZ adapter global Discovering state
```

They are not the same.

Required invariants:

### Other client scanning

```text
adapter.discovering = true
ownsDiscovery = false
```

Then:

```text
BluetoothManager.scanning == false
adapterDiscovering == true
```

Do not claim Auralis owns discovery.

### Auralis owns scan, BlueZ stops globally

If:

```text
ownsDiscovery == true
```

and a later valid Adapter1 update says:

```text
Discovering == false
```

then reconcile.

At minimum:

```text
ownsDiscovery = false
pending operation cleared if appropriate
state = Idle
```

If this occurred unexpectedly while:

```text
desiredScanning == true
```

choose one of these controlled policies:

### Policy A — remain idle and surface an error

or:

### Policy B — perform one controlled re-reconciliation/retry

Do not create an infinite restart loop.

For Phase 2, **Policy A is acceptable and simpler**.

### Start success

A successful StartDiscovery reply may set local ownership true.

Adapter1 `Discovering=true` should confirm real adapter state.

Do not require the UI to wait forever for a property change after the method reply, but do reconcile later contradictory property events.

---

# 15. REQUIRED ADAPTER UPDATE HOOK

Currently adapter changes cause:

```text
DiscoveryManager::onSelectedAdapterChanged()
```

Use or extend this path so DiscoveryManager can inspect:

```text
selected().powered
selected().discovering
selected adapter path
```

and reconcile local state.

It may be cleaner to rename the function to something like:

```cpp
onSelectedAdapterStateChanged()
```

but do not rename solely for aesthetics if that creates unnecessary churn.

The important behavior is what matters.

---

# 16. CONFIRMED ISSUE 4 — `snapshotFailed` EXISTS BUT IS NOT WIRED

`IBlueZClient` exposes:

```cpp
void snapshotFailed(
    const QString& errorName,
    const QString& errorMessage);
```

`BlueZDbusClient` emits it when `GetManagedObjects` fails.

But the current `BluetoothManager::connectClientSignals()` does not consume it.

This means a real D-Bus snapshot failure is logged in the low-level client but is not integrated into the application's Phase 2 error/status lifecycle.

---

# 17. REQUIRED FIX 4 — SNAPSHOT FAILURE PROPAGATION

Connect:

```text
IBlueZClient::snapshotFailed
```

to `BluetoothManager`.

Required behavior:

### Initial snapshot fails

- do not crash;
- do not pretend that a valid adapter snapshot was loaded;
- log the exact D-Bus error name/message;
- surface a concise user-visible error;
- keep retry/Refresh possible.

Recommended mapping:

```text
BluetoothError::DbusCallFailed
```

with detail:

```text
Failed to refresh BlueZ state: <message> (<errorName>)
```

### Refresh snapshot fails after a valid registry already exists

Do **not** immediately destroy the existing device registry merely because one refresh failed.

Keep the last known valid state.

Surface/log the refresh error.

A subsequent successful snapshot must clear the stale snapshot error.

### Successful snapshot after failure

Required:

```text
snapshotFailed -> error shown
snapshotReceived -> error cleared if no higher-priority current error exists
```

Do not leave stale red UI text.

---

# 18. SYSTEM BUS ERROR PROPAGATION

`IBlueZClient` already exposes:

```cpp
systemBusStateChanged(bool connected);
```

The manager currently does not make full use of it.

Wire it into the Phase 2 status/error model.

At minimum:

### Initial system-bus failure

Required UI semantics:

```text
statusText = "System D-Bus is unavailable"
error = NoSystemBus
scan disabled
```

Do not report only:

```text
BlueZUnavailable
```

when the deeper cause is that the system bus itself is unavailable.

### Bus restoration in tests

The fake client should be able to simulate:

```text
false -> true
```

and the manager should recover its status coherently when a subsequent BlueZ/snapshot state becomes valid.

Real production reattachment to a restarted system D-Bus daemon does not need a complex reconnection framework for Phase 2 unless already easy to support.

---

# 19. CONFIRMED ISSUE 5 — PARSER WARNINGS ARE CREATED BUT DROPPED

The parser returns:

```cpp
struct ParseWarning {
    QString property;
    QString message;
};
```

through:

```text
DeviceParseResult
AdapterParseResult
```

The existing parsing paths generally use:

```text
parsed.device
parsed.adapter
```

while ignoring:

```text
parsed.warnings
```

This means malformed optional BlueZ fields can silently disappear.

The Phase 2 design explicitly includes:

```text
MalformedDbusPayload
```

yet the warning pipeline is not fully connected.

---

# 20. REQUIRED FIX 5 — PARSER WARNING DIAGNOSTICS

Every parse warning must become an observable diagnostic.

At minimum log:

```text
object path
interface
property
warning message
```

Example:

```text
MalformedDeviceProperty
path=/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
property=RSSI
message="Expected signed 16-bit integer"
```

Use:

```text
auralisBluetooth
```

logging.

Do not use uncontrolled `qDebug()`.

Do not crash.

Do not reject an entire device because one optional property is malformed.

---

# 21. PARSER WARNING FLOW FOR ALL PATHS

Handle warnings from:

### Snapshot parse

```text
BluetoothManager::handleSnapshot()
```

For both:

```text
Adapter1
Device1
```

### InterfacesAdded parse

```text
BluetoothManager::handleInterfacesAdded()
```

### Device property changes

Currently parsing occurs inside `DeviceRegistry::applyPropertyChanges()`.

Make warnings observable.

Acceptable approaches:

```text
A. DeviceRegistry emits parseWarning(...)
B. DeviceRegistry logs using the project logging category
C. parsing is moved to a layer where BluetoothManager can consume warnings
```

Choose the smallest coherent change.

### Adapter property changes

Same requirement for:

```text
AdapterManager::applyPropertyChanges()
```

---

# 22. MALFORMED D-BUS SIGNAL PAYLOAD LOGGING

Harden `BlueZDbusClient` decoding.

For example, current InterfacesAdded handling extracts the interface map from:

```text
message.arguments()[1]
```

If:

```text
message.arguments().size() < 2
```

do not silently emit an empty interface map as though it were normal.

Log a warning such as:

```text
MalformedDbusPayload
signal=InterfacesAdded
path=...
```

Likewise, if decoding a supported Adapter1/Device1 payload fails unexpectedly, log it.

Do not turn normal absence of optional Device1 properties into an error.

---

# 23. UI ERROR PRIORITY

Avoid multiple subsystems fighting over one text string.

Define an explicit priority such as:

```text
1. NoSystemBus
2. BlueZUnavailable
3. NoAdapter
4. AdapterPoweredOff
5. active Start/Stop D-Bus operation failure
6. snapshot/refresh failure
7. nonfatal parser warning
```

You do not need a huge error stack.

A simple coherent strategy is enough.

But these must be true:

```text
a stale lower-priority error cannot survive after recovery
a parser warning must not permanently hide a serious adapter failure
a successful snapshot can clear a previous snapshot failure
successful StartDiscovery clears a prior start error
```

If parser warnings are log-only, document that they are intentionally non-fatal diagnostics.

That is acceptable.

---

# 24. CONFIRMED ISSUE 6 — TEST DOUBLE CANNOT REPRODUCE ASYNC RACES

The current:

```text
FakeBlueZClient
```

completes:

```text
startDiscovery()
stopDiscovery()
```

synchronously.

Therefore:

```text
startScan();
stopScan();
```

cannot realistically simulate:

```text
StartDiscovery still pending
```

This is why the current unit tests cannot prove the Start→Stop race is fixed.

---

# 25. REQUIRED FIX 6 — ASYNC-CONTROLLABLE FAKE BLUEZ CLIENT

Extend `FakeBlueZClient` with test controls.

Recommended API:

```cpp
void setAutoCompleteStart(bool enabled);
void setAutoCompleteStop(bool enabled);

void completeStartSuccess();
void completeStartFailure(
    const QString& errorName,
    const QString& errorMessage);

void completeStopSuccess();
void completeStopFailure(
    const QString& errorName,
    const QString& errorMessage);
```

Also track:

```text
startRequests
stopRequests
lastStartPath
lastStopPath
```

which already exist.

When automatic completion is disabled:

```text
startDiscovery()
```

must record the request but not emit completion until the test explicitly completes it.

Likewise for Stop.

Preserve existing convenient synchronous behavior as the default so current tests do not need unnecessary rewriting.

---

# 26. REQUIRED UNIT TESTS — ERROR RECOVERY

Add tests that would fail on the audited implementation.

At minimum:

## 26.1 NoAdapter error clears after snapshot/adapter arrival

Flow:

```text
BlueZ available
manager initialize
no adapter initially
verify errorText == NoAdapter message

emit/add powered hci0
verify canStartScan == true
verify statusText == "Ready to scan"
verify errorText is empty
```

## 26.2 AdapterPoweredOff clears after power-on

```text
adapter present Powered=false
verify scan disabled
verify AdapterPoweredOff error

update Powered=true
verify scan enabled
verify errorText empty
verify status Ready to scan
```

## 26.3 BlueZUnavailable clears after BlueZ returns

```text
BlueZ unavailable
verify unavailable error

BlueZ true
snapshot powered adapter
verify available
verify scan enabled
verify stale error removed
```

## 26.4 Snapshot failure clears after successful refresh

```text
valid BlueZ
snapshot failure emitted
verify DbusCallFailed surfaced

successful snapshot emitted
verify stale snapshot error cleared
```

## 26.5 NoSystemBus is distinct

```text
systemBusConnected=false
BlueZ=false

initialize

verify status/error says System D-Bus unavailable
not merely BlueZ unavailable
```

---

# 27. REQUIRED UNIT TESTS — DISCOVERY SERIALIZATION

Using deferred FakeBlueZClient completions:

## 27.1 Start pending, then Stop

Assertions:

```text
after startScan:
startRequests == 1
stopRequests == 0

after stopScan while Start pending:
startRequests == 1
stopRequests == 0

after completeStartSuccess:
stopRequests == 1

after completeStopSuccess:
scanning == false
canStartScan == true
canStopScan == false
```

## 27.2 Start pending, Stop, then Start again

```text
startScan()
stopScan()
startScan()
completeStartSuccess()
```

Required:

```text
stopRequests == 0
scanning == true
```

because final user intent is scanning.

## 27.3 Start pending then Start again

No duplicate StartDiscovery:

```text
startRequests == 1
```

## 27.4 Start failure after user requested Stop

```text
startScan()
stopScan()
completeStartFailure(...)
```

Required:

```text
stopRequests == 0
scanning == false
state recoverable
```

## 27.5 Stop pending, then Start

```text
start successful
stopScan()
startScan() while Stop pending
completeStopSuccess()
```

Required:

```text
second StartDiscovery issued only after Stop completes
```

## 27.6 Stop failure

Verify:

- no false "clean stopped" claim;
- error surfaced;
- ownership/state remain coherent;
- retry remains possible according to design.

---

# 28. REQUIRED UNIT TESTS — ADAPTER DISCOVERING RECONCILIATION

## 28.1 Another client scanning

Set:

```text
adapter Discovering=true
ownsDiscovery=false
```

Verify:

```text
manager.adapterDiscovering() == true
manager.scanning() == false
```

## 28.2 Auralis scan then Adapter1 Discovering=false

After successful Auralis start:

```text
ownsDiscovery=true
scanning=true
```

Emit:

```text
PropertiesChanged Adapter1 Discovering=false
```

Verify:

```text
scanning=false
```

and state/error follows the chosen controlled policy.

## 28.3 Global discovering remains true after Auralis Stop

Simulate:

```text
Auralis StopDiscovery succeeds
Adapter1 Discovering remains true
```

because another client is also scanning.

Verify:

```text
manager.scanning() == false
manager.adapterDiscovering() == true
```

This distinction is critical.

---

# 29. REQUIRED UNIT TESTS — BLUEZ SERVICE INTERRUPTION

Where feasible with the fake client:

```text
start operation pending
BlueZ becomes unavailable
late old completion emitted
BlueZ returns with valid adapter
```

Verify old callback does not restore stale ownership or corrupt the new state.

If a generation counter is implemented, test it directly through behavior.

---

# 30. REQUIRED UNIT TESTS — PARSER WARNING PROPAGATION

Current parser tests already validate malformed fields.

Add at least one test around the consuming layer proving malformed input is:

```text
logged/emitted as warning
device still represented
application does not crash
```

If warning signals are introduced in DeviceRegistry/AdapterManager, use `QSignalSpy`.

Example:

```text
existing device
PropertiesChanged RSSI = wrong QVariant type
```

Expected:

```text
warning emitted
device remains in registry
hasRssi handled according to parser semantics
```

Do not require a malformed optional property to remove the device.

---

# 31. CONFIRMED ISSUE 7 — LIVE INTEGRATION TEST DOES NOT PROVE LIVE DISCOVERY

The current:

```text
tests/integration/tst_BlueZLiveIntegration.cpp
```

does several useful checks:

```text
system bus exists
org.bluez exists
GetManagedObjects succeeds
adapter enumerates
StartDiscovery succeeds
```

However, it currently has two critical weaknesses.

---

# 32. LIVE TEST WEAKNESS A — EARLY RETURN

The current test effectively does:

```cpp
if (!manager.canStartScan()) {
    qWarning() << ...;
    return;
}
```

When the developer explicitly sets:

```text
AURALIS_RUN_BLUETOOTH_INTEGRATION=1
```

the test must be strict.

If there is:

```text
no adapter
adapter powered off
cannot start scan
```

the explicitly enabled integration test should **fail with a clear diagnostic**, not return as though the test succeeded.

Replace silent success behavior with:

```text
QFAIL / QVERIFY2
```

when the opt-in test prerequisites promised by the developer are not satisfied.

The default non-enabled test may still:

```text
QSKIP
```

---

# 33. LIVE TEST WEAKNESS B — DEVICE COUNT IS ONLY PRINTED

The current test waits a few seconds and logs:

```text
deviceCount
```

but does not assert that Auralis receives any live Device1/model activity after Start Scan.

That is insufficient for the Phase 2 exit gate.

The key Phase 2 claim is:

> Starting discovery causes real BlueZ device events to reach the Auralis registry/model.

The live test must prove that path.

---

# 34. REQUIRED FIX 7 — PROVE LIVE DEVICE ACTIVITY

Before starting discovery:

```cpp
QAbstractItemModel* model = manager.devices();
QSignalSpy rowsInsertedSpy(model, &QAbstractItemModel::rowsInserted);
QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);
```

or an equivalent mechanism.

Record:

```text
initial deviceCount
```

Start discovery.

Then wait for real model activity.

Accept evidence such as:

```text
rowsInserted > 0
OR
dataChanged > 0
```

during the active scan window.

Why both?

A device can already exist in BlueZ's cached object tree before the test starts.

In that case discovery may update:

```text
RSSI
ManufacturerData
ServiceData
other Device1 properties
```

instead of creating a brand-new object.

Therefore requiring only:

```text
deviceCount increases
```

is too brittle.

But merely printing `deviceCount` is too weak.

---

# 35. OPTIONAL EXPECTED-DEVICE ADDRESS FOR STRONG HARDWARE VALIDATION

Support an optional environment variable:

```text
AURALIS_EXPECT_DEVICE_ADDRESS
```

Example:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF \
ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

When set:

1. Start discovery.
2. Wait for live model activity.
3. Search model rows for:

```text
AddressRole
```

matching the expected address.
4. Fail if not observed before timeout.

This creates a very strong exit-gate test when validating a known test phone/headset/hearing device.

Do not require this variable for normal CI.

---

# 36. LIVE TEST TIMEOUT

The existing fixed:

```text
4 second
```

wait can be too short for real radio discovery.

Use an event-driven wait with a reasonable timeout.

Recommended default:

```text
15 seconds
```

or another value justified by actual test behavior.

Do not use a long blind sleep if `QTRY_VERIFY_WITH_TIMEOUT` can wait for the desired condition.

The test should fail with an actionable message:

```text
No live Bluetooth device activity was observed.
Put a nearby Classic/BLE device into discoverable/advertising mode
and rerun with AURALIS_RUN_BLUETOOTH_INTEGRATION=1.
```

---

# 37. LIVE TEST STOP VALIDATION

After discovery activity is observed:

```text
manager.stopScan()
```

Wait until Auralis local ownership is released.

Require:

```text
manager.scanning() == false
```

Do **not** require:

```text
adapterDiscovering == false
```

because another BlueZ client may still own a discovery session.

This is an important correctness distinction.

After Stop:

```text
canStartScan should become true
```

when prerequisites remain valid.

---

# 38. LIVE TEST REPEATED-SCAN CHECK

If practical, add a second scan cycle in the opt-in integration test:

```text
Start
observe live activity
Stop
Start again
observe activity/reconciliation
```

At minimum verify that the model does not create obvious duplicate rows for the exact same BlueZ object path.

Do not make this test fragile due to expected BlueZ cache behavior.

A separate manual repeated-scan check is still required.

---

# 39. DEVICE DEDUPLICATION REGRESSION CHECK

Do not change current object-path identity semantics.

Keep:

```text
BlueZ object path
```

as the authoritative live identity.

If touching `DeviceRegistry`, preserve:

```text
same path -> update existing row
not new row
```

Add/retain tests proving:

```text
snapshot device
then same InterfacesAdded path
-> one registry row
```

and:

```text
PropertiesChanged
-> dataChanged
not model reset
```

---

# 40. MODEL PERFORMANCE REGRESSION GUARD

Do not “solve” correction issues by resetting the whole device model.

Preserve:

```text
beginInsertRows/endInsertRows
beginRemoveRows/endRemoveRows
dataChanged
```

Do not call:

```text
beginResetModel()
```

for routine RSSI/device updates.

This is already one of the better parts of the implementation.

---

# 41. SNAPSHOT / EVENT RACE REGRESSION GUARD

Do not remove the current snapshot + live signal model.

Keep:

```text
subscribe to BlueZ signals
GetManagedObjects snapshot
reconcile
live InterfacesAdded/Removed/PropertiesChanged
```

The registry must remain idempotent when the same device appears through:

```text
snapshot
+
live signal
```

---

# 42. BLUEZ SERVICE RECOVERY

Preserve existing:

```text
QDBusServiceWatcher
```

behavior.

Required sequence:

```text
BlueZ disappears
    -> available false
    -> adapter/device live state cleared safely
    -> Auralis local discovery ownership cleared
    -> scan disabled

BlueZ returns
    -> available true
    -> new GetManagedObjects
    -> adapters/devices repopulate
    -> powered adapter returns manager to Idle
    -> stale availability error clears
```

Add a fake-client unit test for this complete recovery path.

---

# 43. REFRESH SEMANTICS

Keep Refresh as:

```text
request new GetManagedObjects snapshot
reconcile current BlueZ objects
do not arbitrarily restart scan
```

After this remediation:

### Refresh success

- updates/reconciles state;
- clears prior snapshot failure;
- does not duplicate rows.

### Refresh failure

- preserves last known valid registry;
- displays/logs error;
- permits another Refresh.

---

# 44. DISCOVERY UI BEHAVIOR AFTER FIX

The QML does not need major redesign.

It must simply reflect corrected backend state.

Required examples:

### Normal idle

```text
BlueZ: Ready
Adapter: <name/address>
Power: On
Scan: Idle
Status: Ready to scan
Error: <none>

Start Scan enabled
Stop Scan disabled
```

### Starting

```text
Status: Starting discovery...
Start disabled
Stop may represent desired stop action depending API
```

If Stop is allowed while Start is pending, it must change desired intent without issuing overlapping D-Bus calls.

### Scanning

```text
Scan: Active
Status: Scanning...
Start disabled
Stop enabled
```

### Start failed

```text
Scan: Idle
errorText contains concise failure
Start enabled for retry
```

### Recovered

After successful retry:

```text
errorText empty
```

### Adapter off

```text
Power: Off
Start disabled
Error/Status: Bluetooth is powered off
```

### Adapter on again

```text
Power: On
Start enabled
Error cleared
Status: Ready to scan
```

---

# 45. SCANNING PROPERTY SEMANTICS

Preserve this semantic:

```text
BluetoothManager.scanning
```

means:

> Auralis owns an active discovery request/session.

Do not redefine it as:

```text
Adapter1.Discovering
```

because another process may be scanning.

Keep:

```text
adapterDiscovering
```

as the raw/global BlueZ adapter property.

This separation must be tested.

---

# 46. `canStartScan` SEMANTICS AFTER SERIALIZATION

After adding desired/pending state, define:

```text
canStartScan
```

as a UI action capability.

Recommended:

```text
system bus connected
BlueZ available
adapter exists
adapter powered
desiredScanning == false
```

During a pending Stop with final desired state false, Start can either:

### Option A
be enabled to allow changing desired intent immediately,

or:

### Option B
remain disabled until pending operation finishes.

Either is acceptable.

If Option A is chosen, clicking Start while Stop is pending must only update:

```text
desiredScanning=true
```

and must not issue Start until Stop completes.

Pick one and test it.

Do not let QML cause overlapping D-Bus operations.

---

# 47. `canStopScan` SEMANTICS AFTER SERIALIZATION

Recommended:

```text
canStopScan = desiredScanning || ownsDiscovery || pending Start
```

so the user can cancel a Start intent before StartDiscovery completes.

Clicking Stop during pending Start should:

```text
desiredScanning=false
```

without immediately sending StopDiscovery.

Again, test this behavior.

---

# 48. SHUTDOWN BEHAVIOR

Review shutdown with the new state machine.

Current BluetoothManager shutdown attempts to stop discovery.

Do not introduce a deadlock or synchronous wait.

At minimum:

- clear desired scanning;
- if a discovery session is owned, issue best-effort stop;
- do not start a new scan during shutdown;
- invalidate pending state;
- do not allow a late callback to resurrect scanning after shutdown;
- clear registry/adapter state safely.

A full graceful asynchronous shutdown wait is not required for Phase 2 if the application is terminating.

But stale callbacks after shutdown must not modify destroyed state.

---

# 49. LOGGING REQUIREMENTS

Preserve and improve structured Phase 2 logging.

Important events:

```text
SystemBusUnavailable
BlueZUnavailable
BlueZAvailable
BlueZSnapshotRequested
BlueZSnapshotReceived
BlueZSnapshotFailed

AdapterAdded
AdapterRemoved
AdapterRecovered
AdapterPoweredOff
AdapterPoweredOn

DiscoveryIntentChanged
DiscoveryStartRequested
DiscoveryStarted
DiscoveryStartFailed
DiscoveryStopRequested
DiscoveryStopped
DiscoveryStopFailed
DiscoveryStateReconciled

DeviceAdded
DeviceRemoved

MalformedDbusPayload
MalformedDeviceProperty
MalformedAdapterProperty
```

Do not log every RSSI update at INFO.

Use DEBUG/TRACE for noisy updates if needed.

---

# 50. DO NOT ADD SHELL FALLBACKS

This remains a strict requirement.

Forbidden production fixes:

```text
bluetoothctl scan on
bluetoothctl devices
busctl
dbus-send
btmgmt
QProcess
system(...)
popen(...)
```

Do not add these as “fallbacks” when D-Bus fails.

Fix the D-Bus state/error behavior instead.

---

# 51. FILES LIKELY TO CHANGE

The exact set depends on the implementation, but likely changes include:

```text
include/auralis/bluetooth/DiscoveryManager.h
src/bluetooth/DiscoveryManager.cpp

include/auralis/bluetooth/BluetoothManager.h
src/bluetooth/BluetoothManager.cpp

include/auralis/bluetooth/AdapterManager.h
src/bluetooth/AdapterManager.cpp

include/auralis/bluetooth/DeviceRegistry.h
src/bluetooth/DeviceRegistry.cpp

src/bluetooth/BlueZDbusClient.cpp

tests/unit/bluetooth/FakeBlueZClient.h
tests/unit/bluetooth/tst_BluetoothManager.cpp
tests/unit/bluetooth/tst_DeviceRegistry.cpp
tests/integration/tst_BlueZLiveIntegration.cpp

docs/phase-2-validation.md
```

You may add a dedicated:

```text
tst_DiscoveryManager.cpp
```

if that produces cleaner lifecycle tests.

Do not create unnecessary layers.

---

# 52. IMPLEMENTATION ORDER

Follow this order.

## Step 1 — Reproduce baseline

Before edits:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

If a clean build directory is needed:

```bash
cmake -S . -B build -G Ninja
```

Record existing result.

## Step 2 — Add failing unit tests first

Add tests for:

```text
stale NoAdapter error
stale AdapterPoweredOff error
Start -> Stop race
Start -> Stop -> Start race
raw Adapter1 Discovering mismatch
snapshot failure
```

Make sure at least the intended tests fail on the old code.

## Step 3 — Repair DiscoveryManager

Add:

```text
desired state
pending operation
serialized reconciliation
stale callback safety
adapter Discovering reconciliation
correct recovery error clearing
```

## Step 4 — Repair manager-level errors

Wire:

```text
systemBusStateChanged
snapshotFailed
successful snapshot recovery
```

## Step 5 — Wire parser warnings

Log/emit all parser warnings.

## Step 6 — Upgrade FakeBlueZClient

Allow delayed Start/Stop completions.

## Step 7 — Complete unit coverage

Run all Bluetooth unit tests.

## Step 8 — Strengthen live integration test

Remove false-success early return.

Require actual live Device1/model activity.

## Step 9 — Documentation

Update validation instructions.

## Step 10 — Clean regression build

Run the exact clean workflow.

## Step 11 — Real hardware exit gate

Run Auralis on the target laptop with a real nearby discoverable/advertising Bluetooth device.

Do not declare success before this.

---

# 53. REQUIRED CLEAN BUILD

Before final completion:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

All normal tests must pass without requiring a Bluetooth adapter.

Do not disable failing tests.

---

# 54. REQUIRED OPT-IN LIVE TEST

With Bluetooth enabled and at least one nearby device advertising/discoverable:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

If testing a known device:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS=<TEST_DEVICE_ADDRESS> \
ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

Do not hard-code a real personal device address into source control.

---

# 55. REQUIRED MANUAL UI VALIDATION

Launch:

```bash
./build/apps/desktop/auralis-desktop
```

Then perform all of the following.

## Validation A — Normal startup

Verify:

```text
BlueZ Ready
adapter displayed
Power On
Ready to scan
no stale red error
```

## Validation B — Start scan

Click:

```text
Start Scan
```

Verify:

```text
Scanning...
real device activity appears
device rows insert/update
RSSI updates when available
```

No terminal command may be required.

## Validation C — Stop scan

Click:

```text
Stop Scan
```

Verify:

```text
Auralis local Scan becomes Idle
Start becomes available again
```

Do not require global Adapter1 Discovering to become false if another client scans.

## Validation D — Repeated scan

Start again.

Verify:

```text
same BlueZ object path does not duplicate
model remains stable
```

## Validation E — Power recovery

Turn Bluetooth adapter off through normal OS settings if safe.

Verify:

```text
scan disabled
powered-off error shown
```

Turn Bluetooth back on.

Verify:

```text
adapter recovers
scan enabled
stale powered-off error disappears
```

## Validation F — Rapid Start/Stop

Click Start and Stop rapidly.

Verify:

```text
no crash
no permanent Starting/Stopping state
no unexpected active scan left behind
```

Repeat multiple times.

---

# 56. OPTIONAL DEVELOPER VERIFICATION

Developer-only tools may be used independently to compare BlueZ state:

```bash
bluetoothctl show
bluetoothctl devices
busctl tree org.bluez
```

But Auralis must not call them internally.

The real validation is the Auralis UI/model itself.

---

# 57. PHASE 2 EXIT-GATE CHECKLIST AFTER REMEDIATION

Do not mark Phase 2 done unless every item is true.

```text
[ ] Phase 1 still builds and launches
[ ] clean CMake configure succeeds
[ ] Ninja build succeeds
[ ] normal CTest succeeds

[ ] QtDBus system-bus backend unchanged/preserved
[ ] BlueZ service recovery still works
[ ] GetManagedObjects bootstrap works
[ ] InterfacesAdded works
[ ] InterfacesRemoved works
[ ] PropertiesChanged works

[ ] stale NoAdapter error clears
[ ] stale AdapterPoweredOff error clears
[ ] stale BlueZUnavailable error clears
[ ] successful scan clears previous start error
[ ] successful snapshot clears previous snapshot error
[ ] NoSystemBus is distinguishable

[ ] snapshotFailed is connected
[ ] snapshot failure does not erase last good registry
[ ] parser warnings are observable/logged
[ ] malformed optional payload does not crash

[ ] desiredScanning is represented
[ ] pending Start/Stop operation is represented
[ ] Start and Stop calls are serialized
[ ] Start -> immediate Stop is safe
[ ] Start -> Stop -> Start is safe
[ ] duplicate Start does not issue duplicate request
[ ] duplicate Stop is safe
[ ] late stale callback cannot corrupt new BlueZ generation/state

[ ] local scanning ownership is distinct from Adapter1 Discovering
[ ] other-client global discovery does not set Auralis scanning=true
[ ] Discovering=false reconciles stale Auralis ownership

[ ] fake BlueZ client can defer async completions
[ ] race-condition tests pass
[ ] recovery tests pass
[ ] snapshot failure tests pass
[ ] parser warning tests pass

[ ] opt-in live test fails if adapter cannot scan
[ ] opt-in live test observes real model/device activity
[ ] live Stop releases Auralis ownership
[ ] optional expected-device address check works

[ ] QML shows no stale error after recovery
[ ] Start Scan works from UI
[ ] Stop Scan works from UI
[ ] Refresh works after prior failure
[ ] real Bluetooth device appears/updates
[ ] repeated scan does not duplicate same BlueZ object

[ ] no bluetoothctl production backend
[ ] no QProcess discovery backend
[ ] no Phase 3 functionality added
[ ] no PipeWire functionality added
```

---

# 58. DEFINITION OF DONE

Phase 2 is corrected only when all of the following behaviors are true:

### Behavior 1

```text
BlueZ starts unavailable
then becomes available
then adapter snapshot arrives
```

Result:

```text
Ready to scan
no stale error
```

### Behavior 2

```text
adapter Powered=false
then Powered=true
```

Result:

```text
scan becomes enabled
powered-off error clears
```

### Behavior 3

```text
Start pressed
Stop pressed before StartDiscovery reply
```

Result:

```text
no overlapping D-Bus operation
Start completes
one Stop follows
final state Idle
```

### Behavior 4

```text
Start
Stop
Start
while first Start is pending
```

Result:

```text
final state Discovering
no unnecessary Stop
```

### Behavior 5

```text
another client is scanning
Adapter1 Discovering=true
```

Result:

```text
Auralis scanning=false
adapterDiscovering=true
```

### Behavior 6

```text
Auralis believes it owns discovery
BlueZ reports Discovering=false
```

Result:

```text
Auralis reconciles ownership
does not remain stuck scanning
```

### Behavior 7

```text
GetManagedObjects fails
```

Result:

```text
clear error shown/logged
old valid registry retained
Refresh/retry possible
```

### Behavior 8

```text
malformed optional Device1 property
```

Result:

```text
warning logged
device remains represented
no crash
```

### Behavior 9

```text
live Start Scan on target laptop
nearby device advertising/discoverable
```

Result:

```text
rowsInserted or dataChanged occurs from live BlueZ device activity
```

This final behavior is the decisive Phase 2 proof.

---

# 59. FINAL ENGINEERING REPORT REQUIRED FROM THE AI IDE

After making the corrections, return a report with these exact sections.

## A. Audit Issues Fixed

For each issue:

```text
Issue
Root cause
Files changed
Fix
Tests added
```

Cover:

```text
stale errors
Start/Stop race
Adapter1 Discovering reconciliation
snapshotFailed propagation
system bus error distinction
parser warnings
async fake client
live integration validation
```

## B. Discovery State Machine

Show final state/intent model.

Include:

```text
desiredScanning
ownsDiscovery
pendingOperation
adapter/global Discovering
```

Explain how they differ.

## C. Files Changed

List every file.

## D. Unit Test Results

Show exact test output summary.

## E. Clean Build Result

Show:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## F. Live Integration Result

Show the result of:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

State whether real model activity was observed.

If:

```text
AURALIS_EXPECT_DEVICE_ADDRESS
```

was used, report that the expected address was observed without hard-coding it into source.

## G. Manual UI Validation

Report:

```text
startup
Start Scan
real device appearance/update
Stop Scan
repeated scan
rapid Start/Stop
adapter power recovery
```

## H. Remaining Limitations

Only legitimate Phase 2 limitations.

Examples:

```text
pairing intentionally deferred
connection intentionally deferred
LE privacy addresses are not durable identity
global discovery may remain true because other BlueZ clients scan
```

Do not list a Phase 2 exit-gate failure as a harmless limitation.

## I. Final Gate

End with exactly one:

```text
PHASE 2 EXIT GATE: PASSED
```

or:

```text
PHASE 2 EXIT GATE: NOT PASSED
```

If not passed, list blockers.

---

# 60. FINAL INSTRUCTION

Do not rewrite Phase 2.

Do not expand scope.

Repair the current implementation.

The most important outcomes are:

```text
1. stale errors always recover
2. asynchronous Start/Stop is serialized and race-safe
3. local discovery ownership never gets confused with global Adapter1 Discovering
4. snapshot/system-bus failures reach the application error model
5. malformed BlueZ properties produce diagnostics instead of silent drops
6. tests can reproduce asynchronous races
7. the opt-in BlueZ test proves live Device1/model activity
8. a real nearby Bluetooth device is successfully observed from the Auralis UI
```

Only after those are demonstrated should Auralis Phase 2 be locked as complete.

**Do not begin Phase 3 until:**

```text
PHASE 2 EXIT GATE: PASSED
```
