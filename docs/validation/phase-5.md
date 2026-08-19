# Phase 5 Validation — Audio Routing Engine

Location: `docs/validation/phase-5.md` (moved from `docs/phase-5-validation.md`
during documentation reorganization).

Validated: **2026-08-19**. Closure evidence: [phase-5-audit.md](phase-5-audit.md).

Phase 5 adds an **additive** native PipeWire routing engine on top of Phase 4 graph observation. Auralis creates and destroys only links it owns. It does not hijack WirePlumber, does not delete other clients' links, and does not grow `SessionManager`.

## Ownership token / global-ID invariant

Ownership is an opaque `ownershipToken` issued at `pw_core_create_object` time, **before** a global id is assigned. `globalId == 0` means the proxy is pending bind, not that the link is unowned. Destroy, rollback, and shutdown always go by token, including pending proxies. A foreign WirePlumber link on the same port pair is never treated as owned and never makes a route operational.

## Source types

| Type | Classification |
|---|---|
| Application playback stream | `media.class=Stream/Output/Audio` with audio output ports |
| Physical capture source | `media.class=Audio/Source` (non-virtual) |
| Virtual source | `Audio/Source` with `node.virtual` or adapter factory without `device.api` |
| Sink monitor | `Audio/Sink` **monitor output ports** (not monitor nodes as destinations) |

Capture streams (`Stream/Input/Audio`), control/MIDI ports, and ordinary sink input ports are not sources. Destination classification remains the Phase 4 sink-only `AudioEndpoint` path. Source ids are `src:{serial|nodeName}:{mediaClass}` (or `:SinkMonitor`). Port ids are never persisted as identity.

## Disconnect, timers, and recovery

- An enabled route whose PipeWire connection enters `Error` / `Stopping` / `Stopped` becomes **Degraded** with `PipeWireDisconnected`. Owned tokens are destroyed; the route does not stay `Active`.
- Replan/activation runs only when the connection is `Connected` **and** initial registry sync has completed. `Connected` without sync must not resurrect links.
- Fresh rebind after reconnect uses **new** tokens and global ids.
- Source or destination disappearance: **Degraded**, structured `SourceRemoved` / `DestinationRemoved`, remaining owned links destroyed. Logical source/destination ids are kept for `RebindOnGraphReplacement`.
- Each activate/replan attempt has its own `QTimer` and generation. Success or timeout on route A does not cancel route B. Stale generation callbacks are ignored. `removeRoute` and `shutdown` stop those timers.

## Volume limits

- Normalized range `0.0..1.0`, clamped; mute is a boolean.
- Applied with `SPA_PROP_volume` / `SPA_PROP_mute` on the destination node proxy when present.
- Volume capability requires a bound Node that advertises writable `SPA_PARAM_Props`. It is not inferred from “any Node”.
- Unsupported nodes report `VolumeControlUnsupported` and do **not** fail the route.
- Route volume fans out to every destination; a partial apply is reported if some destinations fail.
- There is no loudness-normalization DSP.

## Clean build and tests

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "tst_AudioRoute|tst_AudioSources|tst_RoutePlanner|tst_AudioRouter|tst_VolumeController" --output-on-failure
./build/tests/unit/audio/tst_AudioRouter -v2
```

## Live commands

Default `ctest` skips live routing. Enable explicitly:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 ctest --test-dir build -R tst_AudioRoutingLiveIntegration --output-on-failure
```

Exact Bluetooth destination (Smokin' Buds):

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" ctest --test-dir build -R tst_AudioRoutingLiveIntegration --output-on-failure
```

If `AURALIS_EXPECT_DEVICE_ADDRESS` is set and that Bluetooth endpoint is missing, the live test **fails** and dumps enumerated endpoints. It does not fall back to a built-in speaker or the first available sink.

After deactivate the live test requires **both**:

- `router->ownedLinkCount() == 0`
- `PipeWireObjectStore` contains zero links with `auralis.route.id` equal to the deactivated route

Workflow: PipeWire ready → pick a playback source (or the **test-only** low-volume `pw_stream` sine in the test binary) → playback endpoint → create/activate → observe Auralis-owned links → deactivate → owned bookkeeping empty **and** the object store has no remaining tagged link.

`pw_stream` exists only in `tst_AudioRoutingLiveIntegration`. Production `include/`, `src/`, and `apps/` do not create streams or call `pw-link` / `wpctl`.

## Fan-out and WirePlumber limits

- One source can fan out to multiple playback endpoints (stereo → two sinks = four owned links).
- Additive routing **adds** Auralis links. WirePlumber may still keep default playback links. Deactivate removes only Auralis-owned links.
- Some Bluetooth/USB sinks reject a second input graph or pause when another exclusive stream holds the device. That appears as `Degraded` / `LinkEnteredErrorState`, not as destruction of foreign links.
- Reconnect uses **RebindOnGraphReplacement**: while a route stays logically enabled, new node ids for the same source/destination identity are replanned. Stale PipeWire ids and tokens are never reused.

## Unit coverage

| Target | Covers |
|---|---|
| `tst_AudioRoute` | ids, states, duplicate dests, structured errors |
| `tst_AudioSources` | stream / source / sink-not-source / monitor ports |
| `tst_RoutePlanner` | mono, stereo, 1→2 dests (4 pairs), reversed registry, missing channels, control ports, dest gone |
| `tst_AudioRouter` | state machine, pending-token rollback, dest/source loss, rebind, disconnect → Degraded, foreign same-port link ignored, owned-link Error, remove/deactivate while activating, stale timeout generation, shutdown timers, independent per-route timeouts, QML NOTIFY |
| `tst_VolumeController` | clamp, mute, unsupported, partial route volume |
