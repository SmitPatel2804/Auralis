# Linux virtual audio output

Auralis uses PipeWire's loopback module on Linux. It does not need a kernel
driver, a Microsoft-style driver signature, or root privileges at runtime.

## User flow

1. Start Auralis.
2. On the Audio Routing page, click **USE AS SYSTEM OUTPUT** and leave it held.
   GNOME Settings often meters this loopback sink without making it the default,
   and Bluetooth sinks with higher session priority steal the default back to a
   headset. Auralis writes and reclaims the PipeWire default-sink metadata.
   Use **RELEASE SYSTEM OUTPUT** when you want the desktop to pick a headset or
   speakers again.
3. In Auralis, select **Auralis System Audio** as the route/session source.
   Routing a browser or other app stream instead of that source leaves one
   headset on the OS path and the other on Auralis, which sounds like lag.
4. Select one or more connected playback endpoints and activate the route or
   session. Auralis fans session members through a latency-compensated mix so
   host-visible delays are aligned automatically. Dual Bluetooth A2DP still
   cannot be sample-perfect when headsets do not report air delay.

Applications write once to the virtual sink. The paired PipeWire source is the
only object Auralis routes to the selected speakers or Bluetooth endpoints.
This avoids the duplicate direct-to-speaker path that occurs when an ordinary
physical output remains selected in the operating system.

## Lifetime and installation

The Linux package installs:

```text
/usr/share/pipewire/pipewire.conf.d/90-auralis-virtual-output.conf
```

PipeWire reads this vendor drop-in when the user's PipeWire daemon starts. A
logout/login is the least disruptive way to load it after the first package
installation. Developers may instead restart their user audio services:

```bash
systemctl --user restart pipewire pipewire-pulse wireplumber
```

That command temporarily interrupts all desktop audio. Do not run it while an
important recording or call is active.

Before that first restart, and for unpackaged build-tree runs, Auralis loads an
equivalent app-lifetime loopback module. This makes the endpoint available
immediately while Auralis is running. The Audio Routing page distinguishes the
persistent and app-lifetime states.

The persistent sink can remain listed after Auralis closes, but no Auralis
route is active while the application is closed. Select another system output
if audio should bypass Auralis.

## Graph identity and safety

The two stable node names are:

```text
auralis_virtual_output         Audio/Sink
auralis_virtual_output.source  Stream/Output/Audio
```

Both nodes carry `auralis.virtual.output=true` plus a sink/source role. Auralis
hides the sink from its destination list and hides any duplicate sink-monitor
source. The source has the stable application id
`src:auralis-system-audio`, so saved sessions survive PipeWire global-id churn
and daemon restarts.

The source stream disables automatic linking. Auralis creates only the links
for an activated route and selected destinations. The virtual sink deliberately
has a low session priority, so installation does not silently replace the
user's current default output. The sink half is not marked `node.virtual` and
disables idle suspend so Pulse clients such as pavucontrol can select it.
GNOME Settings still often omits this loopback sink; use **USE AS SYSTEM
OUTPUT** in Auralis instead of relying on that panel. The System Audio source
stream remains virtual and is not offered as a system output.

## Diagnostics

The Audio Routing card reports whether the virtual output is unavailable,
starting, ready for this app lifetime, persistent, or selected as the system
default. The standard execution log also records provisioning, default-sink
changes, graph readiness, and retry exhaustion.

Useful read-only cross-checks are:

```bash
wpctl status
pw-cli ls Node
```

Auralis itself does not execute or parse these command-line tools.

If the endpoint does not appear:

```bash
systemctl --user status pipewire wireplumber
journalctl --user -u pipewire -u wireplumber --since today
```

Confirm that `libpipewire-module-loopback` is installed. On Debian/Ubuntu it is
provided by the normal PipeWire module packages pulled in by the Auralis DEB.

## Validation

The deterministic tests cover metadata parsing, persistent/runtime identity,
port readiness, stable source classification, and feedback exclusion. The
opt-in live test starts from the real registry and requires the loopback graph
to become ready:

```bash
AURALIS_RUN_PIPEWIRE_INTEGRATION=1 \
ctest --test-dir build -R '^tst_PipeWireLiveIntegration$' --output-on-failure
```

Set `AURALIS_EXPECT_DEVICE_ADDRESS` to additionally require mapping to a real
Bluetooth audio endpoint. Final latency, drift, codec, and two-device sync
checks require the intended Linux computer and physical Bluetooth devices.

CI runs the same live test twice against isolated PipeWire daemons: once with
the app-lifetime fallback and once with the packaged drop-in. This validates
both provisioning paths and the package configuration syntax.
