# Phase 1 Validation

Run from the repository root. Phase 1 is complete only when every step succeeds.

## Clean-room sequence

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

## Expected results

### `cmake -S . -B build -G Ninja`

- Qt 6 is found.
- Internal targets resolve (`auralis-core`, `auralis-bluetooth`, `auralis-audio`, `auralis-devices`, `auralis-session`, `auralis-ui`, `auralis-desktop`).
- Ninja build files are generated.
- No network downloads occur.

### `cmake --build build`

- Libraries, QML resources, tests, and `auralis-desktop` compile and link.
- The desktop binary is at `./build/apps/desktop/auralis-desktop`.

### `ctest --test-dir build --output-on-failure`

- All Phase 1 tests pass.
- Tests do not require Bluetooth hardware, root, or a live PipeWire graph.
- The desktop backend smoke test uses `QT_QPA_PLATFORM=offscreen`.

### `./build/apps/desktop/auralis-desktop`

The window title contains `Auralis`. Status values come from C++ and should read:

```text
AURALIS

System Status
Bluetooth     Ready
Audio         Ready
PipeWire      Ready

Auralis Core Ready
```

A developer note explains that this is foundation readiness, not live hardware validation:

```text
Foundation services initialized — hardware integration begins in Phase 2+
```

Startup logs should identify logger, configuration, service skeletons, core readiness, and QML load.

The process should exit cleanly when the window is closed.

If no display server is available, launch with:

```bash
QT_QPA_PLATFORM=offscreen ./build/apps/desktop/auralis-desktop
```

## Scope audit

Production source should not contain Phase 2+ Bluetooth/PipeWire implementation or shell-command integration:

```bash
grep -R "StartDiscovery\|StopDiscovery\|org.bluez.Device1\|bluetoothctl\|wpctl\|pactl\|pw-cli" -n . \
    --exclude-dir=build \
    --exclude='*.md'
```

Expected: no matches in production code.
