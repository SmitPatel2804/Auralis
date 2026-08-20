# Bluetooth button event-path audit (Phase A)

**Machine:** development host for Auralis Phase 8 button policy  
**Date:** 2026-08-20  
**Scope:** Determine how Play/Pause (and related media keys) arrive for paired BlueZ audio devices before implementing ALLOW/DISALLOW.

## Devices observed

| Alias | Address | BlueZ path | Connected during audit |
|-------|---------|------------|------------------------|
| Aavante Bar A2060 Dolby | `41:42:E7:EB:DE:3E` | `/org/bluez/hci0/dev_41_42_E7_EB_DE_3E` | No |
| Rockerz 255 Touch | `EE:D0:0D:A4:1D:DA` | `/org/bluez/hci0/dev_EE_D0_0D_A4_1D_DA` | No |
| Smokin' Buds | `88:08:94:9D:B4:22` | `/org/bluez/hci0/dev_88_08_94_9D_B4_22` | No |

## BlueZ media interfaces

All three device objects expose `org.bluez.MediaControl1` (Connected=false while unpaired-from-transport / disconnected). No live `MediaPlayer1` child objects were present while disconnected. `MediaControl1` does **not** provide a documented intercept API that Auralis can use to drop AVRCP pass-through before the desktop receives it — Backend A is not viable as a fake MPRIS/D-Bus interceptor.

## Linux input / udev

- `/proc/bus/input/devices` contained only platform/USB/HID devices (keyboard, HyperX mouse, touchpad, ALSA, Dell WMI). **No** `Bus=0005` (Bluetooth) entries and no `Uniq=` matching the addresses above.
- `/dev/input/by-id` listed only USB HyperX entries.
- `udevadm` scan for `ID_BUS=bluetooth` on `/sys/class/input/event*` returned **empty** (expected while no BT audio device is connected and exposing a consumer-control HID/AVRCP input node).

`libinput debug-events` / `evtest` were not run against a live headset in this pass because no Bluetooth input node was present.

## Chosen V1 backend

**Backend B — evdev consumer-control suppression**, when a connected device exposes an input node whose udev/`Uniq`/`PHYS` correlates to the stable Bluetooth address.

When no correlatable evdev node exists (AVRCP-only / disconnected / kernel does not create input):

- Policy + UI + persistence still ship.
- Effective state is **Unsupported on this transport** (honest; no fake MPRIS intercept).
- DISALLOW must not disconnect, mute, or block A2DP.

## Live validation note

Connect a headset, re-check `/proc/bus/input/devices` and `ID_BUS=bluetooth` / `UNIQ`, then confirm `KEY_PLAYPAUSE` (or equivalent) on that node with `evtest` if `/dev/input/event*` permissions allow. If access is denied for the user session, document a udev `TAG+="uaccess"` / `MODE` rule — do not require root-by-default.
