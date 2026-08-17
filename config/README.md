Optional configuration samples may be added here in later phases.

Phase 1 application settings are owned by `ConfigurationManager` and stored through Qt `QSettings`.
Runtime overrides:

- `AURALIS_LOG_FILE_ENABLED`
- `AURALIS_LOG_FILE_PATH`
- `AURALIS_UI_SHOW_DEVELOPER_STATUS`

Precedence: environment override > persisted user setting > built-in default.
