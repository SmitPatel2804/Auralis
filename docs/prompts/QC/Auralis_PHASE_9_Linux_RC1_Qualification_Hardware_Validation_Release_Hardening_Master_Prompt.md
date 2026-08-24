# AURALIS — PHASE 9 LINUX RC1 QUALIFICATION, HARDWARE VALIDATION & RELEASE HARDENING MASTER PROMPT

**Project:** Auralis  
**Target:** Ubuntu Linux reference machine / real Bluetooth hardware  
**Phase:** 9  
**Primary goal:** Convert the now-working Phase 0–8 Linux implementation into a rigorously qualified **internal Linux RC1 candidate** without destabilizing the working architecture.  
**Execution environment:** Run this prompt inside Cursor / the AI IDE on the **actual Linux machine** where Auralis has already been observed to work correctly with real Bluetooth audio devices.  
**Repository root:** The Auralis repository currently containing `CMakeLists.txt`, `src/`, `include/`, `ui/`, `tests/`, `docs/`, `scripts/`, and `apps/desktop/`.

---

# 0. EXECUTIVE DIRECTIVE

You are acting as the senior C++/Qt/Linux Bluetooth/PipeWire engineer responsible for **Phase 9 qualification and release hardening** of Auralis.

The application has already crossed the most important feasibility boundary: it has been built and exercised on the real Linux machine and the project owner reports that the current build runs correctly. The repository snapshot already includes extensive Phase 8 recovery, packaging, stress-test infrastructure, multi-device routing fixes, and per-device Bluetooth media-button ALLOW/DISALLOW support.

Your job is **not** to rewrite the application and **not** to reimplement Phase 8.

Your job is to:

1. preserve the known-good implementation;
2. establish a reproducible baseline from the exact current checkout;
3. independently verify all Phase 0–8 behavior that matters for an actual Linux release candidate;
4. formally close the remaining Phase 8 live/hardware evidence gap using the actual machine;
5. validate the already-implemented Bluetooth device-button policy on real hardware;
6. validate two-device fan-out, recovery, persistence, virtual system output, and long-running stability;
7. identify and fix only defects that are reproduced or strongly evidenced;
8. strengthen tests/diagnostics/scripts only where they materially improve release confidence;
9. validate the Linux package without silently depending on the build tree;
10. produce a rigorous Phase 9 qualification audit;
11. declare an internal Linux RC1 technical PASS only if the evidence actually satisfies every mandatory gate.

Do **not** claim a public release if owner-controlled package metadata remains unresolved.

---

# 1. AUTHORITATIVE CURRENT BASELINE — DO NOT IGNORE

Before changing anything, treat the following as known repository facts from the supplied current snapshot. Reconfirm them from the repository instead of blindly trusting this list.

## 1.1 Current project / build facts

The top-level CMake project currently declares:

```text
Project: Auralis
Version: 0.1.0
Language: C++20
Qt minimum: 6.8
Linux Bluetooth backend: BlueZ over D-Bus
Linux audio backend: PipeWire
Desktop UI: Qt Quick / QML
```

The Linux reference environment documented by the project includes approximately:

```text
Ubuntu 26.04 LTS
Kernel 7.0.0-29-generic
GCC/G++ 15.2.0
CMake 4.2.3
Ninja 1.13.2
Qt 6.10.2
BlueZ 5.85
PipeWire 1.6.2
```

Do not hard-code those versions as universal minimums. Record the actual machine values at Phase 9 runtime.

## 1.2 Current phase status from repository documentation

The current README reports Phases 0–7 as implemented and Phase 8 as software-complete, while older Phase 8 audit text still contains a live/hardware evidence gap.

The existing Phase 8 audit currently records approximately:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PENDING
PHASE 8 OVERALL: PENDING HARDWARE VALIDATION
```

The project owner has now personally tested the application on the actual Linux machine and reports that it runs correctly.

**Important:** that report is strong contextual evidence, but this Phase 9 task must still capture reproducible technical evidence before changing the repository's formal hardware gate from PENDING to PASS.

## 1.3 Existing Phase 8 recovery/hardening implementation

The repository already contains, among other things:

- `RecoveryManager`;
- `SystemPowerMonitor`;
- service retry/backoff behavior;
- BlueZ/system-bus recovery;
- PipeWire reconnect and initial graph-sync timeout handling;
- suspend/resume coordination;
- persistent recovery preferences;
- log rotation/retention;
- diagnostics integration;
- software recovery integration tests;
- Phase 8 stress tests;
- sanitizer build option;
- CPack Linux packaging;
- `.desktop` and AppStream metadata;
- Linux virtual audio output support;
- multi-device shared-source fan-out correction.

Do not create a second recovery engine.

Do not introduce a competing Bluetooth reconnect policy.

Do not introduce a second route/session owner.

## 1.4 Existing per-device Bluetooth button policy

The current repository already contains:

```text
include/auralis/bluetooth/DeviceButtonPolicy.h
include/auralis/bluetooth/BluetoothButtonControlManager.h
src/bluetooth/BluetoothButtonControlManager.cpp
tests/unit/bluetooth/tst_BluetoothButtonControlManager.cpp
docs/BLUETOOTH_BUTTON_EVENT_PATH.md
```

`BluetoothManager` and `BluetoothDeviceListModel` already expose button-policy behavior to QML, and `DevicesPage.qml` already invokes the policy.

The implementation is based on Linux evdev endpoint correlation and an exclusive `EVIOCGRAB` where a Bluetooth-correlated consumer-control input endpoint exists.

Existing semantics that must be preserved:

- default policy is **ALLOW**;
- policy is per Bluetooth device/address;
- DISALLOW must never disconnect A2DP;
- DISALLOW must never mute the device as a substitute for button suppression;
- DISALLOW must never destroy an active Auralis audio route;
- when a controllable input node cannot be safely correlated, the UI must report an honest unsupported state rather than pretending suppression succeeded;
- normal runtime must not require running Auralis as root;
- a permission failure must be represented as a permission/capability problem, not as successful suppression.

**Do not reimplement this feature from scratch. Validate it on the real machine and fix only evidence-backed issues.**

## 1.5 Existing relevant tests

The repository already contains hardware-independent and opt-in live tests such as:

```text
tst_BlueZLiveIntegration
tst_PipeWireLiveIntegration
tst_AudioRoutingLiveIntegration
tst_SessionLiveIntegration
tst_Phase7HardwareLive
tst_SessionManagedReconnectIntegration
tst_ServiceRecoveryIntegration
tst_Phase8RecoveryHarness
tst_Phase8Stress
tst_DesktopBackendSmoke
tst_QmlComponents
```

The repository also contains broad unit suites under:

```text
tests/unit/audio/
tests/unit/bluetooth/
tests/unit/core/
tests/unit/devices/
tests/unit/recovery/
tests/unit/session/
```

Do not duplicate an existing test merely to produce a new Phase 9 filename. Reuse and extend existing tests where that gives better coverage.

## 1.6 Existing packaging truthfulness constraint

The current repository contains a `LICENSE` file whose content explicitly states that licensing terms have **not yet been selected** and that the software must not be publicly distributed until the project owner approves a license.

Current package metadata also uses a truthful placeholder contact similar to:

```text
Auralis (contact pending)
```

Therefore:

> **Phase 9 may create an internal Linux RC1 technical candidate, but it must not declare public redistribution readiness unless the project owner has explicitly supplied and approved the missing license/contact metadata.**

Do not invent a license.

Do not invent a maintainer email.

Do not silently change the project to MIT/GPL/Apache/proprietary terms.

---

# 2. PHASE 9 PRIMARY OUTCOME

Phase 9 is successful only when the actual Linux machine demonstrates that the current architecture is not merely functional once, but **repeatably stable and release-candidate worthy** for the currently validated two-device workflow.

At minimum, Phase 9 must prove:

```text
clean build
  + full hardware-independent regression
  + two real Bluetooth devices simultaneously connected
  + both represented as usable audio endpoints
  + one source routed to both simultaneously
  + system-audio virtual output path works
  + per-device button ALLOW/DISALLOW behaves correctly when supported
  + disconnect/reconnect of one member does not unnecessarily tear down the other
  + recovery after supported service/graph disruptions works
  + persistence survives application restart
  + suspend/resume behavior is sane
  + no obvious resource/lifetime regression under churn
  + package can be built and launched independently of the source/build tree
  + no mandatory test is silently skipped
  + evidence is written to a durable audit
```

Only then may the implementation be called an **internal Linux RC1 technical candidate**.

---

# 3. PHASE 9 NON-NEGOTIABLE BOUNDARY

## 3.1 In scope

Phase 9 includes:

- current-checkout baseline capture;
- full clean rebuild;
- CTest regression;
- extended Phase 8 stress execution;
- ASan/UBSan pass where supported;
- actual BlueZ hardware validation;
- actual PipeWire validation;
- actual two-device fan-out validation;
- Linux virtual system-output validation;
- real Bluetooth button-policy validation;
- session persistence/restart validation;
- controlled recovery tests;
- controlled device churn;
- suspend/resume validation;
- shutdown/restart validation;
- package build/inspection/staged execution;
- resource-growth observation;
- diagnostics/log-quality review;
- narrowly scoped bug fixes discovered during validation;
- regression tests for every code defect fixed;
- creation of Phase 9 validation documentation and audit evidence;
- optional 3/4-device observational preflight if the hardware is physically available.

## 3.2 Out of scope unless a Phase 9 blocker proves it is absolutely required

Do **not** use Phase 9 to implement:

- a brand-new synchronization engine;
- automatic acoustic latency measurement;
- major per-device delay compensation features;
- Auracast;
- LE Audio architecture replacement;
- new custom transmitter hardware;
- a kernel audio driver;
- a different GUI framework;
- a full UI redesign;
- an unrelated database layer;
- cloud services;
- telemetry upload;
- automatic update infrastructure;
- Windows architecture rewrite;
- macOS architecture rewrite;
- a new Bluetooth transport abstraction just for style;
- speculative refactors unrelated to reproduced release blockers.

Phase 9 is about **qualification and hardening**, not feature expansion.

## 3.3 Do not destabilize known-good behavior

Every modification must preserve the established contracts from Phases 0–8.

A change is unacceptable if it fixes a rare test scenario but breaks:

- discovery;
- pair/trust/connect/disconnect;
- endpoint enumeration;
- routing;
- multi-device fan-out;
- session activation;
- virtual output;
- recovery;
- QML startup;
- package startup;
- device-button policy;
- existing platform abstractions.

---

# 4. SAFETY AND OPERATOR-CONTROL RULES

This phase intentionally includes tests that can disturb Bluetooth/audio state. Be disciplined.

## 4.1 Never silently use `sudo`

Do not automatically execute privileged commands.

If a validation scenario requires administrative privileges, stop that specific scenario and print the exact command for the operator to approve/run.

Examples include some BlueZ daemon restart or system D-Bus operations.

## 4.2 Never restart the whole machine automatically

Reboot and suspend/resume tests require operator awareness.

Prepare the test, persist the expected next state/evidence path, and clearly tell the operator what action is required.

Do not initiate an unexpected reboot.

## 4.3 Destructive Bluetooth operations require opt-in

The existing live test infrastructure already distinguishes destructive operations such as forgetting devices.

Do not forget/unpair devices merely to increase test coverage unless the operator explicitly enables the destructive path.

Pair/trust relationships are valuable test state.

## 4.4 Do not expose hardware identifiers in committed documentation unnecessarily

Raw local audit logs may contain Bluetooth addresses.

For durable committed Markdown:

- prefer aliases such as `Device A` / `Device B`;
- redact full MAC addresses unless technically necessary;
- if an address is needed to prove correlation, mask part of it;
- keep machine-specific raw dumps in a gitignored audit directory.

## 4.5 No fake PASS

A test that was skipped is **SKIPPED**, not PASS.

A test that could not be run due to missing hardware is **NOT EXERCISED**.

A scenario that requires operator action and was not performed is **PENDING**.

A scenario is **PASS** only when actual evidence supports it.

---

# 5. FIRST ACTION — REPOSITORY RECONNAISSANCE

Do not edit code immediately.

From the repository root, inspect the exact current checkout.

Capture at minimum:

```bash
pwd

git status --short
git branch --show-current
git rev-parse HEAD
git log -1 --oneline

git diff --stat
git diff --cached --stat

cmake --version | head -1
ninja --version
g++ --version | head -1
qmake6 --version || true
bluetoothctl --version || true
pkg-config --modversion libpipewire-0.3 || true
pkg-config --modversion dbus-1 || true
uname -a
cat /etc/os-release
```

Also inspect:

```text
README.md
CMakeLists.txt
cmake/AuralisOptions.cmake
cmake/AuralisPackaging.cmake
apps/desktop/
src/bluetooth/
include/auralis/bluetooth/
src/audio/
include/auralis/audio/
src/session/
include/auralis/session/
src/recovery/
include/auralis/recovery/
src/core/
include/auralis/core/
ui/qml/
tests/CMakeLists.txt
tests/integration/CMakeLists.txt
tests/integration/
tests/unit/
scripts/validation/
docs/PHASE_8_IMPLEMENTATION_AUDIT.md
docs/PHASE_8_RIGOROUS_VALIDATION_AUDIT.md
docs/BLUETOOTH_BUTTON_EVENT_PATH.md
docs/architecture/LINUX_VIRTUAL_AUDIO_OUTPUT.md
docs/roadmap/
docs/specification/
```

Search specifically for current implementations of:

```text
BluetoothButtonControlManager
EVIOCGRAB
DeviceButtonPolicy
setDeviceButtonPolicy
RecoveryManager
SystemPowerMonitor
ReconnectPolicy
PipeWireManager
AudioRouter
RoutingCoordinator
SessionManager
SessionPersistence
VirtualAudioDevice
Auralis System Audio
CPack
AURALIS_RUN_BLUETOOTH_INTEGRATION
AURALIS_EXPECT_DEVICE_ADDRESSES
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION
AURALIS_RUN_PHASE7_HARDWARE
AURALIS_RUN_STRESS
```

Build a short internal architecture map before touching code.

---

# 6. CREATE A PHASE 9 EVIDENCE DIRECTORY

Create a machine-local, gitignored evidence directory such as:

```text
audit-phase9-linux-rc/
```

It should contain raw evidence generated on the current machine, for example:

```text
audit-phase9-linux-rc/
├── baseline/
├── builds/
├── ctest/
├── sanitizer/
├── bluetooth/
├── pipewire/
├── routing/
├── buttons/
├── recovery/
├── suspend-resume/
├── soak/
├── package/
├── diagnostics/
└── summary/
```

Do not commit large raw logs unless the repository explicitly expects them.

The durable result must be summarized in a committed Markdown audit document later.

---

# 7. FREEZE THE KNOWN-GOOD BASELINE WITHOUT DAMAGING GIT HISTORY

The current checkout is valuable because the owner has already observed it working on real hardware.

Record:

```text
current branch
current HEAD
working-tree cleanliness
submodule state if any
CMake/toolchain versions
Linux/BlueZ/PipeWire versions
Qt version
Bluetooth controller identity/model where available
```

Write this baseline into the Phase 9 audit evidence.

If the working tree is dirty, do **not** erase or reset the changes.

Do not run:

```text
git reset --hard
git clean -fdx
git checkout -- .
```

unless the project owner has explicitly authorized losing uncommitted work.

A local tag/branch can be recommended for the owner, but do not push anything to a remote and do not rewrite history.

Suggested human-approved label:

```text
auralis-linux-known-good-pre-phase9
```

This is a safety checkpoint, not a release version claim.

---

# 8. BASELINE BUILD — NO HARDWARE DEPENDENCE

Use a fresh build directory so an old build cannot hide missing dependencies or stale generated files.

Prefer something similar to:

```bash
rm -rf build-phase9-baseline
cmake -S . -B build-phase9-baseline -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-phase9-baseline
ctest --test-dir build-phase9-baseline --output-on-failure
```

If repository presets are the authoritative path, use the appropriate existing Linux preset and document the exact commands.

Mandatory rules:

- full configure must succeed;
- full build must succeed;
- no warnings-as-errors workaround should be added to hide defects;
- default CTest must complete without requiring actual Bluetooth hardware;
- record total tests, passed, failed, skipped, and duration;
- a default-suite failure blocks Phase 9 immediately until understood.

Do not proceed as if the baseline is healthy if the clean default suite fails.

---

# 9. EXTENDED SOFTWARE STRESS BASELINE

Run the existing Phase 8 stress label in normal and extended modes.

At minimum inspect the current stress-test contract before invoking it.

Expected repository behavior includes `AURALIS_RUN_STRESS=1` for extended stress.

Run something equivalent to:

```bash
ctest --test-dir build-phase9-baseline -L stress --output-on-failure

AURALIS_RUN_STRESS=1 \
ctest --test-dir build-phase9-baseline -L stress --output-on-failure
```

If `AURALIS_STRESS_ITERATIONS` exists and is suitable, add one larger bounded run and record the count.

Do not invent an extreme iteration count that makes the machine unusable.

Required evidence:

- stress test count;
- iteration count;
- elapsed time;
- failures;
- crashes;
- assertions;
- log storms;
- abnormal resource growth if observed.

---

# 10. SANITIZER QUALIFICATION

Create a separate sanitizer build.

Use the repository-supported option:

```bash
rm -rf build-phase9-asan
cmake -S . -B build-phase9-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAURALIS_ENABLE_SANITIZERS=ON
cmake --build build-phase9-asan
```

Then run the hardware-independent test suite.

If the current environment still requires the known leak-detector caveat, document it precisely instead of hiding it. For example, if `detect_leaks=0` is still necessary due to environment/toolchain behavior:

```bash
ASAN_OPTIONS=detect_leaks=0 \
ctest --test-dir build-phase9-asan --output-on-failure
```

Do not state "ASan fully clean" if leak detection was disabled.

Report separately:

```text
AddressSanitizer findings
UndefinedBehaviorSanitizer findings
LeakSanitizer exercised? yes/no
```

Any reproducible ASan/UBSan defect in Auralis code is a Phase 9 blocker.

---

# 11. LIVE MACHINE PRE-FLIGHT

Before connecting/disconnecting anything, capture the host state.

Useful commands include:

```bash
bluetoothctl list
bluetoothctl show
bluetoothctl devices
bluetoothctl devices Connected

systemctl status bluetooth --no-pager || true
systemctl --user status pipewire --no-pager || true
systemctl --user status pipewire-pulse --no-pager || true
systemctl --user status wireplumber --no-pager || true

wpctl status || true
pw-cli info 0 || true
pactl info || true
```

Capture:

- controller powered state;
- BlueZ service availability;
- PipeWire availability;
- WirePlumber availability;
- currently connected Bluetooth devices;
- current sinks/sources;
- whether `Auralis System Audio` already exists;
- whether the application is currently running.

Do not treat a degraded pre-flight as an application failure until the host dependency state is understood.

---

# 12. ACTUAL BLUEZ LIVE INTEGRATION

Use the existing `tst_BlueZLiveIntegration` rather than creating a duplicate Phase 9 BlueZ probe unless the existing test is missing a clearly necessary assertion.

Inspect its environment contract first.

The current repository supports variables such as:

```text
AURALIS_RUN_BLUETOOTH_INTEGRATION=1
AURALIS_EXPECT_DEVICE_ADDRESS
AURALIS_EXPECT_DEVICE_ADDRESSES
AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS
```

For a two-device non-destructive run, use the actual device addresses from the operator environment, for example conceptually:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESSES="<DEVICE_A_MAC>,<DEVICE_B_MAC>" \
ctest --test-dir build-phase9-baseline \
  -R '^tst_BlueZLiveIntegration$' \
  --output-on-failure
```

Do not literally pass `<DEVICE_A_MAC>` to Bash. Replace placeholders with real values.

Validate:

1. system bus is connected;
2. `org.bluez` is registered;
3. Auralis finds the adapter;
4. adapter is usable/powered;
5. scan starts;
6. actual discovery activity occurs;
7. expected devices are observed;
8. device lifecycle commands work as intended;
9. both expected devices can remain connected simultaneously on the reference adapter if the hardware allows it;
10. the test exits cleanly.

If the adapter cannot keep two A2DP devices connected at once, do not hide it. Capture the controller/BlueZ evidence and classify it as an architecture/hardware limitation requiring owner review.

---

# 13. TWO-DEVICE HARDWARE SESSION QUALIFICATION

This is a **mandatory Phase 9 gate** for the current Linux RC1 target.

Use two actual Bluetooth audio devices that are already known to work with Auralis.

Assign neutral aliases in documentation:

```text
Device A
Device B
```

Capture the actual addresses only in local raw evidence when needed.

## 13.1 Connection gate

Prove that both devices are simultaneously:

- known to BlueZ;
- connected;
- represented in Auralis;
- represented as available audio playback endpoints where expected.

## 13.2 Auralis UI gate

Launch the desktop application from the current build and verify:

- application opens without fatal error;
- Bluetooth status is truthful;
- both devices appear;
- connection state matches the host;
- device aliases/names are sensible;
- no duplicate phantom device rows are created;
- no repeating error toast/log storm occurs while healthy.

## 13.3 Session gate

Create or restore a two-device session and verify:

- both devices are session members;
- one source is selected;
- activation completes;
- each member's route is active;
- the session state becomes the appropriate healthy state;
- one member is not silently replacing the other.

---

# 14. LIVE PIPEWIRE ENDPOINT AND ROUTING QUALIFICATION

Use existing live integration tests where possible.

At minimum run:

```text
tst_PipeWireLiveIntegration
tst_AudioRoutingLiveIntegration
```

with the required opt-in environment variables.

Inspect the test source before deciding which device address must be supplied.

For `tst_AudioRoutingLiveIntegration`, the current code supports `AURALIS_EXPECT_DEVICE_ADDRESS`.

Required validation:

1. PipeWire reaches Connected;
2. initial graph sync completes;
3. playback endpoints are enumerated;
4. the expected Bluetooth device maps to the correct endpoint;
5. route creation succeeds;
6. route removal succeeds;
7. no unrelated foreign PipeWire links are destroyed;
8. link ownership semantics remain correct;
9. stale PipeWire IDs are not treated as stable identities.

---

# 15. SHARED-SOURCE TWO-DESTINATION FAN-OUT — CRITICAL REGRESSION GATE

The current repository contains a post-Phase-8 fix for a serious issue in which routing one shared source to multiple destinations could tear down sibling links.

This behavior must be validated on the real machine.

Create the topology:

```text
One Auralis source
      |
      +----> Device A
      |
      +----> Device B
```

Verify all of the following:

- both routes exist simultaneously;
- activating Device B does not destroy Device A's route;
- refreshing the session does not oscillate between devices;
- no `LinkCreationFailed` toast storm occurs;
- the routing coordinator does not repeatedly recreate healthy links;
- both devices audibly render the same program content;
- stopping/removing one destination leaves the sibling route intact when semantics require it;
- deactivating the full session removes only the links owned by that session/Auralis.

Capture a PipeWire graph/link dump before, during, and after the test.

If any fan-out regression appears, this is an immediate Phase 9 blocker.

---

# 16. SYSTEM AUDIO / VIRTUAL OUTPUT QUALIFICATION

Validate the Linux virtual-output path documented by the project.

The expected user workflow currently includes an `Auralis System Audio` virtual output / source path.

Test with actual system media, not only a synthetic unit-test source.

Suggested sources:

- local media player;
- browser audio;
- a deterministic local audio file.

Validate:

1. the Auralis virtual output exists or can be established as documented;
2. it can be selected as the system output using Auralis's supported workflow;
3. Auralis can use the resulting system-audio source;
4. one copy of the system mix is routed to both Device A and Device B;
5. audio is continuous on both;
6. application restart does not create uncontrolled duplicate virtual nodes;
7. PipeWire/WirePlumber restart does not leave permanent duplicate stale virtual outputs;
8. closing Auralis behaves according to the documented persistent/fallback virtual-output design.

Do not claim lip-sync or device-to-device acoustic synchronization in Phase 9 unless it is actually measured. Simultaneous playback is not automatically the same as synchronized playback.

---

# 17. REAL BLUETOOTH BUTTON ALLOW/DISALLOW VALIDATION — MANDATORY

This is a critical part of Phase 9 because the current code already implements the feature and the owner specifically requires control over accidental Bluetooth media-button input.

## 17.1 First determine the actual event path

For each connected test device, inspect:

```bash
cat /proc/bus/input/devices
ls -l /dev/input/by-id /dev/input/by-path 2>/dev/null || true
udevadm info --query=property --name=/dev/input/eventX
```

Use `evtest` only when appropriate and installed, and only against the exact event node correlated to the target Bluetooth device.

Never grab/test an arbitrary keyboard or mouse node.

Confirm correlation using one or more of:

- Bluetooth MAC/Uniq;
- Phys path containing the device address;
- udev Bluetooth properties;
- known event-node behavior.

## 17.2 ALLOW test

For each supported device:

1. set Device buttons = **ALLOW** in Auralis;
2. confirm effective status is Allowed/appropriate;
3. play media;
4. press the device's Play/Pause button;
5. confirm the normal host/media behavior occurs;
6. repeat enough times to prove the event path is real and not a coincidence.

## 17.3 DISALLOW test

While the device remains connected and audio remains routed:

1. set Device buttons = **DISALLOW**;
2. confirm the UI reports Suppressed or the correct effective state;
3. press Play/Pause repeatedly;
4. confirm those consumer-control events no longer spam/toggle the host media state;
5. confirm A2DP audio stays connected;
6. confirm the Auralis route remains alive;
7. confirm the other Bluetooth device remains unaffected unless it too is DISALLOW;
8. confirm Auralis does not freeze or block its UI thread.

## 17.4 Per-device isolation test

With Device A and Device B connected:

```text
Device A = DISALLOW
Device B = ALLOW
```

Verify:

- Device A buttons are suppressed when its transport is supported;
- Device B buttons still behave normally;
- Device A audio continues;
- Device B audio continues;
- the wrong evdev node is never grabbed;
- no global media-key blocking occurs.

Then swap the policy:

```text
Device A = ALLOW
Device B = DISALLOW
```

Repeat.

## 17.5 Toggle-under-playback test

While audio is actively playing:

```text
ALLOW -> DISALLOW -> ALLOW -> DISALLOW -> ALLOW
```

Repeat at least 10 controlled cycles per supported device.

Validate:

- no file-descriptor leak;
- no stale grab;
- no double-close;
- no inability to restore normal button behavior;
- no audio disconnect;
- no crash.

## 17.6 Disconnect/reconnect persistence test

For a device configured as DISALLOW:

1. disconnect it;
2. confirm any active grab is released;
3. reconnect it;
4. confirm the stored policy remains DISALLOW;
5. confirm suppression is reapplied when a correlatable input endpoint appears;
6. verify audio still functions.

## 17.7 Application restart persistence test

1. set Device A = DISALLOW;
2. cleanly exit Auralis;
3. verify any evdev grab is released on exit;
4. restart Auralis;
5. confirm the persisted policy remains DISALLOW;
6. connect/sync Device A;
7. confirm suppression is correctly re-established;
8. change back to ALLOW and verify normal button behavior returns.

## 17.8 Unsupported transport test

If a device does not expose a safely correlatable evdev endpoint:

- Auralis must not fake success;
- effective state should be Unsupported/appropriate;
- audio routing must remain usable;
- the UI must remain clear;
- no unrelated input device may be grabbed.

## 17.9 Permission-denied test

If the event node exists but the user session cannot open/grab it:

- do not tell the user to run the entire application as root;
- show/record Permission Denied or equivalent;
- document the least-privilege remediation path;
- prefer udev/session `uaccess` style access where appropriate;
- never broaden `/dev/input` permissions indiscriminately.

If a udev rule is added to the repository, make it narrow, reviewable, documented, and justified by the exact device-access model.

---

# 18. DEVICE CHURN / RECOVERY MATRIX

Once healthy two-device playback is established, deliberately exercise controlled failures.

The invariant is:

> A failure of one destination should not unnecessarily stop the healthy sibling destination.

For each scenario, capture:

```text
initial state
trigger
time detected
session state transition
route state Device A
route state Device B
reconnect attempts
recovery time
final state
user-visible notification
diagnostics/log evidence
```

## 18.1 Power off Device B

While both devices play:

1. power off Device B;
2. verify Device A continues where technically possible;
3. verify Auralis detects Device B loss;
4. verify session becomes Degraded/Recovering according to current policy;
5. verify no duplicate reconnect loop is created;
6. verify no healthy Device A route is destroyed;
7. power Device B back on;
8. verify managed reconnect/rebind;
9. verify Device B rejoins;
10. verify final session state becomes healthy.

## 18.2 Repeat for Device A

Do not assume symmetry from one successful device.

## 18.3 Rapid but bounded power cycle

Perform several operator-controlled on/off cycles on one device.

Do not create unsafe or abusive radio churn.

Validate retry backoff, state coalescing, and absence of notification storms.

## 18.4 Out-of-range test — optional but valuable

If practical, move one device out of range until disconnect, then restore it.

Classify this as a real RF recovery test.

Do not block Phase 9 solely because the physical environment cannot make this practical; mark it Not Exercised if unavailable.

---

# 19. PIPEWIRE / WIREPLUMBER RECOVERY ON THE ACTUAL MACHINE

These are mandatory where they can be performed safely in the user session.

## 19.1 PipeWire restart

With Auralis open and ideally a session configured:

```bash
systemctl --user restart pipewire
```

If the machine uses a different user-service arrangement, detect it first.

Validate:

- Auralis notices service loss;
- old graph objects are invalidated;
- reconnect is bounded;
- initial registry sync completes after return;
- stale callbacks from the old graph cannot mutate the new graph;
- endpoints repopulate;
- the session reconciles only after dependencies are ready;
- duplicate links are not created;
- user-visible status is understandable.

## 19.2 WirePlumber restart

Run only after confirming the user service exists:

```bash
systemctl --user restart wireplumber
```

Validate graph churn recovery and route/session restoration.

## 19.3 pipewire-pulse restart if separately managed

Only test if it is a separately meaningful service on this host.

Do not restart random services for coverage theater.

---

# 20. BLUEZ / ADAPTER RECOVERY ON THE ACTUAL MACHINE

BlueZ restart can require privileges and disconnect all Bluetooth devices.

Prepare the test first.

## 20.1 Adapter power toggle

If permitted through the normal user Bluetooth control path, test:

```text
adapter powered -> off -> on
```

Validate:

- Auralis reports adapter loss truthfully;
- discovery/connection actions disable appropriately;
- no tight retry loop occurs;
- adapter return is observed;
- device snapshots recover;
- remembered session/device state is preserved;
- reconnection follows existing policy.

## 20.2 BlueZ daemon restart

Do not silently invoke privileged restart commands.

If the operator approves a BlueZ restart, validate:

- `org.bluez` disappearance;
- pause of reconnect work that cannot succeed while BlueZ is absent;
- bus/service-return detection;
- generation fencing against stale D-Bus completions;
- fresh object snapshot;
- recovery of device state;
- session reconciliation only after required dependencies are healthy.

If BlueZ restart is not performed, mark it PENDING/NOT EXERCISED rather than PASS.

---

# 21. SUSPEND / RESUME QUALIFICATION

This is a mandatory laptop-specific Phase 9 gate unless the execution environment cannot suspend.

Before suspend:

1. start Auralis;
2. connect Device A + Device B;
3. configure/activate a representative session;
4. note `restoreOnResume` preference;
5. capture current route and recovery state;
6. flush audit notes to disk.

Then perform a real laptop suspend using operator-approved action.

After resume validate:

- Auralis process survives or behaves according to design;
- duplicate PrepareForSleep events do not corrupt state;
- reconnect timers do not leak across suspend epochs;
- system bus/logind subscription is healthy;
- BlueZ state is refreshed;
- PipeWire state is refreshed;
- old volatile endpoint IDs are not reused incorrectly;
- session restoration obeys `restoreOnResume`;
- Device A and Device B can return to usable state;
- no duplicate Auralis links remain;
- UI status eventually settles;
- no infinite Recovery state remains.

Repeat at least twice if the machine behaves consistently.

Also validate `restoreOnResume = false` once:

- the application must not resurrect playback/session intent against the user's preference.

---

# 22. APPLICATION RESTART / PERSISTENCE QUALIFICATION

Test clean application lifecycle separately from system suspend.

## 22.1 Healthy restart

With known devices and a persisted session:

1. cleanly close Auralis;
2. verify no `auralis-desktop` process remains;
3. verify device-button grabs are released;
4. verify log file closes cleanly;
5. restart Auralis;
6. verify known devices reappear appropriately;
7. verify stored session metadata loads;
8. verify stored recovery preferences load;
9. verify button policies load;
10. verify no duplicate PipeWire virtual output/link accumulation.

## 22.2 Restart while a device is absent

Persist a two-device session, close Auralis, power off Device B, then restart Auralis.

The application must:

- load the session without crashing;
- represent the absent member honestly;
- preserve Device A usability;
- avoid a permanent spinner/infinite state;
- use bounded reconnect/recovery behavior.

## 22.3 Restart after prior recovery

After a PipeWire or device recovery cycle, cleanly restart the app and ensure recovered transient state has not corrupted persistence.

---

# 23. CONNECT / DISCONNECT HARDWARE STRESS

Automated unit stress is not a substitute for real Bluetooth churn.

Perform a bounded actual-hardware stress sequence.

Recommended minimum where practical:

```text
10 complete connect/disconnect cycles per target device
10 alternating A/B connection cycles
10 two-device session activate/deactivate cycles
10 button policy toggle cycles per supported device under playback
```

If hardware/firmware makes a particular count impractical, use a smaller justified number and document it.

For every cycle, look for:

- state divergence between BlueZ and Auralis;
- duplicate rows;
- stale endpoints;
- stale routes;
- repeated reconnect jobs;
- increasing open FDs;
- increasing memory without settling;
- notification storms;
- QML binding errors;
- crashes;
- audio that stops on the healthy sibling.

---

# 24. RESOURCE / LIFETIME OBSERVATION

During long-running/churn tests, record basic process health.

Useful commands:

```bash
pidof auralis-desktop
ps -o pid,etime,%cpu,%mem,rss,vsz,nlwp,cmd -p <PID>
ls /proc/<PID>/fd | wc -l
cat /proc/<PID>/status
```

Where useful, sample periodically to a CSV/text file.

Track at least:

```text
time
RSS
VSZ
thread count
open FD count
CPU percentage
active session state
connected-device count
active route count if observable
```

Look especially for monotonic growth after:

- ALLOW/DISALLOW toggles;
- repeated device reconnects;
- repeated session activate/deactivate;
- PipeWire restarts;
- application-idle periods.

A small one-time allocation increase is not automatically a leak. Investigate persistent monotonic growth that does not stabilize.

---

# 25. SOAK TEST

A release candidate needs evidence beyond short test cases.

Perform a real two-device playback soak using a stable local/system source.

Minimum useful qualification target:

```text
2 hours continuous playback
```

Preferred stronger evidence:

```text
6 hours continuous playback
```

Optional endurance evidence:

```text
12 hours
```

Do not fabricate completion of a duration that was not actually run.

During the soak:

- keep both devices routed;
- capture resource snapshots periodically;
- capture connection/session transitions;
- record any dropout;
- record any device reboot/disconnect;
- record any spontaneous route recreation;
- monitor log growth/rotation;
- confirm diagnostics remain responsive.

At the end, report:

```text
planned duration
actual duration
continuous-audio result Device A
continuous-audio result Device B
dropout count observed
reconnect count
session state at end
RSS delta
FD delta
log rotation behavior
```

If the soak is interrupted by external causes, record the actual duration and cause.

---

# 26. LOGGING / DIAGNOSTICS QUALITY REVIEW

Phase 8 already added logging/recovery diagnostics. Phase 9 must ensure those logs are genuinely useful on hardware.

During failures, verify logs can answer:

- which service disappeared?;
- which device disconnected?;
- which retry attempt is occurring?;
- which session is recovering?;
- why a route was removed/recreated?;
- whether PipeWire initial sync completed?;
- whether a button policy was applied?;
- which effective button state resulted?;
- whether an input endpoint was unsupported or permission denied?;
- whether recovery exhausted?;
- whether suspend/resume was observed?;
- whether a stale generation/callback was ignored?;
- whether log rotation occurred?;

Required properties:

- no rapid duplicate message storm while healthy;
- no Bluetooth MAC/address exposure in UI beyond what is already intentionally shown;
- no secret/token data;
- bounded file growth;
- rotated logs retained according to configuration;
- user-facing errors are concise;
- technical logs remain sufficiently specific.

If logs are missing crucial root-cause context, improve them narrowly without introducing noisy per-frame/per-event spam.

---

# 27. QML / GUI RELEASE-CANDIDATE CHECK

Run the existing offscreen GUI tests and manually exercise the actual interface.

Mandatory checks:

- startup page loads;
- navigation works;
- Devices page loads;
- Audio Routing page loads;
- Settings page loads;
- Diagnostics page loads;
- no unresolved QML import errors;
- no binding loop spam;
- no `TypeError`/`ReferenceError` flood;
- device list updates dynamically;
- button ALLOW/DISALLOW controls remain responsive;
- unsupported/permission states fit in the UI;
- connect/disconnect controls cannot be spammed into inconsistent state;
- session state is understandable;
- degraded/recovering state is visible without freezing the UI.

Do not undertake a visual redesign unless a UI defect blocks safe operation.

---

# 28. PACKAGE / INSTALL-TREE QUALIFICATION

Phase 8 already contains CPack support. Phase 9 must prove the package is usable without silently reaching back into the source tree or build tree.

## 28.1 Release-style build

Use a clean release directory, for example:

```bash
rm -rf build-phase9-release
cmake -S . -B build-phase9-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-phase9-release
ctest --test-dir build-phase9-release --output-on-failure
```

## 28.2 Staged install

Use a non-root staging prefix/DESTDIR where possible.

Example concept:

```bash
rm -rf audit-phase9-linux-rc/package/stage
DESTDIR="$PWD/audit-phase9-linux-rc/package/stage" \
cmake --install build-phase9-release --prefix /usr
```

Inspect the staged tree.

Verify expected artifacts such as:

```text
/usr/bin/auralis-desktop
/usr/share/applications/io.github.auralis.Auralis.desktop
/usr/share/metainfo/io.github.auralis.Auralis.metainfo.xml
/usr/share/icons/...
/usr/share/doc/auralis/LICENSE
```

and any bundled Qt/runtime layout required by the project's current packaging strategy.

## 28.3 CPack

Build the Linux package:

```bash
cpack --config build-phase9-release/CPackConfig.cmake
```

or the repository's established package target.

Record generated artifacts.

## 28.4 Inspect DEB without installing

Use:

```bash
dpkg-deb -I <auralis-package.deb>
dpkg-deb -c <auralis-package.deb>
```

Validate:

- architecture;
- package name;
- version;
- dependencies;
- maintainer/contact truthfulness;
- installed paths;
- desktop entry;
- AppStream metadata;
- license file;
- no build-directory paths embedded where they should not be.

## 28.5 Extract-and-launch test

Without installing system-wide:

```bash
rm -rf audit-phase9-linux-rc/package/extracted
mkdir -p audit-phase9-linux-rc/package/extracted
dpkg-deb -x <auralis-package.deb> audit-phase9-linux-rc/package/extracted
```

Launch from the extracted tree using the repository's documented bundled-runtime invocation approach.

The test must prove Auralis does not depend on the original build tree.

An offscreen timeout-based smoke is useful, but Phase 9 should additionally launch the packaged build normally on the actual machine if practical.

## 28.6 Public distribution block

Unless the project owner has explicitly changed the licensing/contact facts, preserve:

```text
PUBLIC RELEASE READINESS: BLOCKED — OWNER LICENSE/CONTACT METADATA PENDING
```

A technically good package is not automatically legally/publicly releasable.

---

# 29. CLEAN-MACHINE PORTABILITY PREPARATION

If only the development laptop is available during Phase 9, prepare—not fake—the clean-machine test.

Create/update documentation that a second-machine qualification must later verify:

- package install dependencies;
- bundled Qt behavior;
- desktop launch;
- BlueZ discovery;
- PipeWire graph;
- virtual output;
- Bluetooth permissions;
- two-device routing.

If another clean Ubuntu 26.04 machine is actually available, perform the package test there and record it.

If not, mark:

```text
SECOND CLEAN MACHINE: NOT EXERCISED
```

Do not block the internal development-machine RC1 technical gate solely because a second host is unavailable, but keep public release confidence lower until it is done.

---

# 30. OPTIONAL THREE/FOUR-DEVICE SCALING PREFLIGHT

The original Auralis product specification ultimately targets investigation of 2–4 devices, but Phase 9 must not destabilize the now-working two-device core merely to chase an unproven larger count.

If three or four compatible Bluetooth devices are physically available, perform an **observational preflight** only.

Measure:

```text
how many devices BlueZ can keep connected simultaneously
how many A2DP playback endpoints appear
whether one source can fan out to N destinations
whether existing link conflict semantics remain correct
CPU/RSS changes
radio instability/dropouts
recovery behavior when one member leaves
```

Do not silently redefine Phase 9 failure if the host adapter cannot sustain 3/4 links.

Instead produce evidence for the next architecture/scaling phase.

Possible conclusion examples:

```text
3-device software fan-out viable on reference adapter
4-device connection limited by controller/BlueZ/device behavior
additional adapter/LE Audio research required
```

If fewer than three devices are available:

```text
3/4-device scaling preflight: NOT EXERCISED — HARDWARE UNAVAILABLE
```

---

# 31. DEFECT-HANDLING RULE

If a live test fails, do not immediately patch symptoms.

Use this sequence:

```text
1. reproduce
2. capture logs/state
3. identify ownership layer
4. determine whether host, device, BlueZ, PipeWire, WirePlumber, permissions, or Auralis is responsible
5. write a focused regression test when the defect is in Auralis
6. implement the smallest architectural fix
7. run focused test
8. run affected subsystem suite
9. run full CTest
10. rerun the exact hardware scenario
11. update audit evidence
```

A fix without a reproduction/regression test should be exceptional and explicitly justified.

---

# 32. ARCHITECTURAL OWNERSHIP RULES DURING FIXES

Preserve existing ownership boundaries.

## Bluetooth

`BluetoothManager` / BlueZ client / registry / reconnect policy remain responsible for Bluetooth state.

`BluetoothButtonControlManager` remains responsible for the button policy.

Do not make QML directly open `/dev/input` or call BlueZ D-Bus.

## Audio

`PipeWireManager`, endpoint registry, router, and related audio classes remain responsible for PipeWire graph state.

Do not put PipeWire graph mutation into QML.

## Session

`SessionManager` / routing coordinator remain the higher-level session owners.

Do not create hidden route ownership in the UI.

## Recovery

`RecoveryManager` remains the central recovery orchestrator.

Do not add a second global recovery state machine.

## UI

QML should invoke stable application-facing APIs and render state.

Do not make QML the source of truth for backend state.

---

# 33. BUTTON-CONTROL-SPECIFIC CODE REVIEW

Even if live tests pass, review the current button implementation for release blockers exposed by real hardware.

Inspect at least:

```text
address normalization
/proc/bus/input/devices parsing
Uniq/Phys correlation
input endpoint cache invalidation
open flags
EVIOCGRAB application/release
file descriptor ownership
shutdown release
policy persistence
connected-state reapply
unsupported state
permission-denied state
per-device isolation
QML model role updates
```

Verify no code path can:

- grab an input endpoint based only on a fuzzy device name;
- leave a grab active after application shutdown;
- leak an FD on failed ioctl;
- retain a stale endpoint after disconnect;
- show Suppressed when `EVIOCGRAB` failed;
- reinterpret disconnected state as policy reset;
- turn DISALLOW into a Bluetooth disconnect;
- write button policy on every harmless model refresh;
- cause a busy probe loop over `/proc/bus/input/devices`.

If the implementation works correctly on the hardware, do not refactor it merely to make it more elaborate.

---

# 34. RECOVERY-SPECIFIC CODE REVIEW

Use hardware evidence to verify previously fixed Phase 8 invariants remain true:

- bus health checking remains active after a healthy period;
- generation fencing rejects stale BlueZ snapshot completions;
- PipeWire initial sync cannot wait forever;
- disabling auto-recovery cancels pending/coalesced recovery appropriately;
- suspend/resume handling is idempotent;
- logind subscription can recover after bus/service return;
- session reconciliation waits for required dependencies;
- snapshot refreshes are coalesced;
- user-facing attempt numbering never begins at attempt 0;
- one destination failure preserves healthy peers;
- explicit user stop/delete intent always beats automatic recovery.

Only change code if actual evidence or a newly written regression test proves a problem.

---

# 35. MULTI-DEVICE ROUTING INVARIANTS

These are mandatory and must be explicit in tests/audit.

```text
One source output may legitimately feed multiple Auralis-owned destination inputs.
```

Therefore:

- source-side fan-out is allowed;
- destination-input exclusivity may still be enforced according to current router semantics;
- adding a sibling destination must not be treated as a conflict merely because the source is shared;
- Auralis must not destroy foreign links it does not own;
- Auralis must not destroy healthy peer links it owns as part of the same valid fan-out;
- deactivation must remove only the correct Auralis-owned links.

Test both stereo channels / relevant ports, not merely a single representative link if the backend creates per-channel links.

---

# 36. SESSION RECOVERY INVARIANTS

For a two-member active session:

```text
Device A healthy + Device B disconnected
```

Expected behavior is generally:

```text
Device A remains usable
Device B enters recovery/degraded state
session becomes Recovering/Degraded as designed
bounded reconnect attempts occur
Device B rejoins when available
session returns healthy when all dependencies/routes are healthy
```

It is not acceptable for the application to unnecessarily tear down Device A just because Device B disappeared.

When recovery exhausts:

- healthy members remain healthy where supported;
- session exits endless Recovering;
- error state is user-visible;
- retry work stops according to policy.

---

# 37. NO ROOT AS A NORMAL RUNTIME REQUIREMENT

Auralis must continue to run as a normal desktop user.

Phase 9 must reject any "fix" that requires launching the whole GUI with:

```bash
sudo ./auralis-desktop
```

If button control needs input-device access, solve permissions at the narrow device/session level.

If BlueZ actions require policy/agent integration, use the correct D-Bus/BlueZ model.

If packaging needs system installation, that is separate from runtime privilege.

---

# 38. TEST MATRIX — REQUIRED RESULT TABLE

Create a durable Phase 9 table with rows at least for:

| Area | Scenario | Mandatory? | Result | Evidence |
|---|---|---:|---|---|
| Build | Fresh RelWithDebInfo configure/build | Yes | | |
| Tests | Full default CTest | Yes | | |
| Tests | Extended stress label | Yes | | |
| Sanitizer | ASan/UBSan CTest | Yes | | |
| BlueZ | Adapter/live discovery | Yes | | |
| BlueZ | Device A lifecycle | Yes | | |
| BlueZ | Device B lifecycle | Yes | | |
| BlueZ | Two simultaneous connections | Yes | | |
| PipeWire | Live graph sync | Yes | | |
| Routing | Device A route | Yes | | |
| Routing | Device B route | Yes | | |
| Routing | Shared-source A+B fan-out | Yes | | |
| System audio | Auralis virtual system output | Yes | | |
| Buttons | Device A ALLOW | If supported | | |
| Buttons | Device A DISALLOW | If supported | | |
| Buttons | Device B ALLOW | If supported | | |
| Buttons | Device B DISALLOW | If supported | | |
| Buttons | Per-device isolation | If supported | | |
| Buttons | Restart persistence | Yes | | |
| Recovery | Power off/on Device A | Yes | | |
| Recovery | Power off/on Device B | Yes | | |
| Recovery | PipeWire restart | Yes | | |
| Recovery | WirePlumber restart | Yes | | |
| Recovery | Adapter off/on | When safe | | |
| Recovery | BlueZ daemon restart | Operator-approved | | |
| Power | Real suspend/resume | Yes | | |
| Persistence | App restart with healthy session | Yes | | |
| Persistence | App restart with one device absent | Yes | | |
| Stress | Repeated hardware churn | Yes | | |
| Soak | >=2h two-device playback | Yes | | |
| Package | Release build | Yes | | |
| Package | DEB build | Yes | | |
| Package | DEB inspect | Yes | | |
| Package | Extracted-tree launch | Yes | | |
| Package | Normal packaged launch | Preferred | | |
| Scaling | 3-device preflight | Optional | | |
| Scaling | 4-device preflight | Optional | | |
| Portability | Second clean machine | Optional/next gate | | |

Use only these result labels:

```text
PASS
FAIL
SKIPPED
NOT EXERCISED
PENDING OPERATOR ACTION
BLOCKED
```

Do not use ambiguous labels such as "looks okay".

---

# 39. NEW/UPDATED VALIDATION SCRIPT — RECOMMENDED

If the repository does not already have a single safe Phase 9 orchestrator, add one such as:

```text
scripts/validation/run-phase9-linux-rc.sh
```

The script should:

- be Bash with strict mode;
- run from or resolve the repository root;
- create an evidence directory;
- capture environment versions;
- configure/build a clean tree;
- run default CTest;
- run stress label;
- optionally run sanitizer build;
- run safe live tests only when explicit environment variables opt in;
- never guess Bluetooth addresses;
- never run `sudo`;
- never forget devices by default;
- clearly distinguish required environment variables;
- emit a final machine-readable-ish summary;
- return non-zero if a mandatory executed test fails.

Do not make the script automatically suspend the laptop or restart privileged services.

Those scenarios should remain operator-controlled with documented steps.

If a script like this already exists in an equivalent form, extend it rather than adding duplication.

---

# 40. VALIDATION DOCUMENT — REQUIRED

Add or update:

```text
docs/validation/phase-9-linux-rc.md
```

It must explain how another engineer on the Linux machine can reproduce the Phase 9 qualification.

Include:

- prerequisites;
- safe/non-destructive defaults;
- required environment variables;
- two-device address syntax;
- commands for software tests;
- commands for live BlueZ tests;
- commands for live PipeWire tests;
- instructions for button-policy testing;
- recovery scenarios;
- suspend/resume procedure;
- soak procedure;
- package validation;
- expected outputs;
- result classification rules;
- privacy/redaction guidance;
- no-root requirement;
- public-release metadata warning.

---

# 41. PHASE 9 AUDIT DOCUMENT — REQUIRED

Create:

```text
docs/PHASE_9_LINUX_RC1_QUALIFICATION_AUDIT.md
```

This is the durable engineering record.

It must include the following sections.

## 41.1 Audit identity

```text
Date/time
Host OS
Kernel
Git revision
Branch
Dirty/clean status
Compiler
CMake
Ninja
Qt
BlueZ
PipeWire
WirePlumber status
Bluetooth adapter summary
```

## 41.2 Baseline regression

Record exact clean-build and CTest results.

## 41.3 Sanitizer results

Clearly state whether leak detection was enabled.

## 41.4 Hardware inventory

Use redacted aliases for durable docs.

Example:

```text
Device A — headset/earbud model — address redacted
Device B — headset/earbud model — address redacted
```

## 41.5 Two-device connection evidence

Include results and relevant timestamps.

## 41.6 PipeWire / fan-out evidence

Record endpoint and route behavior.

## 41.7 System-audio evidence

Record actual user media path.

## 41.8 Button policy evidence

Per device, record:

```text
input event path found? yes/no
effective ALLOW state
ALLOW physical-button result
effective DISALLOW state
DISALLOW physical-button result
audio remained connected? yes/no
route remained active? yes/no
policy persisted across reconnect? yes/no
policy persisted across app restart? yes/no
```

## 41.9 Recovery evidence

Include device power-off/rejoin, PipeWire, WirePlumber, adapter, BlueZ if exercised.

## 41.10 Suspend/resume evidence

Record actual suspend cycles and results.

## 41.11 Hardware stress evidence

Record cycle counts and failures.

## 41.12 Soak evidence

Record actual duration and resource deltas.

## 41.13 Package evidence

Record package file, metadata, contents, staged/extracted launch result.

## 41.14 Known limitations

Be explicit.

Examples:

```text
public license pending
maintainer contact pending
second-machine testing not yet exercised
3/4-device scaling not yet exercised
automatic acoustic synchronization is a later phase
```

## 41.15 Defects found/fixed

For each code fix:

```text
symptom
root cause
files changed
new regression test
test results
live retest result
```

## 41.16 Exit gates

Use the exact gate model from Section 47 below.

---

# 42. README / ROADMAP TRUTHFULNESS UPDATE

Only after the evidence exists, update status documentation.

If Phase 8 live/hardware validation has now been independently reproduced successfully, it is appropriate to change the README/audit status from hardware pending to hardware pass.

Do not update it earlier.

Potential status wording after successful evidence:

```text
Phase 8: Complete — software + actual Linux hardware validation PASS
Phase 9: Linux RC1 technical qualification PASS (internal candidate)
```

But preserve a separate warning if public distribution metadata is unresolved.

Also correct stale roadmap text that claims an older implementation boundary if doing so is straightforward and evidence-based.

Do not rewrite historical audit documents to erase the fact that hardware had previously been pending. Prefer adding a dated closure note.

---

# 43. VERSIONING / RC NAMING RULE

Do not arbitrarily change `project(Auralis VERSION 0.1.0)` to `1.0.0`.

Do not create a public release tag automatically.

If the project owner has not supplied a release/versioning policy, keep the existing source version and describe the output as:

```text
Internal Linux RC1 technical candidate based on Auralis 0.1.0
```

If an internal package suffix is needed, implement it only if the existing CMake/CPack versioning model can do so cleanly and without breaking Debian version semantics.

A version bump is not required to pass technical qualification.

---

# 44. PUBLIC RELEASE METADATA RULE

The repository currently states that licensing terms are not selected.

Therefore the final audit must distinguish:

```text
TECHNICAL RC READINESS
```

from:

```text
PUBLIC DISTRIBUTION READINESS
```

If license/contact remain unresolved, the correct outcome can be:

```text
LINUX RC1 TECHNICAL EXIT GATE: PASS
PUBLIC DISTRIBUTION READINESS: BLOCKED — OWNER LICENSE/CONTACT METADATA PENDING
```

That is not a technical failure.

It is a truthful release-management boundary.

---

# 45. SECURITY / PRIVACY CHECK

Before declaring RC technical PASS, review for obvious desktop security/privacy regressions.

Verify:

- no application-wide root requirement;
- no world-writable broad `/dev/input` permission workaround;
- no command injection from device names;
- no shelling out with unescaped Bluetooth aliases;
- no arbitrary file write through diagnostic filenames;
- no secrets in logs;
- no unexpected network telemetry;
- no destructive device forget/reset as default behavior;
- no package post-install script performing unrelated privileged work;
- input-device access is narrowly justified;
- QSettings/session data remains in appropriate per-user locations.

Do not turn Phase 9 into a theoretical penetration-testing project. Fix concrete high-confidence issues that affect the release boundary.

---

# 46. FAILURE CLASSIFICATION

When something goes wrong, classify it before coding.

Use one of:

```text
AURALIS SOFTWARE DEFECT
HOST CONFIGURATION
BLUEZ / KERNEL LIMITATION
PIPEWIRE / WIREPLUMBER LIMITATION
DEVICE FIRMWARE / PROFILE BEHAVIOR
PERMISSION CONFIGURATION
TEST HARNESS DEFECT
EXPECTED UNSUPPORTED CAPABILITY
OPERATOR / ENVIRONMENT INTERRUPTION
UNKNOWN — REQUIRES MORE EVIDENCE
```

Do not blame Auralis for a powered-off adapter.

Do not blame the host for a reproducible Auralis state-machine defect.

---

# 47. PHASE 9 EXIT GATES

Use these exact gates in the final audit.

## Gate A — Software regression

PASS only if:

- clean configure/build passes;
- full default CTest passes;
- extended stress passes;
- no unresolved sanitizer defect remains.

## Gate B — Phase 8 hardware closure

PASS only if actual machine evidence proves:

- live BlueZ works;
- live PipeWire works;
- two actual devices can participate in the intended current workflow;
- two-device fan-out works;
- system audio path works.

If Gate B passes, the old Phase 8 hardware PENDING status may be formally closed with a dated note.

## Gate C — Button policy

PASS only if:

- ALLOW behavior is validated;
- DISALLOW suppression is validated on at least one actually supported target input path;
- per-device isolation is validated where two supported nodes are available, or limitation is truthfully documented;
- audio remains unaffected;
- reconnect/restart persistence is correct;
- unsupported/permission states are truthful.

If neither available device exposes a suppressible evdev endpoint, do not fabricate a PASS for physical suppression. In that case classify software policy as PASS but hardware suppression as NOT EXERCISED/UNSUPPORTED and explicitly decide whether that blocks the target RC use case.

## Gate D — Recovery

PASS only if:

- one-device loss does not unnecessarily tear down a healthy peer;
- device reconnect works;
- PipeWire restart recovers;
- WirePlumber churn recovers;
- suspend/resume passes;
- no unbounded recovery loop remains.

BlueZ daemon restart may remain operator-dependent if not safely exercised, but adapter/service recovery must have strong software test coverage and whatever live evidence is available.

## Gate E — Persistence/lifecycle

PASS only if:

- clean exit works;
- restart works;
- sessions/preferences/button policy persist correctly;
- absent-device restart is handled sanely;
- no stale grab remains after shutdown.

## Gate F — Stress/soak

PASS only if:

- bounded actual hardware churn passes;
- at least the minimum actual soak target has completed;
- no crash, unbounded memory/FD growth, or repeated route corruption is observed.

If the soak has not actually completed, this gate is PENDING.

## Gate G — Packaging

PASS only if:

- Release build passes;
- package builds;
- package metadata/contents are sane;
- extracted/staged tree can launch independently of the build tree;
- normal runtime remains non-root.

## Gate H — Technical Linux RC1

PASS only if Gates A–G all PASS.

Expected final wording:

```text
PHASE 8 SOFTWARE EXIT GATE: PASS
PHASE 8 HARDWARE EXIT GATE: PASS
PHASE 8 OVERALL: PASS

PHASE 9 SOFTWARE/REGRESSION GATE: PASS
PHASE 9 LIVE HARDWARE GATE: PASS
PHASE 9 BUTTON POLICY GATE: PASS
PHASE 9 RECOVERY GATE: PASS
PHASE 9 PERSISTENCE/LIFECYCLE GATE: PASS
PHASE 9 STRESS/SOAK GATE: PASS
PHASE 9 PACKAGING GATE: PASS

PHASE 9 LINUX RC1 TECHNICAL EXIT GATE: PASS
```

Only print those PASS values if the evidence genuinely supports them.

Separately print:

```text
PUBLIC DISTRIBUTION READINESS: BLOCKED — OWNER LICENSE/CONTACT METADATA PENDING
```

if those owner decisions remain unresolved.

---

# 48. WHEN PHASE 9 MUST FAIL OR REMAIN PENDING

Do not force a PASS if any mandatory item has one of these unresolved outcomes:

- reproducible crash;
- default CTest regression;
- ASan/UBSan memory/lifetime defect;
- two-device fan-out tears sibling links;
- one-device disconnect stops the healthy peer without architectural necessity;
- stale route accumulation;
- infinite Recovering state;
- uncontrolled reconnect loop;
- application cannot recover from PipeWire/WirePlumber restart;
- suspend/resume corrupts state;
- button DISALLOW disconnects audio;
- button ALLOW cannot release an existing grab;
- wrong input node can be grabbed;
- FD leak under button toggles;
- package depends on source/build tree;
- normal runtime requires root;
- mandatory soak not completed;
- mandatory hardware scenario was skipped and no valid equivalent evidence exists.

In those cases use:

```text
PHASE 9 LINUX RC1 TECHNICAL EXIT GATE: FAIL
```

or:

```text
PHASE 9 LINUX RC1 TECHNICAL EXIT GATE: PENDING
```

with exact blockers.

---

# 49. REQUIRED REGRESSION TESTS FOR ANY NEW FIX

Every Phase 9 code correction must add or strengthen the closest appropriate automated test.

Examples:

## Button-policy bug

Extend:

```text
tests/unit/bluetooth/tst_BluetoothButtonControlManager.cpp
```

and/or relevant Bluetooth model tests.

## BlueZ stale-state bug

Extend:

```text
tst_BluetoothManager
tst_BlueZDbusClientBusRecovery
tst_Phase8RecoveryHarness
```

as appropriate.

## PipeWire graph bug

Extend:

```text
tst_PipeWireManager
tst_AudioRouter
tst_RoutingCoordinator
tst_Phase8RecoveryHarness
```

as appropriate.

## Session recovery bug

Extend:

```text
tst_SessionManager
tst_SessionManagedReconnectIntegration
tst_ServiceRecoveryIntegration
```

as appropriate.

## QML bug

Extend:

```text
tst_QmlComponents
tst_DesktopBackendSmoke
```

where realistic.

Do not create low-value tests that merely assert a constant exists.

---

# 50. AFTER EVERY FIX — REQUIRED TEST LADDER

Use this sequence:

```text
1. newly added focused regression test
2. affected subsystem tests
3. affected integration tests
4. full default CTest
5. stress label if state/recovery/router affected
6. sanitizer suite if C++ lifetime/state ownership affected
7. exact live hardware reproduction
8. two-device fan-out regression
9. packaged/release smoke if packaging/runtime paths affected
```

Do not wait until the very end to discover a Phase 3/5/6 regression.

---

# 51. FINAL CLEAN REBUILD

After all fixes are complete, perform one more completely fresh build from the final source tree.

Example:

```bash
rm -rf build-phase9-final
cmake -S . -B build-phase9-final -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-phase9-final
ctest --test-dir build-phase9-final --output-on-failure
```

Then rerun the mandatory final live smoke:

```text
launch app
connect/confirm Device A + Device B
activate two-device source fan-out
play real system audio
verify both outputs
verify representative button ALLOW/DISALLOW behavior
cleanly deactivate
cleanly exit
```

This final smoke must use the final code, not an earlier build.

---

# 52. REQUIRED FILE CHANGES — EXPECTED, NOT MANDATORY IF ALREADY PRESENT

A clean Phase 9 implementation will likely add/update only a small number of release/validation artifacts, such as:

```text
docs/PHASE_9_LINUX_RC1_QUALIFICATION_AUDIT.md
docs/validation/phase-9-linux-rc.md
scripts/validation/run-phase9-linux-rc.sh
README.md
```

Core source files should change only if Phase 9 finds an actual defect.

This is intentional.

A qualification phase with zero core-code changes can be a success if the current implementation genuinely passes.

---

# 53. DO NOT "IMPROVE" WORKING CODE WITHOUT EVIDENCE

Forbidden behavior includes:

- broad namespace/file renames;
- changing public APIs for aesthetics;
- replacing Qt containers with STL everywhere;
- moving ownership across subsystems without a bug;
- changing retry constants with no failing scenario;
- replacing PipeWire mechanisms because another API looks cleaner;
- replacing evdev button suppression because a theoretical alternative seems interesting;
- reformatting hundreds of unrelated files;
- upgrading dependency minimums without reason;
- changing package identity;
- changing AppStream ID;
- inventing a new version number;
- inventing legal metadata.

Release hardening should reduce uncertainty, not increase change surface.

---

# 54. PHASE 9 ACCEPTANCE CRITERIA — DETAILED

The implementation is acceptable only if all of the following are true.

## 54.1 Build/test

- [ ] Fresh configure passes.
- [ ] Fresh build passes.
- [ ] Full default CTest passes.
- [ ] Extended stress passes.
- [ ] ASan/UBSan shows no unresolved Auralis defect.

## 54.2 Bluetooth

- [ ] Reference adapter is detected.
- [ ] Live discovery works.
- [ ] Device A is found/managed.
- [ ] Device B is found/managed.
- [ ] A and B can be simultaneously connected for the target workflow.
- [ ] Device state does not duplicate/diverge under refresh.

## 54.3 Audio

- [ ] PipeWire connects.
- [ ] Initial graph sync completes.
- [ ] Device A playback endpoint resolves.
- [ ] Device B playback endpoint resolves.
- [ ] Route to A works.
- [ ] Route to B works.
- [ ] Shared-source A+B fan-out works.
- [ ] A sibling route survives valid peer changes.
- [ ] System audio can be routed through Auralis.

## 54.4 Button policy

- [ ] Default is ALLOW.
- [ ] Policy persists per device.
- [ ] ALLOW permits normal button behavior on supported hardware.
- [ ] DISALLOW suppresses the target device's consumer-control path on supported hardware.
- [ ] Audio remains connected during DISALLOW.
- [ ] Route remains active during DISALLOW.
- [ ] Releasing to ALLOW restores normal behavior.
- [ ] Per-device policy does not globally affect other devices.
- [ ] Disconnect releases the grab.
- [ ] Reconnect reapplies stored DISALLOW where supported.
- [ ] App shutdown releases every active grab.
- [ ] Unsupported path is honest.
- [ ] Permission denied is honest.

## 54.5 Recovery

- [ ] Power-off/rejoin Device A works.
- [ ] Power-off/rejoin Device B works.
- [ ] Healthy peer remains usable.
- [ ] PipeWire restart recovers.
- [ ] WirePlumber restart recovers.
- [ ] Adapter/service churn behaves safely.
- [ ] Suspend/resume recovers.
- [ ] `restoreOnResume` is respected.
- [ ] No endless recovery loop.

## 54.6 Lifecycle/persistence

- [ ] App clean exit leaves no stale process.
- [ ] No stale evdev grab remains.
- [ ] Session state persists correctly.
- [ ] Preferences persist.
- [ ] Button policy persists.
- [ ] Restart with absent member is sane.

## 54.7 Stability

- [ ] Hardware churn passes the selected cycle count.
- [ ] Two-device soak meets the actual minimum duration.
- [ ] No crash.
- [ ] No unbounded FD growth.
- [ ] No obvious unbounded RSS growth.
- [ ] Logs rotate/bound correctly.
- [ ] No repeated route recreation storm.

## 54.8 Package

- [ ] Release build passes.
- [ ] DEB builds.
- [ ] DEB contents are correct.
- [ ] Dependency metadata is sensible.
- [ ] Extracted package launches without build tree.
- [ ] Normal runtime does not require root.
- [ ] Legal/public metadata remains truthful.

---

# 55. REQUIRED FINAL RESPONSE FROM THE AI IDE

When all work for this prompt is complete, your final response to the project owner must be structured and concise but evidence-heavy.

Use this shape:

```text
PHASE 9 IMPLEMENTATION / QUALIFICATION COMPLETE

1. Baseline
- Git revision:
- Branch:
- OS/kernel:
- Qt:
- BlueZ:
- PipeWire:

2. Code changes
- <file>: <reason>
...
(or "No core source changes required")

3. Validation artifacts created/updated
- docs/PHASE_9_LINUX_RC1_QUALIFICATION_AUDIT.md
- docs/validation/phase-9-linux-rc.md
- scripts/validation/run-phase9-linux-rc.sh
...

4. Automated tests
- Default CTest: X/X PASS
- Stress: PASS/FAIL
- ASan/UBSan: X/X PASS / caveats

5. Actual hardware
- Device A: PASS/...
- Device B: PASS/...
- Two-device simultaneous connection: PASS/...
- Shared-source fan-out: PASS/...
- System audio: PASS/...

6. Button policy
- Device A ALLOW:
- Device A DISALLOW:
- Device B ALLOW:
- Device B DISALLOW:
- Per-device isolation:
- Persistence/reconnect:

7. Recovery
- Device power cycle:
- PipeWire restart:
- WirePlumber restart:
- Adapter/BlueZ:
- Suspend/resume:

8. Stress/soak
- Hardware churn cycles:
- Soak actual duration:
- Resource observations:

9. Package
- Artifact:
- DEB inspection:
- Extracted launch:
- Normal packaged launch:

10. Exit gates
PHASE 8 SOFTWARE EXIT GATE: ...
PHASE 8 HARDWARE EXIT GATE: ...
PHASE 8 OVERALL: ...
PHASE 9 SOFTWARE/REGRESSION GATE: ...
PHASE 9 LIVE HARDWARE GATE: ...
PHASE 9 BUTTON POLICY GATE: ...
PHASE 9 RECOVERY GATE: ...
PHASE 9 PERSISTENCE/LIFECYCLE GATE: ...
PHASE 9 STRESS/SOAK GATE: ...
PHASE 9 PACKAGING GATE: ...
PHASE 9 LINUX RC1 TECHNICAL EXIT GATE: ...
PUBLIC DISTRIBUTION READINESS: ...

11. Remaining blockers / next phase
- ...
```

Do not say "100% production ready" merely because the technical RC gate passed.

---

# 56. EXPECTED NEXT PHASE AFTER A SUCCESSFUL PHASE 9

Do **not** implement this during Phase 9, but record evidence that will guide it.

A logical next engineering phase is one of:

```text
Phase 10A — 3/4-device scaling and adapter-capability characterization
```

and/or:

```text
Phase 10B — measurable per-device latency / synchronization foundation
```

The choice must be based on Phase 9 evidence.

If 3/4 independent A2DP links are already viable, proceed toward scaling characterization and then synchronization.

If the adapter/BlueZ stack cannot sustain the desired number of links, perform an architecture review before building synchronization features on an invalid transport assumption.

Do not claim synchronization simply because both outputs are audible.

---

# 57. DEFINITION OF DONE

Phase 9 is done when:

1. the exact current known-good repository state was preserved and recorded;
2. the application passes a fresh clean software regression on the actual Linux machine;
3. two actual Bluetooth devices are successfully qualified together;
4. real PipeWire shared-source fan-out is confirmed;
5. real system audio is confirmed through Auralis;
6. the per-device button policy is physically validated wherever the transport exposes a controllable input path;
7. recovery from actual device and audio-service churn is validated;
8. suspend/resume is validated;
9. persistence and shutdown are validated;
10. hardware stress and minimum soak are completed;
11. the release package is independently inspected/launched;
12. every defect fixed has an appropriate regression test;
13. the final full CTest passes again from a fresh final build;
14. the audit document truthfully records PASS/FAIL/PENDING/NOT EXERCISED outcomes;
15. Phase 8 hardware status is updated only if evidence closes it;
16. Linux RC1 technical status is declared only if every mandatory gate passes;
17. public distribution remains blocked if owner license/contact metadata is still unresolved.

The engineering standard for this phase is:

> **Preserve the working Auralis core, attack it with real-world Linux hardware conditions, fix only proven defects, and leave behind reproducible evidence strong enough that the next phase starts from a qualified platform rather than another optimistic prototype.**

---

# 58. START NOW

Begin with repository reconnaissance and baseline capture.

Do not ask for confirmation for ordinary safe inspection/build/test work.

When a test needs:

- real Bluetooth device addresses,
- destructive pairing/forget operations,
- privileged service restart,
- suspend/reboot,

use the values already available on the actual machine where possible and preserve operator control for disruptive actions.

Do not substitute placeholders into shell commands.

Do not skip mandatory tests silently.

Do not reimplement already-working Phase 8 features.

Drive the repository to a truthful, evidence-backed **Phase 9 Linux RC1 technical qualification**.
