# Auralis documentation

All project documentation lives here. Directory READMEs under `config/` and `tools/` stay with those folders.

## Current implementation

| Document | Purpose |
|---|---|
| [../README.md](../README.md) | Build, run, test, and Phase 1 status |
| [architecture.md](architecture.md) | Modules, BlueZ, PipeWire observation, QML exposure |
| [phase-1-validation.md](phase-1-validation.md) | Clean-room build/test checklist for Phase 1 |
| [phase-2-validation.md](phase-2-validation.md) | Phase 2 discovery validation |
| [phase-3-validation.md](phase-3-validation.md) | Phase 3 device management validation |
| [phase-4-validation.md](phase-4-validation.md) | Phase 4 PipeWire endpoint validation |

## Product specification

Engineering pack derived from the source specification. Start at [specification/README.md](specification/README.md).

| Document | Purpose |
|---|---|
| [00_source_specification.md](specification/00_source_specification.md) | Untouched source specification |
| [01_project_overview.md](specification/01_project_overview.md) | Scope, goals, non-goals, validation gates |
| [02_laptop_technical_architecture.md](specification/02_laptop_technical_architecture.md) | High-level laptop architecture |
| [03_tech_stack.md](specification/03_tech_stack.md) | Selected stack and rationale |
| [04_component_design.md](specification/04_component_design.md) | Module boundaries and responsibilities |
| [05_bluetooth_device_management.md](specification/05_bluetooth_device_management.md) | Discovery, pairing, connection, device states |
| [06_audio_pipeline.md](specification/06_audio_pipeline.md) | Capture, processing, routing, outputs |
| [07_synchronization_latency.md](specification/07_synchronization_latency.md) | Synchronization, latency, drift |
| [08_session_state_recovery.md](specification/08_session_state_recovery.md) | Session lifecycle, reconnect, fault isolation |
| [09_testing_validation_plan.md](specification/09_testing_validation_plan.md) | Feasibility and hardware test plan |
| [10_development_roadmap.md](specification/10_development_roadmap.md) | Milestone sequence |
| [11_risk_register.md](specification/11_risk_register.md) | Risks and mitigations |
| [12_acceptance_criteria.md](specification/12_acceptance_criteria.md) | Measurable success criteria |
| [13_decisions_open_questions.md](specification/13_decisions_open_questions.md) | Architecture decisions and open questions |

## Roadmap, diagrams, prompts

| Location | Contents |
|---|---|
| [roadmap/](roadmap/) | Detailed phased development plan |
| [diagrams/](diagrams/) | PlantUML sources and rendered SVGs |
| [prompts/](prompts/) | Historical AI-IDE implementation prompts, by phase |
