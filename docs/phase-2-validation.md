# Phase 2 Validation

Run from the repository root.

## Clean-room sequence

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

CMake must find Qt 6 DBus. Hardware-independent tests must pass without a Bluetooth adapter.

## Expected desktop behavior

The window still shows the Phase 1 system status rows.

The **Bluetooth Discovery** section should show:

- BlueZ availability
- Adapter name/address when present
- Power on/off
- Scan idle/active
- Device count
- Start Scan / Stop Scan / Refresh

Start Scan is enabled only when BlueZ is available, an adapter is present and powered, and Auralis does not already own discovery.

If BlueZ is missing, the app still launches and scan is disabled.

If the adapter is powered off, status is `Bluetooth is powered off` and scan is disabled. Auralis does not turn Bluetooth on.

If no display server is available:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

## Manual hardware scan

1. Launch `./build/apps/desktop/auralis-desktop` as the desktop user.
2. Confirm adapter state.
3. Put a discoverable Classic or BLE device nearby.
4. Click **Start Scan**.
5. Confirm devices appear without using a terminal.
6. Confirm name/address when BlueZ provides them, and RSSI or `Signal: Unknown`.
7. Click **Stop Scan**.
8. Start scan again and confirm no duplicate rows for the same BlueZ object path.

Do not pair or connect devices as part of Phase 2.

## Live automated test

Skipped by default:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

If enabled without BlueZ, the test fails with a diagnostic rather than skipping.

## Common errors

| Status | Meaning |
|---|---|
| System D-Bus is unavailable | `QDBusConnection::systemBus()` is not connected |
| BlueZ is unavailable | `org.bluez` is not on the bus |
| Bluetooth adapter not available | No Adapter1 object |
| Bluetooth is powered off | Adapter present, `Powered=false` |

## Scope

Phase 2 does not implement pairing, connect/disconnect, PipeWire, or audio routing.
