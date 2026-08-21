# Windows virtual audio output architecture

## Goal

Expose **Auralis Virtual Output** in Windows sound settings. When a user selects it, Windows renders application/system audio into Auralis instead of a physical speaker. Auralis then fans the single captured stream out to the Bluetooth endpoints selected in a session.

This removes the current copy-capture topology that can produce a direct Windows path plus a delayed Auralis path to the same headset.

## Supported design

1. A WaveRT virtual-render endpoint derived from Microsoft's SysVAD architecture accepts the Windows audio-engine stream.
2. The endpoint's loopback pin copies the actual Windows audio-engine render ring. The desktop process reads it through standard event-driven WASAPI loopback; there is no Auralis IOCTL or private kernel IPC surface.
3. Auralis owns user-mode buffering, format conversion and fan-out to selected endpoints.
4. The UI detects driver availability and whether Windows currently has the virtual endpoint selected.
5. Uninstall and crash behavior restore a physical Windows default endpoint and never strand the user without audio.

Microsoft references:

- SysVAD sample: https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/
- Process loopback sample used by the current fallback mode: https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/
- Driver signing policy: https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing
- Driver security checklist: https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/driver-security-checklist

## Why it is a separate deliverable

A production virtual endpoint is a Windows driver, not an ordinary Qt feature. It needs the WDK, administrator-approved installation, Microsoft-compatible signing, security review, Driver Verifier/HLK coverage and dedicated failure/recovery testing. The app cannot safely create a new kernel endpoint on every launch. A signed installer installs it once; Windows then keeps the endpoint registered and Auralis detects it on every launch. An unsigned test driver must not be placed in the normal desktop ZIP or installed silently.

Per-device Bluetooth media-button suppression is also **not** provided by the audio driver. Windows AVRCP/HID control enforcement needs a separately designed and signed media-control component because the current desktop APIs do not reliably expose the originating headset for system-wide suppression.

## Implemented application path

The repository contains the x64 driver source and a buildable but unsigned
package. It exposes only **Auralis Virtual Output**, with no fake
microphone and no custom control device. The desktop app recognizes that render
endpoint, prevents routing it back into itself, exposes **Auralis System Audio**
as the session source and captures its mix using event-driven WASAPI loopback.

The package is deliberately excluded from normal CPack output until production
signing is configured. Shipping an unsigned kernel driver in the desktop ZIP
would create both a broken install experience and an avoidable security/reputation
risk.

## Safe fallback

The desktop application uses Windows process-loopback capture. This captures an endpoint-independent copy of an application's rendered audio but does not intercept the application's original Windows output. Auralis therefore rejects a copy-mode route whose destination is the current Windows default endpoint. The rejection prevents delayed double playback and explains how to resolve it.

No raw PCM content is written to diagnostic logs. Logs contain lifecycle events, device/route identifiers, formats, packet/byte totals and errors.

## Release gates

- Reproducible MSVC + WDK build and source/license audit.
- Standard-WASAPI boundary review and malformed-format/stream-state tests.
- Multi-client, format-change, suspend/resume, hot-unplug and crash recovery tests.
- Clock-drift and end-to-end latency measurements across at least two Bluetooth devices.
- Driver Verifier and HLK/attestation path completed.
- Signed installer/uninstaller with rollback and restore-default-output behavior.
- Desktop executable and driver binaries code-signed before antivirus/reputation validation.
