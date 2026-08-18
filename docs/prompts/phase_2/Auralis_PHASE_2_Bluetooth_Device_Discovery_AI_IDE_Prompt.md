# Auralis — PHASE 2 Bluetooth Device Discovery
## Deep Implementation Prompt for an AI Coding IDE

**Project:** Auralis  
**Target:** Ubuntu Linux desktop/laptop  
**Phase:** 2 — Bluetooth Device Discovery  
**Primary stack:** C++ / Qt 6 / QML / QtDBus / BlueZ  
**Baseline:** PHASE 1 is complete, builds cleanly, tests pass, and `auralis-desktop` launches successfully.  
**Primary Phase 2 milestone:** Nearby Bluetooth Classic and Bluetooth Low Energy devices appear live inside Auralis without requiring terminal interaction.

---

# 0. ROLE AND OPERATING MODE

You are acting as the senior C++/Qt/Linux Bluetooth engineer responsible for implementing **Auralis PHASE 2 — Bluetooth Device Discovery** in an existing repository.

You are not building a throwaway prototype.

You are extending an already-working Phase 1 architecture and must preserve:

- clean separation of concerns;
- existing public interfaces unless a change is justified;
- existing build behavior;
- existing tests;
- existing logging/configuration infrastructure;
- the desktop application's ability to launch;
- future compatibility with Phase 3 device management;
- future compatibility with Phase 4 PipeWire audio integration;
- future compatibility with LE Audio work.

Do not blindly replace working Phase 1 code.

Before editing anything, inspect the repository and understand what Phase 1 already created.

Your implementation must be incremental, testable, and production-oriented.

---

# 1. NON-NEGOTIABLE PHASE BOUNDARY

Implement **only PHASE 2**.

PHASE 2 means:

> Discover nearby Bluetooth Classic and BLE devices through BlueZ over the Linux **system D-Bus**, maintain their live state in Auralis, expose them to Qt/QML, and provide working scan controls.

Do **not** implement Phase 3 functionality in this phase.

Specifically, do not add production functionality for:

- pairing;
- cancel pairing;
- trust/untrust;
- connect/disconnect;
- forget/remove device;
- automatic reconnect;
- a custom BlueZ pairing agent;
- profile connection state machines.

Do **not** implement Phase 4 functionality:

- PipeWire registry monitoring;
- Bluetooth-to-PipeWire endpoint mapping;
- audio endpoint creation;
- audio profile selection.

Do **not** implement Phase 5+ functionality:

- audio routing;
- multi-output audio;
- sessions;
- synchronization;
- latency compensation.

Existing Phase 1 stubs for later phases must remain intact.

If a future-facing interface needs a tiny extension to support clean Phase 2 integration, make the smallest compatible change possible and document it.

---

# 2. SOURCE OF TRUTH FOR PHASE 2

PHASE 2 objective:

> Enable Auralis to discover nearby Bluetooth Classic and BLE devices through BlueZ.

Production discovery must communicate directly with:

```text
org.bluez
```

through the Linux **system D-Bus**.

The application must not launch and parse:

```text
bluetoothctl
btmgmt
busctl
dbus-send
```

as part of production functionality.

Those tools may only be used manually by a developer for independent diagnostics.

Important D-Bus interfaces for this phase:

```text
org.bluez.Adapter1
org.bluez.Device1
org.freedesktop.DBus.ObjectManager
org.freedesktop.DBus.Properties
```

Core Phase 2 component model:

```text
BluetoothManager
|
+-- AdapterManager
+-- DiscoveryManager
+-- BlueZDbusClient
+-- BluetoothDevice
+-- DeviceRegistry
+-- BluetoothDeviceListModel
```

Adapt names to the existing repository only where the Phase 1 code already established equivalent abstractions.

Do not create duplicate manager classes merely because this prompt suggests a name.

---

# 3. VERIFIED PLATFORM ASSUMPTIONS

The existing Auralis development baseline is Linux/Ubuntu and has already been validated.

Relevant environment:

```text
Ubuntu 26.04 LTS
Linux 7.0
x86_64

C++ toolchain:
GCC / G++
CMake
Ninja

Qt 6
Qt Quick / QML
Qt DBus available or expected to be available through Qt 6

Bluetooth:
BlueZ
system D-Bus
Bluetooth adapter present
BR/EDR supported
BLE supported
central role supported
peripheral role supported
```

The previously validated controller is the laptop's Qualcomm Atheros Bluetooth controller.

Do not introduce a new platform abstraction framework in Phase 2.

Linux + BlueZ is the concrete backend for this phase.

However, preserve clean interfaces so a different backend could theoretically be introduced later.

---

# 4. FIRST TASK — INSPECT THE EXISTING REPOSITORY

Before writing code, inspect the entire Phase 1 structure relevant to this implementation.

At minimum inspect:

```text
CMakeLists.txt
apps/desktop/
src/core/
src/bluetooth/
src/devices/
include/auralis/core/
include/auralis/bluetooth/
include/auralis/devices/
ui/
tests/
config/
README.md
```

Also search the repository for:

```text
ApplicationCore
BluetoothManager
DeviceManager
Logger
ConfigurationManager
QML registration
QQmlApplicationEngine
qmlRegisterType
qmlRegisterSingleton
setContextProperty
Qt6::DBus
Qt6::Bluetooth
QAbstractListModel
```

Determine:

1. Which Bluetooth interfaces/stubs already exist.
2. Which classes are QObject-based.
3. Which ownership model Phase 1 uses.
4. How services are initialized by `ApplicationCore`.
5. How backend state is exposed to QML.
6. How logging is performed.
7. How errors are represented.
8. How tests are structured.
9. Which CMake targets own Bluetooth code.
10. Whether Qt DBus is already linked.

Do not restructure the entire repository if the existing Phase 1 structure already provides appropriate locations.

### Required inspection output before major edits

Internally form a concise implementation map:

```text
Existing class/file
    ->
Phase 2 responsibility
    ->
Required modification
```

Then implement against that map.

---

# 5. PHASE 1 REGRESSION GUARDRAIL

The following baseline workflow already works and must continue to work after Phase 2:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure

./build/apps/desktop/auralis-desktop
```

Do not finish the task with the repository in a state where any of those commands fail.

No hidden manual code-generation step may be required between configure and build.

No environment variable should be mandatory for the normal desktop application.

---

# 6. REQUIRED ARCHITECTURE

Implement a layered architecture approximately like this:

```text
+----------------------------------------------------+
|                    QML UI                          |
|                                                    |
| Scan | Stop | Refresh | Device List | Adapter      |
+-------------------------+--------------------------+
                          |
                          v
+----------------------------------------------------+
|          BluetoothDeviceListModel / UI API         |
+-------------------------+--------------------------+
                          |
                          v
+----------------------------------------------------+
|                 DeviceRegistry                     |
| stable records | dedup | update | remove | count   |
+-------------------------+--------------------------+
                          |
                          v
+----------------------------------------------------+
|                 BluetoothManager                   |
| top-level Bluetooth subsystem coordination         |
+-------------------+----------------+---------------+
                    |                |
                    v                v
          +----------------+   +---------------------+
          | AdapterManager |   | DiscoveryManager    |
          +-------+--------+   +----------+----------+
                  |                       |
                  +-----------+-----------+
                              |
                              v
                    +--------------------+
                    | BlueZDbusClient    |
                    | QtDBus / systemBus |
                    +---------+----------+
                              |
                              v
                       org.bluez
                              |
                              v
                       Linux Bluetooth
```

Key design rule:

> The QML layer must never contain D-Bus logic.

QML should operate on simple properties, models, signals, and invokable application methods.

D-Bus type parsing belongs in the Bluetooth backend.

---

# 7. CMAKE / DEPENDENCY WORK

Check existing CMake first.

Add Qt D-Bus only where needed.

Expected Qt pattern:

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    DBus
)
```

or equivalent integration with the project's existing component discovery.

The Bluetooth target that actually uses QtDBus should link:

```cmake
Qt6::DBus
```

Do not unnecessarily link QtDBus into every Auralis target.

Do not add external D-Bus C libraries if QtDBus is sufficient.

Do not introduce Boost, glibmm, sdbus-c++, or another IPC framework for this phase unless the repository already intentionally uses one.

### Build requirement

The build must fail clearly at configure time if the required QtDBus development component is missing.

Do not silently compile a fake Bluetooth implementation when DBus is absent on the supported Linux target.

---

# 8. BLUEZ D-BUS CLIENT

Create or complete a dedicated BlueZ D-Bus client.

Suggested conceptual class:

```cpp
class BlueZDbusClient : public QObject
```

The exact class name may differ if Phase 1 already provides an equivalent type.

Responsibilities:

- connect to the **system bus**;
- validate D-Bus connectivity;
- observe whether `org.bluez` is available;
- obtain the initial BlueZ object snapshot;
- subscribe to BlueZ object/interface changes;
- subscribe to property changes;
- provide asynchronous Adapter1 method calls;
- convert raw D-Bus data into application-friendly data;
- emit clean Qt signals;
- surface D-Bus failures without crashing.

The client should not own QML presentation behavior.

### System bus

Use:

```cpp
QDBusConnection::systemBus()
```

Do not use `sessionBus()` for BlueZ.

Validate:

```cpp
connection.isConnected()
```

If the system bus is unavailable, expose an error state.

---

# 9. BLUEZ SERVICE AVAILABILITY

Auralis must correctly handle BlueZ being unavailable.

Recommended behavior:

1. Start the Bluetooth subsystem.
2. Check the system bus.
3. Check/watch the ownership of:

```text
org.bluez
```

4. If BlueZ exists, initialize the BlueZ object model.
5. If BlueZ disappears while Auralis is running:
   - stop reporting discovery as active;
   - mark the adapter unavailable;
   - surface an error/status message;
   - invalidate or clear transient BlueZ device objects safely;
   - do not crash.
6. If BlueZ returns:
   - allow reinitialization;
   - rebuild state from `GetManagedObjects`;
   - do not require restarting Auralis.

Using a `QDBusServiceWatcher` is appropriate.

Full production-grade service recovery/backoff belongs to later hardening, but Phase 2 must not become unusable or crash when BlueZ is absent.

---

# 10. OBJECT MANAGER BOOTSTRAP

Do not assume discovery begins from an empty BlueZ object tree.

BlueZ can already contain:

- adapters;
- cached devices;
- paired devices;
- connected devices;
- previously discovered objects.

At backend initialization, call:

```text
org.freedesktop.DBus.ObjectManager.GetManagedObjects
```

against the BlueZ root object.

Use this initial snapshot to discover:

```text
org.bluez.Adapter1
org.bluez.Device1
```

objects that already exist.

Then subscribe to:

```text
org.freedesktop.DBus.ObjectManager.InterfacesAdded
org.freedesktop.DBus.ObjectManager.InterfacesRemoved
```

and:

```text
org.freedesktop.DBus.Properties.PropertiesChanged
```

for live updates.

### Ordering requirement

Avoid the classic race where:

1. snapshot is fetched;
2. a device appears;
3. signal subscription starts too late;
4. Auralis misses the device.

Prefer to establish signal subscriptions before or as part of initialization and then reconcile the snapshot.

The implementation must be idempotent so duplicate information from snapshot + event cannot create duplicate registry rows.

---

# 11. ASYNCHRONOUS D-BUS CALLING MODEL

Do not block the QML/UI event loop with long synchronous D-Bus calls.

Prefer QtDBus asynchronous facilities such as:

```text
QDBusInterface::asyncCall(...)
QDBusPendingCall
QDBusPendingCallWatcher
QDBusPendingReply
```

Use callbacks/signals to update state after the reply arrives.

For small property reads where the existing repository intentionally uses synchronous calls, evaluate whether they are safe, but do not introduce repeated blocking reads inside discovery event processing.

### Important

The UI must not optimistically assume scanning has begun simply because the button was clicked.

State should transition according to the actual request lifecycle and BlueZ adapter properties.

---

# 12. ADAPTER MANAGER

Implement or complete an AdapterManager-style component.

Responsibilities:

- discover available BlueZ Adapter1 objects;
- choose an active/default adapter;
- expose adapter state;
- detect adapter removal;
- detect adapter property changes;
- provide the adapter object path to DiscoveryManager.

Minimum adapter state to expose:

```text
objectPath
address
name
alias
powered
discoverable
pairable
discovering
available
```

Optional but useful:

```text
modalias
roles/capabilities where available
```

### Adapter selection strategy

For Phase 2:

1. Prefer an adapter already selected by existing configuration if such configuration exists.
2. Otherwise prefer `/org/bluez/hci0` if present.
3. Otherwise choose the first suitable Adapter1 object.
4. Never hard-code the entire application to assume only hci0 can exist.

If multiple adapters are present, Phase 2 does not need a full adapter selection UI unless the existing design already provides one.

The architecture must still avoid making `hci0` an irreversible assumption.

---

# 13. ADAPTER POWER STATE

Scanning cannot begin if the adapter is not ready/powered.

Required behavior:

```text
adapter missing
    -> Scan disabled
    -> status = "Bluetooth adapter not available"

adapter present, Powered=false
    -> Scan disabled
    -> status = "Bluetooth is powered off"

adapter present, Powered=true
    -> Scan enabled
```

Do not automatically change the adapter's Powered property in Phase 2 unless that behavior already exists by design.

The required Phase 2 control is discovery, not system Bluetooth power administration.

---

# 14. DISCOVERY MANAGER

Implement a dedicated discovery lifecycle manager.

Suggested state machine:

```text
Unavailable
    |
    v
Idle
    |
    +---- startScan() ----> Starting
    |                         |
    |                  StartDiscovery success
    |                         |
    |                         v
    |                    Discovering
    |                         |
    |                  stopScan()
    |                         |
    |                         v
    |                     Stopping
    |                         |
    |                 StopDiscovery success
    |                         |
    +<------------------------+
```

Error transitions may move to:

```text
Error
```

but the manager should recover to a useful state when the adapter/BlueZ becomes available again.

Possible enum:

```cpp
enum class DiscoveryState {
    Unavailable,
    Idle,
    Starting,
    Discovering,
    Stopping,
    Error
};
```

Expose state in a QML-friendly form.

### Authoritative discovering state

The BlueZ Adapter1:

```text
Discovering
```

property is authoritative.

Use method reply + Adapter1 property changes together.

Avoid a fragile boolean that can permanently diverge from BlueZ.

---

# 15. START DISCOVERY

Implement:

```text
org.bluez.Adapter1.StartDiscovery()
```

through D-Bus.

Expected flow:

```text
QML Scan button
    |
    v
BluetoothManager.startScan()
    |
    v
DiscoveryManager.startScan()
    |
    v
validate BlueZ + adapter + powered + state
    |
    v
BlueZDbusClient.startDiscovery(adapterPath)
    |
    v
org.bluez.Adapter1.StartDiscovery()
    |
    v
reply + Adapter1 Discovering=true
    |
    v
UI shows scanning
```

Handle BlueZ errors cleanly.

At minimum map/log:

```text
org.bluez.Error.NotReady
org.bluez.Error.Failed
org.bluez.Error.InProgress
```

BlueZ versions may vary in the exact error surfaced.

Do not crash or leave the UI permanently stuck in `Starting`.

---

# 16. STOP DISCOVERY

Implement:

```text
org.bluez.Adapter1.StopDiscovery()
```

through D-Bus.

Important BlueZ behavior:

Discovery sessions are shared/ref-counted between clients.

Auralis must only release the discovery session it acquired.

Do not assume stopping Auralis discovery necessarily means the adapter's global `Discovering` property must immediately become false if another client is also scanning.

The UI should distinguish:

- Auralis owns an active discovery request;
- BlueZ adapter is globally discovering.

For a simple Phase 2 UI, one `isScanning` property may represent Auralis's discovery operation while a secondary diagnostic property can expose raw Adapter1 `Discovering`.

At minimum, do not repeatedly call StopDiscovery when Auralis does not own a discovery request.

Handle:

```text
org.bluez.Error.NotReady
org.bluez.Error.Failed
org.bluez.Error.NotAuthorized
```

and unexpected errors.

---

# 17. DISCOVERY FILTER POLICY

The Phase 2 goal is to discover both:

- Bluetooth Classic / BR/EDR;
- Bluetooth Low Energy.

BlueZ's discovery transport defaults to `auto`.

For the first correct implementation:

- do not apply a restrictive UUID filter;
- do not filter only to hearing-device names;
- do not filter to only LE;
- do not filter to only BR/EDR;
- do not hide devices simply because their name is absent.

If using:

```text
Adapter1.SetDiscoveryFilter
```

use a deliberately broad policy such as `Transport="auto"` and document why.

However, it is acceptable and simpler to rely on the default discovery behavior during Phase 2.

Be aware that setting a discovery filter changes RSSI update behavior in BlueZ.

Avoid generating excessive UI updates without need.

---

# 18. BLUETOOTH DEVICE DOMAIN MODEL

Implement a real BluetoothDevice domain representation.

It must be able to represent incomplete devices because BlueZ properties may appear gradually.

Minimum fields:

```text
internalId
objectPath
adapterPath
address
addressType
name
alias
rssi
hasRssi
paired
connected
trusted
blocked
servicesResolved
classOfDevice
hasClassOfDevice
icon
appearance
hasAppearance
uuids
manufacturerData
serviceData
lastSeen
```

Also consider storing:

```text
txPower
hasTxPower
legacyPairing
modalias
bonded
```

if parsing them is straightforward.

Do not force optional D-Bus properties to fake values that look real.

Examples:

Bad:

```text
RSSI = 0
```

when RSSI is unknown.

Better:

```text
hasRssi = false
rssi = 0
```

or use `std::optional<qint16>` internally and expose a separate QML validity role.

### Display name policy

For display:

1. use `Alias` when available;
2. else `Name`;
3. else address;
4. else a neutral label such as `Unknown Bluetooth Device`.

Do not use the device name as its identity.

---

# 19. DEVICE IDENTITY AND DEDUPLICATION

DeviceRegistry must not create duplicate rows every time BlueZ emits another update.

Use BlueZ object path as the authoritative identity for the lifetime of a BlueZ Device1 object.

Also maintain a secondary index by suitable identity data such as:

```text
adapter + address + addressType
```

when the address is available.

Rules:

- never deduplicate solely by Name;
- never deduplicate solely by Alias;
- never deduplicate solely by RSSI;
- two devices with the same user-visible name may be distinct devices;
- `InterfacesAdded` for an existing object must become an update, not a duplicate;
- initial `GetManagedObjects` data followed by the same interface event must remain one row.

### LE privacy caveat

Some LE devices use random/private addresses.

Phase 2 does **not** need to guarantee durable cross-restart identity for an unpaired privacy-enabled LE device.

Do not invent persistence semantics that BlueZ cannot guarantee before pairing.

Persistent known-device identity belongs to later phases.

---

# 20. DEVICE1 PROPERTY PARSING

Parse the BlueZ `org.bluez.Device1` properties defensively.

Relevant properties include:

```text
Address
AddressType
Name
Alias
Icon
Class
Appearance
UUIDs
Paired
Bonded
Connected
Trusted
Blocked
Adapter
LegacyPairing
Modalias
RSSI
TxPower
ManufacturerData
ServiceData
ServicesResolved
```

Treat optional properties as optional.

Do not reject an entire device because:

- Name is missing;
- RSSI is missing;
- UUIDs are missing;
- ManufacturerData is absent;
- ServiceData is absent.

A newly discovered BLE device can legitimately be incomplete.

### D-Bus type conversion

Keep D-Bus-specific decoding inside the Bluetooth backend.

Convert raw variants into stable application types such as:

```text
QString
QStringList
bool
qint16
quint16
quint32
QByteArray
QHash<quint16, QByteArray>
QHash<QString, QByteArray>
QVariantMap
```

Use whichever representation best matches existing project conventions.

Do not expose `QDBusArgument` or raw `QDBusVariant` objects to QML.

---

# 21. DEVICE REGISTRY

Implement DeviceRegistry as the canonical collection of Bluetooth devices known to the Auralis Bluetooth subsystem.

Required operations:

```text
upsertDevice(...)
updateDeviceProperties(...)
removeDevice(objectPath)
clearTransientDevices()
findByObjectPath(...)
findByAddress(...)
contains(...)
count()
```

Exact function names may differ.

Required behavior:

- add new devices;
- update existing devices in place;
- deduplicate;
- preserve stable rows/objects during property updates;
- remove a device when BlueZ removes its Device1 interface/object;
- update `lastSeen`;
- notify Qt model/view consumers precisely;
- avoid full model reset for every RSSI change.

### Last seen semantics

Update `lastSeen` when Auralis receives meaningful live evidence for a device, for example:

- first discovery/interface appearance;
- RSSI update;
- advertisement ManufacturerData update;
- ServiceData update;
- another Device1 property event observed while scanning.

Use a clear UTC or local `QDateTime` convention consistently.

Prefer UTC internally.

---

# 22. INTERFACES ADDED

Handle:

```text
org.freedesktop.DBus.ObjectManager.InterfacesAdded
```

When the signal contains:

```text
org.bluez.Adapter1
```

route it to adapter management.

When the signal contains:

```text
org.bluez.Device1
```

parse its properties and upsert it into DeviceRegistry.

One InterfacesAdded signal may contain multiple interfaces.

Do not assume Device1 is the only interface present.

Ignore unrelated BlueZ interfaces safely.

---

# 23. INTERFACES REMOVED

Handle:

```text
org.freedesktop.DBus.ObjectManager.InterfacesRemoved
```

If `org.bluez.Device1` is removed for an object path:

- remove the corresponding live registry entry or transition it according to the existing registry design;
- emit proper model row removal notifications;
- do not retain a zombie row indefinitely.

If `org.bluez.Adapter1` is removed:

- mark the adapter unavailable;
- stop/discard Auralis discovery ownership safely;
- disable scan controls;
- surface status;
- select another valid adapter if one exists.

Do not crash if a removal event references an object Auralis has not seen.

---

# 24. PROPERTIES CHANGED

Handle:

```text
org.freedesktop.DBus.Properties.PropertiesChanged
```

For Device1:

- update only supplied properties;
- process the invalidated-property list;
- preserve all other current values;
- update relevant model roles;
- update lastSeen when appropriate.

For Adapter1:

- update Powered;
- Discovering;
- Name/Alias;
- Discoverable;
- Pairable;
- other state used by the UI.

### Invalidated properties

If a property is invalidated, mark it unknown/unavailable instead of preserving stale data forever.

For example:

```text
RSSI invalidated
```

should result in:

```text
hasRssi = false
```

unless a fresh value is subsequently received.

---

# 25. DEVICE LIST MODEL FOR QML

Expose discovered devices through a proper Qt model.

Prefer:

```cpp
QAbstractListModel
```

or the existing Phase 1 model abstraction if already present.

Suggested roles:

```text
InternalIdRole
ObjectPathRole
AddressRole
AddressTypeRole
DisplayNameRole
NameRole
AliasRole
RssiRole
HasRssiRole
PairedRole
ConnectedRole
TrustedRole
BlockedRole
ServicesResolvedRole
IconRole
ClassRole
HasClassRole
AppearanceRole
HasAppearanceRole
UuidsRole
LastSeenRole
TransportHintRole
```

Manufacturer/service advertisement blobs do not need to be rendered on the main Phase 2 screen, but should remain available in the backend and may optionally be exposed for diagnostics.

### Model correctness

Implement:

```text
rowCount()
data()
roleNames()
```

correctly.

For insertions use:

```text
beginInsertRows()
endInsertRows()
```

For removals:

```text
beginRemoveRows()
endRemoveRows()
```

For changes:

```text
dataChanged(...)
```

with relevant roles.

Do not call `beginResetModel()` for every device update.

Stable model rows are important because later phases may maintain selection/state tied to devices.

---

# 26. TRANSPORT / DEVICE TYPE HINT

Phase 2 UI should show address/type information.

Do not pretend every device can be perfectly categorized as `Classic`, `BLE`, or `Dual` from one property alone.

Use defensible hints.

At minimum:

```text
AddressType = random/public
```

is useful for LE-oriented information but is not itself a complete dual-mode capability classification.

Possible implementation approach:

- expose the raw BlueZ `AddressType`;
- expose UUID/class/appearance information;
- optionally derive a best-effort `transportHint`.

If deriving:

```text
BLE
Classic
Classic / BLE
Unknown
```

document the heuristic and do not use it for critical behavior.

For the main UI, displaying raw `AddressType` plus a best-effort label is acceptable.

---

# 27. BLUETOOTH MANAGER PUBLIC API

The existing `BluetoothManager` should become the QML/application-facing coordinator.

Suggested public Qt-facing API:

```cpp
Q_PROPERTY(bool available READ available NOTIFY availableChanged)
Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY adapterPoweredChanged)
Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
Q_PROPERTY(QAbstractItemModel* devices READ devices CONSTANT)
```

Suggested invokable/slot methods:

```cpp
Q_INVOKABLE void startScan();
Q_INVOKABLE void stopScan();
Q_INVOKABLE void refresh();
```

Adapt to existing Phase 1 exposure style.

### `refresh()` semantics

Define refresh clearly.

Recommended Phase 2 behavior:

```text
if currently scanning:
    reconcile/refresh current BlueZ object snapshot
    continue scanning

if idle:
    refresh BlueZ object snapshot
    optionally perform a stop/start scan only if explicitly designed
```

Do not implement `refresh()` as destructive remove/re-add churn unless necessary.

A simple and predictable option is:

1. re-query `GetManagedObjects`;
2. reconcile adapter/device registry;
3. leave current discovery ownership unchanged.

---

# 28. ERROR MODEL

Errors must be visible to both logs and UI.

Distinguish:

```text
NoSystemBus
BlueZUnavailable
NoAdapter
AdapterPoweredOff
DiscoveryStartFailed
DiscoveryStopFailed
DbusCallFailed
MalformedDbusPayload
AdapterRemoved
```

You may use an enum plus human-readable messages.

Do not expose only opaque strings deep in D-Bus callbacks.

Log the original D-Bus error name and message where available.

Example structured log intent:

```text
WARN Bluetooth DiscoveryStartFailed
adapter=/org/bluez/hci0
dbusError=org.bluez.Error.NotReady
message="Resource Not Ready"
```

Use the existing Logger rather than `std::cout` or uncontrolled `qDebug()` calls throughout production code.

Temporary debug logging may be used during development but clean it before finishing.

---

# 29. EXPECTED ERROR CASES

PHASE 2 must handle all of these intentionally:

## 29.1 BlueZ not running

Expected:

- application still launches;
- Bluetooth status shows unavailable;
- scan control disabled;
- no crash.

## 29.2 No Bluetooth adapter

Expected:

- status clearly says no adapter;
- scan disabled;
- empty device list is valid.

## 29.3 Bluetooth adapter powered off

Expected:

- adapter shown as present but powered off;
- scan disabled;
- no repeated failed StartDiscovery loop.

## 29.4 Discovery already running

Possible causes:

- Auralis already requested it;
- another client is scanning;
- BlueZ global adapter state is Discovering.

Expected:

- no duplicate Auralis discovery ownership;
- no button-spam race;
- UI remains consistent.

## 29.5 StartDiscovery D-Bus failure

Expected:

- leave `Starting`;
- return to usable state;
- log exact D-Bus error;
- display concise error;
- retry possible.

## 29.6 StopDiscovery D-Bus failure

Expected:

- do not falsely claim a clean stop;
- reconcile Adapter1 Discovering state;
- allow future recovery.

## 29.7 Adapter removed while scanning

Expected:

- stop Auralis discovery ownership locally;
- disable controls;
- mark unavailable or switch adapter;
- no dangling DBus interface pointer;
- no crash.

## 29.8 Device disappears

Expected:

- registry/model handles removal;
- QML row disappears cleanly;
- no invalid model index access.

## 29.9 Incomplete device properties

Expected:

- device still appears;
- fallback display name used;
- absent RSSI is represented as unknown.

## 29.10 Malformed/unexpected D-Bus payload

Expected:

- validate before conversion;
- log warning;
- ignore bad field or object safely;
- never crash because a QVariant has an unexpected type.

---

# 30. UI REQUIREMENTS

Extend the current Phase 1 QML shell rather than replacing it with a full final GUI.

The Phase 2 UI is a functional engineering UI.

Required controls:

```text
Start Scan
Stop Scan
Refresh
```

Required status:

```text
BlueZ availability
Adapter availability
Adapter powered state
Scanning state
Device count
Latest non-fatal error/status
```

Required device list fields:

```text
Display name
Address
Address type / transport hint
RSSI / signal strength
```

Recommended optional indicators:

```text
Paired
Connected
Services resolved
```

These are read-only observations in Phase 2.

Do not add Pair or Connect buttons yet.

---

# 31. UI STATE BEHAVIOR

Button rules:

### Start Scan

Enabled only when:

```text
BlueZ available
AND adapter available
AND adapter powered
AND not Starting
AND not already owned scanning
```

### Stop Scan

Enabled when Auralis owns a discovery request or is in a state where stop is meaningful.

### Refresh

Enabled when the backend is initialized.

Prevent rapid repeated button clicks from creating duplicate pending D-Bus calls.

### Scanning indicator

Show a clear live state such as:

```text
Scanning...
```

not merely a button state.

---

# 32. RSSI DISPLAY

RSSI is optional.

When available, show:

```text
-51 dBm
```

When absent, show:

```text
Signal: Unknown
```

Optionally derive a simple visual strength indicator, but retain the numeric RSSI value.

Do not overcomplicate Phase 2 with calibrated distance estimation.

RSSI is not a reliable physical-distance measurement.

---

# 33. QML PERFORMANCE

Discovery can generate frequent property changes.

Avoid needless QML object recreation.

Requirements:

- update only affected model rows;
- update only changed roles where practical;
- do not rebuild the whole model for every RSSI signal;
- avoid expensive JavaScript loops over the full list on every update;
- keep D-Bus parsing in C++.

A modest number of nearby devices should update smoothly.

---

# 34. OBJECT OWNERSHIP / LIFETIME

Follow the repository's Phase 1 ownership style.

Recommended lifetime:

```text
ApplicationCore
    owns
BluetoothManager
    owns/coordinatess
AdapterManager
DiscoveryManager
BlueZDbusClient
DeviceRegistry
BluetoothDeviceListModel
```

Use QObject parent ownership where appropriate.

Avoid:

- raw owning pointers without clear lifetime;
- QObject double ownership;
- QML owning backend service singletons unintentionally;
- callbacks capturing destroyed objects.

When using `QDBusPendingCallWatcher`, ensure watcher lifetime and callback target lifetime are safe.

---

# 35. THREADING

QtDBus can operate naturally with the Qt event loop.

Do not add a worker thread solely because Bluetooth is asynchronous.

For Phase 2, keep architecture simple unless profiling or existing code requires otherwise.

Requirements:

- no blocking wait loops;
- no polling thread repeatedly calling BlueZ;
- no `sleep()` in the UI path;
- no busy waiting for `Discovering=true`;
- use signals/events.

---

# 36. LOGGING

Use the Phase 1 logging subsystem.

Recommended Phase 2 log events:

```text
BluetoothSubsystemInitializing
SystemBusConnected
SystemBusUnavailable
BlueZAvailable
BlueZUnavailable
BlueZSnapshotRequested
BlueZSnapshotReceived
AdapterAdded
AdapterUpdated
AdapterRemoved
AdapterSelected
DiscoveryStartRequested
DiscoveryStarted
DiscoveryStopRequested
DiscoveryStopped
DiscoveryFailed
DeviceAdded
DeviceUpdated
DeviceRemoved
MalformedDeviceProperty
```

Avoid logging every tiny RSSI update at INFO level.

High-frequency updates should be DEBUG/TRACE if logged at all.

---

# 37. TESTABILITY REQUIREMENT

Hardware-independent logic must be testable without a Bluetooth adapter.

Do not tightly couple:

```text
DeviceRegistry
Device1 property parser
BluetoothDeviceListModel
Discovery state logic
```

to a live BlueZ daemon.

Use dependency boundaries.

One reasonable pattern:

```text
IBlueZClient / BlueZClient abstraction
        |
        +-- BlueZDbusClient   production
        |
        +-- FakeBlueZClient   tests
```

If introducing an explicit interface would conflict with the existing Phase 1 design, achieve equivalent injection through constructor dependencies or a small transport abstraction.

Do not over-engineer a plugin system.

---

# 38. UNIT TESTS — REQUIRED

Add comprehensive automated tests.

At minimum:

## 38.1 Device parsing tests

Test Device1 map parsing for:

- complete Classic-style device;
- complete BLE-style device;
- missing Name;
- missing Alias;
- missing RSSI;
- random AddressType;
- UUID list;
- class;
- appearance;
- ManufacturerData;
- ServiceData;
- malformed optional field;
- invalidated RSSI.

## 38.2 DeviceRegistry tests

Test:

- add device;
- duplicate add becomes update;
- update one property preserves others;
- remove device;
- remove unknown path is safe;
- secondary address lookup;
- two same-name devices remain distinct;
- device count signal;
- lastSeen update;
- clear/reconcile behavior.

## 38.3 Model tests

Test:

- rowCount;
- roleNames;
- data values;
- insertion notification;
- update notification;
- removal;
- unknown RSSI role;
- display name fallback.

## 38.4 Discovery state tests

Using a fake client:

- idle -> starting -> discovering;
- start failure -> recoverable state;
- discovering -> stopping -> idle;
- stop failure;
- adapter powered off;
- adapter disappears;
- BlueZ disappears;
- duplicate start request ignored/rejected;
- duplicate stop request safe.

## 38.5 Adapter selection tests

Where practical:

- hci0 preferred;
- fallback adapter chosen;
- powered state update;
- selected adapter removal.

---

# 39. INTEGRATION TESTS

Add integration coverage that can coexist with CI systems having no Bluetooth hardware.

Do not make every `ctest` run require a physical adapter.

Recommended split:

```text
unit tests
    -> always run

BlueZ live integration tests
    -> opt-in / environment-gated
```

Example gate:

```text
AURALIS_RUN_BLUETOOTH_INTEGRATION=1
```

If the current project has a different convention, use it.

Possible live integration tests:

- system bus connects;
- `org.bluez` is visible;
- `GetManagedObjects` succeeds;
- at least one Adapter1 object can be enumerated;
- StartDiscovery/StopDiscovery round-trip when hardware is available.

Integration tests must fail with clear diagnostics when explicitly enabled but prerequisites are absent.

They should be skipped cleanly when not enabled.

---

# 40. OPTIONAL DBUS FIXTURE TESTING

If time and repository architecture permit, consider a test double that emits BlueZ-like object manager events on a private/session D-Bus.

This can test:

```text
InterfacesAdded
InterfacesRemoved
PropertiesChanged
```

end-to-end without real hardware.

Do not let building a sophisticated fake D-Bus server delay the core Phase 2 deliverable.

The mandatory requirement is clean injection plus unit-testable registry/state logic.

---

# 41. REFRESH / RECONCILIATION ALGORITHM

Implement a robust reconciliation path from `GetManagedObjects`.

Suggested algorithm:

```text
snapshotAdapterPaths = {}
snapshotDevicePaths = {}

for each managed object:
    if Adapter1:
        upsert adapter
        snapshotAdapterPaths += path

    if Device1:
        parse
        upsert registry device
        snapshotDevicePaths += path

reconcile adapters

for device objects:
    remove registry entries whose BlueZ object path
    no longer exists in snapshot
    only if they are defined as BlueZ-live transient objects
```

Be careful not to delete future persistent records introduced by later phases.

Phase 2 registry should clearly distinguish BlueZ live objects from eventual persisted known devices.

---

# 42. RACE CONDITIONS TO PREVENT

Explicitly guard against:

## Rapid Scan clicking

```text
start
start
start
```

must not create three owned StartDiscovery sessions.

## Start then immediate Stop

A Stop request arriving while StartDiscovery reply is pending must resolve safely.

Use a desired-state or serialized operation approach if necessary.

## Adapter removal during pending StartDiscovery

Pending callback must check that the adapter/current operation is still valid.

## BlueZ restart

Callbacks from old state must not re-mark the new service instance incorrectly.

A simple generation token / initialization generation is acceptable if needed.

## Duplicate Device1 events

Registry must be idempotent.

---

# 43. RECOMMENDED DISCOVERY OPERATION STRATEGY

Keep discovery requests serialized.

A clean model:

```text
desiredScanning = true/false
ownedDiscoverySession = true/false
pendingOperation = none/start/stop
```

Then reconcile:

```text
if desiredScanning
AND prerequisites valid
AND !ownedDiscoverySession
AND no operation pending:
    StartDiscovery

if !desiredScanning
AND ownedDiscoverySession
AND no operation pending:
    StopDiscovery
```

This helps handle rapid user intent changes.

Do not let the state machine become excessively complex for Phase 2, but make button spam safe.

---

# 44. SECURITY / PERMISSIONS

Auralis should operate as the normal desktop user.

Do not:

- run the application as root;
- call `sudo`;
- alter system D-Bus policy automatically;
- edit BlueZ daemon configuration as part of normal startup;
- install permissive D-Bus rules.

If a D-Bus authorization failure occurs, surface it as a clear error and document it.

Discovery through normal BlueZ policy should be performed through the user's normal session/application privileges.

---

# 45. NO SHELL-COMMAND BACKEND

This requirement is strict.

The following is forbidden in production code:

```cpp
QProcess bluetoothctl
QProcess busctl
QProcess dbus-send
QProcess btmgmt
system("bluetoothctl ...")
popen(...)
```

for discovering or parsing Bluetooth devices.

There should be no regex parser over `bluetoothctl devices` output.

The reason is architectural:

- terminal output is not a stable application API;
- asynchronous BlueZ state is lost;
- parsing is brittle;
- future lifecycle management requires real D-Bus objects.

If existing Phase 1 diagnostic code uses QProcess for unrelated development diagnostics, do not expand that pattern into Bluetooth discovery.

---

# 46. SOURCE FILE ORGANIZATION

Use the existing repository structure.

If the Phase 1 structure matches the roadmap, a reasonable target could look like:

```text
include/auralis/bluetooth/
    BluetoothManager.hpp
    BlueZDbusClient.hpp
    AdapterManager.hpp
    DiscoveryManager.hpp
    BluetoothDevice.hpp
    DeviceRegistry.hpp
    BluetoothDeviceListModel.hpp

src/bluetooth/
    BluetoothManager.cpp
    BlueZDbusClient.cpp
    AdapterManager.cpp
    DiscoveryManager.cpp
    BluetoothDevice.cpp
    DeviceRegistry.cpp
    BluetoothDeviceListModel.cpp
```

Potential helper:

```text
BlueZTypes.hpp/.cpp
BlueZPropertyParser.hpp/.cpp
```

Do not create files merely to match this list if equivalent Phase 1 files already exist.

Prefer evolving existing stubs.

---

# 47. BLUEZ CONSTANTS

Centralize D-Bus constants rather than scattering magic strings.

Example conceptual constants:

```text
kBlueZService = "org.bluez"
kObjectManagerInterface = "org.freedesktop.DBus.ObjectManager"
kPropertiesInterface = "org.freedesktop.DBus.Properties"
kAdapterInterface = "org.bluez.Adapter1"
kDeviceInterface = "org.bluez.Device1"
```

Likewise centralize frequently used property names.

Do not build an enormous abstraction around string constants.

---

# 48. PROPERTY PARSER DESIGN

Make Device1 property parsing reusable and independently testable.

A useful conceptual API:

```cpp
BluetoothDeviceData parseDevice(
    const QString& objectPath,
    const QVariantMap& properties);

DevicePropertyChanges parseDeviceChanges(
    const QVariantMap& changed,
    const QStringList& invalidated);
```

or equivalent.

The parser should:

- check types;
- return partial valid data;
- report parse warnings;
- avoid throwing/terminating for optional bad fields.

If project style uses exceptions, do not throw on routine absent optional properties.

---

# 49. INTERNAL DATA VS QOBJECT DEVICE INSTANCES

Choose one consistent model.

Possible design A:

```text
DeviceRegistry stores BluetoothDeviceData values
BluetoothDeviceListModel reads values
```

Possible design B:

```text
DeviceRegistry owns stable BluetoothDevice QObject instances
BluetoothDeviceListModel references them
```

Either is acceptable.

Preferred properties:

- stable identity;
- no row churn on updates;
- simple tests;
- clear ownership;
- no QML ownership confusion.

Do not create a new QObject for a device on every `PropertiesChanged`.

---

# 50. QML EXPOSURE

Use the Phase 1-established pattern.

Possible patterns:

```text
context property
registered singleton
QML element
ApplicationCore property
```

Do not introduce multiple competing exposure mechanisms.

The QML layer should be able to write behavior conceptually like:

```qml
Button {
    text: "Start Scan"
    enabled: bluetoothManager.canStartScan
    onClicked: bluetoothManager.startScan()
}

ListView {
    model: bluetoothManager.devices
}
```

Avoid exposing BlueZ object proxies directly into QML.

---

# 51. SUGGESTED PHASE 2 QML LAYOUT

Keep styling consistent with Phase 1.

Conceptual screen:

```text
AURALIS

Bluetooth Discovery
------------------------------------------------

BlueZ:      Ready
Adapter:    smit / E8:9E:B4:13:4C:CC
Power:      On
Scan:       Active
Devices:    7

[ Start Scan ] [ Stop Scan ] [ Refresh ]

Nearby Devices
------------------------------------------------

Hearing Aid L
AA:BB:CC:DD:EE:01
random / BLE
RSSI: -51 dBm

Hearing Aid R
AA:BB:CC:DD:EE:02
random / BLE
RSSI: -56 dBm

Headphones
AA:BB:CC:DD:EE:03
public
RSSI: -67 dBm
```

Do not spend Phase 2 effort on final visual polish, animations, or a full navigation redesign.

Functionality and state correctness come first.

---

# 52. DEVICE SORTING

A stable useful ordering is recommended.

Possible default:

1. devices with known RSSI;
2. stronger signal first;
3. then display name/address.

However, continuously resorting rows on every RSSI update can make the UI jump.

For Phase 2, prefer a stable discovery order unless the existing UI has sorting support.

If sorting by RSSI is implemented, use a proxy model and consider throttling/reasonable stability.

Do not make model correctness harder than necessary.

---

# 53. FILTERING

DeviceRegistry should be designed so filtering can be added.

Phase 2 main UI should not hide arbitrary devices.

Optional UI filters may include:

```text
All
Named
Paired
BLE-ish
Classic-ish
```

but are not required for the exit gate.

Do not implement name-based "hearing aid only" filtering as the default.

Auralis needs visibility into what the real hardware exposes.

---

# 54. TEST DATA

Create realistic synthetic Device1 maps for tests.

Examples should include:

### BLE device

```text
Address      = AA:BB:CC:DD:EE:01
AddressType  = random
Alias        = Hearing Aid L
RSSI         = -51
Appearance   = ...
UUIDs        = [...]
ManufacturerData = {...}
ServiceData      = {...}
```

### Classic device

```text
Address      = AA:BB:CC:DD:EE:03
AddressType  = public
Name         = Headphones
Class        = ...
RSSI         = -67
UUIDs        = [...]
```

### Incomplete device

```text
Address      = AA:BB:CC:DD:EE:99
AddressType  = random
```

Tests must prove the incomplete device is still represented.

---

# 55. DOCUMENTATION

Update project documentation after implementation.

At minimum add/update a Phase 2 section explaining:

- Auralis uses BlueZ D-Bus directly;
- system bus is used;
- how discovery architecture works;
- build dependency on Qt6 DBus;
- how to start/stop scan in the UI;
- how to run tests;
- how to enable live Bluetooth integration tests if added;
- common errors:
  - BlueZ unavailable;
  - adapter missing;
  - adapter powered off.

Do not document pairing as implemented.

---

# 56. COMMENTS AND CODE QUALITY

Write comments for:

- non-obvious D-Bus signatures;
- BlueZ discovery ownership/ref-count behavior;
- registry identity decisions;
- LE random-address limitations;
- race-condition handling.

Do not comment trivial C++ syntax.

Use the existing code style.

Apply:

- RAII;
- const-correctness;
- `override`;
- explicit ownership;
- scoped enums where appropriate;
- sensible `[[nodiscard]]` if project style uses it.

Avoid giant manager files containing every concern.

---

# 57. BUILD WARNING POLICY

Do not introduce new compiler warnings.

If the project uses warnings-as-errors, Phase 2 must compile cleanly under that policy.

Be careful with:

- sign conversions;
- QVariant conversions;
- unused signal parameters;
- narrowing `int16/uint16/uint32`;
- Qt deprecated APIs.

Use Qt 6 APIs.

Do not write new Qt 5 compatibility code unless the repository explicitly supports Qt 5.

---

# 58. MANUAL HARDWARE VALIDATION

After automated tests pass, validate with a real Bluetooth environment.

Required flow:

1. Launch Auralis normally as the desktop user.
2. Confirm adapter state is shown.
3. Put at least one discoverable Bluetooth device nearby.
4. Click **Start Scan** inside Auralis.
5. Confirm devices appear without invoking a terminal command.
6. Confirm device name/address are populated when BlueZ provides them.
7. Confirm RSSI updates when BlueZ provides it.
8. Bring another device into discovery mode.
9. Confirm it appears live.
10. Click **Stop Scan**.
11. Confirm Auralis releases its scan request.
12. Start scan again.
13. Confirm no duplicate rows are created.
14. Turn off or remove a nearby transient device as practical and confirm state remains stable.
15. Test Bluetooth powered-off behavior if safely possible.
16. Restore Bluetooth and confirm Auralis can return to a usable state.

Do not pair/connect devices as part of the Phase 2 acceptance gate.

---

# 59. OPTIONAL DEVELOPER DIAGNOSTICS

For manual comparison only, a developer may independently inspect BlueZ using system tools.

But Auralis must not require them.

Example developer-only verification ideas:

```text
bluetoothctl show
bluetoothctl devices
busctl tree org.bluez
dbus-monitor
```

Do not integrate those commands into production code or tests that are supposed to validate the actual implementation path.

---

# 60. EXIT GATE — REQUIRED

PHASE 2 is complete only when Auralis can independently:

1. connect to BlueZ through the system D-Bus;
2. detect a Bluetooth adapter;
3. represent adapter availability/power/discovery state;
4. request discovery using `Adapter1.StartDiscovery`;
5. receive real BlueZ device creation/events;
6. parse Device1 properties;
7. build/update DeviceRegistry;
8. deduplicate devices;
9. react to `PropertiesChanged`;
10. react to device removal;
11. stop discovery using `Adapter1.StopDiscovery`;
12. expose a live Qt model;
13. display nearby devices in QML;
14. show device count;
15. show address/type information;
16. show RSSI when available;
17. handle no adapter / powered-off / BlueZ unavailable without crashing;
18. pass automated tests;
19. pass a clean rebuild;
20. launch the desktop application normally.

No terminal interaction may be required for the user workflow.

---

# 61. PHASE 2 DEFINITION OF DONE

All of the following must be true:

```text
[ ] Phase 1 behavior still works
[ ] clean CMake configure succeeds
[ ] Ninja build succeeds
[ ] CTest succeeds
[ ] application launches
[ ] QtDBus dependency is correctly scoped
[ ] BlueZ system bus client exists
[ ] BlueZ availability is monitored
[ ] GetManagedObjects bootstrap works
[ ] Adapter1 objects are tracked
[ ] active adapter is selected safely
[ ] Powered state reaches UI
[ ] StartDiscovery works from Auralis UI
[ ] StopDiscovery works from Auralis UI
[ ] discovery state is event-driven
[ ] InterfacesAdded handled
[ ] InterfacesRemoved handled
[ ] PropertiesChanged handled
[ ] Device1 parser handles optional data
[ ] DeviceRegistry deduplicates
[ ] DeviceRegistry updates in place
[ ] DeviceRegistry removes stale BlueZ objects
[ ] lastSeen maintained
[ ] QAbstractListModel exposed to QML
[ ] model inserts are correct
[ ] model updates use dataChanged
[ ] model removals are correct
[ ] device count updates
[ ] RSSI shown when available
[ ] unknown RSSI handled
[ ] address/address type shown
[ ] malformed payload does not crash
[ ] BlueZ unavailable does not crash
[ ] adapter missing does not crash
[ ] adapter powered off does not crash
[ ] adapter removal does not crash
[ ] duplicate scan requests handled
[ ] logs use Phase 1 Logger
[ ] no production QProcess bluetoothctl backend
[ ] no pairing implementation
[ ] no connection implementation
[ ] no PipeWire implementation
[ ] unit tests cover parser/registry/model/state
[ ] hardware-independent tests run without Bluetooth
[ ] manual real-device scan succeeds
```

Do not declare Phase 2 complete if only mocked discovery works.

At least one real BlueZ discovery run must be validated on the target laptop.

---

# 62. IMPLEMENTATION ORDER

Follow this order unless the existing repository strongly suggests a safer equivalent.

## Step 1 — Repository audit

Understand current Phase 1 code.

Build and test before touching it.

Record the baseline result.

## Step 2 — QtDBus build integration

Add `Qt6::DBus` to the correct target.

Rebuild immediately.

## Step 3 — BlueZ constants/types/parser

Create small reusable D-Bus decoding utilities.

Add parser unit tests before live discovery.

## Step 4 — BlueZDbusClient

Implement:

```text
system bus
BlueZ service watcher
ObjectManager snapshot
InterfacesAdded
InterfacesRemoved
PropertiesChanged
Adapter1 async calls
```

Keep raw D-Bus handling here.

## Step 5 — AdapterManager

Track and select adapters.

Expose adapter state.

Test selection/state logic.

## Step 6 — BluetoothDevice + DeviceRegistry

Implement domain data, upsert, update, removal, identity, lastSeen.

Add registry tests.

## Step 7 — BluetoothDeviceListModel

Expose stable rows/roles.

Add model tests.

## Step 8 — DiscoveryManager

Implement lifecycle and serialized Start/Stop logic.

Use fake client tests.

## Step 9 — BluetoothManager integration

Connect:

```text
BlueZDbusClient
AdapterManager
DiscoveryManager
DeviceRegistry
DeviceListModel
```

Expose clean Qt API.

## Step 10 — ApplicationCore integration

Initialize Bluetooth subsystem in the established Phase 1 lifecycle.

Do not bypass ApplicationCore from `main.cpp` unless that is already the architecture.

## Step 11 — QML Phase 2 screen

Add functional controls/status/list.

No Pair/Connect functionality.

## Step 12 — Automated verification

Run clean build + all normal tests.

## Step 13 — Real hardware validation

Scan real nearby Classic/BLE devices.

## Step 14 — Documentation

Update README/docs.

## Step 15 — Final regression

Run exact clean build flow again.

---

# 63. CONTINUOUS VERIFICATION DURING IMPLEMENTATION

Do not make the entire Phase 2 change in one giant unverified batch.

After each meaningful increment:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

When CMake structure changes, reconfigure.

Before final completion, do a fully clean build:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure

./build/apps/desktop/auralis-desktop
```

Fix failures before continuing.

Do not disable existing tests to get green status.

---

# 64. GIT STRATEGY

Do not create commits unless the user explicitly wants the AI IDE to commit.

However, organize work so it could map cleanly to commits like:

```text
phase-2: add BlueZ D-Bus client

phase-2: implement adapter and discovery management

phase-2: implement Bluetooth device registry and Qt model

phase-2: add Bluetooth discovery UI

phase-2: add Bluetooth discovery tests and diagnostics
```

After the full phase is manually verified, the project may be tagged:

```text
v0.2-phase2
```

Do not tag before the real-device exit gate passes.

---

# 65. ARCHITECTURAL DECISIONS TO PRESERVE FOR PHASE 3

Build Phase 2 so Phase 3 can add:

```text
Pair()
CancelPairing()
Trusted
Connect()
Disconnect()
RemoveDevice()
Agent1
```

without rewriting discovery.

Specifically preserve:

- BlueZDbusClient as the low-level API boundary;
- DeviceRegistry as the canonical live device store;
- stable Device1 object paths;
- AdapterManager;
- asynchronous D-Bus calls;
- PropertiesChanged event handling;
- QML model stability.

Phase 3 should be able to extend these, not replace them.

---

# 66. ARCHITECTURAL DECISIONS TO PRESERVE FOR PHASE 4

Do not equate:

```text
BluetoothDevice == AudioEndpoint
```

They are different domain objects.

Phase 2 discovers Bluetooth devices.

Later Phase 4 will map connected Bluetooth devices to PipeWire audio endpoints.

Therefore do not add fields like:

```text
pipeWireNodeId
audioRouteId
```

to the Phase 2 Bluetooth device model unless they already belong in a higher-level cross-domain model.

Keep Bluetooth discovery clean.

---

# 67. LE AUDIO / HEARING DEVICE CAUTION

The discovery layer must not assume a "Bluetooth hearing aid" behaves like a standard A2DP headset.

A discovered device may ultimately use:

```text
Bluetooth Classic
A2DP
BLE
LE Audio
ASHA
vendor-specific behavior
```

Phase 2's job is observation and discovery.

Capture the metadata BlueZ exposes:

- address/type;
- UUIDs;
- appearance;
- manufacturer data;
- service data;
- class where applicable.

Do not prematurely classify compatibility.

This metadata will be important for later real-device compatibility analysis.

---

# 68. REJECT THESE IMPLEMENTATION SHORTCUTS

Do not use any of the following as the final Phase 2 solution:

### Shortcut A

```text
QProcess("bluetoothctl scan on")
```

Rejected.

### Shortcut B

Polling:

```text
bluetoothctl devices
```

every second.

Rejected.

### Shortcut C

Using only `QBluetoothDeviceDiscoveryAgent` and bypassing the required BlueZ D-Bus architecture.

Rejected for the primary Auralis backend.

### Shortcut D

A hard-coded demo list of fake devices in QML.

Rejected.

### Shortcut E

Full model reset on every D-Bus event.

Rejected.

### Shortcut F

Blocking the UI until StartDiscovery returns.

Rejected.

### Shortcut G

Assuming every Device1 has Name + RSSI.

Rejected.

### Shortcut H

Implementing pairing now because Device1 has a Pair method.

Rejected; that is Phase 3.

---

# 69. FINAL AI IDE DELIVERABLE

When implementation is complete, provide a final engineering report containing:

## A. Summary

What was implemented.

## B. Files changed

List every created/modified file and why.

## C. Architecture

Explain final flow:

```text
BlueZ
 -> QtDBus
 -> BlueZDbusClient
 -> Adapter/Discovery managers
 -> DeviceRegistry
 -> Qt model
 -> QML
```

## D. D-Bus coverage

State which interfaces/signals/methods are used.

Expected:

```text
ObjectManager.GetManagedObjects
ObjectManager.InterfacesAdded
ObjectManager.InterfacesRemoved
Properties.PropertiesChanged
Adapter1.StartDiscovery
Adapter1.StopDiscovery
```

## E. Test report

Show:

- unit tests added;
- normal CTest result;
- live integration result if enabled;
- manual real-device result.

## F. Build report

Show the result of:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## G. Manual UI verification

Confirm:

- app launched;
- adapter shown;
- Start Scan worked;
- real devices appeared;
- RSSI/address shown;
- Stop Scan worked;
- repeated scan did not duplicate rows.

## H. Known limitations

List only genuine Phase 2 limitations.

Expected examples:

- pairing intentionally not implemented;
- connection intentionally not implemented;
- unpaired LE privacy addresses are not durable identities;
- device capability classification is best-effort;
- full recovery hardening deferred.

## I. Phase gate conclusion

End with exactly one of:

```text
PHASE 2 EXIT GATE: PASSED
```

or:

```text
PHASE 2 EXIT GATE: NOT PASSED
```

If not passed, list remaining blockers.

Do not claim success merely because the code compiles.

---

# 70. FINAL EXPECTED USER EXPERIENCE

After successful PHASE 2 implementation, the user should be able to:

```text
Launch Auralis
    |
    v
See Bluetooth adapter status
    |
    v
Click "Start Scan"
    |
    v
Nearby real Bluetooth devices appear live
    |
    +--> name/alias
    +--> address
    +--> address/type information
    +--> RSSI when available
    |
    v
Device rows update as BlueZ properties change
    |
    v
Click "Stop Scan"
```

No terminal command is required.

No pairing is performed yet.

No audio routing is performed yet.

That is the complete and correct PHASE 2 boundary.

---

# 71. REFERENCE BEHAVIOR OF THE UNDERLYING APIs

Use these facts when implementing, but verify against the actual headers/docs available in the development environment when necessary:

### BlueZ Adapter1

`StartDiscovery()` creates Device objects as devices are discovered.

`StopDiscovery()` releases the discovery session acquired by the caller.

Discovery is shared between clients, so the global adapter `Discovering` property may reflect other clients too.

Transport discovery can be `auto`, `bredr`, or `le`; Phase 2 requires broad Classic + BLE discovery, therefore default/auto behavior is appropriate.

### BlueZ Device1

Important read-only/read-write state available through properties includes:

```text
Address
AddressType
Name
Alias
Icon
Class
Appearance
UUIDs
Paired
Bonded
Connected
Trusted
Blocked
Adapter
LegacyPairing
Modalias
RSSI
TxPower
ManufacturerData
ServiceData
ServicesResolved
```

Some properties are optional.

Properties change asynchronously through standard D-Bus `PropertiesChanged`.

### D-Bus ObjectManager

Use a snapshot + event model:

```text
GetManagedObjects
InterfacesAdded
InterfacesRemoved
```

This is preferred to manually polling every child object.

### QtDBus

Use Qt's system-bus connection and asynchronous call/watcher APIs.

The supported CMake integration is through:

```text
Qt6::DBus
```

---

# 72. IMPLEMENT NOW

Begin by inspecting the existing Auralis Phase 1 repository.

Do not rewrite it from scratch.

Then implement PHASE 2 incrementally in the ordered plan above.

Keep the application buildable throughout the work.

Do not advance into Phase 3.

The end goal is not "Bluetooth code exists."

The end goal is:

> **Auralis itself can start and stop real BlueZ Bluetooth discovery, maintain a correct live device registry, and display nearby Classic/BLE devices in Qt/QML with no terminal interaction.**
