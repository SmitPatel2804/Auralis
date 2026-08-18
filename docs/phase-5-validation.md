# Phase 5 Validation — Audio Routing Engine

Phase 5 adds an **additive** native PipeWire routing engine on top of Phase 4 graph observation. Auralis creates and destroys only links it owns. It does not hijack WirePlumber, does not delete other clients' links, and does not grow `SessionManager`.

## Source types

| Type | Classification |
|---|---|
| Application playback stream | `media.class=Stream/Output/Audio` with audio output ports |
| Physical capture source | `media.class=Audio/Source` (non-virtual) |
| Virtual source | `Audio/Source` with `node.virtual` or adapter factory without `device.api` |
| Sink monitor | `Audio/Sink` **monitor output ports** (not monitor nodes as destinations) |

Capture streams (`Stream/Input/Audio`), control/MIDI ports, and ordinary sink input ports are not sources. Destination classification remains the Phase 4 sink-only `AudioEndpoint` path. Source ids are `src:{serial|nodeName}:{mediaClass}` (or `:SinkMonitor`). Port ids are never persisted as identity.

## Volume limits

- Normalized range `0.0..1.0`, clamped; mute is a boolean.
- Applied with `SPA_PROP_volume` / `SPA_PROP_mute` on the destination node proxy when present.
- Unsupported nodes report `VolumeControlUnsupported` and do **not** fail the route.
- Route volume fans out to every destination; a partial apply is reported if some destinations fail.
- There is no loudness-normalization DSP.

## Live commands

Default `ctest` skips live routing. Enable explicitly:

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 ctest --test-dir build -R tst_AudioRoutingLiveIntegration --output-on-failure
```

Optional Bluetooth destination check (Smokin' Buds used in Phase 4 live mapping):

```bash
AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 AURALIS_EXPECT_DEVICE_ADDRESS="88:08:94:9D:B4:22" ctest --test-dir build -R tst_AudioRoutingLiveIntegration --output-on-failure
```

Workflow: PipeWire ready → pick a playback source (or the **test-only** low-volume `pw_stream` sine in the test binary) → playback endpoint → create/activate → observe Auralis-owned links → deactivate → owned links gone.

`pw_stream` exists only in `tst_AudioRoutingLiveIntegration`. Production `include/`, `src/`, and `apps/` do not create streams or call `pw-link` / `wpctl`.

## Fan-out and WirePlumber limits

- One source can fan out to multiple playback endpoints (stereo → two sinks = four owned links).
- Additive routing **adds** Auralis links. WirePlumber may still keep default playback links. Deactivate removes only Auralis-owned links.
- Some Bluetooth/USB sinks reject a second input graph or pause when another exclusive stream holds the device. That appears as `Degraded` / `LinkEnteredErrorState`, not as destruction of foreign links.
- Reconnect uses **RebindOnGraphReplacement**: while a route stays logically enabled, new node ids for the same source/destination identity are replanned. Stale PipeWire ids are never reused.

## Unit coverage

| Target | Covers |
|---|---|
| `tst_AudioRoute` | ids, states, duplicate dests, structured errors |
| `tst_AudioSources` | stream / source / sink-not-source / monitor ports |
| `tst_RoutePlanner` | mono, stereo, 1→2 dests (4 pairs), reversed registry, missing channels, control ports, dest gone |
| `tst_AudioRouter` | state machine, rollback, dest/source loss, rebind, disconnect, activate/deactivate races |
| `tst_VolumeController` | clamp, mute, unsupported, partial route volume |
