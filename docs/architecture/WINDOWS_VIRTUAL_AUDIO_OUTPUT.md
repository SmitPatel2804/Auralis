# Windows virtual audio output architecture

## Goal

Expose **Auralis Virtual Output** in Windows sound settings. When a user selects it, Windows renders application/system audio into Auralis instead of a physical speaker. Auralis then fans the single captured stream out to the Bluetooth endpoints selected in a session.

This removes the current copy-capture topology that can produce a direct Windows path plus a delayed Auralis path to the same headset.

## Supported design

1. A WaveRT virtual-render endpoint derived from Microsoft's SysVAD architecture accepts the Windows audio-engine stream.
2. A user-mode bridge transports PCM frames and timing metadata from the driver to the Auralis desktop process through a narrowly scoped, access-controlled interface.
3. Auralis owns buffering, clock alignment, resampling and fan-out to selected endpoints.
4. The UI detects driver availability, reports its version/health, and only offers exclusive virtual-output mode when the bridge is healthy.
5. Uninstall and crash behavior restore a physical Windows default endpoint and never strand the user without audio.

Microsoft references:

- SysVAD sample: https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/
- Process loopback sample used by the current fallback mode: https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/
- Driver signing policy: https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing
- Driver security checklist: https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/driver-security-checklist

## Why it is a separate deliverable

A production virtual endpoint is a Windows driver, not an ordinary Qt feature. It needs the WDK, administrator-approved installation, Microsoft-compatible signing, security review, Driver Verifier/HLK coverage and dedicated failure/recovery testing. An unsigned test driver must not be placed in the normal desktop ZIP or installed silently.

Per-device Bluetooth media-button suppression is also **not** provided by the audio driver. Windows AVRCP/HID control enforcement needs a separately designed and signed media-control component because the current desktop APIs do not reliably expose the originating headset for system-wide suppression.

## Current safe fallback

The desktop application uses Windows process-loopback capture. This captures an endpoint-independent copy of an application's rendered audio but does not intercept the application's original Windows output. Auralis therefore rejects a copy-mode route whose destination is the current Windows default endpoint. The rejection prevents delayed double playback and explains how to resolve it.

No raw PCM content is written to diagnostic logs. Logs contain lifecycle events, device/route identifiers, formats, packet/byte totals and errors.

## Release gates

- Reproducible MSVC + WDK build and source/license audit.
- Least-privilege IPC threat model and fuzz tests.
- Multi-client, format-change, suspend/resume, hot-unplug and crash recovery tests.
- Clock-drift and end-to-end latency measurements across at least two Bluetooth devices.
- Driver Verifier and HLK/attestation path completed.
- Signed installer/uninstaller with rollback and restore-default-output behavior.
- Desktop executable and driver binaries code-signed before antivirus/reputation validation.
