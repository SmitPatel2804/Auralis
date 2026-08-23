# Auralis actual-Linux validation — 2026-08-23

**Verdict: READY FOR LINUX HARDWARE BETA**

Decisive evidence:

- Deterministic CTest **51/51 PASS** on `build/linux-low-resource` and again on `build-linux-sanitized` (ASan+UBSan, `--parallel 1`, `ctest -j1`). No AddressSanitizer/UBSan reports in that CTest log or in a 12 s ASan GUI smoke (`timeout` 124, QML loaded, Virtual Output ready, default sink not stolen).
- After the user put **Smokin' Buds** and **Rockerz 255 Touch** in pairing mode: both paired/connected with A2DP sinks. OS scan while they stayed connected succeeded. Phase 7 live hardware: **single-device session Active**; **two-device session** created two Session routes that both reached **Active** (brief `Starting→Degraded→Active` while the second link bound). **Zero** `LinkCreationFailed` lines. Default sink was restored to built-in analog so OS BT default would not double the route. BT sink volumes were 8%.
- Extracted DEB/TGZ smoke **PASS** after the uncommitted distro-Qt packaging fix.
- Remaining blocks are non-core for audio routing beta: no `dpkg -i`, no PW/WP restart, no adapter toggle, no suspend, no 20-minute endurance, live ALLOW/DISALLOW events not exercised (`input` group missing; AVRCP `Uniq=` empty so policy cannot match headset MACs — UI should remain Unsupported, not a silent allow).

---

## 1. Host and tool versions

| Item | Value |
|---|---|
| Date | 2026-08-23, Asia/Kolkata (UTC+5:30) |
| Distro | Ubuntu 26.04 LTS |
| Kernel | 7.0.0-29-generic x86_64 |
| Session | Wayland (`DISPLAY=:0`) |
| RAM / swap (during work) | 7.1 GiB total, ~4.0 GiB available, 4.0 GiB swap unused |
| Disk (repo) | 147G filesystem, ~118G free |
| `ulimit -n` | 524288 |
| Qt | 6.10.2 (distro `/usr`; project minimum 6.8) |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| g++ | 15.2.0 |
| PipeWire | 1.6.2 |
| WirePlumber | 0.5.13 |
| BlueZ (`bluetoothctl`) | 5.85 |
| User services | `pipewire`, `pipewire-pulse`, `wireplumber` active |
| Default sink | built-in analog stereo except while OS auto-switched to a BT sink on connect; restored to analog before Auralis routing tests |
| `lintian` / `patchelf` | not installed (not apt-installed) |

Resource snapshot before the low-resource build (and rechecked later) showed enough headroom for **one** build job and **ctest -j1**. Jobs were never derived from `nproc`. Builds, CTest, packaging, the GUI, and sanitizers were not overlapped.

## 2. Git state

- Branch: `master`
- Starting / HEAD commit: `1220656` — *Reduce Linux CPU and RAM use by coalescing graph and session work, and add a bounded low-resource build preset.*
- Prior in-tree Linux fixes already committed as `9a38cd1` (virtual-output port wait, Linux sound-settings invokable, System Audio not labeled `(mic)`). Those are part of the tree under test, not additional dirty files.
- Pre-existing trees **left untouched** as build sources: root `build/`, `build-linux-validation`, incomplete-then-reused `build-linux-package`. Primary validation tree: **`build/linux-low-resource`**.

Uncommitted at completion (packaging only):

- `apps/desktop/CMakeLists.txt`
- `cmake/AuralisPackaging.cmake`

`git diff --check`: clean (no whitespace errors).

## 3. Build and commands

Primary:

```bash
AURALIS_BUILD_JOBS=1 bash scripts/validation/run-linux-low-resource.sh
```

Preset: Release, IPO off, sanitizers off, Ninja, tests ON, `--parallel 1`, `ctest -j1`.

Binary: `build/linux-low-resource/apps/desktop/auralis-desktop`  
`file`: ELF 64-bit LSB pie, x86-64, dynamically linked, not stripped.  
`ldd`: no `not found`.

Isolated PipeWire helper (after CTest, not concurrent with the host GUI):

```bash
bash scripts/ci/run-pipewire-virtual-output.sh runtime build/linux-low-resource
bash scripts/ci/run-pipewire-virtual-output.sh persistent build/linux-low-resource
```

Packaging (serial, after tests): `build-linux-package` Release, `-DAURALIS_BUILD_TESTS=OFF`, `--parallel 1`, `cpack` DEB+TGZ. Artifacts at repo root:

- `auralis_0.1.0_amd64.deb`
- `auralis-0.1.0-x86_64.tar.gz`

`dpkg -i` was **not** run.

Sanitizers (user-approved, after live BT): **run**.

```bash
cmake -S . -B build-linux-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAURALIS_BUILD_TESTS=ON \
  -DAURALIS_ENABLE_SANITIZERS=ON
cmake --build build-linux-sanitized --parallel 1
ctest --test-dir build-linux-sanitized --output-on-failure --timeout 180 -j1
```

Build ~18 min serial; CTest 51/51 in 44 s. ASan GUI smoke: `build-linux-sanitized/apps/desktop/auralis-desktop` ~12 s.

## 4. Automated tests

| Suite | Result | Notes |
|---|---|---|
| CTest `build/linux-low-resource` | **51/51 PASS**, 0 failed | Default CTest is not hardware proof. Opt-in live tests **QSKIP** unless env is set; CTest still records Passed. |
| `AURALIS_RUN_PIPEWIRE_INTEGRATION=1` | **PASS** | Virtual Output ready; default sink remained analog-stereo. |
| `AURALIS_RUN_BLUETOOTH_INTEGRATION=1` | **PASS** | 1 adapter, 2 paired devices, scan start/stop. Side effect: auto-reconnect attempts to both paired devices. |
| `AURALIS_RUN_SESSION_INTEGRATION=1` | **PASS** including two-device add | Addresses only on the command line; both addDevice calls Accepted. |
| `AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1` | **PASS** | Low-amplitude tone to first available sink (built-in). |
| `AURALIS_RUN_PHASE7_HARDWARE=1` | **PASS** (4 passed, 0 failed) | Single session Active; two-device Connected 2/2, mappedBt=4, two routes Active. Discovery ran while both were already connected. Test cleanup disconnects members. |
| Windows-only opt-in tests | **NOT APPLICABLE** | |
| Isolated helper runtime | **PASS** | |
| Isolated helper persistent | **PASS** | |
| CTest `build-linux-sanitized` | **51/51 PASS** | ASan+UBSan; no sanitizer diagnostics in the CTest log. |

## 5. Phase results

| Phase | Result | Evidence (short) |
|---|---|---|
| 1 Audit | **PASS** | Host/git/resource snapshot recorded. Qt 6.10.2 / PipeWire 1.6.2 vs project min Qt 6.8. Extra build dirs preserved. |
| 2–3 Configure/build/CTest | **PASS** | Low-resource runner; `file`/`ldd` clean. QSKIP treated as skip, not live PASS. |
| 4 Isolated PipeWire | **PASS** | Helper against `build/linux-low-resource`; one Virtual Output + System Audio; teardown; not overlapped with host GUI. |
| 5 Packaging | **PASS** after fix (extract smoke) | First `cpack -G DEB` **FAIL** on distro QML `RPATH_SET`. After uncommitted fix: DEB+TGZ built; extract launch QML loaded, Virtual Output ready, default sink not stolen, `timeout` 124; `ldd` has no build-tree refs. **No `dpkg -i`.** `lintian` N/A (not installed). |
| 6 Live desktop / virtual output | **PASS** (runtime) | Unpackaged GUI: QML root loaded; runtime `auralis_virtual_output` + `auralis_virtual_output.source`; `VirtualOutputReady selected=false persistent=false`; nodes gone after exit; second start no leftover duplicates; default sink never stolen. Persistent **install** path **BLOCKED** (no `dpkg -i` / user-service restart). |
| 7 BlueZ discovery / management | **PASS** (core); adapter toggle **BLOCKED** | Pairing-mode re-pair: both Classic headsets Connected with A2DP. GUI/OS scan while connected did not drop them. Auralis `DiscoveryStarted` during Phase 7 two-device while 2/2 already connected. Soundbar still page-timeout on auto-reconnect. App “Scan BLE” UI not clicked. Adapter off/on not done. |
| 8 System-audio routing / sessions | **PASS** (core) | `paplay` is `Stream/Output/Audio`; Phase 7 tone source routed to BT playback endpoints. OS default set back to analog before routing. 10× route churn and GUI Sessions screenshot not done. Latency: 48000 Hz / quantum 1024; no E2E offset. |
| 9 Multi-device session / endurance | **PASS** (activate); endurance **BLOCKED** | Two Session routes both Active; no `LinkCreationFailed`. Aug 20 dest-77 failure **not reproduced**. 20-minute endurance not run (RAM/time after sanitizers). |
| 10 ALLOW/DISALLOW | **BLOCKED** (non-core) | While connected, AVRCP nodes `/dev/input/event16` and `event17` exist (`root:input`, user **not** in `input`). `Uniq=` empty; `Phys=` is the **adapter** address, so `probeInputs()` cannot match headset MACs. Live grab not attempted. Honest Unsupported expected. |
| 11 Failure injection | **PARTIAL / BLOCKED** | Second instance / fresh XDG as before. Phase 7 cleanup disconnects session members (observed). PW/WP restart, adapter toggle, suspend **BLOCKED**. |
| 12 Logging | **PASS** | Env and fresh-profile file logs as before. Phase 7 log correlated launch → mapped BT endpoints → RouteActive → deactivate. |
| 13 Sanitizers | **PASS** | `build-linux-sanitized` Debug ASan+UBSan; 51/51 CTest; 12 s GUI smoke with no sanitizer diagnostics. Peak available RAM stayed ~3–4 GiB. |

## 6. Defects found and fixed this run

### Distro Qt DEB/TGZ bundling (`file(RPATH_SET)` failure)

- **Reproduction:** `cpack -G DEB` on this host with Qt from `/usr`.
- **Expected:** package is produced.
- **Actual:** `file RPATH_SET could not write new RPATH` on `qml/QtQuick/libqtquick2plugin.so` because distro QML plugins have no `DT_RPATH`.
- **Root cause:** Qt deploy’s `file(RPATH_SET)` requires an existing RPATH; CI aqt trees still have one, so CI bundling is unchanged.
- **Fix (uncommitted):** if `Qt6_DIR` is `/usr/...` and `patchelf` is missing, set `AURALIS_LINUX_BUNDLE_QT=OFF` and skip `install(SCRIPT)` deploy; declare Debian Qt/QML depends instead. If `patchelf` exists, set `QT_DEPLOY_USE_PATCHELF=ON`.
- **Files:** `apps/desktop/CMakeLists.txt`, `cmake/AuralisPackaging.cmake`.
- **Retest:** DEB+TGZ generated; extract-and-smoke launched; Depends include `libqt6core6t64`, `qml6-module-qtquick*`, `qt6-qpa-plugins`, `qt6-wayland`, plus PipeWire/BlueZ. Cache: `AURALIS_LINUX_BUNDLE_QT=OFF`.
- **Regression test:** none added (packaging path). Do not treat CI aqt bundling as covered by this host.

DEB also contains `/usr/share/pipewire/pipewire.conf.d/90-auralis-virtual-output.conf` and the desktop entry. Persistent behavior after install was not tested.

## 7. PipeWire node evidence

Runtime (unpackaged GUI):

- Sink `auralis_virtual_output` / **Auralis Virtual Output**
- Source `auralis_virtual_output.source` / **Auralis System Audio**
- `VirtualOutputReady selected=false persistent=false` (does not steal default)
- After process exit: **no leftover** `auralis` nodes
- Isolated helper: runtime and persistent modes **PASS** (idempotence/teardown per helper)

Graph quantum/rate: 48000 Hz, quantum 1024 (min 32, max 2048).

## 8. BlueZ / device evidence (addresses redacted)

Earlier the same day, the headset failed with `br-connection-key-missing` and the soundbar with `br-connection-page-timeout`.

Continuation (user pairing mode):

- **Rockerz 255 Touch**: pair + trust + connect **PASS**; A2DP sink appeared; Classic (Class of Device, Audio Sink UUID).
- **Smokin' Buds**: remove + re-pair (key-missing) then connect **PASS**; A2DP sink appeared.
- Both stayed Connected during GUI + OS scan. Phase 7 logged `Connected 2/2` and `DiscoveryStarted` while they were already connected.
- Soundbar remains paired; auto-reconnect still hits page-timeout (not used as a session member).
- Phase 7 cleanup issues `DisconnectRequested` on session members. A later ASan GUI launch showed the two headsets Connected again (auto-reconnect).

OS discovery also saw nearby unpaired advertisements (redacted). Auralis GUI BLE list was not clicked.

## 9. Routing topology

- WirePlumber switched the **OS default sink** to a BT output on connect. Restored to built-in analog **before** Auralis routing so the session was not a valid doubling test against OS-BT + Auralis-BT.
- Phase 7 single: one Session route, links to the headset playback node, **Active**.
- Phase 7 pair: two Session routes (one dest each). First route Active caused a brief session **Degraded**, then second route Active → session **Active**. Destinations were the two `bluez_output.*` playback nodes (not Virtual Output). **No `LinkCreationFailed`.**
- Inaudible `paplay --volume 0`: `Stream/Output/Audio` vs physical `Audio/Sink`.
- Aug 20 dest-77 failure not reproduced on this pair.
- Latency: 48000 Hz, quantum 1024; no loopback offset.

## 10. Packaging

| Check | Result |
|---|---|
| Architecture | amd64 |
| Generators | DEB + TGZ |
| Qt strategy on this host | unbundled; Debian Qt/QML depends |
| Extracted launch | PASS (timeout, not crash) |
| Build-tree library refs | none observed |
| PipeWire conf in package | present under `/usr/share/pipewire/pipewire.conf.d/` |
| `dpkg -i` | not run |
| `lintian` | not installed |

## 11. Logging excerpts (redacted)

Host preference `fileEnabled=false` — empty user log dir until env override.

Env-enabled file log (`AURALIS_LOG_FILE_ENABLED=1`, path under `/tmp/…`; user conf left `fileEnabled=false`):

```
INFO ui QML root loaded
INFO bluetooth BlueZSnapshotApplied adapters= 1 devices= 2
INFO audio PipeWire VirtualOutputCreateRequested persistence=runtime
INFO audio PipeWire VirtualOutputReady selected= false persistent= false
INFO bluetooth ManagedReconnectTerminalFailure "<redacted soundbar path>" attempts= 1 "br-connection-page-timeout"
```

Fresh XDG profile (did not touch `~/.config/Auralis`): created  
`/tmp/auralis-fresh-data-20260823/Auralis/Auralis/logs/auralis-20260823-122604-….log`.

## 12. Sanitizer results

- Tree: `build-linux-sanitized`, Debug, `AURALIS_ENABLE_SANITIZERS=ON`, `--parallel 1`.
- Compile: 384/384, **BUILD_EXIT:0** (~18 min). Available RAM stayed above 3 GiB; swap unused beyond ~1 MiB.
- CTest: **51/51 PASS**, 44 s, `-j1`, timeout 180. No `AddressSanitizer` / `LeakSanitizer` / UBSan `runtime error` lines in `/tmp/auralis-asan-ctest.log`.
- GUI smoke: 12 s, QML root loaded, `VirtualOutputReady selected=false`, leftover nodes none, no sanitizer lines on stderr. `detect_leaks=0` for the GUI only (Qt leak noise); CTest used `detect_leaks=1` with `halt_on_error=0`.

## 13. Remaining limitations

- **Commit the packaging fix** before shipping a distro-Qt DEB; HEAD `1220656` still fails `cpack` without `patchelf`.
- Live ALLOW/DISALLOW: user not in `input`; AVRCP `Uniq` empty. Least-privilege path is adding the user to `input` or a udev tag for those two event nodes — not done.
- 20-minute endurance, `dpkg -i`, PW/WP restart, adapter toggle, suspend: not run.
- Auto-reconnect still hammers the unreachable soundbar (terminal failure after page-timeout).
- Two GUI instances / no single-instance lock.
- GUI Sessions page not screenshot-verified.
- `patchelf` / `lintian` not installed.
- Phase 7 plays a tone and **disconnects** members in cleanup; volume was 8%.

## 14. Git status at completion

```
 M apps/desktop/CMakeLists.txt
 M cmake/AuralisPackaging.cmake
?? docs/validation/linux-actual-machine-validation-2026-08-23.md
```

```
 apps/desktop/CMakeLists.txt  | 63 +++++++++++++++++++++++++++++---------------
 cmake/AuralisPackaging.cmake |  7 +++++
 2 files changed, 49 insertions(+), 21 deletions(-)
```

`git diff --check`: no issues.

HEAD remains `1220656`. Nothing committed or pushed.

---

**Overall: READY FOR LINUX HARDWARE BETA.**

Packaging on this host needs the uncommitted distro-Qt change. Core PipeWire + two-headset session routing passed; sanitizers were clean on the serial Debug suite.
