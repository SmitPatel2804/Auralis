# Validation

Phase exit gates for the implemented Auralis laptop prototype. Run these from the repository root after a clean configure/build.

These documents describe **how to verify** a completed phase. They are not the product specification.

| Document | Phase | Purpose |
|---|---|---|
| [phase-1.md](phase-1.md) | 1 Foundation | Clean-room build, CTest, desktop smoke |
| [phase-2.md](phase-2.md) | 2 Discovery | BlueZ adapter/scan UI and live discovery test |
| [phase-3.md](phase-3.md) | 3 Device management | Pair/trust/connect/forget, Agent1, reconnect |
| [phase-4.md](phase-4.md) | 4 PipeWire endpoints | Graph observation and Bluetooth correlation |
| [phase-4-audit.md](phase-4-audit.md) | 4 | Independent verification of the Phase 4 tree |
| [../phase-5-validation.md](../phase-5-validation.md) | 5 Audio routing | Additive links, source types, live routing test |

Default `ctest` must pass without Bluetooth hardware and without a live PipeWire daemon. Live tests are opt-in via environment variables documented in the phase files.

Host-specific audit dumps (`audit-phase*/`) stay at the repository root and are gitignored. The Markdown audit is the durable record.
