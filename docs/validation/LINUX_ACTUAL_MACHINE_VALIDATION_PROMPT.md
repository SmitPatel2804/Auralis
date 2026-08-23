# Auralis — Actual Linux Machine Validation and Remediation Prompt

Copy this entire document into Codex or another capable AI coding IDE running **inside the Auralis repository on a real Linux desktop**. The agent must execute the tests, fix reproducible defects within scope, rerun affected tests, and leave a dated evidence report in the repository.

---

## Role and mission

You are validating Auralis on an actual Linux desktop with real PipeWire, WirePlumber, BlueZ, Qt, audio applications, and Bluetooth hardware. Work from the repository root.

Your mission is to:

1. Audit the repository and the Linux host before changing anything.
2. Build Auralis from a clean, separate build directory.
3. Run every relevant automated test and inspect every skip or failure.
4. Validate the packaged and unpackaged application against real Linux services and hardware.
5. Exercise every user-visible feature and important sub-feature, including negative and recovery paths.
6. Fix defects that can be safely reproduced and are within the repository's scope.
7. Rerun all affected tests after every fix, then rerun the complete suite.
8. Write a precise, evidence-backed report under `docs/validation/`.

Do not claim success merely because the project compiles or because a test is skipped. Distinguish explicitly between:

- **PASS** — exercised and verified with evidence.
- **FAIL** — exercised and did not meet the expected behavior.
- **BLOCKED** — could not be exercised because a named dependency, permission, device, or user action was unavailable.
- **NOT APPLICABLE** — demonstrably irrelevant on this host.

A skipped test is never automatically a pass. Investigate why it skipped.

## Safety and authorization rules

- Preserve all pre-existing user changes. Start with `git status --short` and never discard, overwrite, reset, or clean unrelated work.
- Do not use `git reset --hard`, `git clean`, broad deletion, or destructive filesystem commands.
- Use a new build directory such as `build-linux-validation`; do not delete an existing user build.
- Read any `AGENTS.md`, repository instructions, and relevant documentation before editing.
- Do not run the GUI as root.
- Ask for explicit approval before installing packages, changing system configuration, restarting user/system services, suspending the machine, modifying udev rules, or installing a generated package.
- Prefer user-scoped configuration and reversible operations.
- Do not weaken Linux security, add broad input-device permissions, or grant access to all `/dev/input/event*` devices merely to make a test pass.
- Never conceal a limitation with a successful-looking UI state. Unsupported or permission-denied behavior must be reported honestly.
- Do not connect, disconnect, allow, or disallow hardware without telling the user immediately before the action. The user has authorized rigorous Auralis testing, but potentially disruptive system-wide actions still need a clear warning.
- Avoid testing at unsafe listening volumes. Begin muted or very low and ask the user before audible playback.
- Do not commit, push, publish, or open a pull request unless the user explicitly requests it.

## Resource guardrails

This validation must remain usable on a constrained desktop while the AI IDE is
also running:

- Record `free -h`, `df -h .`, and `ulimit -a` before building.
- Never run builds, tests, sanitizers, packaging, or the Auralis GUI concurrently.
- Use one build job and one test job by default. Increase the build to two jobs
  only when the machine has clear memory headroom; never derive concurrency from
  `nproc`.
- Prefer `bash scripts/validation/run-linux-low-resource.sh` for the first
  deterministic pass. The runner deliberately rejects more than two build jobs.
- Do not start the sanitizer phase automatically. Ask first and run it only when
  the release suite is clean and the host has enough free RAM and swap.
- Watch available memory during long live/endurance checks. Stop the active test
  cleanly if the desktop begins swapping continuously, available memory falls
  below 1 GiB, or the UI becomes unresponsive. Report the resource boundary; do
  not launch concurrent retries.
- Keep raw logs bounded and do not load an entire large log into the IDE/chat.
  Use `tail`, `rg`, or another streaming filter and retain only relevant excerpts.

## Expected Linux implementation under test

Confirm these expectations against the current code rather than blindly assuming them:

- Auralis is a Qt 6 desktop application built with CMake and C++.
- Linux audio integration uses PipeWire/WirePlumber.
- The runtime fallback can create an Auralis virtual output and a corresponding monitor/source.
- A persistent PipeWire configuration is packaged from `data/pipewire/90-auralis-virtual-output.conf`.
- Expected user-facing names include **Auralis Virtual Output** and **Auralis System Audio**.
- Linux Bluetooth behavior uses BlueZ and must continue discovering devices even when one is already connected through the OS.
- Classic Bluetooth audio devices and BLE-only devices must not flood the same primary device list.
- Sessions should show running application/audio streams, such as browsers and media apps, rather than physical sinks such as built-in speakers or Bluetooth headphones.
- ALLOW/DISALLOW represents device input-event policy and must be honestly wired to supported Linux behavior.
- Execution logging should be enabled by default and written to a timestamped log file.
- Auralis must not silently steal the OS default output unless the user explicitly chooses the Auralis virtual output.

Relevant starting points may include:

- `CMakeLists.txt`
- `src/`
- `tests/`
- `scripts/ci/`
- `data/pipewire/90-auralis-virtual-output.conf`
- `.github/workflows/`
- `docs/`

Use `rg`/`rg --files` to locate renamed or additional files.

## Phase 1 — Repository and host audit

Capture the output of these read-only checks in the final report. Adjust commands only when the distribution requires an equivalent.

```bash
pwd
git status --short
git branch --show-current
git rev-parse HEAD
find .. -name AGENTS.md -print
uname -a
cat /etc/os-release
cmake --version
ninja --version
g++ --version
qmake6 --version 2>/dev/null || true
qtpaths6 --qt-version 2>/dev/null || true
pkg-config --modversion Qt6Core 2>/dev/null || true
pkg-config --modversion libpipewire-0.3 2>/dev/null || true
pipewire --version 2>/dev/null || true
wireplumber --version 2>/dev/null || true
bluetoothctl --version 2>/dev/null || true
systemctl --user --no-pager --full status pipewire.service pipewire-pulse.service wireplumber.service 2>&1 || true
loginctl show-session "${XDG_SESSION_ID:-self}" 2>/dev/null || true
```

Audit dependencies declared by the project and compare them with the host. Confirm the actual minimum versions in CMake rather than relying on this prompt. Pay particular attention to Qt 6.8 or the repository's declared minimum, PipeWire 0.3.60 or the declared minimum, development headers, DBus, BlueZ, and packaging tools.

If dependencies are missing, list the exact distro-native package names and ask before installing them. If installation is declined, continue all work that does not require those dependencies and mark only the relevant checks blocked.

Inspect the current worktree before editing. Treat every pre-existing modification as user-owned unless clearly created during this run.

## Phase 2 — Clean configure and build

On a constrained host, run the bounded script below and treat its configure,
build, and deterministic test as Phases 2 and 3. Reuse
`build/linux-low-resource`; do not create a second equivalent validation build.

```bash
AURALIS_BUILD_JOBS=1 bash scripts/validation/run-linux-low-resource.sh
```

The following commands are the manual alternative when the preset cannot be
used. Use an independent build directory and prefer Ninja. Do not run both
paths merely to duplicate the same coverage.

```bash
cmake -S . -B build-linux-validation -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAURALIS_BUILD_TESTS=ON
cmake --build build-linux-validation --parallel 1
```

For all later commands, substitute `build/linux-low-resource` for
`build-linux-validation` when the bounded runner was selected.

Requirements:

- Record configure warnings and disabled features.
- Treat unexpected missing optional integrations as findings, not harmless noise.
- Confirm that the desktop executable and all test executables were produced.
- Run `ldd` on the desktop executable and report every `not found` dependency.
- Check the resulting binary architecture with `file`.
- Do not solve warnings by globally disabling them.

Example inspection:

```bash
file build-linux-validation/bin/auralis-desktop 2>/dev/null || \
  file build-linux-validation/auralis-desktop
ldd build-linux-validation/bin/auralis-desktop 2>/dev/null || \
  ldd build-linux-validation/auralis-desktop
```

Locate the executable if its path differs.

## Phase 3 — Complete deterministic test suite

First list tests, then run all tests with failure output:

```bash
ctest --test-dir build-linux-validation -N
ctest --test-dir build-linux-validation \
  --output-on-failure \
  --no-tests=error \
  --timeout 180 \
  -j1
```

Then run the Linux-specific and integration-related tests serially and verbosely. Derive exact names from `ctest -N`; do not invent missing tests.

```bash
ctest --test-dir build-linux-validation -V -j1 \
  -R 'Linux|PipeWire|BlueZ|Bluetooth|Audio|Session|Device|Log|Package|Virtual'
```

Also inspect CTest logs:

```bash
sed -n '1,240p' build-linux-validation/Testing/Temporary/LastTest.log 2>/dev/null || true
rg -n 'SKIP|QSKIP|FAIL|FAILED|WARNING|not found|unsupported' \
  build-linux-validation/Testing tests src CMakeLists.txt
```

For every skip, state:

1. The test name.
2. The skip condition.
3. Whether the real Linux host should satisfy it.
4. What was done to exercise the skipped path manually or through a corrected test.

Run any repository-provided lint, static analysis, and format-check targets. Do not run auto-format across unrelated user code.

## Phase 4 — Isolated real PipeWire integration

Inspect `scripts/ci/run-pipewire-virtual-output.sh` completely before executing it. Run its supported modes against a clean, isolated PipeWire environment:

```bash
bash scripts/ci/run-pipewire-virtual-output.sh runtime
bash scripts/ci/run-pipewire-virtual-output.sh persistent
```

The helper may currently assume a build directory named `build`. Do not overwrite or delete an existing user build to satisfy that assumption. Either safely improve the helper to accept a build-directory argument, or create a separate, non-destructive build at the expected path only if it does not collide with user data.

For each mode verify with `pw-cli`, `pw-dump`, `wpctl`, or `pactl` as appropriate:

- Exactly one expected virtual sink/output appears.
- The expected monitor/source appears.
- Node names and descriptions are stable and user-friendly.
- Creating the node is idempotent.
- Re-running activation does not create duplicates.
- Teardown removes only runtime-created objects.
- The persistent configuration loads in a clean temporary environment.
- A malformed or unavailable PipeWire environment fails clearly without crashing or hanging.

Record the exact node IDs/properties and relevant commands. A mocked unit test does not replace this phase.

## Phase 5 — Release packaging and extracted-package smoke test

Configure and build a clean Release package using the repository's intended packaging flow:

```bash
cmake -S . -B build-linux-package -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DAURALIS_BUILD_TESTS=OFF
cmake --build build-linux-package --parallel 1
cpack --config build-linux-package/CPackConfig.cmake -G DEB
cpack --config build-linux-package/CPackConfig.cmake -G TGZ
```

Locate the generated package files rather than assuming their names or output directory. Validate:

```bash
find . -maxdepth 3 -type f \( -name '*.deb' -o -name '*.tar.gz' -o -name '*.tgz' \) -printf '%p\n'
dpkg-deb --info path/to/package.deb
dpkg-deb --contents path/to/package.deb
```

Extract into a new temporary directory and launch from the extracted tree without installing system-wide:

```bash
validation_tmp="$(mktemp -d)"
dpkg-deb -x path/to/package.deb "$validation_tmp/deb-root"
find "$validation_tmp/deb-root" -maxdepth 6 -type f -print
```

Verify:

- Desktop executable launches from the staged/extracted artifact.
- Qt and QML dependencies are present or correctly declared, according to the packaging strategy.
- No library lookup points back into the build tree.
- The PipeWire configuration is at the intended packaged path.
- Desktop entry, icon, metadata, licenses, and runtime dependencies are correct.
- Package architecture matches the machine/build target.
- The application does not require running as root.
- An offline machine would not require unlisted dependencies.

Use `lintian` if installed. Missing `lintian` alone should not trigger an install without approval.

## Phase 6 — Live desktop and virtual-output validation

Run the unpackaged executable first from a terminal so stdout/stderr and the timestamped application log can be correlated. Then repeat critical checks using the extracted package.

Before launch, capture baseline audio state:

```bash
wpctl status
pactl info 2>/dev/null || true
pactl list short sinks 2>/dev/null || true
pactl list short sources 2>/dev/null || true
```

Validate the runtime fallback:

1. Start Auralis with normal user privileges.
2. Confirm the UI becomes responsive and no crash loop occurs.
3. Confirm **Auralis Virtual Output** and **Auralis System Audio** are detected or created as designed.
4. Confirm a second refresh/start does not create duplicate nodes.
5. Confirm Auralis does not silently change the system default output.
6. Confirm closing Auralis leaves or removes runtime nodes according to the documented lifecycle, with no leaked duplicate objects.
7. Restart Auralis and confirm state is rediscovered without requiring a manual UI refresh.

For persistent package behavior, installation and user-service restart require approval. If approved:

1. Capture the current package/service/config state and establish a rollback path.
2. Install only the locally built package by the normal distro mechanism.
3. Restart only the necessary **user** PipeWire/WirePlumber services.
4. Confirm the virtual endpoint appears even before Auralis launches, if that is the promised persistent behavior.
5. Launch and close Auralis repeatedly; confirm no duplicates.
6. Confirm the endpoint remains registered after Auralis closes, where documented.
7. Reboot validation may be performed only with explicit user coordination; otherwise mark it blocked, not passed.

## Phase 7 — Real BlueZ discovery and device management

Use the user's available Bluetooth devices. First record baseline state without changing it:

```bash
bluetoothctl show
bluetoothctl devices
bluetoothctl devices Connected 2>/dev/null || true
bluetoothctl devices Paired 2>/dev/null || true
```

Test these cases in the GUI and correlate them with BlueZ state and logs:

1. Launch while a Bluetooth audio device is already connected through the OS. It must appear promptly without requiring a manual refresh.
2. While that device remains connected, scan for additional devices. Discovery must continue and newly found devices must appear.
3. Verify the existing connected device is not duplicated during scan updates.
4. Verify classic Bluetooth audio devices are in the primary device list.
5. Verify BLE-only/peripheral devices appear in the separate BLE list and do not flood the audio-device list.
6. Verify repeated scan/stop/scan cycles do not leak entries, freeze the UI, or lose the connected device.
7. Verify connect, app-managed disconnect, and **Manage in OS** behavior with clear ownership and status feedback.
8. Verify an OS-connected device can be managed from the app only when that operation is actually supported and intended.
9. Verify BlueZ powered-off, adapter-missing, permission-denied, and device-out-of-range states produce actionable UI/errors rather than stale success.
10. Verify device status updates arrive without forcing a full page refresh.

Do not disconnect the user's active audio device until immediately warning them. Reconnect it and verify recovery afterward.

## Phase 8 — Real system-audio routing

Use at least two ordinary audio-producing applications when possible, for example a browser and a local media player. Start at low volume.

Validate:

1. Running applications/streams appear in Auralis Sessions by application/source identity.
2. Physical outputs such as built-in audio and Bluetooth devices do **not** appear as session applications.
3. New streams started after Auralis launches appear without restarting the app.
4. Closed streams disappear or become inactive without leaving confusing duplicates.
5. Multiple browser tabs/processes are grouped or labeled consistently with the product design.
6. Selecting the OS output **Auralis Virtual Output** makes the source available to Auralis through **Auralis System Audio**.
7. Audio reaches only chosen session members, subject to platform capability.
8. There is no simultaneous direct OS route to the same Bluetooth device when testing Auralis-routed playback. A direct route plus the Auralis route creates audible doubling/echo and is not a valid latency test.
9. Mute, volume, member add/remove, session activate/deactivate, and source switching work while audio is running.
10. Route creation/destruction repeated at least ten times does not crash, hang, duplicate streams, or leak nodes.

Measure latency as objectively as available tools allow. At minimum report the PipeWire graph quantum/rate and observed path. If an audio loopback or recording setup is available, measure end-to-end offset; otherwise label latency assessment as user-observed and do not invent milliseconds.

## Phase 9 — Multi-device session, synchronization, and endurance

If two compatible audio outputs are available, test a real multi-device session:

1. Add both devices as members.
2. Activate the session with a stable source.
3. Verify both receive audio.
4. Exercise per-device controls and session-level controls.
5. Remove and re-add one member while playing.
6. Disconnect/reconnect one member and verify recovery.
7. Play continuously for at least 20 minutes and observe drift, underruns, reconnect loops, memory growth, and CPU use.

Capture relevant PipeWire statistics when available. Synchronization quality that requires human hearing must be recorded as user-observed. If only one device exists, mark true multi-device and drift validation blocked while completing all single-device lifecycle tests.

If the repository has opt-in live-test environment variables, inspect the tests to discover their exact names. Set real device addresses only for the intended test invocation; do not persist personal device identifiers in source files or reports.

## Phase 10 — ALLOW/DISALLOW input-event policy

Determine the exact meaning and backend implementation of ALLOW/DISALLOW from code and documentation. On Linux, verify:

- The buttons are visible where intended and their labels/state are clear.
- Supported Bluetooth media/control input events are accepted in ALLOW state.
- The same supported events are blocked or ignored in DISALLOW state without breaking audio playback.
- State survives refresh/re-enumeration as designed.
- Permission denial is surfaced honestly.
- Unsupported devices are reported as unsupported; the UI must not claim success.
- No implementation requires running the entire app as root.
- No overly broad udev rule grants access to unrelated input devices.

If evdev permissions prevent testing, report the exact device node, ownership, group, and ACL using read-only inspection. Propose the least-privileged rule, but do not install or change it without approval.

## Phase 11 — Failure injection and recovery

Exercise safe failure paths. Service restarts and hardware changes require user approval immediately before the action.

- Launch with PipeWire unavailable; verify bounded failure, useful status, and recovery after PipeWire returns.
- Restart WirePlumber/PipeWire while Auralis is running; verify reconnection or a clear recoverable state.
- Toggle the Bluetooth adapter off/on; verify stale devices are cleared and connected devices return.
- Remove a selected source or output during playback.
- Close an audio-producing app during an active session.
- Revoke or deny a needed permission where safely possible.
- Start a second Auralis instance and verify documented behavior.
- Launch with a non-writable or invalid log location if configurable.
- Use a fresh user profile/config directory to detect reliance on stale local state.
- Suspend/resume only with explicit coordination; otherwise mark this check blocked.

No failure path may cause an indefinite hang, silent false-success state, uncontrolled retry loop, or crash without diagnostic logging.

## Phase 12 — Logging and diagnostics

Find the actual log directory from the running application. Verify by behavior, not only code inspection:

- Logging is enabled by default.
- A timestamped log is created per documented lifecycle.
- Startup, platform detection, PipeWire setup, BlueZ discovery, connection changes, session lifecycle, routing changes, policy changes, errors, and shutdown are traceable.
- Log lines have timestamps, severity, component/category, and enough context to follow execution.
- Errors include an actionable reason and relevant platform error, without unbounded spam.
- Secrets, tokens, private payloads, and unnecessary personal identifiers are not logged.
- Rotation/retention prevents unbounded disk growth.
- Logs are flushed on normal shutdown and remain useful after an abnormal termination.

Correlate at least one complete workflow—from launch through scan, route activation, playback, and shutdown—with the log. Include only redacted excerpts in the report.

## Phase 13 — Sanitizer build

Only after user approval and after confirming adequate resource headroom, create
another independent debug build with AddressSanitizer and UndefinedBehaviorSanitizer.
Use the project-supported option and keep both build and test concurrency at one.
If the host is constrained, mark this optional phase blocked by resources rather
than risking another exhausted desktop.

Typical GCC/Clang flags are:

```bash
-fsanitize=address,undefined -fno-omit-frame-pointer
```

The project-supported bounded commands are:

```bash
cmake -S . -B build-linux-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAURALIS_BUILD_TESTS=ON \
  -DAURALIS_ENABLE_SANITIZERS=ON
cmake --build build-linux-sanitized --parallel 1
ctest --test-dir build-linux-sanitized --output-on-failure --timeout 180 -j1
```

Build, run the deterministic tests, launch the application, and exercise a representative scan/session/routing lifecycle. Report sanitizer findings with stacks. LeakSanitizer limitations caused by third-party desktop libraries must be investigated and narrowly classified, not globally suppressed without evidence.

## Remediation rules

When a defect is reproducible:

1. Preserve the smallest useful reproduction and evidence.
2. Identify the root cause before editing.
3. Make the smallest coherent cross-platform fix; do not regress Windows or macOS abstractions.
4. Add or strengthen an automated regression test when feasible.
5. Keep real-host tests safely opt-in if they depend on hardware or alter desktop state.
6. Build and run the targeted test.
7. Run related integration tests.
8. Run the complete suite.
9. Repeat the affected live workflow.
10. Run `git diff --check` and review the final diff for unrelated changes.

Do not paper over problems by weakening assertions, converting failures to skips, adding arbitrary sleeps, silently catching errors, hard-coding this machine's device address, or claiming unsupported behavior succeeded.

## Required evidence report

Create this file when complete:

```text
docs/validation/linux-actual-machine-validation-YYYY-MM-DD.md
```

The report must include:

1. Date/time/timezone, distro/kernel/session type, CPU architecture, Qt, compiler, CMake, Ninja, PipeWire, WirePlumber, and BlueZ versions.
2. Git branch, starting commit, and a note about pre-existing dirty files without exposing unrelated content.
3. Build configurations and exact commands.
4. Complete automated test totals: passed, failed, skipped, and blocked.
5. A row or clearly structured item for every phase and sub-feature with PASS/FAIL/BLOCKED/NOT APPLICABLE.
6. For each failure: reproduction, expected result, actual result, root cause, files changed, regression test, and retest evidence.
7. Real PipeWire node evidence and duplicate/idempotence results.
8. Real BlueZ/device evidence with hardware addresses redacted.
9. Audio-routing topology used, including whether direct OS routing was disabled during Auralis latency checks.
10. Latency/synchronization method and results, clearly separating measurements from subjective observation.
11. Packaging contents/dependency/launch results.
12. Log-path and logging-quality results with redacted excerpts.
13. Sanitizer results.
14. Remaining limitations, untested hardware paths, and exact reasons.
15. `git status --short`, `git diff --stat`, and `git diff --check` results at completion.

Do not paste enormous raw logs into the report. Store useful artifacts under a dated subdirectory such as `docs/validation/evidence/linux-YYYY-MM-DD/` only when they are reasonably sized, sanitized, and appropriate to keep in Git. Otherwise cite the local path and summarize.

## Release gate

Finish with exactly one overall verdict:

- **READY FOR LINUX HARDWARE BETA** — all deterministic tests pass, packaging and extracted launch pass, core live PipeWire/BlueZ/session/routing flows pass, there are no critical/high defects, and all remaining blocks are non-core and precisely documented.
- **NOT READY — FIXES REQUIRED** — one or more reproducible product defects remain.
- **VALIDATION INCOMPLETE — BLOCKED** — the code may be sound, but missing hardware, permissions, dependencies, or approval prevented enough core live validation that readiness cannot be judged.

List the decisive evidence immediately below the verdict.

## Final response to the user

Return a concise summary containing:

- Overall verdict.
- Tests/builds/packages completed.
- Defects found and fixed.
- Remaining failures or blocks.
- Paths to the validation report and any evidence directory.
- Exact list of files changed.
- Any user action still needed.

Do not say “everything works” unless every mandatory gate above was actually exercised and passed.
