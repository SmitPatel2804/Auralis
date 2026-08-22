# Phase 4 Validation — PipeWire Endpoints and Bluetooth Correlation

Phase 4 connects to the user PipeWire instance, classifies audio nodes as `AudioEndpoint` objects, and maps Bluetooth endpoints onto the existing Phase 3 `DeviceRegistry`. **Routing, link creation, and session orchestration are Phase 5+ and are not implemented.**

## Clean-room gate

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

Configure requires `libpipewire-0.3` development files. Default `ctest` does **not** require a live PipeWire daemon to pass (the live test skips). `tst_PipeWireManager` starts/stops the native client and is safe without asserting Connected.

## Manual — built-in audio

1. Launch `auralis-desktop`.
2. Confirm **PipeWire** shows **Connected** (or a clear Error if the user session has no PipeWire).
3. Confirm **Audio Endpoints** lists built-in playback and/or capture objects.
4. Confirm those rows are **not** mapped to a Bluetooth device.
5. Play audio from an ordinary application (browser, player). Confirm application `Stream/Output/Audio` nodes do **not** appear as Auralis endpoints.

## Manual — Auralis system audio

1. Launch Auralis and open **Audio Routing**.
2. Confirm **Auralis Virtual Output** reports Ready (persistent) or Ready
   (while Auralis runs).
3. Select **Auralis Virtual Output** in the desktop sound settings.
4. Confirm the card changes to **Selected as system output** and the source
   list contains exactly one **Auralis System Audio** row.
5. Confirm **Auralis Virtual Output** is not present in the destination list.
6. Activate a route from **Auralis System Audio** to one connected playback
   endpoint, play browser audio, and confirm only that selected endpoint plays.
7. Deactivate the route and confirm Auralis-owned links are removed.

## Manual — Bluetooth audio device

1. Use existing Phase 2/3 scan, pair, trust, connect.
2. Confirm BlueZ **Connected** on the device row.
3. Until PipeWire publishes the node, Audio should show **Initializing...**.
4. When the endpoint appears, it should be **Available**, with a plausible direction/profile, **Mapped** to the same Phase 3 device.
5. Disconnect in Auralis; the endpoint should disappear (current-graph registry).
6. Reconnect; a new PipeWire global ID should map again to the same logical endpoint/device.
7. No `wpctl` / `pactl` / `pw-dump` is required for the workflow. Those commands are diagnostics only.

## Manual — already connected at startup

1. Connect a Bluetooth audio device before launching Auralis.
2. Launch Auralis.
3. Phase 3 should show the existing BlueZ device; Phase 4 should enumerate the existing PipeWire node and map without a reconnect.

## Manual — rapid churn

Connect/disconnect several times. Expect no crash, no duplicate endpoints, no stale mapped row for a removed node.

## Environment variables

| Variable | Effect |
|---|---|
| `AURALIS_RUN_PIPEWIRE_INTEGRATION=1` | Run live PipeWire integration (`tst_PipeWireLiveIntegration`) |
| `AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF"` | With the flag above, also require a mapped Bluetooth endpoint for that address |

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build -R tst_PipeWireLiveIntegration --output-on-failure
```

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
AURALIS_EXPECT_DEVICE_ADDRESS="AA:BB:CC:DD:EE:FF" \
ctest --test-dir build -R tst_PipeWireLiveIntegration --output-on-failure
```

If the flag is set, the live test requires a real PipeWire connection, complete
registry sync, a ready virtual sink/source graph, stable Auralis source
classification, and feedback exclusion. Failure includes a diagnostics dump
(`state=... devices=... nodes=... endpoints=... virtualOutput=...`).

## Mapping strategy

Priority: normalized `api.bluez5.address`, then BlueZ object path, then node→PipeWire-device ownership, then unique exact name. Ambiguous names are left unmapped.

Bluetooth address comparison accepts `aa:bb:…`, `AA_BB_…`, and `AA-BB-…` and canonicalizes to `AA:BB:CC:DD:EE:FF`.

## Developer diagnostics

`wpctl status`, `pw-cli ls`, `pw-dump`, and `pactl info` may be used by a developer to cross-check the graph. Auralis must not execute or parse them.

When `showDeveloperStatus` is enabled, the Audio Endpoints panel shows an in-memory diagnostics line from `PipeWireManager::diagnosticsText()`.

## Phase 4 limitations

- No PipeWire link/route creation.
- No format negotiation or SPA pod parsing beyond optional `audio.rate` / `audio.channels` properties.
- No sophisticated reconnect if the PipeWire daemon restarts (Phase 8). The graph is cleared and status becomes Error/Unavailable.
- `DeviceManager` / `SessionManager` remain stubs.
