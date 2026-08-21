# Auralis Virtual Audio driver

This Windows-only WaveRT driver exposes one render endpoint named **Auralis
Virtual Output**. Windows applications render to that endpoint. The desktop app
then reads the endpoint mix through standard WASAPI loopback and sends it to the
outputs selected in Auralis.

The driver intentionally exposes no microphone, custom IOCTL, private shared
memory interface, Bluetooth sideband device, USB sideband device, or bundled
audio-processing object. Its device-object ACL permits only SYSTEM and
administrators; normal audio access goes through the Windows audio service.

## Source and license

The implementation is derived from Microsoft's SysVAD sample in
`microsoft/Windows-driver-samples` commit
`717778a20ba4dd2440fe609f69153a1f8a64f597`. Microsoft's MS-PL is preserved in
`LICENSE.microsoft-samples.txt`. Auralis changes the endpoint set, INF identity,
security policy, and SysVAD loopback path so it copies the actual system render
ring instead of generating the sample's synthetic tone.

## Build

Requirements:

- Visual Studio 2022 C++ Build Tools, including matching Spectre-mitigated libs
- The pinned Windows WDK/SDK NuGet packages (10.0.26100.6584)
- 64-bit MSBuild
- Network access for the pinned `Microsoft.Windows.WDK.x64` NuGet package on
  the first restore

From a Developer PowerShell at the repository root:

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe" `
  drivers\windows\auralis-virtual-audio\AuralisVirtualAudio.sln `
  /restore /m /nodeReuse:false /p:Configuration=Release /p:Platform=x64
```

The unsigned build is written under
`TabletAudioSample\x64\Release\AuralisVirtualAudio`. The project explicitly sets
`SignMode=Off`: a local build must never manufacture or trust a test certificate
without a separate, deliberate lab workflow. Catalog generation is disabled by
default because it belongs to the production signing pipeline. That pipeline
enables it explicitly with `/p:AuralisGenerateCatalog=true` before submitting
the package for Microsoft signing.

## Release and installation boundary

Do **not** install the repository build on a normal workstation. Do not disable
Secure Boot, enable Windows TESTSIGNING, or add an ad-hoc certificate to a user's
trusted stores. A production release requires:

1. Microsoft attestation or HLK submission and a production-signed catalog.
2. Driver Verifier, HLK audio tests, suspend/resume, format-change, multi-client,
   long-duration and uninstall/rollback validation on dedicated test machines.
3. A signed elevated installer that installs the exact INF and records its exact
   published OEM INF name for safe uninstall.
4. Code signing and reputation validation for the desktop executable and
   installer as well as the kernel package.

After that one-time signed installation, Windows owns the endpoint lifecycle:
it remains in the Windows output list even while Auralis is closed. On startup,
the app detects it, offers **Auralis System Audio** as a source, and indicates
whether **Auralis Virtual Output** is the current Windows output.

The app must never run broad `pnputil /delete-driver` commands. Uninstall must
target only the recorded OEM INF and restore a physical default output before
removing the endpoint.
