# Phase 3 Validation — Bluetooth Device Management

Phase 3 adds pair/trust/connect/disconnect/forget/reconnect, a BlueZ Agent1 export, and QML lifecycle controls. Discovery from Phase 2 is unchanged.

## Clean-room gate

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

Default `ctest` does **not** pair, connect, or forget bonded devices.

## Manual demo (20 steps)

1. Launch `auralis-desktop`.
2. Confirm **BlueZ** shows Ready when `org.bluez` is present.
3. Confirm **Agent** shows Registered after BlueZ is available.
4. Start scan and wait for a target device.
5. Note device address and object path in the list (internal id = object path).
6. Pair an unpaired device; confirm **Pairing…** then **Paired** only after BlueZ sets `Paired`.
7. Accept or enter any Agent prompt (`PairingPrompt` dialog).
8. Trust the paired device; confirm **Trusted** after BlueZ property update.
9. Connect; confirm **Connecting…** then **Connected** from BlueZ only.
10. Open **Services** and verify UUID list with friendly names where known.
11. Disconnect; confirm **Connected** clears from BlueZ state.
12. Reconnect manually; confirm connect succeeds again.
13. Disconnect again.
14. Forget the device (removes bond via `Adapter1.RemoveDevice`).
15. Rescan and confirm the device is gone or returns as discovered-only.
16. Stop scan; confirm scan state returns to Idle.
17. Refresh; confirm registry reconciles without crash.
18. Quit and relaunch; confirm app starts without BlueZ present (offline OK).
19. Relaunch with BlueZ; confirm agent re-registers.
20. Scope audit: no `bluetoothctl`, no PipeWire routing, `DeviceManager` remains stub.

## Environment variables

| Variable | Effect |
|---|---|
| `AURALIS_RUN_BLUETOOTH_INTEGRATION=1` | Run live BlueZ integration test |
| `AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:…` | Require address in scan; opt-in pair/connect/disconnect |
| `AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS=1` | Allow forget/remove in live integration test |

Pair/connect live steps run only when `AURALIS_EXPECT_DEVICE_ADDRESS` is set. Forget runs only with both integration flag and destructive flag.

## Unit tests (always on)

- `tst_DeviceLifecycle` — trust/connect/pair/forget, duplicate ops, InProgress, user disconnect suppresses reconnect
- `tst_ReconnectPolicy` — backoff, max attempts, cancel, pause, onConnected reset
- `tst_BlueZAgent` — pending requests, accept/reject/PIN, cancel, expired id
- `tst_BluetoothDbusError` — BlueZ error name mapping

Phase 2 tests must remain green.

## Known limitations

- No `ConnectProfile` routing; raw Device1 `Connect` only.
- No GATT/service discovery beyond BlueZ `UUIDs` property.
- Auto-reconnect is bounded and config-driven; suppressed after explicit disconnect/forget.
- Agent capability defaults to `KeyboardDisplay`; `RequestDefaultAgent` is not called.
- PipeWire / Phase 4 audio routing is not implemented.
