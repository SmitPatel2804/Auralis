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
- Scan idle/active (Auralis local ownership, not raw Adapter1 `Discovering`)
- Device count
- Start Scan / Stop Scan / Refresh
- Empty error text after recovery

Start Scan is enabled when the system bus is connected, BlueZ is available, an adapter is present and powered, and the user is not already requesting a scan.

Stop Scan is enabled while the user wants a scan, Auralis owns a discovery session, or a StartDiscovery request is still pending. Clicking Stop during a pending Start only changes intent; it does not send overlapping D-Bus calls.

If BlueZ is missing, the app still launches and scan is disabled.

If the adapter is powered off, status is `Bluetooth is powered off` and scan is disabled. Auralis does not turn Bluetooth on. Turning power back on must clear that error and show `Ready to scan`.

If the system D-Bus is unavailable, status/error is `System D-Bus is unavailable`, not merely `BlueZ is unavailable`.

If no display server is available:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

## Local ownership vs global Discovering

`Scan: Active` means Auralis owns a discovery session.

`adapterDiscovering` is the raw BlueZ Adapter1 `Discovering` property. Another client can keep that true after Auralis stops. The UI must not require global Discovering to become false.

Parser warnings for malformed optional Device1/Adapter1 fields are log-only (`auralis.bluetooth`). They are non-fatal diagnostics and must not remain as red UI error text.

## Manual hardware scan

1. Launch `./build/apps/desktop/auralis-desktop` as the desktop user.
2. Confirm BlueZ Ready, adapter displayed, Power On, Ready to scan, no stale red error.
3. Put a discoverable Classic or BLE device nearby.
4. Click **Start Scan**. Confirm Scanning... and real device rows insert/update, including RSSI when BlueZ provides it.
5. Click **Stop Scan**. Auralis Scan becomes Idle and Start is available again.
6. Start scan again and confirm no duplicate rows for the same BlueZ object path.
7. Click Start and Stop rapidly. The UI must not crash, stay stuck in Starting/Stopping, or leave an unexpected active scan.
8. Turn the adapter off in OS settings, confirm scan is disabled and the powered-off error is shown, then turn it back on and confirm the error clears.

Do not pair or connect devices as part of Phase 2.

## Live automated test

Skipped by default. When enabled it must fail (not silently pass) if the adapter cannot scan, and it must observe live model activity (`rowsInserted` or `dataChanged`) within 15 seconds.

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

Optional strong check for a known nearby device. Do not commit a personal address:

```bash
AURALIS_RUN_BLUETOOTH_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS=AA:BB:CC:DD:EE:FF \
ctest --test-dir build -R tst_BlueZLiveIntegration --output-on-failure
```

If enabled without BlueZ, without a powered adapter, or without live Device1/model activity, the test fails with a diagnostic rather than skipping.

## Common errors

| Status | Meaning |
|---|---|
| System D-Bus is unavailable | `QDBusConnection::systemBus()` is not connected |
| BlueZ is unavailable | `org.bluez` is not on the bus |
| Bluetooth adapter not available | No Adapter1 object |
| Bluetooth is powered off | Adapter present, `Powered=false` |
| Failed to refresh BlueZ state | `GetManagedObjects` failed; last good registry is kept; Refresh retries |

## Scope

Phase 2 does not implement pairing, connect/disconnect, PipeWire, or audio routing.
