# AURALIS — PER-BLUETOOTH-DEVICE BUTTON ALLOW / DISALLOW CONTROL — IMPLEMENTATION MASTER PROMPT

**Project:** Auralis  
**Feature:** Per-device Bluetooth hardware-button policy  
**Primary user goal:** Let the user explicitly **ALLOW** or **DISALLOW** button actions originating from each connected Bluetooth device so accidental button presses cannot spam Play/Pause or other media-control commands.  
**Target:** Current Auralis C++20 / Qt 6 / BlueZ / PipeWire Linux desktop architecture.

---

# 0. FEATURE INTENT

Auralis must provide a user-facing control for every relevant Bluetooth audio device:

```text
Device buttons: ALLOW
```

or:

```text
Device buttons: DISALLOW
```

When ALLOW is selected:

```text
the device's media/consumer-control buttons behave normally
```

When DISALLOW is selected:

```text
button actions originating from that Bluetooth device must not reach the desktop/media-player control path
```

The motivating failure case is accidental repeated headset/hearing-device presses causing:

```text
PLAY
PAUSE
PLAY
PAUSE
...
```

or equivalent media-control spam.

This feature must be **per Bluetooth device**, persisted across disconnect/reconnect and application restart.

It must not break:

- Bluetooth audio playback;
- device pairing;
- device connection;
- A2DP/HFP audio profiles;
- PipeWire routing;
- session restoration;
- volume routing unrelated to button blocking unless policy explicitly blocks volume buttons;
- Phase 0–8 reliability behavior.

---

# 1. DO NOT ASSUME HOW THE BUTTON EVENTS ARRIVE

This is critical.

Bluetooth headset buttons can reach Linux through different paths depending on hardware/profile/BlueZ/kernel configuration.

Possible paths include:

```text
AVRCP pass-through control
BlueZ MediaControl / media APIs
MPRIS mediation
kernel uinput / evdev consumer-control device
HID over GATT / HIDP
other BlueZ-generated input devices
```

Do not immediately implement a random MPRIS hook.

First determine the actual event path on the target Linux machine and for the target Bluetooth devices.

---

# 2. CURRENT AURALIS ARCHITECTURE TO PRESERVE

The current codebase already contains:

```text
BlueZDbusClient
IBlueZClient
BluetoothManager
BluetoothDeviceData
DeviceRegistry
BluetoothDeviceListModel
DeviceLifecycleManager
ReconnectPolicy
ConfigurationManager
DeviceRow.qml
DevicesPage.qml
PipeWireManager
SessionManager
RecoveryManager
```

`BluetoothDeviceData` already provides useful per-device stable information such as:

```text
object path
adapter path
Bluetooth address
address type
name/alias
paired
connected
UUIDs
```

Auralis also already maintains address-based identity for reconnect behavior.

Reuse these mechanisms.

Do not use the transient BlueZ object path as the durable settings key.

---

# 3. REQUIRED USER SEMANTICS

Implement a per-device persisted policy.

Recommended enum:

```cpp
enum class DeviceButtonPolicy {
    Allow,
    Disallow
};
```

Default:

```text
Allow
```

This avoids silently changing existing Linux Bluetooth behavior after upgrade.

The user explicitly opts into blocking.

---

# 4. POLICY SCOPE

At minimum the feature must block accidental **media transport buttons** from a device when policy is DISALLOW.

Required commands to consider:

```text
Play
Pause
Play/Pause toggle
Next
Previous
Stop
Fast-forward
Rewind
```

The specific minimum target is:

```text
Play/Pause
```

because that is the user-reported problem.

However, implement the architecture so policy can cleanly cover the entire media/consumer-control set.

---

# 5. DO NOT CONFUSE BUTTON BLOCKING WITH AUDIO BLOCKING

DISALLOW must not mean:

```text
disconnect Bluetooth
block audio transport
block A2DP
mute audio
unpair device
set BlueZ Device1.Blocked=true
```

Those are incorrect.

The device must remain:

```text
paired
connected
usable for audio
routable through PipeWire
```

Only the device-originating control event path is suppressed.

---

# 6. REQUIRED PHASE A — ACTUAL-MACHINE BUTTON EVENT PATH AUDIT

Before implementation, add a diagnostic investigation step.

On the actual development Linux machine, identify what happens when each physical Bluetooth button is pressed.

Use safe tools available on the system.

Possible diagnostics include:

```bash
bluetoothctl info <device>
busctl tree org.bluez
busctl introspect org.bluez <device-or-control-path>
```

Inspect for:

```text
org.bluez.MediaControl1
org.bluez.MediaPlayer1
org.bluez.MediaTransport1
HID interfaces
```

Also inspect Linux input devices:

```bash
cat /proc/bus/input/devices
ls -l /dev/input/by-id/
ls -l /dev/input/by-path/
```

Where permissions permit, inspect events using tools already installed, e.g.:

```text
libinput debug-events
evtest
```

Do not install random packages without need.

Record for each test device:

```text
Bluetooth address
BlueZ object path
input device name
input event node
udev properties
button event type/code
whether Play/Pause is KEY_PLAYPAUSE or another key
whether the event originates from a dedicated consumer-control node
```

If the device uses AVRCP without an evdev path, record the actual D-Bus path.

---

# 7. REQUIRED DECISION TREE

After investigation, choose the correct suppression backend.

## Backend A — BlueZ/AVRCP-level suppression

Use only if Auralis can reliably receive/intercept the device's AVRCP media-control command **before** it is forwarded to the desktop/media player.

This is preferred when technically valid because it is Bluetooth-aware and naturally per-device.

Do not pretend `org.bluez.MediaControl1` is an event interceptor if the API does not provide that capability.

Verify against actual BlueZ behavior.

---

## Backend B — evdev/uinput consumer-control suppression

Use when Bluetooth button events appear as Linux input events.

This is likely the most robust route for OS-level per-device suppression.

The implementation may:

```text
identify the specific Bluetooth-origin input node
open it
optionally EVIOCGRAB it while DISALLOW is active
consume blocked media key events
```

If ALLOW:

```text
do not grab the input node
```

or release any previous grab.

Important: an exclusive `EVIOCGRAB` blocks all events from that input node from other applications. If the node contains both wanted and unwanted buttons, decide policy carefully.

---

## Backend C — userspace forwarding/filtering

If the device's consumer-control node contains mixed events that must selectively pass through, consider:

```text
exclusive grab original device
read events
drop blocked keys
re-emit allowed events through a virtual uinput device
```

This is more complex and must not be implemented unless necessary.

If used, handle:

```text
device identity
virtual-device loop prevention
permissions
shutdown cleanup
crash recovery
```

---

# 8. FEATURE V1 SEMANTIC RECOMMENDATION

Unless actual hardware requires selective forwarding, implement V1 as:

```text
ALLOW
    Auralis does not interfere with this device's button input.

DISALLOW
    Auralis suppresses media/consumer-control button input generated by this device.
```

This is a master per-device button gate.

The UI should make this clear.

If the device has a dedicated media-control input node, V1 can block the entire node when DISALLOW.

If the node contains non-media functions that must remain usable, implement selective filtering rather than hiding that limitation.

---

# 9. PERSISTENT PER-DEVICE IDENTITY

Do not persist policy by:

```text
/org/bluez/hci0/dev_XX_XX_...
```

alone.

Use the project's stable Bluetooth identity logic.

Recommended durable key:

```text
adapter identity + normalized device Bluetooth address + address type
```

or reuse the existing address-index key if appropriate.

A reconnect may result in a new BlueZ object path.

Policy must survive:

```text
disconnect
reconnect
BlueZ restart
system D-Bus restart
application restart
device object-path replacement
```

---

# 10. DATA MODEL

Add a durable button policy to the appropriate per-device settings metadata.

Possible design:

```cpp
struct DeviceControlMetadata {
    bool autoReconnectEnabled = true;
    DeviceButtonPolicy buttonPolicy = DeviceButtonPolicy::Allow;
};
```

Do not overload reconnect semantics if a cleaner new structure is better.

Possible names:

```text
DevicePreferences
BluetoothDevicePreferences
DevicePolicy
DeviceControlPolicy
```

Keep domain meaning clear.

---

# 11. PERSISTENCE

The current in-memory reconnect metadata is not sufficient if it is not persisted across application restart.

Persist button policy using the project's existing configuration/persistence conventions.

Requirements:

```text
default = Allow
stable-key based
atomic/safe persistence
unknown device policy handled safely
invalid value falls back to Allow
```

Avoid writing on every model refresh.

Write only when the user changes policy.

---

# 12. BLUETOOTH DEVICE MODEL

Expose button policy through the device data/model.

For example add roles:

```text
ButtonPolicyRole
ButtonsAllowedRole
CanControlButtonPolicyRole
ButtonControlStatusRole
```

Recommended user-facing status values:

```text
Allowed
Blocked
Unavailable
Not detected
Permission required
```

Do not represent backend errors as silently “Blocked.”

---

# 13. BLUETOOTHMANAGER API

Add user-facing invokable methods such as:

```cpp
Q_INVOKABLE bool setDeviceButtonsAllowed(const QString& devicePath, bool allowed);
Q_INVOKABLE bool deviceButtonsAllowed(const QString& devicePath) const;
```

or policy-based equivalents.

Because object paths are transient, `BluetoothManager` should:

```text
resolve devicePath → stable device identity
persist policy by stable identity
tell button-control service about policy change
update model
```

---

# 14. NEW BUTTON CONTROL SERVICE

Do not bury input-device logic inside QML or `BluetoothManager`.

Create a focused subsystem such as:

```text
BluetoothButtonControlManager
DeviceButtonPolicyManager
BluetoothInputGuard
```

Recommended responsibilities:

```text
track stable Bluetooth device policies
discover/match OS input-control endpoints
apply/release suppression
observe hotplug/removal
report runtime state/errors
recover after service/device churn
```

Example interface:

```cpp
class BluetoothButtonControlManager : public QObject {
    Q_OBJECT
public:
    void initialize();
    void shutdown();

    void setPolicy(const StableBluetoothDeviceId&, DeviceButtonPolicy);
    DeviceButtonPolicy policy(const StableBluetoothDeviceId&) const;

    void notifyBluetoothDeviceAvailable(...);
    void notifyBluetoothDeviceUnavailable(...);

signals:
    void effectiveStateChanged(...);
    void suppressionError(...);
};
```

Use actual project naming conventions.

---

# 15. INPUT ENDPOINT DISCOVERY

If using evdev:

Do not assume `/dev/input/event17` remains constant.

Discover by stable device metadata.

Use udev/sysfs information to correlate input endpoint with the Bluetooth device.

Potential properties to inspect:

```text
ID_BUS=bluetooth
PRODUCT
NAME
PHYS
UNIQ
ID_PATH
parent sysfs device
Bluetooth address encoded in phys/uniq/parent path
```

Use `libudev` if already available or add a narrowly scoped dependency only if justified.

Alternatively use `/sys/class/input` safely.

Do not scrape `evtest` output in production.

---

# 16. INPUT ENDPOINT HOTPLUG

Bluetooth input endpoints can appear/disappear when:

```text
device connects
device disconnects
BlueZ restarts
system bus restarts
profile changes
laptop resumes
```

The button-control service must handle hotplug.

Required behavior:

```text
policy = Disallow
device connects
→ matching input endpoint appears
→ suppression automatically applied
```

```text
device disconnects
→ endpoint disappears
→ manager releases file descriptor/grab safely
→ policy remains persisted
```

```text
device reconnects with new /dev/input/eventN
→ suppression automatically re-applied
```

---

# 17. EVDEV GRAB SAFETY

If using:

```cpp
ioctl(fd, EVIOCGRAB, 1)
```

then:

- open only the exact matched input device;
- keep fd lifetime RAII-managed;
- release grab on policy ALLOW;
- release on disconnect;
- release on shutdown;
- close fd on failure;
- never leak a grab across process teardown;
- log device identity and effective state;
- do not busy-poll.

Use `QSocketNotifier` or a dedicated minimal event reader only if events need to be consumed while grabbed.

Understand that grabbing means the kernel routes events only to Auralis.

---

# 18. SELECTIVE FILTERING IF REQUIRED

If the physical input node includes both media and non-media events and V1 must allow some buttons:

Recognize Linux input codes such as:

```text
KEY_PLAYPAUSE
KEY_PLAY
KEY_PAUSE
KEY_NEXTSONG
KEY_PREVIOUSSONG
KEY_STOPCD
KEY_FASTFORWARD
KEY_REWIND
KEY_VOLUMEUP
KEY_VOLUMEDOWN
KEY_MUTE
```

Define a policy set.

For the user's core requirement, Play/Pause must be suppressible.

Potential future modes:

```text
AllowAll
BlockTransport
BlockAllMediaButtons
```

But do not overcomplicate V1 unless needed.

---

# 19. DO NOT SPAM PLAY/PAUSE WHEN BLOCKING

When DISALLOW:

```text
KEY_PLAYPAUSE press
KEY_PLAYPAUSE release
repeat
```

must be consumed.

If the hardware emits a rapid repeating stream:

```text
value = 2 repeat events
```

those must also be consumed.

Do not simply debounce one event and let later repeats through.

---

# 20. DEBOUNCE IS NOT THE PRIMARY FEATURE

A debounce/rate limiter alone is not equivalent to DISALLOW.

The user wants explicit:

```text
ALLOW / DISALLOW
```

A debounce may be added as defensive protection, but:

```text
DISALLOW = zero media-control actions reach the system
```

for blocked buttons.

---

# 21. UI — DEVICE LIST / DEVICE DETAILS

Add an obvious control for each relevant Bluetooth device.

Recommended location:

```text
DevicesPage → selected device → Controls section
```

or directly on each `DeviceRow` if layout remains clean.

Example:

```text
Hardware buttons
[ Allow ] [ Disallow ]
```

or:

```text
☑ Allow device buttons
```

Prefer wording that is unmistakable.

When unchecked:

```text
Device buttons are blocked by Auralis
```

Add concise helper text:

```text
Block this device's media buttons to prevent accidental Play/Pause commands.
```

---

# 22. UI STATES

The UI must distinguish policy from effective backend state.

Example:

```text
Policy: Disallow
Status: Blocked
```

or:

```text
Policy: Disallow
Status: Waiting for device input endpoint
```

or:

```text
Policy: Disallow
Status: Permission required
```

Do not show “Blocked” when suppression failed.

---

# 23. PERMISSIONS

If evdev access is required, handle Linux permissions explicitly.

Do not automatically run Auralis as root.

Do not ask users to launch the entire GUI with `sudo`.

Preferred approaches:

```text
udev rule granting narrowly scoped read/grab access
system group such as input (only if acceptable)
small privileged helper with least privilege
```

Choose the narrowest safe architecture.

If a udev rule is required:

- package it explicitly;
- document it;
- validate package install/uninstall;
- avoid granting access to all arbitrary input devices if possible.

---

# 24. SECURITY / PRIVACY BOUNDARY

Auralis should not become a general keyboard logger.

If direct input access is required:

- open only Bluetooth media-control endpoints matched to known Auralis devices;
- ignore keyboard/alphanumeric devices;
- do not store raw key-event history;
- log only high-level blocked/allowed event type where necessary;
- never persist arbitrary input data.

Add device-class validation to prevent accidental grabs of the user's keyboard/mouse.

---

# 25. MATCHING SAFETY

Before applying a grab/filter, require high-confidence matching between:

```text
BluetoothDeviceData
```

and:

```text
Linux input endpoint
```

Use multiple signals where possible:

```text
ID_BUS=bluetooth
Bluetooth address / unique ID
sysfs ancestry
device name
consumer-control capability
```

If matching is ambiguous:

```text
do not grab
report "Unable to identify button endpoint"
```

Fail open rather than risk disabling an unrelated keyboard.

---

# 26. RECOVERYMANAGER INTEGRATION

Button suppression must survive Phase 8 recovery events.

Integrate with existing lifecycle signals.

### System D-Bus / BlueZ restart

Persisted policy remains.

After device/input endpoint returns:

```text
Disallow policy automatically reapplies
```

### Suspend/resume

Before suspend:

```text
no special action required if fd remains valid
```

but handle event node disappearance.

After resume:

```text
re-scan/reconcile endpoints
reapply suppression
```

Do not block session/audio recovery waiting on button-control backend.

This is an auxiliary control feature.

---

# 27. SHUTDOWN

On application shutdown:

```text
release every exclusive input grab
close descriptors
destroy notifier/event-reader state
stop hotplug monitor
```

No stale helper process should remain.

If a helper architecture is used, ensure clean IPC shutdown.

---

# 28. BUTTON POLICY VS DEVICE FORGET

If a device is forgotten from BlueZ:

Decide whether to retain its preference.

Recommended:

```text
forget device
→ delete persisted Auralis button policy for that stable Bluetooth identity
```

because the user explicitly removed the device.

Test this.

---

# 29. DEVICE RECONNECT

Policy must survive normal disconnect.

Example:

```text
Device X:
Buttons = Disallow

disconnect
reconnect
new BlueZ object path
new /dev/input/eventN

Result:
Buttons still Disallow
suppression automatically re-established
```

This is mandatory.

---

# 30. MULTIPLE DEVICES

Per-device behavior must be independent.

Example:

```text
Headset A → Buttons DISALLOW
Headset B → Buttons ALLOW
```

Then:

```text
A Play/Pause → blocked
B Play/Pause → reaches system normally
```

No global kill switch unless separately added.

This is the key multi-device test.

---

# 31. TWO DEVICES WITH SAME DISPLAY NAME

Never identify policy only by device name.

Example:

```text
"Galaxy Buds"
"Galaxy Buds"
```

must remain distinct using stable Bluetooth identity.

---

# 32. QML MODEL ROLES

If implemented in `BluetoothDeviceListModel`, add roles with proper change notifications.

Possible roles:

```text
ButtonsAllowedRole
ButtonPolicyRole
ButtonControlEffectiveRole
ButtonControlStatusTextRole
```

Update only affected rows.

Do not reset the entire model when one toggle changes.

---

# 33. DETAILS API

`BluetoothManager::deviceDetails()` should include:

```text
buttonsAllowed
buttonPolicy
buttonControlEffective
buttonControlStatus
```

where appropriate.

This allows the right-side device detail panel to stay synchronized.

---

# 34. USER-FACING ERRORS

Possible errors:

```text
Input endpoint not found
Permission denied
Endpoint ambiguous
Grab failed
Backend unavailable
Device disconnected
```

Represent these non-fatally.

Do not mark the Bluetooth device disconnected because button suppression failed.

---

# 35. LOGGING

Add structured logs for:

```text
button policy changed
stable device identity
input endpoint matched
suppression applied
suppression released
blocked media key type
permission error
ambiguous input endpoint
endpoint hotplug/replacement
recovery reapply
```

Avoid logging every repeat event at INFO level.

Use debug/trace or rate-limited logging for repeated blocked events.

---

# 36. METRICS / DIAGNOSTICS

If Auralis diagnostics has a device panel, add:

```text
Button policy
Effective suppression state
Matched input endpoint
Last suppression error
```

Do not expose sensitive arbitrary input data.

---

# 37. UNIT TESTS — POLICY STORAGE

Add tests:

```text
defaultPolicyIsAllow
setPolicyToDisallow
policyPersistsAcrossManagerRecreation
invalidStoredPolicyFallsBackToAllow
differentDevicesHaveIndependentPolicy
deviceObjectPathChangeKeepsPolicy
forgetDeviceRemovesPolicy
```

---

# 38. UNIT TESTS — DEVICE MATCHING

Using fake udev/input metadata:

```text
matchesBluetoothConsumerControlEndpoint
rejectsNonBluetoothKeyboard
rejectsAmbiguousMatch
matchesSameDeviceAfterEventNodeChanges
sameNameDevicesRemainDistinct
```

---

# 39. UNIT TESTS — SUPPRESSION

With a fake input backend:

```text
allowDoesNotGrabDevice
disallowGrabsMatchedMediaEndpoint
changingDisallowToAllowReleasesGrab
disconnectReleasesGrab
reconnectReappliesGrab
shutdownReleasesAllGrabs
permissionFailureReportsErrorWithoutBreakingBluetooth
```

---

# 40. EVENT FILTER TESTS

If selective filtering is used:

```text
disallowDropsPlayPausePress
disallowDropsPlayPauseRelease
disallowDropsPlayPauseRepeat
disallowDropsNextPreviousIfPolicyRequires
allowForwardsNormally
blockedDeviceDoesNotAffectAllowedDevice
```

If entire consumer-control node is grabbed and swallowed, test equivalent semantics.

---

# 41. INTEGRATION TEST — TWO DEVICES

Create fake devices:

```text
A = AA:AA:AA:AA:AA:AA
B = BB:BB:BB:BB:BB:BB
```

Policies:

```text
A = Disallow
B = Allow
```

Inject:

```text
A KEY_PLAYPAUSE
B KEY_PLAYPAUSE
```

Assert:

```text
A event suppressed
B event not suppressed
```

Then reconnect both with changed BlueZ object paths/input nodes.

Policies/effective states must remain correct.

---

# 42. INTEGRATION TEST — BUTTON SPAM

Inject 100+ rapid Play/Pause events from a DISALLOW device:

```text
press
repeat
repeat
release
...
```

Assert:

```text
zero media-control actions escape
zero unbounded queue growth
no CPU busy loop
no log flood at INFO/WARN
```

Then switch ALLOW and prove subsequent events are no longer blocked.

---

# 43. INTEGRATION TEST — AUDIO UNAFFECTED

While policy is DISALLOW:

```text
device stays connected
PipeWire endpoint remains available
route stays active
session stays active
audio status remains Available
```

Button suppression must not interfere with audio transport.

---

# 44. INTEGRATION TEST — RECOVERY

Simulate:

```text
Disallow policy active
BlueZ disappears
device/input endpoint removed
BlueZ returns
device returns
input endpoint returns
```

Assert:

```text
policy still Disallow
suppression re-established automatically
audio recovery remains independent
```

Also simulate suspend/resume.

---

# 45. ACTUAL-MACHINE VALIDATION

After software tests pass, validate with at least two physical Bluetooth devices if available.

For each:

1. pair/connect;
2. identify button event path;
3. set Buttons = ALLOW;
4. press Play/Pause 10 times;
5. verify normal media behavior;
6. set Buttons = DISALLOW;
7. press Play/Pause repeatedly/hold button if applicable;
8. verify zero Play/Pause reaches desktop;
9. verify audio continues;
10. disconnect/reconnect;
11. verify policy remains DISALLOW;
12. repeat button test;
13. restart Auralis;
14. verify persisted policy;
15. restart BlueZ if safe;
16. verify suppression returns after recovery.

For two devices simultaneously:

```text
A = DISALLOW
B = ALLOW
```

verify per-device independence.

---

# 46. ACCIDENTAL-HOLD TEST

Some devices repeat while a button is held.

Test:

```text
press-and-hold Play/Pause
```

for at least several seconds.

DISALLOW must block:

```text
initial press
repeat events
release
```

and must not produce multiple delayed Play/Pause actions after release.

---

# 47. CRASH/FAILURE SAFETY

If Auralis crashes while an evdev grab is active, the kernel should release the grab when fd/process dies.

Still test orderly shutdown.

If using a separate helper:

- helper must detect client death;
- release grabs;
- exit or return to safe state;
- never leave input permanently blocked.

---

# 48. PACKAGE REQUIREMENTS

If new runtime pieces are needed:

```text
udev rules
helper executable
systemd user service
polkit policy
```

package them explicitly.

Do not require undocumented manual copying.

Validate `.deb` contents.

If no privileged helper is needed, keep packaging minimal.

---

# 49. CMAKE

Add dependencies narrowly.

Potential Linux headers/libraries:

```text
linux/input.h
libudev
```

Do not add a large input framework if unnecessary.

Feature-gate platform-specific code if useful.

Auralis is currently Linux-focused, so a Linux implementation is acceptable, but keep the subsystem boundary clean.

---

# 50. UI ACCEPTANCE CRITERIA

For a selected Bluetooth device, the user must be able to see:

```text
Hardware buttons
ALLOW / DISALLOW
```

with current persisted state.

Changing it must take effect immediately where possible.

If the device is disconnected:

```text
policy remains editable
```

and is applied when it reconnects.

If backend suppression cannot currently be applied:

```text
policy stays Disallow
status explains why it is not effective
```

---

# 51. BEHAVIOR ACCEPTANCE CRITERIA

## ALLOW

- [ ] no Auralis suppression;
- [ ] normal device media keys function;
- [ ] no unnecessary input grab;
- [ ] policy persists.

## DISALLOW

- [ ] Play/Pause from that device is suppressed;
- [ ] repeated Play/Pause spam is suppressed;
- [ ] other devices are unaffected;
- [ ] Bluetooth audio remains functional;
- [ ] reconnect restores suppression;
- [ ] app restart restores policy;
- [ ] BlueZ recovery restores suppression;
- [ ] suspend/resume restores suppression if endpoint changes.

---

# 52. SAFETY ACCEPTANCE CRITERIA

- [ ] no global keyboard grab;
- [ ] no accidental mouse/keyboard suppression;
- [ ] fail-open on ambiguous device matching;
- [ ] no root GUI;
- [ ] no raw key logging;
- [ ] every input grab released on shutdown;
- [ ] permissions documented;
- [ ] package includes required rules/helper if needed.

---

# 53. REQUIRED DOCUMENTATION

Create a technical document such as:

```text
docs/BLUETOOTH_BUTTON_CONTROL.md
```

Include:

```text
feature semantics
Linux event path chosen
why that backend was selected
device matching strategy
permission model
persistence key
recovery behavior
known hardware variations
troubleshooting
actual-machine test commands
```

Also update README feature list only after implementation works.

---

# 54. DIAGNOSTIC COMMAND DOCUMENTATION

Document commands the developer can run to diagnose a new headset:

```text
bluetoothctl info
/proc/bus/input/devices
/dev/input/by-id
/dev/input/by-path
udevadm info
evtest/libinput if installed
busctl org.bluez inspection
```

Do not require these commands for normal end users.

---

# 55. DO NOT IMPLEMENT THESE INCORRECT SHORTCUTS

Do not:

- set `Device1.Blocked=true`;
- disconnect the headset to stop its buttons;
- mute audio;
- globally disable all media keys;
- intercept only Auralis's own Play/Pause action while other players still receive it;
- identify devices by display name alone;
- persist `/dev/input/eventN`;
- persist only BlueZ object path;
- run the whole app as root;
- use a debounce as a substitute for DISALLOW;
- claim success from a fake-only test without real-machine verification.

---

# 56. IMPLEMENTATION ORDER

Recommended:

```text
1. inspect actual event path
2. document chosen backend
3. stable per-device policy model/persistence
4. button-control subsystem
5. endpoint matching/hotplug
6. suppression backend
7. BluetoothManager/model integration
8. QML control
9. unit tests
10. multi-device integration tests
11. spam/recovery tests
12. actual-machine validation
13. packaging/permissions
14. documentation
```

---

# 57. REQUIRED FINAL TEST COMMANDS

Run normal suite:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Run targeted:

```bash
ctest --test-dir build -R Button --output-on-failure -V
ctest --test-dir build -R Bluetooth --output-on-failure -V
ctest --test-dir build -R Device --output-on-failure -V
ctest --test-dir build -R Recovery --output-on-failure -V
ctest --test-dir build -R Session --output-on-failure -V
```

If sanitizer support exists:

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAURALIS_ENABLE_SANITIZERS=ON

cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

---

# 58. REQUIRED FINAL RESPONSE FROM CURSOR

Return:

## Event-path discovery

For each physical device tested:

```text
device
Bluetooth address
actual Linux event path
event node / BlueZ interface
Play/Pause event code
```

## Architecture selected

Explain:

```text
BlueZ-level
evdev-grab
selective uinput filtering
```

and why.

## Files changed

Group:

```text
Bluetooth
button-control backend
device persistence
models/API
QML
tests
packaging
docs
```

## Behavior proof

Report:

```text
Device A policy
Device B policy
Play/Pause events injected
blocked count
allowed count
audio state during test
reconnect result
restart persistence result
```

## Permissions

State exactly what access is required.

## Regression

Provide complete CTest totals and sanitizer result.

## Hardware result

Clearly distinguish:

```text
software tests PASS
actual machine PASS
actual physical device PASS
not run
```

---

# 59. FINAL ACCEPTANCE STATE

Do not call the feature complete until:

```text
A Bluetooth device can be set to DISALLOW,
rapid Play/Pause button presses from that exact device produce zero system media-control actions,
another device set to ALLOW continues to work normally,
audio routing remains unaffected,
and the policy survives reconnect/restart.
```

That is the definition of success.
