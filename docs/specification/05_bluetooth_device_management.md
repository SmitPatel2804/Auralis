# 5. Bluetooth Device Management

## 5.1 Purpose

The Bluetooth subsystem is responsible for making target devices discoverable and manageable from the application while respecting what the operating system exposes.

The product requirement is that the user can place a hearing device into normal pairing mode and operate the pairing/connection flow from the Auralis experience.

The implementation may orchestrate OS Bluetooth behavior rather than implementing a raw Bluetooth stack itself.

## 5.2 Device lifecycle

```text
Not Known
   |
Scan
   v
Discovered
   |
Pair
   v
Pairing --> Failed
   |
   v
Paired
   |
Connect
   v
Connecting --> Failed
   |
   v
Connected
   |
Wait for usable audio endpoint
   v
Audio Ready
```

## 5.3 Discovery

Bluetooth discovery should:

- identify the active adapter;
- expose device name/address/identity available to the application;
- update RSSI only if relevant/available;
- avoid duplicate UI rows for the same physical device;
- preserve previously paired devices in the model.

## 5.4 Pairing

Pairing UX should expose:

- “Pairing…” state;
- success/failure;
- any required confirmation flow;
- timeout;
- retry.

Pairing must be treated separately from audio-route readiness.

## 5.5 Connection

After pairing, the application should request connection and observe:

- Bluetooth connection state;
- relevant profile state where visible;
- whether PipeWire exposes an audio sink;
- whether an output stream can actually be created.

A device is only considered **Auralis Ready** when the required audio route exists and has been validated.

## 5.6 Device-to-audio-endpoint mapping

A critical implementation task is correlating:

```text
BlueZ device
      |
      +----> PipeWire Bluetooth audio node/sink
```

The mapping mechanism must be tested against the selected Linux environment.

The application should cache a stable internal device ID but must handle endpoint recreation across reconnects.

## 5.7 Reconnection

Recommended baseline:

- detect unexpected disconnect;
- mark only that device degraded;
- keep unaffected devices playing;
- retry using bounded backoff;
- when the endpoint reappears, recreate/rebind its output session;
- refill buffer;
- resynchronize;
- rejoin the playback group.

## 5.8 Capability matrix

For every target model capture:

| Field | Example value |
|---|---|
| Manufacturer | TBD |
| Model | TBD |
| Bluetooth version | TBD |
| Audio technology/profile | TBD |
| Standard sink visible | Yes/No/TBD |
| Pairing flow | TBD |
| Same-model simultaneous test | Pass/Fail |
| Mixed-model test | Pass/Fail |
| Notes | TBD |

This matrix is a Phase 0 deliverable from the source specification.

## 5.9 Failure classes

Distinguish:

- adapter unavailable;
- discovery failure;
- pairing rejected;
- pairing timeout;
- connection failure;
- connected but no audio endpoint;
- endpoint appears then disappears;
- output stream creation failure;
- radio/interference dropout;
- device power-off;
- device sleep.

Each should result in a structured diagnostic event.
