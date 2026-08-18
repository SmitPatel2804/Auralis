# Phase 3 Validation — Bluetooth Device Management

Phase 3 adds pair/trust/connect/disconnect/forget/reconnect, a BlueZ Agent1 export, bounded auto-reconnect, and QML lifecycle controls. Discovery from Phase 2 is unchanged.

## Clean-room gate

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

Default `ctest` does **not** pair, connect, or forget bonded devices. Live device management remains opt-in.

## Manual demo (20 steps)

1. Launch `auralis-desktop`.
2. Confirm **BlueZ** shows Ready when `org.bluez` is present.
3. Confirm **Agent** shows Registered after BlueZ is available.
4. Start scan and wait for a target device.
5. Note device address and object path in the list (internal id = object path).
6. Pair an unpaired device; confirm **Pairing…** then **Paired** only after BlueZ sets `Paired`.
7. If a pairing prompt appears, accept or enter credentials in `PairingPrompt`. During pairing, confirm **Cancel** is available and that cancelling returns the device to an unpaired idle state without a stale pairing prompt.
8. Trust the paired device; confirm **Trusted** after BlueZ property update.
9. Connect; confirm **Connecting…** then **Connected** from BlueZ only.
10. Open **Services** and verify UUID list with friendly names where known.
11. Disconnect; confirm **Connected** clears from BlueZ state.
12. Reconnect manually; confirm connect succeeds again.
13. If possible, simulate an unexpected disconnect and confirm bounded auto-reconnect attempts resume with backoff and stop at the configured limit.
14. Disconnect again and confirm no automatic reconnect occurs after the explicit user disconnect.
15. Forget the device only when destructive testing is intentionally enabled (removes bond via `Adapter1.RemoveDevice`).
16. Rescan and confirm the device is gone or returns as discovered-only.
17. Stop scan; confirm scan state returns to Idle.
18. Refresh; confirm registry reconciles without crash.
19. Quit and relaunch; confirm app starts without BlueZ present (offline OK), then relaunch with BlueZ and confirm the agent re-registers and reconnect eligibility is restored only after the snapshot rebuild completes.
20. Scope audit: no `bluetoothctl`, no PipeWire routing, `DeviceManager` remains stub.

## Environment variables

| Variable | Effect |
|---|---|
| `AURALIS_RUN_BLUETOOTH_INTEGRATION=1` | Run live BlueZ integration test |
| `AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:…` | Require address in scan; opt-in pair/connect/disconnect |
| `AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1` | Allow forget/remove in live integration test |
| `AURALIS_BLUETOOTH_AGENT_CAPABILITY=KeyboardDisplay` | Override Agent1 capability registration (`NoInputNoOutput`, `DisplayOnly`, `DisplayYesNo`, `KeyboardOnly`, `KeyboardDisplay`) |

Pair/trust/connect/disconnect/reconnect live steps run only when `AURALIS_EXPECT_DEVICE_ADDRESS` is set. Forget runs only with both integration flag and destructive flag.

## Reconnect and cancellation semantics

- `Pairing -> CancellingPairing` is an explicit lifecycle transition. Cancelling pairing invalidates any pending pairing prompt and issues exactly one `Device1.CancelPairing()` call.
- Automatic reconnect is bounded by `ReconnectPolicy` and only retries reconnect-originated failures that map to retryable Bluetooth errors such as `ConnectionFailed`, `TimedOut`, or `NotReady`.
- Explicit `Disconnect` and `Forget` suppress reconnect and cancel any pending retry timer.
- BlueZ loss pauses reconnect scheduling. Reconnect evaluation resumes only after BlueZ returns and the object-manager snapshot has rebuilt the registry.

## Auralis-owned reconnect metadata

BlueZ-owned state (`Paired`, `Connected`, `Trusted`, `UUIDs`, object paths) may disappear when `bluetoothd` restarts. Auralis-owned reconnect intent must survive that rebuild.

`DeviceLifecycleManager` retains a small in-memory map:

```text
QHash<addressIndexKey, DeviceReconnectMetadata>
```

where `DeviceReconnectMetadata` holds:

```text
userDisconnectRequested
autoReconnectEnabled
```

**Stable identity:** `addressIndexKey(adapterPath, address, addressType)` — the same key used by `DeviceRegistry` for deduplication. Metadata is **not** keyed by display name, alias, or object path.

**Update rules:**

| Event | Metadata |
|---|---|
| Explicit `Disconnect` | `userDisconnectRequested = true` |
| Explicit `Connect` / `Reconnect` | `userDisconnectRequested = false` |
| Successful `Connected=true` | suppression cleared |
| `Forget` | entry removed |
| `autoReconnectEnabled` change | retained when user-controlled |

## BlueZ loss and snapshot restoration order

**On BlueZ loss (`BluetoothManager::handleBlueZAvailable(false)`):**

```text
notify lifecycle (pause reconnect, abort pending ops)
clear live DeviceRegistry
clear AdapterRegistry
```

Auralis reconnect metadata remains in `DeviceLifecycleManager` (not stored in the disposable registry).

**On BlueZ return + snapshot (`handleSnapshot`):**

```text
rebuild adapters from ObjectManager
rebuild devices from ObjectManager
reconcile live paths
applyStoredMetadataToRegistry()   // restore userDisconnectRequested / autoReconnectEnabled
onSnapshotApplied()               // resume reconnect policy + evaluate candidates
```

`ReconnectPolicy::resumeAll()` is **not** called before metadata restoration. `DeviceLifecycleManager::onDevicePropertiesChanged()` reacquires the device pointer after `checkPropertyCompletion()` because registry mutation may invalidate prior references.

## Manager-level BlueZ restart tests

`tst_BluetoothManager` exercises the production recovery path (including `DeviceRegistry::clear()`):

- `explicitDisconnectSurvivesBlueZRestart` — intentional disconnect stays suppressed after BlueZ loss/return
- `unexpectedDisconnectReconnectsAfterBlueZRestart` — eligible devices still auto-reconnect after recovery
- `forgetClearsRetainedReconnectMetadata` — forget removes retained metadata; rediscovered devices do not inherit suppression
- `manualReconnectClearsDisconnectSuppression` — manual reconnect clears suppression; subsequent unexpected disconnect is eligible again

`tst_DeviceLifecycle::disconnectPropertyCompletionSurvivesRegistryMutation` covers registry pointer safety during property completion.

## Manual BlueZ restart validation

Perform these developer-only scenarios when hardware is available (do not add daemon-control commands to production code):

**Intentional disconnect survives restart:**

1. Pair/trust/connect a test device.
2. Explicitly Disconnect from Auralis.
3. Restart `bluetoothd` (or otherwise simulate BlueZ daemon restart).
4. Let Auralis recover via ObjectManager snapshot.
5. Confirm the device remains paired and disconnected.
6. Confirm Auralis does **not** auto-reconnect.
7. Click Reconnect manually; confirm connection succeeds.

**Unexpected disconnect recovers:**

1. Connect a device without an explicit user disconnect.
2. Restart `bluetoothd` while the device is paired but disconnected (or disconnect unexpectedly, then restart BlueZ before reconnect completes).
3. Confirm Auralis resumes bounded auto-reconnect for the eligible device.


Phase 2 tests must remain green.

## Live integration test behavior

When `AURALIS_RUN_BLUETOOTH_INTEGRATION=1` and `AURALIS_EXPECT_DEVICE_ADDRESS` is set, `tst_BlueZLiveIntegration` now verifies:

1. BlueZ is available and the Auralis agent registers.
2. The expected device is discovered.
3. Pairing completes when required.
4. Trust succeeds and is reflected by BlueZ state.
5. Connect succeeds.
6. Disconnect clears `Connected`.
7. Manual reconnect succeeds.
8. Forget runs only with `AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1`; otherwise the test prints `SKIPPED: destructive Bluetooth operations not enabled`.

## Known limitations

- No `ConnectProfile` routing; raw Device1 `Connect` only.
- No GATT/service discovery beyond BlueZ `UUIDs` property.
- Auto-reconnect is bounded and config-driven; suppressed after explicit disconnect/forget.
- Agent capability defaults to `KeyboardDisplay`; `RequestDefaultAgent` is not called.
- PipeWire / Phase 4 audio routing is not implemented.
