# As-built architecture

Documentation for how the **current codebase** is structured (Phase 6). Product-intent architecture lives in [../specification/](../specification/README.md).

| Document | Purpose |
|---|---|
| [overview.md](overview.md) | Modules, BlueZ, PipeWire, `AudioRouter`, QML integration |
| [session-engine.md](session-engine.md) | Phase 6 multi-device session layer, persistence, recovery |
| [cross-platform-backends.md](cross-platform-backends.md) | Linux, Windows, and macOS backend selection and parity boundaries |
| [LINUX_VIRTUAL_AUDIO_OUTPUT.md](LINUX_VIRTUAL_AUDIO_OUTPUT.md) | PipeWire virtual sink, persistence, routing flow, and diagnostics |
| [WINDOWS_VIRTUAL_AUDIO_OUTPUT.md](WINDOWS_VIRTUAL_AUDIO_OUTPUT.md) | Windows signed-driver virtual endpoint design |

Phase exit gates: [../validation/](../validation/README.md).
