# Auralis documentation

All project documentation lives here. Directory READMEs under `config/` and `tools/` stay with those folders.

```text
docs/
  README.md                 this hub
  architecture/             as-built design of the current tree
  diagrams/                 PlantUML sources and rendered SVGs
  prompts/phase-N/          historical AI-IDE implementation prompts
  roadmap/                  long-form phased development plan
  specification/            product-intent engineering pack
  validation/               phase exit gates and independent audits
```

## How to read this tree

| If you want | Start here |
|---|---|
| Build, run, and test the current app | [../README.md](../README.md) |
| How the current code is structured | [architecture/](architecture/README.md) |
| Whether a completed phase still passes | [validation/](validation/README.md) |
| What the product is intended to become | [specification/](specification/README.md) |
| The planned phase sequence | [roadmap/](roadmap/README.md) |
| Architecture diagrams | [diagrams/](diagrams/README.md) |
| Historical implementation prompts | [prompts/](prompts/README.md) |

[architecture/](architecture/README.md) describes the **as-built** system (Phase 6). The specification pack describes broader **product intent**.

## Current implementation

| Document | Purpose |
|---|---|
| [../README.md](../README.md) | Build, run, test, and Phase 6 status |
| [architecture/overview.md](architecture/overview.md) | Modules, BlueZ, PipeWire, AudioRouter, sessions, QML |
| [architecture/session-engine.md](architecture/session-engine.md) | Session engine architecture and policies |
| [validation/phase-1.md](validation/phase-1.md) | Phase 1 foundation checklist |
| [validation/phase-2.md](validation/phase-2.md) | Phase 2 discovery validation |
| [validation/phase-3.md](validation/phase-3.md) | Phase 3 device management validation |
| [validation/phase-4.md](validation/phase-4.md) | Phase 4 PipeWire endpoint validation |
| [validation/phase-4-audit.md](validation/phase-4-audit.md) | Independent Phase 4 verification record |
| [validation/phase-5.md](validation/phase-5.md) | Phase 5 routing validation |
| [validation/phase-5-audit.md](validation/phase-5-audit.md) | Independent Phase 5 closure audit |
| [validation/phase-6.md](validation/phase-6.md) | Phase 6 session validation |
| [validation/phase-6-audit.md](validation/phase-6-audit.md) | Phase 6 live machine audit |
| [validation/phase-6-final-closure.md](validation/phase-6-final-closure.md) | Phase 6 correction/hardening closure |

## Product specification

Engineering pack derived from the source specification. Start at [specification/README.md](specification/README.md).

## Roadmap, diagrams, prompts

| Location | Contents |
|---|---|
| [roadmap/](roadmap/README.md) | Detailed phased development plan |
| [diagrams/](diagrams/README.md) | PlantUML sources and rendered SVGs |
| [prompts/](prompts/README.md) | Historical AI-IDE implementation prompts, by phase |
