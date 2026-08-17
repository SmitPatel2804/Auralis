# Auralis — Multi-Hearing-Device Audio Hub
## Laptop Prototype Documentation Pack

**Documentation version:** 0.1  
**Baseline date:** 17 August 2026  
**Prototype target:** Linux laptop  
**Core implementation proposal:** C++20 + Qt 6/QML + QtDBus + BlueZ + PipeWire

This documentation pack turns the supplied **Multi-Hearing-Device Audio Hub — Finalized Project Specification V1** into an engineering-oriented laptop prototype plan.

## Important distinction

The source specification defines the product intent and the validation-first architecture. It does **not** prove that an operating system, Bluetooth stack, adapter, or hearing-device model will permit multiple simultaneous independently controlled Bluetooth audio outputs.

Accordingly, this pack separates:

1. **Source-derived requirements** — requirements and architecture stated in the supplied specification.
2. **Proposed laptop implementation** — the Linux/C++/Qt/BlueZ/PipeWire approach selected for the first prototype.
3. **Validation gates** — experiments that must pass before later architecture is treated as confirmed.

## Product objective

> One audio source → multiple individually paired compatible Bluetooth hearing devices → synchronized simultaneous playback.

The laptop application should ultimately:

- Capture system audio.
- Discover compatible Bluetooth hearing devices.
- Pair/connect devices individually.
- Maintain multiple active device sessions.
- Route the same source content to all active devices.
- Buffer outputs independently.
- Compensate for device-to-device latency differences.
- Recover from disconnects without restarting the full session.
- Expose diagnostics needed to understand Bluetooth/audio failures.

## Recommended implementation baseline

| Area | Selection |
|---|---|
| OS | Linux |
| Language | C++20 |
| Build | CMake + Ninja |
| Desktop framework | Qt 6 |
| UI | QML / Qt Quick |
| Bluetooth stack | BlueZ |
| Bluetooth control | BlueZ D-Bus API via QtDBus |
| Audio server | PipeWire |
| Audio API | Native libpipewire / pw_stream |
| Internal media | PCM |
| Buffers | Custom per-device ring buffers |
| Synchronization | Custom C++ synchronization engine |
| Testing support | C++ tests + Python analysis scripts |

## Documentation map

- `00_source_specification.md` — untouched source specification.
- `01_project_overview.md` — scope, goals, non-goals and validation gates.
- `02_laptop_technical_architecture.md` — high-level laptop architecture.
- `03_tech_stack.md` — selected technology stack and rationale.
- `04_component_design.md` — module boundaries and responsibilities.
- `05_bluetooth_device_management.md` — discovery, pairing, connection and device states.
- `06_audio_pipeline.md` — capture, processing, routing and outputs.
- `07_synchronization_latency.md` — synchronization model, latency and drift.
- `08_session_state_recovery.md` — session lifecycle, reconnect and fault isolation.
- `09_testing_validation_plan.md` — feasibility and hardware test plan.
- `10_development_roadmap.md` — implementation sequence.
- `11_risk_register.md` — major risks and mitigations.
- `12_acceptance_criteria.md` — proposed measurable success criteria framework.
- `13_decisions_open_questions.md` — architecture decisions and unresolved items.
- `diagrams/` — PlantUML source files.

## Engineering principle

Do not build the full GUI or synchronization system before proving the transport.

The first decisive experiment is:

> Can one Linux laptop continuously send the same PCM content to two individually connected Bluetooth hearing-device audio sinks at the same time?

If this fails, identify whether the limiting factor is the hearing-device profile, OS/stack, Bluetooth adapter, radio capacity, audio routing, or another constraint before changing the architecture.
