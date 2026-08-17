# 8. Session State and Recovery

## 8.1 Session goals

A session contains:

- one selected audio source;
- zero or more selected hearing devices;
- one active source timeline;
- one output session per active device;
- synchronization state;
- diagnostics.

## 8.2 Session state machine

```text
Idle
 |
Configure
 v
Preparing
 |
 +--> Error
 |
 v
Ready
 |
Start
 v
Buffering
 |
 v
Playing
 |
 +--> Degraded
 |      |
 |      +--> Reconnecting
 |               |
 |               v
 |            Resyncing
 |               |
 |               v
 +------------ Playing
 |
Stop
 v
Stopping
 |
 v
Ready / Idle
```

## 8.3 Device fault isolation

Default V1 policy proposal:

> If one hearing device disconnects, unaffected devices continue playback.

This follows the source specification's requirement that a single disconnect should not require restarting the entire session.

## 8.4 Rejoin sequence

For Device B:

```text
B disappears
   |
mark B disconnected/degraded
   |
keep A/C running
   |
reconnect B
   |
wait for audio endpoint
   |
recreate output stream
   |
prefill B queue
   |
calculate/apply sync target
   |
rejoin B
```

## 8.5 Join during playback

If a new device is added while audio is already running:

1. connect it;
2. wait until audio-ready;
3. create output session;
4. prebuffer;
5. apply sync policy;
6. join at a controlled timeline point.

This behavior is not explicitly specified in the source and should be treated as a V1 product decision.

## 8.6 Restart behavior

Application restart should not corrupt OS Bluetooth pairing state.

Potential policy:

- load known device records;
- discover current adapter/device state;
- restore logical device list;
- do not automatically start audio without user intent;
- allow fast reconnect.

## 8.7 Error model

Errors should include:

- human-readable category;
- machine-readable code;
- component;
- device/session ID;
- timestamp;
- underlying system error where available.

Example:

```json
{
  "event": "output_stream_failed",
  "device_id": "device-b",
  "component": "audio_output",
  "session_id": "2026-08-17-001",
  "reason": "endpoint_removed"
}
```
