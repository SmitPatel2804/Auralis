# Auralis Linux Final Qualification and Release-Readiness Plan

## Document Purpose

This document defines the remaining work required to consider **Auralis fully qualified and release-ready on Linux**.

At the current stage, the major implementation phases are complete and the automated software/package gates have already passed. The remaining Linux work is primarily focused on:

- Real hardware qualification
- Multi-device behavioral validation
- Bluetooth media-button control validation
- Recovery and resilience testing
- Stress and endurance testing
- Fresh-machine/package validation
- Final regression
- Release audit and sign-off

The objective of this document is to convert the remaining Linux work into a precise, auditable qualification process.

---

# 1. Current Linux Status

## 1.1 Completed

The following items are already considered substantially complete:

- Core Linux implementation
- Bluetooth integration architecture
- BlueZ integration
- PipeWire / WirePlumber audio-routing integration
- Multi-device support implementation
- Desktop application implementation
- Automated software tests
- Automated packaging gates
- Two-real-device pairing/connection validation
- Phase 9 software-side qualification

The latest qualification state can be summarized as:

> Automated software and packaging gates: PASS  
> Linux RC1: PENDING mandatory physical hardware/operator qualification

This means Auralis is beyond basic development validation.

The remaining question is no longer:

> "Does Auralis work on Linux?"

The remaining question is:

> "Can Auralis be confidently shipped to real users, demonstrated to hardware manufacturers, and operated reliably under normal and abnormal real-world Bluetooth conditions?"

---

# 2. Definition of Linux Release-Ready

Auralis Linux shall be considered release-ready only when all of the following are true:

1. Automated regression tests pass.
2. Two or more real Bluetooth devices can be operated reliably.
3. Audio routing remains correct during normal use.
4. Failure/recovery scenarios are handled without corrupting system state.
5. Physical Bluetooth media-button ALLOW/DISALLOW behavior works as intended.
6. Long-running operation does not expose instability.
7. Suspend/resume behavior is acceptable.
8. Bluetooth service or adapter interruptions recover correctly.
9. A fresh Linux machine can install and run the packaged build.
10. Uninstalling Auralis does not leave Bluetooth/audio services damaged.
11. Logs and error reporting are usable for diagnosis.
12. A final Linux qualification audit is completed.
13. The release candidate commit/package is frozen and signed off.

---

# 3. Qualification Priorities

## 3.1 Priority Definitions

### P0 — Mandatory Release Gate

Failure blocks Linux release.

### P1 — High Priority

Should pass before release unless a limitation is explicitly accepted and documented.

### P2 — Recommended

Important for product maturity but may not block an initial controlled release.

---

# 4. Remaining Linux Qualification Matrix

| Area | Remaining Check | Priority | Release Blocking |
|---|---|---:|---:|
| Real Bluetooth hardware | Validate with physical headphones/earbuds/speakers | P0 | Yes |
| Dual-device operation | Run two real Bluetooth audio devices simultaneously | P0 | Yes |
| Audio playback | Continuous audio playback to both devices | P0 | Yes |
| Audio routing | Correct PipeWire/WirePlumber routing | P0 | Yes |
| Reconnection | Device power-off/power-on recovery | P0 | Yes |
| App restart | Restart Auralis with devices paired/connected | P0 | Yes |
| System restart | Reboot Linux and validate recovery | P0 | Yes |
| Bluetooth restart | Restart BlueZ or toggle adapter state | P0 | Yes |
| One-device failure | One device fails while the other remains active | P0 | Yes |
| Media-button policy | Validate ALLOW/DISALLOW behavior | P0 | Yes |
| Play/Pause spam protection | Rapid physical button presses must not spam when disabled | P0 | Yes |
| Suspend/resume | Laptop sleep/wake with active devices | P1 | Recommended |
| Bluetooth off/on | Toggle Bluetooth while Auralis runs | P1 | Recommended |
| Adapter removal | Remove/reinsert USB BT adapter if applicable | P1 | Recommended |
| Pair/unpair cycles | Repeated pairing and forgetting | P1 | Recommended |
| Long-duration test | Multi-hour playback | P0 | Yes |
| Rapid reconnect stress | Repeated connect/disconnect loops | P1 | Recommended |
| Application switching | Switch playback between audio applications | P1 | Recommended |
| Device diversity | Different manufacturers/codecs/device types | P0 | Yes |
| Error handling | Useful user-facing errors | P1 | Recommended |
| Logging | Diagnostic quality without uncontrolled spam | P1 | Recommended |
| Packaging | Install release package on clean Linux | P0 | Yes |
| Fresh-machine test | Validate machine without development environment | P0 | Yes |
| Upgrade | Upgrade from prior package where applicable | P2 | No |
| Uninstall | System remains healthy after uninstall | P1 | Recommended |
| Permissions | Normal usage must not unexpectedly require root | P0 | Yes |
| Regression | Full automated suite after final changes | P0 | Yes |
| Audit | Final qualification report | P0 | Yes |
| RC sign-off | Freeze Linux release candidate | P0 | Yes |

---

# 5. Qualification Stage A — Physical Hardware Functional Validation

## 5.1 Objective

Validate Auralis using actual Bluetooth audio devices under normal operating conditions.

Mocks and automated software tests are not sufficient for this stage.

---

## 5.2 Minimum Hardware Setup

Recommended minimum:

- One Linux laptop/desktop running the target supported Linux environment
- One Bluetooth adapter
- Two independent Bluetooth audio devices

Preferred additional hardware:

- Third Bluetooth audio device
- Device from a different manufacturer
- Earbuds/headphones with physical play/pause controls
- Speaker with AVRCP/media controls
- USB Bluetooth adapter, if external-adapter support matters

---

## 5.3 Device Diversity Recommendation

For stronger qualification, use at least:

- Device A — Bluetooth earbuds
- Device B — Bluetooth headphones
- Device C — Bluetooth speaker

Prefer devices from different manufacturers.

If available, include different codec combinations such as:

- SBC
- AAC
- aptX-family variants where supported
- LDAC where supported

Codec qualification shall only be considered complete for codecs actually supported by the Linux Bluetooth/audio stack and test hardware.

---

# 6. Normal-Flow Functional Tests

## 6.1 Launch Test

### Procedure

1. Boot Linux normally.
2. Confirm Bluetooth is enabled.
3. Launch Auralis.
4. Observe application startup.
5. Confirm the Bluetooth adapter is detected.
6. Confirm existing paired devices are represented correctly.

### Pass Criteria

- Application launches successfully.
- No crash.
- No hang.
- Bluetooth subsystem is detected.
- Device state is accurate.
- No unexplained fatal log errors.

---

## 6.2 Device A Pairing

### Procedure

1. Place Device A into pairing mode.
2. Discover it using Auralis.
3. Pair the device.
4. Connect it.
5. Confirm audio profile availability.

### Pass Criteria

- Device is discoverable.
- Pairing completes.
- Connection succeeds.
- UI state matches real device state.
- Audio can be routed to the device.

---

## 6.3 Device B Pairing

Repeat the Device A procedure for Device B.

### Pass Criteria

Same as Device A.

---

# 7. Two-Device Simultaneous Operation

This is one of the most important Linux release gates.

## 7.1 Procedure

1. Launch Auralis.
2. Connect Device A.
3. Connect Device B.
4. Start audio playback.
5. Confirm both devices remain connected.
6. Observe playback behavior.
7. Continue playback for an extended period.
8. Operate Auralis controls normally.

## 7.2 Pass Criteria

- Both devices remain connected.
- Auralis correctly tracks both devices.
- No unexpected disconnect loop.
- Audio routing behaves according to intended Auralis architecture.
- No uncontrolled CPU/memory growth.
- No repeated BlueZ/PipeWire error spam.
- UI stays responsive.
- No deadlocks.
- No fatal service interaction.

---

# 8. Audio Routing Qualification

## 8.1 Tests

Validate:

- Auralis launch with no devices
- Auralis launch with Device A already connected
- Auralis launch with both devices already connected
- Connect Device A after application launch
- Connect Device B during playback
- Disconnect Device A during playback
- Disconnect Device B during playback
- Reconnect Device A
- Reconnect Device B

---

## 8.2 Pass Criteria

After every topology change:

- PipeWire graph remains healthy.
- Audio routing updates correctly.
- No stale routes remain.
- Auralis internal device state matches system state.
- No permanent routing failure occurs.
- Remaining active device continues operating where expected.

---

# 9. One-Device Failure Isolation

A failure of one Bluetooth device must not unnecessarily destroy the session for another device.

## 9.1 Procedure

1. Connect Device A.
2. Connect Device B.
3. Begin playback.
4. Power off Device A abruptly.
5. Observe Device B.
6. Restore Device A.
7. Repeat with Device B.

## 9.2 Pass Criteria

- Remaining device continues operating where technically possible.
- Auralis identifies which device failed.
- No application crash.
- Failed device can be reconnected.
- Reconnection does not require restarting the whole machine.
- State transitions are visible and correct.

---

# 10. Bluetooth Media-Button ALLOW / DISALLOW Qualification

This qualification is mandatory because Auralis is intended to control whether physical Bluetooth media controls are allowed to affect playback.

The goal is to prevent accidental button use from repeatedly issuing play/pause commands.

---

# 11. Media-Button Functional Matrix

| State | User Action | Expected Result |
|---|---|---|
| ALLOWED | Single Play/Pause press | Playback toggles once |
| ALLOWED | Multiple intentional presses | Commands are accepted |
| DISALLOWED | Single Play/Pause press | Command is ignored |
| DISALLOWED | 20 rapid presses | All playback actions are suppressed |
| DISALLOWED | Device A sends command | Ignored |
| DISALLOWED | Device B sends command | Ignored |
| DISALLOWED | Both devices send commands | Ignored |
| Re-enabled | Single press | Function works again |
| Device reconnects | Button policy reapplied | Correct policy remains active |
| Auralis restarts | Policy behavior follows product design | Correct state |
| System reboots | Policy behavior follows product design | Correct state |

---

# 12. Play/Pause Spam Test

## 12.1 Procedure

1. Connect a Bluetooth device with physical media controls.
2. Set media-button policy to DISALLOWED.
3. Start audio playback.
4. Rapidly press Play/Pause at least 20 times.
5. Repeat using Next/Previous where applicable.
6. Observe player state.
7. Observe Auralis logs.
8. Repeat with Device B.
9. Repeat with both devices connected.

## 12.2 Pass Criteria

When physical media controls are DISALLOWED:

- No unintended Play/Pause event reaches the controlled playback path.
- No repeated toggle spam occurs.
- No queue buildup causes delayed media commands.
- No burst of commands is released when controls are re-enabled.
- UI remains responsive.
- Logs may record suppressed commands but must not become unusably noisy.

---

# 13. Policy Re-Enable Test

## Procedure

1. Start with media controls DISALLOWED.
2. Confirm physical button presses are ignored.
3. Switch policy to ALLOWED.
4. Press Play/Pause once.

## Pass Criteria

- The first legitimate command after re-enabling is accepted normally.
- No previously suppressed commands are replayed.
- No stale queued events appear.

---

# 14. Application Restart Qualification

## Procedure

1. Pair both devices.
2. Connect both devices.
3. Begin playback.
4. Close Auralis normally.
5. Relaunch Auralis.
6. Observe device detection/state.
7. Verify media-button policy state.
8. Resume normal operation.

## Pass Criteria

- No corrupted state.
- Devices are rediscovered/re-associated correctly.
- The application does not require re-pairing.
- Persisted settings follow the intended product specification.
- Audio routing returns to a valid state.

---

# 15. Application Crash/Forced-Termination Recovery

Where safe and appropriate:

1. Run Auralis with devices connected.
2. Force-terminate the Auralis process.
3. Confirm BlueZ/PipeWire remain healthy.
4. Relaunch Auralis.
5. Restore operation.

## Pass Criteria

- No system service remains permanently corrupted.
- Restarting Auralis restores operation.
- Devices do not become permanently inaccessible.
- No manual OS repair is required.

---

# 16. System Reboot Qualification

## Procedure

1. Pair Device A and Device B.
2. Use Auralis successfully.
3. Reboot Linux.
4. Log in.
5. Launch Auralis.
6. Observe paired device state.
7. Reconnect devices.
8. Resume playback.

## Pass Criteria

- Pairing records remain valid.
- Auralis launches correctly.
- No stale PID/socket/state artifact prevents startup.
- Bluetooth devices reconnect normally.
- Audio routing is restored.

---

# 17. Bluetooth OFF/ON Recovery

## Procedure

1. Run Auralis.
2. Connect both devices.
3. Turn Linux Bluetooth OFF.
4. Observe Auralis.
5. Turn Bluetooth ON.
6. Reconnect devices.

## Pass Criteria

- Auralis does not crash.
- Adapter unavailable state is represented correctly.
- Re-enabled Bluetooth is detected.
- Devices can reconnect.
- Restarting Auralis should not be mandatory.

---

# 18. BlueZ Restart Recovery

Use only on a development/qualification machine.

Example:

```bash
sudo systemctl restart bluetooth
```

## Procedure

1. Connect devices.
2. Start playback.
3. Restart Bluetooth service.
4. Observe state transitions.
5. Allow BlueZ to return.
6. Attempt reconnection.

## Pass Criteria

- Application survives BlueZ disappearance/reappearance.
- D-Bus objects are rebuilt correctly.
- Old object references are not reused incorrectly.
- Reconnection succeeds.
- No permanent application restart requirement unless explicitly accepted by design.

---

# 19. PipeWire Recovery Test

Use only if safe for the qualification machine.

Possible commands depend on the target distribution and user-session setup.

Validate recovery when:

- PipeWire restarts
- WirePlumber restarts
- Audio graph changes unexpectedly

## Pass Criteria

- Auralis does not crash.
- Audio subsystem restoration is detected.
- Routes can be rebuilt.
- The user receives useful status if recovery requires action.

---

# 20. Suspend / Resume Qualification

## Procedure

1. Connect Device A and Device B.
2. Begin audio playback.
3. Suspend laptop.
4. Wait long enough for Bluetooth connections to drop naturally.
5. Resume laptop.
6. Observe Auralis.
7. Attempt reconnection.
8. Resume playback.

## Pass Criteria

- No application crash.
- Adapter state recovers.
- Device state updates correctly.
- Devices can reconnect.
- Audio routing returns to a usable state.
- Media-button policy remains correct.

---

# 21. Repeated Pair / Unpair Stress

## Recommended Cycle Count

Minimum:

- 10 cycles per primary test device

Preferred:

- 20 or more cycles across multiple devices

## Procedure

For each cycle:

1. Forget device.
2. Put device in pairing mode.
3. Discover.
4. Pair.
5. Connect.
6. Play audio.
7. Disconnect.
8. Remove pairing.
9. Repeat.

## Pass Criteria

- No progressive slowdown.
- No duplicate device entries.
- No stale D-Bus objects.
- No corrupted pairing state.
- No need to reboot after repeated cycles.

---

# 22. Rapid Connect / Disconnect Stress

## Procedure

Repeat:

1. Connect Device A.
2. Disconnect Device A.
3. Reconnect Device A.
4. Repeat with Device B.
5. Repeat while both devices are present.

Recommended:

- 20–50 cycles

## Pass Criteria

- No crash.
- No deadlock.
- No stuck "connecting" state.
- No persistent ghost connection.
- Device state eventually converges with actual BlueZ state.

---

# 23. Long-Duration Endurance Test

This is a mandatory release gate.

## Minimum Recommended Duration

4 hours

## Preferred Duration

8–12 hours

## Strong Qualification

24 hours

---

## 23.1 Procedure

1. Connect two real Bluetooth devices.
2. Begin continuous audio.
3. Keep Auralis running.
4. Periodically inspect:
   - CPU
   - Memory
   - logs
   - device state
   - audio continuity
5. Trigger occasional normal user actions.
6. If media-button control is part of the active configuration, test it periodically.

---

## 23.2 Pass Criteria

- No crash.
- No runaway memory growth.
- No runaway CPU.
- No progressive audio degradation.
- No uncontrolled reconnect loop.
- No repeated command spam.
- UI remains responsive.
- Logs remain manageable.

---

# 24. Audio Application Switching

Validate switching among several Linux applications.

Examples:

- Web browser
- Local media player
- Streaming client
- System notification sound
- Video playback application

## Pass Criteria

- Auralis routing remains stable.
- New streams behave according to the intended routing model.
- Existing Bluetooth device state is unaffected.
- App switching does not corrupt the audio graph.

---

# 25. Error Reporting Qualification

Auralis must handle expected failures cleanly.

Test:

- Pairing failure
- Connection timeout
- Bluetooth disabled
- Adapter absent
- Device powered off
- Device out of range
- PipeWire unavailable
- BlueZ unavailable
- Invalid or stale device state
- Unsupported device/profile

## Pass Criteria

Error messages should:

- Explain what failed.
- Avoid misleading success states.
- Avoid exposing raw internal implementation details unless in diagnostics.
- Suggest recovery where possible.
- Not overwhelm the user with repeated identical messages.

---

# 26. Logging Qualification

Logs must support post-failure diagnosis.

## Logs should include

- Application start/version
- Bluetooth adapter discovery
- Device discovered
- Pairing request/result
- Connection request/result
- Profile state changes
- Audio-routing decisions
- Media-button policy changes
- Suppressed media commands where useful
- Recovery attempts
- Service disconnect/reconnect
- Fatal and non-fatal errors

## Logs must avoid

- Infinite repeated identical lines
- Excessive polling spam
- Sensitive data exposure
- Unbounded growth
- Logs so noisy that meaningful events cannot be found

---

# 27. Permission Qualification

Normal user operation should not unexpectedly require root privileges.

Validate:

- Launching Auralis
- Discovering devices
- Pairing
- Connecting
- Audio routing
- Media-button control
- Reconnecting

## Pass Criteria

Routine use operates under the normal desktop user.

Administrative privileges should only be required for installation or explicitly administrative qualification actions.

---

# 28. Fresh-Machine Package Qualification

This is one of the strongest release-readiness checks.

The package must be tested on a Linux machine or VM that:

- Does not contain the Auralis source tree
- Does not contain the build directory
- Does not depend on the developer environment
- Does not contain manually installed hidden dependencies unless listed as product prerequisites

---

# 29. Fresh-Machine Test Procedure

1. Prepare clean supported Linux system.
2. Update system normally.
3. Install required documented runtime prerequisites only.
4. Install the Auralis release package.
5. Launch Auralis.
6. Pair Device A.
7. Pair Device B.
8. Connect both.
9. Play audio.
10. Test media-button ALLOW/DISALLOW.
11. Restart Auralis.
12. Reboot Linux.
13. Re-test.
14. Uninstall Auralis.

---

# 30. Fresh-Machine Pass Criteria

- Package installs successfully.
- No undocumented development package is required.
- No source-tree path is assumed.
- No build-tree file is required.
- Application launcher/desktop entry works where applicable.
- Runtime dependencies resolve correctly.
- Bluetooth works.
- Audio routing works.
- Two-device operation works.
- Media-button policy works.
- Restart works.
- Reboot works.

---

# 31. Uninstall Qualification

After uninstalling Auralis:

- Bluetooth service must remain functional.
- PipeWire must remain functional.
- WirePlumber must remain functional.
- Previously paired Bluetooth devices should remain usable by Linux unless product design intentionally removes pairing.
- No broken system configuration should remain.
- No system-wide service should remain in a failed state.

Residual user configuration files may remain only if deliberately designed and documented.

---

# 32. Upgrade Qualification

If Auralis will support packaged upgrades:

1. Install an older supported build.
2. Configure/pair devices.
3. Install the new build over it.
4. Launch Auralis.
5. Verify settings/state migration.
6. Validate Bluetooth/audio operation.

## Pass Criteria

- Upgrade succeeds.
- Application launches.
- No incompatible old state causes failure.
- User settings are preserved where intended.
- Migrations are deterministic.

---

# 33. Automated Regression Gate

After every final correction made during physical qualification, rerun the complete automated suite.

Example structure:

```bash
rm -rf build

cmake -S . -B build -G Ninja
cmake --build build

ctest --test-dir build --output-on-failure
```

Use the actual project-required configuration and options where they differ.

## Release Rule

No final release candidate shall be signed off with a failing automated test unless the failure is:

1. Fully understood,
2. Proven unrelated,
3. Explicitly waived,
4. Recorded in the qualification audit.

Preferably, no waivers should exist for RC1.

---

# 34. Release Candidate Freeze

Once all mandatory qualification gates pass:

1. Stop adding features.
2. Fix only release-blocking defects.
3. Rerun regression after every fix.
4. Record final Git commit.
5. Produce release package from that exact commit.
6. Retest the exact package.
7. Tag the release candidate.

Suggested naming:

```text
Auralis Linux RC1
```

Possible Git tag:

```text
v1.0.0-linux-rc1
```

Final tag naming should follow the project's chosen versioning scheme.

---

# 35. Final Evidence Collection

Every physical qualification run should record evidence.

Recommended fields:

```text
Test ID:
Date:
Tester:
Machine:
Linux distribution:
Kernel:
Qt version:
BlueZ version:
PipeWire version:
WirePlumber version:
Bluetooth adapter:
Device A:
Device B:
Device C:
Auralis commit:
Auralis package version:
Test result:
Observed behavior:
Logs:
Screenshots:
Known limitations:
Notes:
```

---

# 36. Recommended Test Status Values

Use only:

- PASS
- FAIL
- BLOCKED
- NOT RUN
- WAIVED

Avoid ambiguous labels such as:

- Mostly works
- Seems fine
- Probably okay
- Partial success

Release qualification should remain evidence-based.

---

# 37. Defect Severity

## Severity 0 — Release Blocker

Examples:

- Application crash during ordinary use
- Two-device operation fundamentally broken
- Audio routing unusable
- DISALLOW policy still allows repeated accidental Play/Pause commands
- Package cannot run on clean machine

## Severity 1 — Critical

Examples:

- Frequent unrecoverable disconnect
- Bluetooth service recovery fails
- Suspend/resume permanently breaks session
- Device state becomes badly inconsistent

## Severity 2 — Major

Examples:

- Recoverable UI state bug
- Poor error messaging
- Occasional reconnection requires manual action

## Severity 3 — Minor

Examples:

- Cosmetic state delay
- Non-critical log wording
- Minor UI alignment issue

---

# 38. Release Blocking Rule

Linux RC1 should not be signed off with:

- Any Severity 0 defect
- Any unresolved mandatory qualification failure
- Any media-button safety/control failure
- Any clean-install/package failure
- Any unexplained dual-device instability

Severity 1 issues should normally also be fixed before release.

---

# 39. Recommended Final Qualification Sequence

Run the remaining Linux work in this exact order.

## Stage A — Physical Functional Qualification

1. Device discovery
2. Pairing
3. One-device playback
4. Two-device operation
5. Audio routing
6. Disconnect/reconnect
7. App restart
8. System reboot

Exit condition:

> Core physical behavior PASS

---

## Stage B — Media-Button Qualification

1. ALLOW
2. DISALLOW
3. Rapid Play/Pause spam
4. Device A policy
5. Device B policy
6. Both-device policy
7. Re-enable
8. Reconnect
9. App restart
10. Reboot

Exit condition:

> Physical media controls PASS

---

## Stage C — Failure and Recovery Qualification

1. Device power-off
2. Out-of-range simulation
3. Bluetooth OFF/ON
4. BlueZ restart
5. PipeWire/WirePlumber recovery
6. Auralis forced termination
7. Suspend/resume
8. Adapter removal/reinsert where applicable

Exit condition:

> Recovery behavior PASS

---

## Stage D — Stress and Endurance

1. Pair/unpair loops
2. Connect/disconnect loops
3. Repeated media-button events
4. Multi-hour dual-device playback
5. Application switching
6. Resource observation

Exit condition:

> Stability PASS

---

## Stage E — Package / Clean-System Qualification

1. Fresh Linux install
2. Package install
3. First launch
4. Pair devices
5. Dual-device operation
6. Media-button policy
7. App restart
8. OS reboot
9. Uninstall

Exit condition:

> Distribution package PASS

---

## Stage F — Final Regression

Run:

- Build
- Unit tests
- Integration tests
- Packaging tests
- Any project-specific validation scripts

Exit condition:

> Automated suite PASS

---

## Stage G — Release Audit

Compile:

- Environment
- Commit
- Package
- Test results
- Hardware matrix
- Known limitations
- Waivers
- Logs
- Screenshots
- Final verdict

Exit condition:

> Qualification evidence complete

---

## Stage H — RC1 Sign-Off

Final conditions:

```text
Automated Tests              PASS
Physical Qualification       PASS
Dual-Device Qualification    PASS
Audio Routing                PASS
Media-Button Qualification   PASS
Spam Suppression             PASS
Recovery Qualification       PASS
Stress / Endurance           PASS
Fresh-Machine Install        PASS
Package Qualification        PASS
Final Regression             PASS
Final Audit                  COMPLETE
```

Then:

```text
AURALIS LINUX RC1 — QUALIFIED
```

---

# 40. Linux Completion Checklist

## Core Hardware

- [ ] Device A pairing validated
- [ ] Device B pairing validated
- [ ] Device A playback validated
- [ ] Device B playback validated
- [ ] Simultaneous two-device operation validated
- [ ] Audio routing validated
- [ ] Device diversity tested

## Reconnection

- [ ] Device A power cycle
- [ ] Device B power cycle
- [ ] One-device failure isolation
- [ ] Both-device reconnect
- [ ] App restart
- [ ] Linux reboot
- [ ] Bluetooth OFF/ON
- [ ] BlueZ restart

## Media Controls

- [ ] ALLOW state validated
- [ ] DISALLOW state validated
- [ ] Single Play/Pause suppressed
- [ ] Rapid Play/Pause spam suppressed
- [ ] Device A buttons tested
- [ ] Device B buttons tested
- [ ] Both devices tested
- [ ] Re-enable validated
- [ ] Reconnect policy validated
- [ ] Restart policy validated
- [ ] No queued command burst

## Recovery

- [ ] PipeWire recovery
- [ ] WirePlumber recovery
- [ ] Auralis forced termination recovery
- [ ] Suspend/resume
- [ ] Adapter removal/reinsert if applicable

## Stress

- [ ] Repeated pair/unpair
- [ ] Repeated connect/disconnect
- [ ] Multi-hour playback
- [ ] CPU behavior acceptable
- [ ] Memory behavior acceptable
- [ ] Logs acceptable
- [ ] No runaway reconnection
- [ ] No UI degradation

## Packaging

- [ ] Fresh supported Linux system prepared
- [ ] Release package installs
- [ ] Application launches without source tree
- [ ] Runtime dependencies correct
- [ ] Dual-device operation works
- [ ] Media-button policy works
- [ ] Restart works
- [ ] Reboot works
- [ ] Uninstall works
- [ ] System Bluetooth/audio remains healthy after uninstall

## Final Release

- [ ] Full automated regression PASS
- [ ] No Severity 0 defects
- [ ] No unresolved mandatory failures
- [ ] Qualification evidence archived
- [ ] Final commit recorded
- [ ] Release package produced from recorded commit
- [ ] Exact package retested
- [ ] Linux RC tag created
- [ ] Final audit completed
- [ ] Linux RC1 signed off

---

# 41. Final Release Decision

Use the following decision logic.

## QUALIFIED

Auralis Linux can be declared qualified when:

- All P0 gates pass.
- No release-blocking defect remains.
- Physical dual-device behavior is proven.
- Media-button ALLOW/DISALLOW behavior is proven on real devices.
- Stress/endurance testing is acceptable.
- Fresh-machine package testing passes.
- Final regression passes.
- Audit evidence is complete.

---

## NOT QUALIFIED

Do not sign off Linux RC1 if any of the following remains:

- Unexplained crash
- Unstable two-device operation
- Broken audio routing
- Incorrect recovery after ordinary disconnects
- Physical button DISALLOW policy leaks Play/Pause commands
- Rapid button spam changes playback while blocked
- Fresh-machine package cannot run
- Unexplained root requirement
- Persistent BlueZ/PipeWire corruption
- Mandatory test not executed

---

# 42. Post-Linux Transition

After Linux qualification:

1. Freeze Linux feature development.
2. Enter maintenance mode for Linux.
3. Preserve the Linux qualification environment.
4. Archive the exact release build and audit.
5. Use Linux as the known-good behavioral reference.
6. Begin Windows implementation/porting.
7. Reuse platform-neutral architecture wherever possible.
8. Re-implement only the OS-specific Bluetooth/audio layers required by Windows.
9. Later extend the same product model to Android.

The recommended platform sequence is therefore:

```text
LINUX
  |
  +--> Complete physical qualification
  +--> Complete stress/recovery qualification
  +--> Complete package qualification
  +--> Freeze Linux RC
  |
  v
WINDOWS
  |
  +--> Implement Windows-specific Bluetooth/audio layer
  +--> Qualify against Linux behavior
  |
  v
ANDROID
```

---

# 43. Final Milestone Definition

The Linux milestone is complete only when the project can state:

> Auralis has passed automated, physical-hardware, multi-device, media-control, recovery, stress, packaging, and release-candidate qualification on the supported Linux environment.

At that point:

```text
AURALIS LINUX — READY
```

and the project can confidently move its primary engineering focus to Windows while maintaining Linux as the first qualified reference platform.

---

# 44. Recommended Immediate Next Action

The next engineering action should be:

> Execute the complete Physical Hardware Functional Qualification and Bluetooth Media-Button Qualification on the actual Linux machine using two real Bluetooth devices.

The first qualification run should capture:

- Full environment information
- Device names/models
- Bluetooth adapter information
- Exact Auralis Git commit
- Exact build/package
- Logs
- Test-by-test PASS/FAIL status
- Any abnormal observations

Any discovered defects should be corrected before proceeding to the long-duration stress and fresh-machine release qualification stages.

---

# 45. Final Target

The final target is not merely:

```text
Auralis works on Linux.
```

The target is:

```text
Auralis Linux has been physically tested,
stress-tested,
recovery-tested,
package-tested,
and release-qualified
on real Bluetooth hardware.
```

Only after that should Linux be considered finished for the v1 release cycle.
