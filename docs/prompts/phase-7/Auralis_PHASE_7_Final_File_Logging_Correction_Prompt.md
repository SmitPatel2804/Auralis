# AURALIS — PHASE 7 FINAL CORRECTION PROMPT
## File Logging Runtime Truthfulness + Negative-Path Regression Tests

## Purpose

This document is a **final focused correction prompt for Cursor AI IDE**.

Auralis Phase 7 is essentially complete. The large Phase 7 GUI architecture and the previously identified blockers have already been corrected, including:

- selected-session state isolation;
- correct `currentSourceId` and `currentMuted` notification semantics;
- Manual vs Session route ownership;
- protection of session-managed routes from the standalone routing planner;
- complete Device Details;
- UUID/services visibility;
- session Duplicate;
- session Restore;
- Diagnostics expansion;
- Diagnostics clipboard integration;
- dark-theme cleanup;
- session activation race handling;
- ConfirmDialog binding-loop correction;
- GUI/presentation regression coverage;
- live Bluetooth/PipeWire validation;
- multi-device session testing.

**Do not rewrite those areas.**

The current Phase 7 repository has only one remaining correctness gap:

> The Settings UI can report file logging as enabled even when `Logger::enableFileLogging()` failed to activate the runtime logging sink.

This violates the core Phase 7 rule that the GUI must reflect authoritative backend truth.

Your task is to fix that issue cleanly, add strong regression tests, rerun the full test suite, perform a small final GUI verification, and update the Phase 7 audit honestly.

---

# 1. STRICT SCOPE

Work only on the remaining file-logging correctness issue and directly related tests/UI error handling.

Do **not**:

- rewrite Bluetooth code;
- rewrite PipeWire code;
- rewrite SessionManager;
- rewrite AudioRouter;
- rewrite RoutingCoordinator;
- redesign the GUI;
- change route ownership;
- change SelectedSessionViewModel;
- change device-details architecture;
- remove working Phase 7 tests;
- alter unrelated persistence behavior;
- begin Phase 8 work.

The expected change set should be small and focused.

---

# 2. FIRST ACTION — INSPECT CURRENT IMPLEMENTATION

Before editing, inspect the exact repository state.

Run:

```bash
pwd
git status --short
git branch --show-current
git log --oneline --decorate -n 20
```

Inspect relevant files:

```bash
rg -n \
  "setFileLoggingEnabled|applyRuntimeFileLogging|enableFileLogging|disableFileLogging|isFileLoggingActive|logFilePath|fileLoggingEnabled|lastErrorText|setLastError|writeValue" \
  src include apps ui tests
```

Read the implementations of:

```text
ConfigurationManager
Logger
SettingsPage / SettingsViewModel if present
ConfigurationManager tests
Logger tests
```

Do not assume the code exactly matches this prompt. Use the actual current code.

---

# 3. KNOWN REMAINING DEFECT

The current implementation is conceptually equivalent to:

```cpp
bool ConfigurationManager::setFileLoggingEnabled(bool enabled)
{
    fileLoggingEnabled_ = enabled;
    emit fileLoggingEnabledChanged();

    applyRuntimeFileLogging();

    return true;
}
```

and:

```cpp
void ConfigurationManager::applyRuntimeFileLogging()
{
    if (fileLoggingEnabled_) {
        Logger::enableFileLogging(logFilePath_);
    } else {
        Logger::disableFileLogging();
    }
}
```

The problem is:

```cpp
Logger::enableFileLogging(...)
```

returns a boolean indicating whether runtime file logging was actually activated.

That result is currently ignored.

Therefore this state is possible:

```text
ConfigurationManager says: enabled
Settings UI says:          enabled
Logger runtime says:       disabled
User-facing error:         none
```

That is incorrect.

---

# 4. FAILURE EXAMPLE

The clearest failing case is:

```text
logFilePath = ""
fileLoggingEnabled = false
```

User enables file logging.

Current flow can become:

```text
ConfigurationManager sets enabled = true
        |
        v
fileLoggingEnabledChanged emitted
        |
        v
QML checkbox becomes checked
        |
        v
Logger::enableFileLogging("")
        |
        v
Logger returns false
        |
        v
failure ignored
        |
        v
ConfigurationManager returns true
```

Final state:

```text
ConfigurationManager::fileLoggingEnabled() == true
Logger::isFileLoggingActive()               == false
Settings checkbox                           == checked
lastErrorText                               == empty
```

This must no longer be possible.

---

# 5. REQUIRED INVARIANT

After this correction, the application must maintain this invariant:

```text
If ConfigurationManager reports file logging enabled,
then the runtime Logger must actually be active,
unless the application explicitly documents a restart-only configuration model.
```

For the current Phase 7 implementation, prefer **live runtime truthfulness** if Logger already supports runtime enable/disable.

The GUI must never claim success when Logger activation failed.

---

# 6. DETERMINE EXISTING LOGGER CONTRACT

Before changing ConfigurationManager, inspect:

```cpp
Logger::enableFileLogging(...)
Logger::disableFileLogging()
Logger::isFileLoggingActive()
Logger::isInitialized()
```

Determine:

- whether `enableFileLogging()` opens the file immediately;
- whether it creates missing directories;
- what happens for an empty path;
- what happens for an invalid path;
- what happens for a non-existent directory;
- what happens for a permission failure;
- whether calling it before Logger initialization is legal;
- whether the function itself logs its failure;
- whether runtime enable/disable is thread-safe;
- whether changing the path while already active is supported.

Do not duplicate Logger validation if Logger already owns it.

ConfigurationManager should coordinate state and error propagation.

Logger should remain the authority on whether file logging activation actually succeeded.

---

# 7. REQUIRED TRANSACTIONAL BEHAVIOR

Enabling file logging must behave transactionally.

Conceptually:

```text
User requests enable
        |
        v
Validate required configuration
        |
        v
Attempt runtime Logger activation
        |
        +---- failure ----> expose error
        |                  keep config disabled
        |                  keep UI disabled
        |
        v
Runtime activation succeeds
        |
        v
Persist enabled state
        |
        +---- persistence failure ----> rollback runtime activation
        |                              expose error
        |
        v
Update ConfigurationManager state
        |
        v
Emit property notification
        |
        v
UI becomes enabled
```

Do not emit success state before runtime success is established.

---

# 8. RECOMMENDED `setFileLoggingEnabled()` SEMANTICS

Adapt to the actual codebase.

A clean design should resemble:

```cpp
bool ConfigurationManager::setFileLoggingEnabled(bool enabled)
{
    if (fileLoggingEnvLocked_) {
        setLastError(...);
        return false;
    }

    if (fileLoggingEnabled_ == enabled) {
        setLastError({});
        return true;
    }

    if (enabled) {
        if (logFilePath_.trimmed().isEmpty()) {
            setLastError(
                tr("Choose a log file path before enabling file logging."));
            return false;
        }

        if (Logger::isInitialized()) {
            if (!Logger::enableFileLogging(logFilePath_)) {
                setLastError(
                    tr("Unable to enable file logging at the selected path."));
                return false;
            }
        }

        if (!writeValue(kFileLoggingEnabledKey, true)) {
            if (Logger::isInitialized()) {
                Logger::disableFileLogging();
            }

            setLastError(
                tr("Unable to save the file logging setting."));
            return false;
        }

        fileLoggingEnabled_ = true;
        emit fileLoggingEnabledChanged();
        setLastError({});
        return true;
    }

    /*
     * Disabling should similarly remain truthful.
     */

    if (Logger::isInitialized()) {
        Logger::disableFileLogging();
    }

    if (!writeValue(kFileLoggingEnabledKey, false)) {
        /*
         * Decide whether runtime state needs rollback.
         * Preserve consistency.
         */
        ...
    }

    fileLoggingEnabled_ = false;
    emit fileLoggingEnabledChanged();
    setLastError({});
    return true;
}
```

This is illustrative.

Do not copy it blindly if the repository uses a different persistence sequence or locking mechanism.

---

# 9. RUNTIME + PERSISTENCE CONSISTENCY

The corrected code must consider **two state owners**:

```text
Configuration persistence
Runtime Logger state
```

They must not silently diverge.

## Enabling

Successful final state:

```text
persisted enabled = true
ConfigurationManager enabled = true
Logger active = true
```

Failure final state:

```text
persisted enabled = false / previous value
ConfigurationManager enabled = false / previous value
Logger active = false / previous safe value
error exposed
```

## Disabling

Successful final state:

```text
persisted enabled = false
ConfigurationManager enabled = false
Logger active = false
```

If persistence fails after runtime disable, choose and document a consistent rollback strategy.

Do not leave:

```text
persisted enabled = true
runtime disabled
ConfigurationManager false
```

without deliberate reconciliation.

---

# 10. HANDLE LOGGER-NOT-INITIALIZED CASE CORRECTLY

Inspect startup order.

If ConfigurationManager can be used before Logger initialization, define correct semantics.

Possible valid behavior:

### Model A

ConfigurationManager persists startup intent even before Logger exists.

At Logger initialization:

```text
ConfigurationManager fileLoggingEnabled == true
-> application applies log path
-> Logger activation attempted
-> activation failure propagates to application diagnostics/settings
```

### Model B

Runtime Logger exists before interactive settings are available.

Then live activation can be mandatory.

Use the repository's actual lifecycle.

Do not introduce a race or make application startup fail unnecessarily.

---

# 11. EMPTY PATH

Explicitly test and handle:

```text
logFilePath == ""
```

Expected behavior for interactive enabling:

```text
setFileLoggingEnabled(true) == false
fileLoggingEnabled() == false
Logger::isFileLoggingActive() == false
lastErrorText is non-empty
```

The Settings UI must remain unchecked.

Prefer an actionable error such as:

```text
Choose a log file path before enabling file logging.
```

Do not expose raw `QFile` internals as the only message.

---

# 12. INVALID / UNUSABLE PATH

Test a path that Logger cannot open.

Examples:

```text
path under a non-existent parent directory
read-only location
directory supplied as file
invalid test-specific path
```

Use a deterministic test that behaves reliably in CI.

Avoid tests that depend on root privileges or machine-specific `/root` access assumptions.

A robust Qt test can often use:

```text
QTemporaryDir
```

and deliberately construct an invalid target.

Expected:

```text
setFileLoggingEnabled(true) == false
fileLoggingEnabled == false
Logger active == false
error exposed
```

---

# 13. VALID PATH

Keep and strengthen the happy-path test.

Use:

```cpp
QTemporaryDir dir;
QVERIFY(dir.isValid());

const QString path = dir.filePath("auralis.log");
```

Expected:

```text
setLogFilePath(path) == true
setFileLoggingEnabled(true) == true

ConfigurationManager.fileLoggingEnabled == true
Logger::isFileLoggingActive() == true
lastErrorText empty
```

Write at least one log event if the Logger API makes this practical.

Verify the file is actually created/writable if existing tests already support it.

---

# 14. DISABLE PATH

After successful enable:

```text
setFileLoggingEnabled(false)
```

Expected:

```text
returns true
ConfigurationManager.fileLoggingEnabled == false
Logger::isFileLoggingActive() == false
lastErrorText empty
```

Test repeated disable:

```text
disable when already disabled
```

It should be safe and idempotent.

---

# 15. ENABLE TWICE

Test:

```text
valid path
enable
enable again
```

Expected:

- no failure;
- no duplicate file sink;
- runtime remains active;
- property does not emit unnecessary state-change signals if unchanged.

Use `QSignalSpy` where appropriate.

---

# 16. CHANGE PATH WHILE ENABLED

Inspect current intended semantics.

If the application supports changing the log path while logging is active:

```text
old path active
change to new valid path
Logger switches safely
configuration persists new path
```

If it intentionally requires restart:

- do not silently imply the path changed live;
- Settings must display a restart-required explanation;
- existing active runtime path should remain predictable.

Do not expand scope beyond the existing intended behavior.

---

# 17. ENVIRONMENT LOCKS

If environment variables or command-line configuration can lock file logging:

```text
fileLoggingEnvLocked_
```

or equivalent, preserve the existing contract.

Test:

```text
environment-locked enabled
environment-locked disabled
```

where feasible.

The UI must not claim an override was accepted if the environment owns the setting.

If controls are disabled in QML for locked settings, preserve that behavior.

---

# 18. ERROR PROPAGATION

Use the project's existing error mechanism.

Likely:

```text
lastErrorText
lastErrorChanged
notification controller
SettingsPage error banner/toast
```

When Logger activation fails:

```text
ConfigurationManager returns false
lastErrorText becomes meaningful
QML receives notification
checkbox remains/refalls to authoritative false
```

Do not create a second independent error mechanism.

---

# 19. QML SETTINGS BEHAVIOR

Inspect SettingsPage.

The file-logging control should behave like:

```text
User toggles ON
        |
        v
ConfigurationManager setter
        |
        +---- true ----> checkbox remains ON
        |
        +---- false ---> authoritative property remains OFF
                        error shown
```

Avoid imperative QML like:

```qml
checked = true
backend.setFileLoggingEnabled(true)
```

The control should bind to authoritative state.

If CheckBox automatically changes before backend confirmation, ensure binding/state reconciliation returns it to false on failure.

---

# 20. REQUIRED UNIT TESTS

Add explicit regression tests.

Suggested names:

```text
fileLoggingEnableValidPathActivatesRuntimeLogger
fileLoggingEnableEmptyPathFailsWithoutStateDivergence
fileLoggingEnableInvalidPathFailsWithoutStateDivergence
fileLoggingDisableDeactivatesRuntimeLogger
fileLoggingRepeatedEnableIsIdempotent
```

Adapt naming to the repository.

---

# 21. REQUIRED EMPTY-PATH TEST

Conceptually:

```cpp
void tst_ConfigurationManager::fileLoggingEnableEmptyPathFails()
{
    // isolated config
    // Logger initialized if required

    QVERIFY(manager.setLogFilePath(QString()));
    QVERIFY(!manager.setFileLoggingEnabled(true));

    QCOMPARE(manager.fileLoggingEnabled(), false);
    QCOMPARE(Logger::isFileLoggingActive(), false);

    QVERIFY(!manager.lastErrorText().trimmed().isEmpty());
}
```

If `setLogFilePath("")` itself is expected to reject empty paths, adapt test to establish the empty/default state naturally.

The invariant is more important than exact setup.

---

# 22. REQUIRED INVALID-PATH TEST

Construct a deterministic Logger-open failure.

Conceptually:

```cpp
QTemporaryDir dir;
QVERIFY(dir.isValid());

const QString impossible =
    dir.filePath("missing-parent/auralis.log");

QVERIFY(manager.setLogFilePath(impossible));

QVERIFY(!manager.setFileLoggingEnabled(true));

QCOMPARE(manager.fileLoggingEnabled(), false);
QCOMPARE(Logger::isFileLoggingActive(), false);
QVERIFY(!manager.lastErrorText().isEmpty());
```

If Logger automatically creates directories, choose another deterministic failure.

Do not use a flaky permission assumption.

---

# 23. REQUIRED HAPPY-PATH TEST

```cpp
QTemporaryDir dir;
QVERIFY(dir.isValid());

const QString path = dir.filePath("auralis.log");

QVERIFY(manager.setLogFilePath(path));
QVERIFY(manager.setFileLoggingEnabled(true));

QCOMPARE(manager.fileLoggingEnabled(), true);
QCOMPARE(Logger::isFileLoggingActive(), true);
QVERIFY(manager.lastErrorText().isEmpty());
```

Then disable and verify both become false.

---

# 24. REQUIRED Q_PROPERTY / SIGNAL TEST

Use:

```cpp
QSignalSpy enabledSpy(
    &manager,
    &ConfigurationManager::fileLoggingEnabledChanged);
```

For failed enable:

```text
property remains false
no successful enabled-state notification should leave QML true
```

For successful enable:

```text
one appropriate state-change notification
```

For idempotent re-enable:

```text
no unnecessary additional state-change event
```

Follow the project's existing signal behavior conventions.

---

# 25. LOGGER TESTS

If Logger has its own unit-test suite, add or preserve direct tests for:

```text
enable valid path -> true
enable invalid path -> false
isFileLoggingActive reflects real sink state
disable -> inactive
```

Avoid duplicating all Logger internals inside ConfigurationManager tests.

ConfigurationManager tests should focus on cross-layer consistency.

---

# 26. DO NOT MASK LOGGER FAILURE

Forbidden:

```cpp
Logger::enableFileLogging(...);
return true;
```

Forbidden:

```cpp
fileLoggingEnabled_ = true;

if (!Logger::enableFileLogging(...))
    Logger::logWarning(...);

return true;
```

Logging the failure is not enough.

The user-facing configuration must remain truthful.

---

# 27. DO NOT FIX BY LYING IN THE UI

Do not solve this only by adding text:

```text
"Logging may fail."
```

The backend state itself must become correct.

Do not leave the checkbox checked after runtime activation failed.

---

# 28. DO NOT CHANGE LOGGER API UNNECESSARILY

If:

```cpp
Logger::enableFileLogging()
```

already returns `bool`, that is sufficient.

Use it.

Do not redesign Logger into an exception/result framework unless the existing API genuinely cannot express the required behavior.

Keep this patch minimal.

---

# 29. REGRESSION SWEEP

After implementing the logging fix, perform a quick source/regression review of previously corrected areas.

Verify these symbols/behaviors still exist:

```text
SelectedSessionViewModel
currentSourceIdChanged
currentMutedChanged
RouteOwnerType
Manual
Session
createSessionRoute
plannerRoute/manual-route selection
Device Details UUID/service display
duplicateSession
restoreLastSession
Diagnostics clipboard
semantic Theme colors
```

Do not edit them unless your logging patch accidentally caused a compilation issue directly involving them.

---

# 30. CLEAN BUILD

After code changes:

```bash
rm -rf build

cmake -S . -B build -G Ninja

cmake --build build
```

No stale-build result is acceptable.

---

# 31. RUN COMPLETE TEST SUITE

Run:

```bash
ctest --test-dir build -N

ctest --test-dir build --output-on-failure
```

Record exact test count.

The previous repository reportedly had 42 tests.

Do not assume it remains 42.

If new tests are added, the count may increase.

Report the real number.

Example:

```text
CTest: 46/46 PASS
```

only if that is the actual result.

---

# 32. RUN TARGETED LOGGING TESTS VERBOSELY

Use the actual test name.

For example:

```bash
ctest --test-dir build \
  -R "ConfigurationManager|Logger" \
  --output-on-failure
```

If needed:

```bash
ctest --test-dir build \
  -V \
  -R "<ACTUAL_TEST_PATTERN>"
```

Make sure all negative-path cases execute.

---

# 33. QML RUNTIME SMOKE

Launch:

```bash
./build/apps/desktop/auralis-desktop
```

or the repository's actual binary path.

Open Settings.

Test file logging visually.

### Case A — no path / invalid path

Attempt enable.

Expected:

```text
checkbox remains OFF
error visible
application remains responsive
Logger remains inactive
```

### Case B — valid path

Choose/set a valid writable path using the existing Settings workflow.

Enable.

Expected:

```text
checkbox ON
Logger active
no error
```

Disable.

Expected:

```text
checkbox OFF
Logger inactive
```

---

# 34. CHECK APPLICATION STDERR

Exercise Settings while capturing output:

```bash
./build/apps/desktop/auralis-desktop \
  > /tmp/auralis-phase7-final.out \
  2> /tmp/auralis-phase7-final.err
```

Inspect:

```bash
cat /tmp/auralis-phase7-final.err
```

Resolve application-caused:

```text
ReferenceError
TypeError
binding loop
undefined property
failed binding
invalid signal
```

Do not ignore QML errors introduced by the patch.

---

# 35. FINAL INTERACTIVE GUI WALKTHROUGH

Because the previous rigorous validation had limited interactive keyboard/mouse coverage, perform a short final walkthrough.

At minimum:

```text
Dashboard
Devices
Sessions
Audio Routing
Diagnostics
Settings
```

Verify every page opens.

Then test:

```text
mouse navigation
keyboard Tab / Shift+Tab
visible focus
Enter/Space activation
Escape dialog close
```

Focus especially on Settings and confirmation dialogs.

This is not a full Phase 7 re-test; it is the final interactive confidence pass.

---

# 36. DO NOT REQUIRE HARDWARE RETEST UNLESS AVAILABLE

The logging correction does not touch Bluetooth/PipeWire/session routing.

Therefore a full two-device hardware run is not mandatory solely for this patch if:

- the previous rigorous hardware validation is documented;
- the patch is isolated to ConfigurationManager/Logger/Settings/tests;
- all Phase 0–7 automated regression tests pass.

If controlled hardware is already connected and retesting is easy, a smoke run is welcome.

Do not make unrelated destructive hardware changes.

---

# 37. UPDATE THE IMPLEMENTATION AUDIT

Update:

```text
docs/PHASE_7_IMPLEMENTATION_AUDIT.md
```

and/or the repository's rigorous validation audit.

Add a section:

```text
Final File Logging Correction
```

Document:

```text
previous defect
root cause
production fix
runtime/persistence invariant
negative-path tests
happy-path tests
GUI result
full CTest result
```

---

# 38. CORRECT THE PREVIOUS AUDIT CLAIM

If the existing audit currently says:

```text
File logging semantics | PASS
```

while the old implementation ignored Logger activation failure, correct the historical record.

Example:

```text
Previous validation gap:
The earlier audit covered the valid-path logging case but did not test
Logger activation failure. A final review identified state divergence
when runtime file opening failed. This has now been corrected and
negative-path regression tests were added.
```

Do not hide the fact that the earlier test was incomplete.

That improves the audit's credibility.

---

# 39. REQUIRED FINAL TEST MATRIX

Include:

| File logging test | Expected | Result |
|---|---|---|
| Valid writable path | runtime ON + config ON | PASS/FAIL |
| Empty path | enable rejected | PASS/FAIL |
| Invalid/unusable path | enable rejected | PASS/FAIL |
| Runtime Logger activation failure | config remains OFF | PASS/FAIL |
| Error text on failure | visible/non-empty | PASS/FAIL |
| Disable after enable | runtime OFF + config OFF | PASS/FAIL |
| Repeated enable | stable/idempotent | PASS/FAIL |
| Full CTest suite | all pass | PASS/FAIL |
| Settings GUI invalid path | remains OFF | PASS/FAIL |
| Settings GUI valid path | turns ON | PASS/FAIL |
| QML stderr gate | clean | PASS/FAIL |
| Keyboard/mouse final walkthrough | usable | PASS/FAIL |

---

# 40. PHASE 7 FINAL EXIT GATE

Only after the correction and verification may the repository say:

```text
PHASE 7 EXIT GATE: PASSED
```

Required:

- [ ] `Logger::enableFileLogging()` result is no longer ignored in the interactive enable flow.
- [ ] Config/runtime state cannot silently diverge.
- [ ] Empty-path enable fails safely.
- [ ] Invalid-path enable fails safely.
- [ ] Failure exposes a useful user-facing error.
- [ ] Settings checkbox remains authoritative.
- [ ] Valid-path enable works.
- [ ] Disable works.
- [ ] Negative-path automated tests exist.
- [ ] Full clean build passes.
- [ ] Full CTest suite passes.
- [ ] QML Settings workflow passes.
- [ ] final keyboard/mouse walkthrough passes.
- [ ] previous Phase 7 fixes remain intact.

If any required item fails:

```text
PHASE 7 EXIT GATE: FAILED
```

Do not move to Phase 8.

---

# 41. EXPECTED FILES TO CHANGE

Do not force these exact paths, but the correction should probably touch only a small subset such as:

```text
src/core/ConfigurationManager.cpp
include/.../ConfigurationManager.hpp
tests/.../tst_ConfigurationManager.cpp
tests/.../tst_Logger.cpp        # only if needed
ui/.../SettingsPage.qml         # only if error/UI reconciliation needs adjustment
docs/PHASE_7_IMPLEMENTATION_AUDIT.md
docs/PHASE_7_RIGOROUS_VALIDATION_AUDIT.md
```

If dozens of unrelated files change, stop and reassess.

---

# 42. FINAL RESPONSE FROM CURSOR

When finished, respond with:

```text
AURALIS PHASE 7 — FINAL LOGGING CORRECTION

Status: PASSED / FAILED

Root cause:
- Logger activation failure was previously ignored by ConfigurationManager.

Production correction:
- ...
- ...

Tests added:
- valid path
- empty path
- invalid/unusable path
- runtime failure rollback
- disable
- idempotency

Verification:
- Clean configure: PASS/FAIL
- Clean build: PASS/FAIL
- CTest: N/N PASS
- Settings GUI: PASS/FAIL
- QML runtime warning gate: PASS/FAIL
- Keyboard/mouse walkthrough: PASS/FAIL

Regression:
- Previous Phase 7 fixes remain intact: YES/NO

Audit updated:
- ...

PHASE 7 EXIT GATE: PASSED / FAILED
```

Never invent test counts.

Never mark an unrun test PASS.

---

# 43. MASTER DIRECTIVE

Perform one precise final Phase 7 correction:

> **Make file logging state transactional and truthful so ConfigurationManager, Logger, persisted configuration, and the Settings GUI can never silently disagree about whether file logging is actually enabled.**

Then prove the correction with negative-path tests, a clean full-suite run, and a final Settings GUI walkthrough.

Do not disturb the already-correct Phase 7 architecture.

Once this invariant is proven and the full suite remains green, Phase 7 may finally be locked.
