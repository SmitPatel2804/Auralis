# Auralis Phase 1 — AI IDE Master Implementation Prompt

**Project:** Auralis  
**Phase:** Phase 1 — Project Foundation  
**Target:** Ubuntu 26.04 LTS laptop, x86_64  
**Primary executable:** `auralis-desktop`  
**Purpose of this document:** Give an AI coding IDE a precise, implementation-ready specification for building Phase 1 of Auralis without leaking Phase 2+ functionality into the foundation.

---

# 0. How to Use This Prompt

Give this entire document to the AI IDE as the controlling implementation prompt for Phase 1.

The AI IDE must treat this document as the **Phase 1 source of truth**. It should implement the work incrementally, compile frequently, run tests after each meaningful increment, and stop when the Phase 1 exit gate is satisfied.

The AI IDE must **not** continue into Bluetooth discovery, pairing, real PipeWire graph control, audio routing, or multi-device session behavior after Phase 1 passes.

If a detail is not explicitly required for Phase 1, prefer the smallest clean abstraction that makes the later feature possible without implementing that later feature now.

---

# 1. AI IDE Role

You are the lead C++/Qt/Linux engineer establishing the production foundation for **Auralis**, a Linux desktop application that will eventually discover, connect, manage, group, and route audio to multiple Bluetooth/hearing devices.

Your job in this phase is **not to build the Bluetooth/audio product yet**.

Your job is to create a repository that is:

- cleanly layered;
- reproducible;
- buildable with CMake + Ninja;
- testable without Bluetooth hardware;
- ready for Qt/QML presentation;
- ready for later BlueZ D-Bus integration;
- ready for later native PipeWire integration;
- explicit about ownership and dependency direction;
- easy to extend without moving major responsibilities between modules later.

Think like a maintainer who expects this codebase to grow substantially.

---

# 2. Phase 1 Mission

Create the official Auralis project foundation.

At completion, the following must be true:

1. The repository has a coherent production-oriented directory structure.
2. CMake configuration succeeds from a clean build directory.
3. Ninja compiles the entire project.
4. The executable `auralis-desktop` launches.
5. Qt Quick/QML loads successfully.
6. A C++ backend object graph initializes successfully.
7. The C++ backend exposes basic subsystem status to QML.
8. Structured application logging initializes before the main application services.
9. Configuration initialization succeeds.
10. Skeleton service layers exist for Bluetooth, audio/PipeWire, devices, and sessions.
11. Hardware-independent unit tests run successfully.
12. CTest can execute the test suite.
13. No production feature depends on shelling out to `bluetoothctl`, `wpctl`, `pactl`, `btmgmt`, or similar utilities.
14. No Phase 2+ behavior is implemented beyond safe stubs/interfaces.
15. The project is ready to be committed/tagged as the completed Phase 1 baseline.

---

# 3. Verified Development Baseline

Assume the following environment is already validated and is the official development baseline.

| Component | Baseline |
|---|---|
| Distribution | Ubuntu 26.04 LTS (`resolute`) |
| Kernel | Linux 7.0.0-29-generic |
| Architecture | x86_64 |
| GCC | 15.2.0 |
| G++ | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Git | 2.53.0 |
| Qt | 6.10.2 |
| Qt Bluetooth | 6.10.2 |
| Qt Multimedia | 6.10.2 |
| BlueZ | 5.85 |
| PipeWire | 1.6.2 |
| D-Bus | 1.16.2 |
| WirePlumber | Running |
| `pipewire-pulse` | Running |

The laptop Bluetooth controller supports BR/EDR and BLE. However, **Phase 1 must not depend on physical Bluetooth hardware being present or reachable at test time**.

---

# 4. Architectural Context

The eventual Auralis architecture is:

```text
+--------------------------------------------------+
|                  AURALIS DESKTOP                 |
|                  Qt / QML UI                     |
+-------------------------+------------------------+
                          |
                          v
+--------------------------------------------------+
|                Application Services              |
|                                                  |
| DeviceManager | AudioRouter | SessionManager     |
+----------------------+---------------------------+
                       |
          +------------+-------------+
          |                          |
          v                          v
+-------------------+      +-----------------------+
| BluetoothManager  |      | PipeWireManager       |
| BlueZ / D-Bus     |      | Audio Graph Control   |
+---------+---------+      +-----------+-----------+
          |                            |
          v                            v
+-------------------+      +-----------------------+
| BlueZ             |      | PipeWire              |
+---------+---------+      +-----------+-----------+
          |                            |
          +-------------+--------------+
                        |
                        v
                 Linux Kernel
```

Phase 1 implements only the **application skeleton and contracts** necessary to support this architecture.

The application layer must not be designed around a specific audio transport. Later, Auralis may support A2DP, HFP/HSP, LE Audio, or future transports.

---

# 5. Non-Negotiable Design Principles

## 5.1 Separation of Concerns

Keep these concerns independent:

- UI/presentation;
- application startup/lifecycle;
- logging;
- configuration;
- Bluetooth management;
- audio/PipeWire management;
- higher-level device state;
- session state;
- persistence hooks;
- transport-specific implementation.

Do not create a god object that owns all application logic.

## 5.2 Dependency Direction

Higher-level modules may depend on lower-level abstractions, but lower-level modules must not depend on the UI.

Target dependency direction:

```text
QML/UI
  |
  v
Desktop App / Presentation Bridge
  |
  v
ApplicationCore
  |
  +--> DeviceManager
  +--> SessionManager
  +--> BluetoothManager interface/stub
  +--> PipeWireManager interface/stub
  +--> ConfigurationManager
  +--> Logger
```

Avoid circular CMake target dependencies.

## 5.3 No Production Shell Command Parsing

Do not implement production features by executing and parsing:

```text
bluetoothctl
wpctl
pactl
btmgmt
busctl
pw-cli
```

Those tools may be documented as manual diagnostics only.

Future production Bluetooth integration must use BlueZ D-Bus APIs.
Future production PipeWire integration should use the native PipeWire API where practical.

## 5.4 Hardware Independence in Phase 1

Phase 1 tests must pass on a machine with no Bluetooth device connected.

Do not make unit tests require:

- Bluetooth scanning;
- `org.bluez` availability;
- a live PipeWire graph;
- headphones or hearing aids;
- elevated privileges;
- network access.

## 5.5 Explicit State

Avoid ambiguous booleans when a lifecycle state is more appropriate.

For Phase 1, simple service readiness can use a small enum such as:

```cpp
enum class ServiceStatus {
    Uninitialized,
    Initializing,
    Ready,
    Error
};
```

Do not invent Phase 2/3 device state machines in this phase.

## 5.6 RAII and Ownership

Use RAII and clear ownership.

Prefer:

- stack ownership where practical;
- `std::unique_ptr` for exclusive dynamic ownership;
- Qt parent ownership for QObject trees where appropriate;
- references/pointers for non-owning relationships with documented lifetime.

Avoid unnecessary global singletons.

---

# 6. Phase Boundary — What Must NOT Be Implemented

The following are explicitly **out of scope** for Phase 1.

Do not implement them, even if dependencies are already installed.

## 6.1 Bluetooth Phase 2+ Features — Forbidden in Phase 1

Do not implement:

- BlueZ `Adapter1.StartDiscovery()`;
- BlueZ `Adapter1.StopDiscovery()`;
- `ObjectManager` device discovery;
- parsing real `Device1` objects;
- RSSI monitoring;
- device registry population from BlueZ;
- pairing;
- trust/untrust;
- connect/disconnect;
- remove/forget;
- pairing agent;
- automatic reconnect;
- BLE GATT operations.

A `BluetoothManager` contract/stub is allowed and required.

## 6.2 PipeWire Phase 4+ Features — Forbidden in Phase 1

Do not implement:

- connecting to the PipeWire core;
- registry enumeration;
- node monitoring;
- port monitoring;
- link monitoring;
- metadata monitoring;
- endpoint resolution;
- route creation;
- volume control;
- stream movement.

A `PipeWireManager` contract/stub is allowed and required.

## 6.3 Routing Phase 5+ Features — Forbidden

Do not implement:

- source selection;
- sink selection;
- route planning;
- PipeWire links;
- multi-output duplication;
- group volume.

## 6.4 Session Phase 6+ Features — Forbidden

Do not implement functional:

- session creation UI;
- group activation;
- recovery policy;
- degraded mode;
- session persistence;
- multi-device synchronization.

A `SessionManager` skeleton is required, but it must remain inert.

## 6.5 Full GUI Phase 7 — Forbidden

Do not build a polished multi-screen UI.

The Phase 1 UI exists only to prove:

- QML loads;
- backend is reachable;
- status properties update;
- the architecture is wired correctly.

---

# 7. Implementation Choices for Phase 1

The roadmap does not prescribe every library-level detail. For this Phase 1 implementation, use the following conservative choices unless the existing repository already contains an intentional alternative.

## 7.1 Language Standard

Use **C++20**.

Reason: it is modern, well-supported by GCC 15.2, and sufficient for the foundation without requiring later modules to depend on newer language features unnecessarily.

Root CMake should set:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

## 7.2 Qt Modules

Phase 1 should use only what it actually needs.

Required:

- `Qt6::Core`
- `Qt6::Gui`
- `Qt6::Qml`
- `Qt6::Quick`

For tests:

- `Qt6::Test`

Do **not** link Qt Bluetooth or Qt Multimedia merely because they are installed unless a compile-time placeholder contract genuinely requires them. Prefer not to link them in Phase 1.

## 7.3 Test Framework

Use **Qt Test + CTest** for Phase 1.

This avoids introducing a network-fetched third-party test dependency during foundation setup.

All test executables must be registered with CTest.

## 7.4 Configuration Backend

Use Qt-native facilities and keep a stable abstraction around them.

Recommended initial implementation:

- `ConfigurationManager` as a C++ class owned by `ApplicationCore`;
- use `QSettings` for user-level persistent configuration;
- support explicit environment-variable overrides for a small set of Phase 1 settings;
- keep configuration keys centralized rather than scattering string literals throughout the codebase.

Do not implement a large schema system in Phase 1.

## 7.5 Logging Backend

Implement an Auralis-owned logging façade using Qt logging facilities.

Recommended foundation:

- `Logger` initializes early;
- use `QLoggingCategory` for subsystem/module categories;
- install a Qt message handler if needed to produce consistent structured lines;
- include timestamp, severity, category/module, and message;
- optional file logging may be supported but must fail gracefully;
- stdout/stderr logging must remain available for developer runs.

Do not add a heavy logging dependency for Phase 1 unless already present in the repository.

---

# 8. Required Repository Structure

Create this structure, refining only where CMake/QML packaging genuinely requires it:

```text
auralis/
|
+-- CMakeLists.txt
+-- cmake/
|   +-- AuralisCompilerWarnings.cmake
|   +-- AuralisOptions.cmake
|
+-- apps/
|   +-- desktop/
|       +-- CMakeLists.txt
|       +-- main.cpp
|       +-- DesktopApplication.h
|       +-- DesktopApplication.cpp
|
+-- src/
|   +-- core/
|   |   +-- CMakeLists.txt
|   |   +-- ApplicationCore.cpp
|   |   +-- Logger.cpp
|   |   +-- ConfigurationManager.cpp
|   |   +-- ServiceStatus.cpp
|   |
|   +-- bluetooth/
|   |   +-- CMakeLists.txt
|   |   +-- BluetoothManager.cpp
|   |
|   +-- audio/
|   |   +-- CMakeLists.txt
|   |   +-- PipeWireManager.cpp
|   |
|   +-- devices/
|   |   +-- CMakeLists.txt
|   |   +-- DeviceManager.cpp
|   |
|   +-- session/
|       +-- CMakeLists.txt
|       +-- SessionManager.cpp
|
+-- include/
|   +-- auralis/
|       +-- core/
|       |   +-- ApplicationCore.h
|       |   +-- Logger.h
|       |   +-- ConfigurationManager.h
|       |   +-- ServiceStatus.h
|       |
|       +-- bluetooth/
|       |   +-- IBluetoothManager.h
|       |   +-- BluetoothManager.h
|       |
|       +-- audio/
|       |   +-- IPipeWireManager.h
|       |   +-- PipeWireManager.h
|       |
|       +-- devices/
|       |   +-- IDeviceManager.h
|       |   +-- DeviceManager.h
|       |
|       +-- session/
|           +-- ISessionManager.h
|           +-- SessionManager.h
|
+-- ui/
|   +-- CMakeLists.txt
|   +-- qml/
|   |   +-- Main.qml
|   +-- components/
|   |   +-- StatusRow.qml
|   +-- assets/
|
+-- tests/
|   +-- CMakeLists.txt
|   +-- unit/
|   |   +-- core/
|   |   |   +-- CMakeLists.txt
|   |   |   +-- tst_ApplicationCore.cpp
|   |   |   +-- tst_ConfigurationManager.cpp
|   |   |   +-- tst_Logger.cpp
|   |   +-- devices/
|   |       +-- CMakeLists.txt
|   |       +-- tst_DeviceManager.cpp
|   +-- integration/
|       +-- CMakeLists.txt
|       +-- tst_DesktopBackendSmoke.cpp
|
+-- tools/
|   +-- README.md
|
+-- config/
|   +-- README.md
|
+-- docs/
|   +-- architecture.md
|   +-- phase-1-validation.md
|
+-- README.md
+-- LICENSE
+-- .gitignore
```

If the AI IDE determines that Qt's QML module organization is cleaner with `qt_add_qml_module()` in another target location, preserve the conceptual structure and document the reason.

Do not flatten all source files into the desktop executable target.

---

# 9. CMake Architecture

## 9.1 Root `CMakeLists.txt`

The root build must:

1. define the project;
2. set C++20;
3. expose project options;
4. locate Qt 6 components;
5. enable testing;
6. add internal module subdirectories in dependency-safe order;
7. add UI and desktop application;
8. add tests when enabled.

Suggested shape:

```cmake
cmake_minimum_required(VERSION 3.25)

project(Auralis
    VERSION 0.1.0
    DESCRIPTION "Multi-hearing-device audio hub for Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AuralisOptions)
include(AuralisCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Test)

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/bluetooth)
add_subdirectory(src/audio)
add_subdirectory(src/devices)
add_subdirectory(src/session)
add_subdirectory(ui)
add_subdirectory(apps/desktop)

if(AURALIS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

Adapt the minimum Qt version only if needed for the installed environment, but the code should target the verified Qt 6.10.2 environment.

## 9.2 Project Options

Create `cmake/AuralisOptions.cmake` with options such as:

```cmake
option(AURALIS_BUILD_TESTS "Build Auralis tests" ON)
option(AURALIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AURALIS_ENABLE_FILE_LOGGING "Enable file logging support" ON)
```

Avoid adding speculative options for LE Audio, pairing agents, routing, etc. in Phase 1.

## 9.3 Compiler Warnings

Create a reusable warnings target or function.

For GCC/Clang, enable a useful baseline such as:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wsign-conversion
-Wshadow
-Wnon-virtual-dtor
-Wold-style-cast
-Woverloaded-virtual
```

Be pragmatic: if a warning produces excessive Qt-generated noise, tune it at the target boundary rather than globally disabling useful warnings.

Warnings-as-errors should be opt-in locally, not mandatory by default.

## 9.4 Internal Targets

Prefer distinct CMake targets:

```text
auralis-core
auralis-bluetooth
auralis-audio
auralis-devices
auralis-session
auralis-ui
auralis-desktop
```

Exact target naming may use namespaces/aliases, for example:

```cmake
add_library(Auralis::Core ALIAS auralis-core)
```

Use `PUBLIC`, `PRIVATE`, and `INTERFACE` visibility intentionally.

Do not let every module link to every other module.

## 9.5 Include Paths

Public headers live under:

```text
include/auralis/...
```

Consumers should include headers like:

```cpp
#include <auralis/core/ApplicationCore.h>
#include <auralis/bluetooth/IBluetoothManager.h>
```

Avoid relative include chains like:

```cpp
#include "../../../include/..."
```

---

# 10. Core Domain Contracts for Phase 1

# 10.1 `ServiceStatus`

Create a tiny common status type for subsystem readiness.

Recommended API:

```cpp
namespace auralis::core {

enum class ServiceStatus {
    Uninitialized,
    Initializing,
    Ready,
    Error
};

QString toString(ServiceStatus status);

} // namespace auralis::core
```

Requirements:

- deterministic string conversion;
- no device-specific states;
- usable by QML-facing status properties indirectly;
- unit-test conversion behavior.

Suggested strings:

```text
Uninitialized
Initializing
Ready
Error
```

---

# 10.2 `Logger`

## Responsibility

The Logger owns application-wide logging initialization and formatting policy.

It does **not** own business state.

## Required behavior

On startup it should be possible to log something equivalent to:

```text
2026-08-18T00:00:00.000+05:30 INFO core Auralis logger initialized
```

Exact formatting may differ, but every line should be able to carry:

- timestamp;
- severity;
- subsystem/category;
- message.

Future messages may later carry device/session IDs, but Phase 1 does not need a structured event object yet.

## Suggested interface

```cpp
namespace auralis::core {

class Logger final {
public:
    struct Options {
        bool enableConsole = true;
        bool enableFile = false;
        QString filePath;
    };

    static bool initialize(const Options& options = {});
    static void shutdown();
    static bool isInitialized();

private:
    Logger() = delete;
};

} // namespace auralis::core
```

You may implement this differently if the code becomes cleaner, but preserve these semantics.

## Logging categories

Define categories for at least:

```text
auralis.core
auralis.config
auralis.bluetooth
auralis.audio
auralis.devices
auralis.session
auralis.ui
```

## Failure behavior

If optional file logging cannot open its target:

- do not crash the application;
- continue console logging;
- report a warning;
- make the failure testable where practical.

## Test expectations

At minimum test:

- initialization can occur;
- repeated initialization is deterministic/idempotent or explicitly rejected safely;
- shutdown resets internal state;
- invalid file path does not terminate the process.

Do not over-engineer a full logging backend in Phase 1.

---

# 10.3 `ConfigurationManager`

## Responsibility

Provide a single owned point for application configuration.

Phase 1 must prove that configuration initializes and can provide stable values to the rest of the application.

## Suggested initial settings

Keep the initial set small and useful:

```text
application/name = Auralis
logging/fileEnabled = false
logging/filePath = <empty or default>
ui/showDeveloperStatus = true
```

Environment overrides may include:

```text
AURALIS_LOG_FILE_ENABLED
AURALIS_LOG_FILE_PATH
AURALIS_UI_SHOW_DEVELOPER_STATUS
```

Use strict parsing for booleans.

## Suggested interface

```cpp
namespace auralis::core {

class ConfigurationManager final {
public:
    ConfigurationManager();

    bool initialize();
    bool isInitialized() const noexcept;

    QString applicationName() const;
    bool fileLoggingEnabled() const;
    QString logFilePath() const;
    bool showDeveloperStatus() const;

private:
    bool initialized_ = false;
};

} // namespace auralis::core
```

A QObject is not required unless signals are genuinely needed.

## Configuration priorities

Define and document deterministic precedence:

```text
Environment override
        >
Persisted user setting
        >
Built-in default
```

## Test expectations

Test:

- defaults;
- initialization;
- valid environment override;
- invalid boolean environment input falls back safely;
- settings do not require hardware;
- multiple manager instances do not corrupt settings.

Use a temporary settings location or test-mode organization/application names so tests never modify the developer's real Auralis settings.

---

# 10.4 `IBluetoothManager` and `BluetoothManager`

## Responsibility in Phase 1

Define the lifecycle contract for the future BlueZ-backed manager.

This is a **stub**.

It must not talk to BlueZ yet.

## Suggested interface

```cpp
namespace auralis::bluetooth {

class IBluetoothManager {
public:
    virtual ~IBluetoothManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;
};

} // namespace auralis::bluetooth
```

Concrete Phase 1 stub:

```cpp
class BluetoothManager final : public IBluetoothManager {
public:
    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;

private:
    auralis::core::ServiceStatus status_ =
        auralis::core::ServiceStatus::Uninitialized;
};
```

## Stub semantics

For Phase 1, successful stub initialization can transition:

```text
Uninitialized -> Initializing -> Ready
```

`Ready` in Phase 1 means:

> the Auralis Bluetooth service object is initialized and ready for a future backend.

It does **not** mean:

> BlueZ is present, the adapter exists, or Bluetooth hardware is ready.

Make this distinction explicit in comments and docs so the Phase 1 status UI is not misinterpreted.

---

# 10.5 `IPipeWireManager` and `PipeWireManager`

Use the same lifecycle pattern as Bluetooth.

Do not connect to PipeWire in Phase 1.

Suggested contract:

```cpp
namespace auralis::audio {

class IPipeWireManager {
public:
    virtual ~IPipeWireManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;
};

} // namespace auralis::audio
```

Again, `Ready` means the **service skeleton** is ready, not that a real audio graph has been inspected.

---

# 10.6 `IDeviceManager` and `DeviceManager`

## Responsibility in the eventual architecture

`DeviceManager` owns higher-level Auralis device state and should eventually sit above transport-specific objects.

## Phase 1 behavior

Keep it intentionally small.

It may expose:

```cpp
virtual bool initialize() = 0;
virtual void shutdown() = 0;
virtual ServiceStatus status() const noexcept = 0;
```

Do not define the full `BluetoothDevice` domain model yet unless needed to make the module compile cleanly.

Do not create fake discovered devices.

Do not tie `DeviceManager` directly to QML.

---

# 10.7 `ISessionManager` and `SessionManager`

## Responsibility in the eventual architecture

Own logical Auralis multi-device sessions.

## Phase 1 behavior

Lifecycle-only skeleton.

No session persistence, state machine, device grouping, route activation, or recovery behavior.

Implement initialization/shutdown/status only.

---

# 10.8 `ApplicationCore`

`ApplicationCore` is the central lifecycle coordinator for backend services.

It is **not** a dumping ground for each subsystem's logic.

## Ownership

It should own or receive ownership of:

- `ConfigurationManager`;
- `IBluetoothManager` implementation;
- `IPipeWireManager` implementation;
- `IDeviceManager` implementation;
- `ISessionManager` implementation.

The Logger can be process-level but must be initialized before service initialization.

## Suggested initialization order

```text
Logger
  |
  v
ConfigurationManager
  |
  v
BluetoothManager stub
  |
  v
PipeWireManager stub
  |
  v
DeviceManager stub
  |
  v
SessionManager stub
  |
  v
ApplicationCore Ready
```

If one required subsystem fails, `ApplicationCore` must:

- enter Error state;
- log the failing subsystem;
- avoid reporting global readiness;
- safely tear down any already-initialized components when appropriate.

## Suggested shutdown order

Reverse initialization order:

```text
SessionManager
DeviceManager
PipeWireManager
BluetoothManager
ConfigurationManager cleanup if any
Logger last
```

## Suggested QObject/QML-facing API

It is reasonable for `ApplicationCore` to be a QObject because it must expose observable status to QML.

Possible API:

```cpp
class ApplicationCore final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY statusChanged)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY statusChanged)
    Q_PROPERTY(QString pipeWireStatus READ pipeWireStatus NOTIFY statusChanged)
    Q_PROPERTY(QString coreStatus READ coreStatus NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY statusChanged)

public:
    explicit ApplicationCore(QObject* parent = nullptr);

    bool initialize();
    void shutdown();

    bool isReady() const noexcept;

    QString bluetoothStatus() const;
    QString audioStatus() const;
    QString pipeWireStatus() const;
    QString coreStatus() const;

signals:
    void statusChanged();
};
```

Clarification for Phase 1 UI terminology:

- `bluetoothStatus`: lifecycle readiness of Bluetooth service stub;
- `pipeWireStatus`: lifecycle readiness of PipeWire service stub;
- `audioStatus`: may represent the audio service layer readiness and can mirror/derive from the PipeWire stub only if documented;
- `coreStatus`: overall `ApplicationCore` readiness.

Avoid falsely implying that hardware connectivity was validated at runtime.

## Dependency injection for testing

Design `ApplicationCore` so unit tests can substitute fake service implementations.

Good approaches include constructor injection or a small dependency struct.

Example:

```cpp
struct ApplicationServices {
    std::unique_ptr<bluetooth::IBluetoothManager> bluetooth;
    std::unique_ptr<audio::IPipeWireManager> pipeWire;
    std::unique_ptr<devices::IDeviceManager> devices;
    std::unique_ptr<session::ISessionManager> sessions;
};
```

This will become valuable in later phases.

Do not hardwire tests to real system services.

---

# 11. Desktop Application Bootstrap

Create a very thin desktop bootstrap layer.

The entry point should be understandable in one screen.

Target flow:

```text
main()
  |
  +--> construct QGuiApplication
  |
  +--> set organization/application metadata
  |
  +--> initialize Logger
  |
  +--> construct ApplicationCore
  |
  +--> ApplicationCore.initialize()
  |
  +--> construct QQmlApplicationEngine
  |
  +--> expose/register ApplicationCore to QML
  |
  +--> load Main.qml
  |
  +--> verify root object created
  |
  +--> app.exec()
  |
  +--> orderly shutdown
```

## Requirements

- Check QML object creation failure.
- Exit non-zero if the root QML fails to load.
- Log startup and shutdown.
- Do not bury core initialization inside QML.
- Do not let QML own core service lifetimes.
- Do not create Bluetooth/PipeWire logic directly inside `main.cpp`.

## QML exposure

Choose one clean Qt 6 pattern:

- `qmlRegisterSingletonInstance`, or
- context property for this simple phase, or
- a dedicated QML singleton wrapper.

Prefer a pattern that can survive growth.

Document the choice in `docs/architecture.md`.

---

# 12. Phase 1 QML UI Specification

The UI must be deliberately minimal.

It should prove backend communication, not product polish.

## 12.1 Required content

Display approximately:

```text
AURALIS

System Status
--------------------------------
Bluetooth     Ready
Audio         Ready
PipeWire      Ready

Auralis Core Ready
```

Because Phase 1 uses service stubs, include a subtle developer-facing note such as:

```text
Foundation services initialized — hardware integration begins in Phase 2+
```

This prevents `Ready` from being mistaken for live device validation.

## 12.2 Required QML behavior

- Window launches successfully.
- Title contains `Auralis`.
- Status values come from C++ properties, not hard-coded strings.
- QML reacts to `statusChanged`.
- Layout remains readable at a reasonable desktop window size.
- QML has no Bluetooth discovery logic.
- QML has no shell command execution.
- QML has no direct PipeWire integration.

## 12.3 `StatusRow.qml`

Create one reusable component to prove UI composition.

Suggested properties:

```qml
property string label
property string status
```

Optional:

```qml
property bool healthy
```

Do not build a design system in Phase 1.

## 12.4 QML resource packaging

Use Qt 6 QML module/resource packaging via CMake so the application does not depend on running from a specific working directory to locate QML files.

The executable should launch from the expected build path without manually exporting QML search paths.

---

# 13. Testing Architecture

Testing is part of Phase 1, not deferred work.

Use **Qt Test** and integrate all tests with **CTest**.

The required command must work:

```bash
ctest --test-dir build --output-on-failure
```

## 13.1 Unit Test Rules

Unit tests must:

- run without root;
- run without Bluetooth hardware;
- run without active discovery;
- not mutate real user settings;
- not depend on internet access;
- not rely on timing-sensitive sleeps where avoidable;
- use fake service implementations for failure-path testing.

## 13.2 `ServiceStatus` tests

Test deterministic enum-to-string conversion.

## 13.3 `ConfigurationManager` tests

Test:

1. defaults;
2. initialization success;
3. environment override precedence;
4. invalid override handling;
5. isolated test settings path;
6. no dependence on external config files.

## 13.4 `Logger` tests

Test safe initialization and shutdown.

Do not try to assert every character of timestamp output if that makes tests brittle.

Focus on lifecycle behavior and failure safety.

## 13.5 Service stub tests

For each skeleton manager:

```text
initial status = Uninitialized
initialize() -> true
status = Ready
shutdown()
status = Uninitialized (or a documented stopped state if you add one)
```

Choose one lifecycle convention and keep it consistent.

## 13.6 `ApplicationCore` tests

Create fakes that can be configured to succeed or fail initialization.

Test:

- all services succeed -> core ready;
- Bluetooth stub failure -> core error;
- PipeWire stub failure -> core error;
- DeviceManager failure -> core error;
- SessionManager failure -> core error;
- initialized services are safely shut down after a later failure;
- repeated shutdown is safe;
- QML-facing status strings match backend state.

## 13.7 Integration smoke test

Create a small test that exercises the desktop backend/QML boundary without requiring real Bluetooth or audio hardware.

Possible approaches:

- load the QML component with a test engine in offscreen mode; or
- test the QObject properties/signals that the UI consumes.

Prefer reliable CI-friendly behavior.

If GUI/QML loading requires display configuration, support:

```text
QT_QPA_PLATFORM=offscreen
```

Document it.

---

# 14. Documentation Deliverables

## 14.1 Root `README.md`

Create a useful developer README containing:

### Project summary

Auralis is a Linux desktop foundation for eventually managing multiple Bluetooth/hearing audio devices and routing audio via BlueZ/PipeWire.

### Current status

Clearly state:

```text
Phase 0: Complete
Phase 1: Implemented / in progress
Phase 2+: Not implemented
```

### Requirements

List the validated baseline stack.

### Build

Exactly document:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

### Run

```bash
./build/apps/desktop/auralis-desktop
```

If the actual generated path differs because of target layout, adjust CMake so the required roadmap path works if practical. If that is not practical, explicitly document the exact path and why.

### Test

```bash
ctest --test-dir build --output-on-failure
```

### Architecture summary

Briefly describe each module and the Phase 1 boundary.

### Explicit non-features

State that the current foundation does not yet scan, pair, connect, enumerate PipeWire nodes, or route audio.

## 14.2 `docs/architecture.md`

Document:

- target architecture;
- module responsibilities;
- dependency direction;
- lifecycle ownership;
- why BlueZ/PipeWire implementations are stubs in Phase 1;
- QML backend exposure method;
- testing seams/dependency injection;
- no shell command production rule.

Include an ASCII diagram.

## 14.3 `docs/phase-1-validation.md`

Create a reproducible validation checklist.

It must include:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

For each step, document expected success criteria.

---

# 15. `.gitignore`

Create a Linux/C++/Qt/CMake appropriate `.gitignore`.

At minimum ignore:

```text
/build/
/build-*/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
compile_commands.json
.ninja_deps
.ninja_log
*.user
*.user.*
*.swp
*~
.DS_Store
```

If `compile_commands.json` is intentionally symlinked/exported for tooling, document that convention instead of blindly ignoring a checked-in symlink.

Do not ignore source QML/assets/config docs.

---

# 16. `LICENSE`

The roadmap requires a `LICENSE` file but does not specify a license type.

**Do not invent a legal license choice without user/project direction.**

For Phase 1, do one of the following:

1. if a license has already been selected elsewhere in the repository/project context, use it exactly;
2. otherwise create a clearly marked placeholder file such as:

```text
Auralis licensing terms have not yet been selected.
Do not distribute this software until the project owner selects and approves a license.
```

Also flag this in the final implementation report as a project-owner decision.

Do not silently choose MIT, GPL, Apache, or another license.

---

# 17. Code Style and Engineering Rules

## 17.1 Namespaces

Use:

```cpp
namespace auralis::core { }
namespace auralis::bluetooth { }
namespace auralis::audio { }
namespace auralis::devices { }
namespace auralis::session { }
```

## 17.2 Naming

Recommended convention:

- Classes: `PascalCase`
- Methods/functions: `camelCase`
- Local variables: `camelCase`
- Private data members: trailing underscore, e.g. `status_`
- Constants: `kPascalCase` or a single consistent project convention
- CMake targets: lowercase hyphenated `auralis-core`

Use one convention consistently.

## 17.3 Header discipline

- Use `#pragma once` or include guards consistently.
- Public headers should include what they use.
- Avoid transitive-include dependence.
- Forward-declare where it meaningfully reduces coupling.

## 17.4 QObject usage

Do not make every class a QObject.

Use QObject only when needed for:

- signals/slots;
- Q_PROPERTY;
- Qt object ownership;
- event-loop integration.

Plain lifecycle/service contracts can remain ordinary C++.

## 17.5 Error handling

For Phase 1:

- return explicit success/failure from initialization functions;
- log contextual error messages;
- do not catch-and-ignore exceptions/errors silently;
- do not use exceptions across Qt signal/slot boundaries;
- avoid process termination for recoverable service init failures.

## 17.6 Comments

Comments should explain **why**, lifecycle assumptions, and phase boundaries.

Avoid comments that merely repeat the code.

Explicitly mark intentional stubs with wording like:

```cpp
// Phase 1 foundation stub. Real BlueZ integration begins in Phase 2.
```

or:

```cpp
// Phase 1 foundation stub. Native PipeWire registry integration begins in Phase 4.
```

---

# 18. Runtime Lifecycle Requirements

## 18.1 Startup

A normal successful run should conceptually log:

```text
Auralis starting
Logger initialized
Configuration initialized
Bluetooth service skeleton initialized
PipeWire service skeleton initialized
Device service skeleton initialized
Session service skeleton initialized
Application core ready
QML root loaded
```

Exact wording is not mandatory.

## 18.2 Shutdown

On normal exit:

- stop/clear session skeleton;
- stop/clear device skeleton;
- stop/clear PipeWire skeleton;
- stop/clear Bluetooth skeleton;
- release core state;
- flush/close file log if enabled;
- shut down logger last.

Repeated `shutdown()` should be safe.

## 18.3 Partial initialization failure

If service N fails after services 1..N-1 succeeded, ensure earlier services are left in a safe state.

This behavior must be unit-tested using injected fakes.

---

# 19. Build Reproducibility Requirements

The project must support an out-of-source build.

The following sequence is the canonical Phase 1 validation:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

The build must not require:

- manually editing generated files;
- running qmake;
- downloading dependencies during build;
- root privileges;
- environment-specific absolute source paths;
- pre-existing `build/` contents.

If a required Qt development component is missing, report it clearly instead of adding hidden runtime workarounds.

---

# 20. Suggested Implementation Sequence

Implement in this order and keep the project compiling as often as possible.

## Step 1 — Bootstrap repository

Create:

- folder tree;
- root CMake;
- CMake options;
- compiler warnings module;
- `.gitignore`;
- README skeleton;
- license placeholder if no license is known.

Validation:

```bash
cmake -S . -B build -G Ninja
```

At this early step, it is acceptable if targets are still minimal, but configuration should become valid quickly.

## Step 2 — Core status + logger

Implement:

- `ServiceStatus`;
- Logger;
- logging categories;
- tests.

Compile and run tests.

## Step 3 — Configuration

Implement `ConfigurationManager` and tests.

Validate environment override behavior.

## Step 4 — Service interfaces

Implement:

- `IBluetoothManager`;
- `IPipeWireManager`;
- `IDeviceManager`;
- `ISessionManager`.

Keep them minimal.

## Step 5 — Service stubs

Implement concrete stub managers with consistent lifecycle semantics and logs.

Add unit tests for each where useful.

## Step 6 — ApplicationCore

Implement:

- service ownership;
- initialization ordering;
- shutdown ordering;
- status aggregation;
- dependency injection seam;
- failure rollback;
- tests with fakes.

## Step 7 — Qt/QML shell

Implement:

- desktop entry point;
- QML module/resource packaging;
- `Main.qml`;
- `StatusRow.qml`;
- C++ status exposure;
- root-object load failure handling.

## Step 8 — UI/backend smoke validation

Verify QML reads live C++ properties.

No hard-coded Ready strings for subsystem values.

## Step 9 — Documentation

Complete:

- `README.md`;
- `docs/architecture.md`;
- `docs/phase-1-validation.md`;
- placeholders in `tools/` and `config/` if those directories would otherwise be empty.

## Step 10 — Clean-room validation

Run the full gate from a deleted build directory.

Fix every failure.

---

# 21. Detailed Acceptance Criteria

Phase 1 is **not complete** until every applicable item below is satisfied.

## 21.1 Repository

- [ ] Root `CMakeLists.txt` exists.
- [ ] `apps/desktop/` exists.
- [ ] `src/core/` exists.
- [ ] `src/bluetooth/` exists.
- [ ] `src/audio/` exists.
- [ ] `src/devices/` exists.
- [ ] `src/session/` exists.
- [ ] Public headers live under `include/auralis/`.
- [ ] `ui/qml/`, `ui/components/`, and `ui/assets/` exist.
- [ ] `tests/unit/` and `tests/integration/` exist.
- [ ] `tools/`, `config/`, `docs/`, and `cmake/` exist.
- [ ] `.gitignore` exists and is sensible.
- [ ] `README.md` exists.
- [ ] `LICENSE` exists or contains an explicit pending-license placeholder.

## 21.2 Build

- [ ] `cmake -S . -B build -G Ninja` succeeds from a clean tree.
- [ ] `cmake --build build` succeeds.
- [ ] Build is out-of-source.
- [ ] No source file depends on generated paths outside the build tree.
- [ ] No build step performs a network download.
- [ ] Internal modules are separate CMake targets.
- [ ] Public/private dependencies are declared intentionally.

## 21.3 Application

- [ ] `auralis-desktop` exists.
- [ ] Executable launches.
- [ ] Logger initializes.
- [ ] Configuration initializes.
- [ ] `ApplicationCore` initializes.
- [ ] Bluetooth service stub initializes.
- [ ] PipeWire service stub initializes.
- [ ] Device service stub initializes.
- [ ] Session service stub initializes.
- [ ] QML root loads.
- [ ] Application exits cleanly.
- [ ] QML load failure produces non-zero exit behavior or an equivalent explicit failure path.

## 21.4 UI

- [ ] Auralis title is visible.
- [ ] System Status section is visible.
- [ ] Bluetooth status comes from C++.
- [ ] Audio status comes from C++.
- [ ] PipeWire status comes from C++.
- [ ] Core readiness comes from C++.
- [ ] Status values are not hard-coded to `Ready` in QML.
- [ ] UI clearly communicates that this is foundation readiness, not live hardware readiness.

## 21.5 Logging

- [ ] Logs contain timestamp.
- [ ] Logs contain severity.
- [ ] Logs contain subsystem/category.
- [ ] Core startup is logged.
- [ ] Core shutdown is logged.
- [ ] Optional file logging failure is graceful.

## 21.6 Configuration

- [ ] Built-in defaults work.
- [ ] QSettings-based persistence is isolated behind `ConfigurationManager`.
- [ ] Environment override precedence is documented.
- [ ] Tests do not modify production user settings.

## 21.7 Tests

- [ ] Test infrastructure is enabled in CMake.
- [ ] Qt Test is linked only to test targets.
- [ ] Tests are registered with CTest.
- [ ] `ctest --test-dir build --output-on-failure` succeeds.
- [ ] `ServiceStatus` behavior is tested.
- [ ] Configuration defaults/overrides are tested.
- [ ] Logger lifecycle/failure safety is tested.
- [ ] ApplicationCore success path is tested.
- [ ] ApplicationCore partial failure path is tested with fakes.
- [ ] Tests require no Bluetooth device.
- [ ] Tests require no root privileges.

## 21.8 Scope discipline

- [ ] No BlueZ discovery implementation exists.
- [ ] No pairing implementation exists.
- [ ] No connect/disconnect implementation exists.
- [ ] No PipeWire registry implementation exists.
- [ ] No audio routing implementation exists.
- [ ] No multi-device session behavior exists.
- [ ] No shell-command-based production integration exists.

---

# 22. Manual Validation Script

After implementation, run exactly this sequence from repository root:

```bash
set -euo pipefail

rm -rf build

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure

./build/apps/desktop/auralis-desktop
```

Expected results:

### CMake configure

- Qt 6 found;
- no unresolved internal targets;
- build files generated successfully.

### Build

- all libraries build;
- QML resources compile/package;
- test executables build;
- `auralis-desktop` links.

### CTest

- all Phase 1 tests pass;
- no real Bluetooth/PipeWire hardware dependency.

### Desktop launch

Window shows:

```text
AURALIS

System Status
Bluetooth     Ready
Audio         Ready
PipeWire      Ready

Auralis Core Ready
```

The exact styling may differ.

The displayed values must originate from C++ backend state.

---

# 23. Failure Scenarios the Foundation Must Handle

Even though Phase 1 uses stubs, the architecture must be testable for service initialization failures.

Test at least these synthetic cases:

## 23.1 Configuration initialization failure

Expected:

- core does not become Ready;
- error is logged;
- later services do not initialize if configuration is mandatory.

## 23.2 Bluetooth service stub failure

Expected:

- core enters Error;
- PipeWire/device/session services do not incorrectly report initialized unless intentionally started earlier;
- already initialized services are cleaned up.

## 23.3 PipeWire service stub failure

Expected:

- core enters Error;
- Bluetooth stub is safely shut down during rollback if that is the chosen lifecycle policy.

## 23.4 QML root load failure

Expected:

- error is logged;
- executable does not continue invisibly as though UI startup succeeded.

## 23.5 Optional log file unavailable

Expected:

- console logging remains usable;
- application can continue unless project configuration explicitly makes file logging mandatory.

---

# 24. Architecture Decisions to Preserve for Future Phases

Do not implement future functionality now, but make these extension points clean.

## 24.1 Phase 2 extension

Future `BluetoothManager` will grow to use:

```text
org.bluez
org.bluez.Adapter1
org.bluez.Device1
org.freedesktop.DBus.ObjectManager
org.freedesktop.DBus.Properties
```

Therefore do not expose shell-output concepts in the interface.

## 24.2 Phase 4 extension

Future `PipeWireManager` will own:

```text
CoreConnection
RegistryMonitor
DeviceMonitor
NodeMonitor
PortMonitor
LinkMonitor
AudioEndpointRegistry
```

Therefore the Phase 1 manager should be a service seam, not a static utility collection.

## 24.3 Device abstraction

Future high-level device code must not assume:

```text
BluetoothDevice == AudioEndpoint
```

They are separate concepts.

Do not prematurely merge them in Phase 1.

## 24.4 Transport abstraction

Later endpoints may be A2DP, HFP/HSP, LE Audio, or future transports.

Avoid naming generic application-level types after a single transport.

## 24.5 Session abstraction

Future `SessionManager` should depend on higher-level capabilities rather than raw BlueZ object paths or raw PipeWire node IDs.

Do not leak those identifiers into its Phase 1 contract.

---

# 25. Do Not Over-Engineer Phase 1

Avoid these tempting but unnecessary additions:

- dependency injection framework;
- service locator framework;
- plugin architecture;
- database;
- JSON schema engine;
- REST API;
- IPC protocol;
- background daemon;
- systemd unit;
- Flatpak packaging;
- AppImage packaging;
- `.deb` packaging;
- full design system;
- theming engine;
- translation framework;
- telemetry;
- cloud integration;
- network service;
- auto-updater;
- LE Audio feature flags;
- custom thread pools.

The best Phase 1 foundation is small, explicit, tested, and extensible.

---

# 26. AI IDE Working Rules

Follow these rules while editing the repository.

## Rule 1 — Inspect before editing

Before changing an existing repository, inspect:

- current tree;
- current CMake files;
- existing source files;
- existing naming conventions;
- Git status.

Do not overwrite meaningful user code without understanding it.

If starting from an empty directory, proceed directly.

## Rule 2 — Build incrementally

After each major structural change, run at least:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Once tests exist, also run:

```bash
ctest --test-dir build --output-on-failure
```

## Rule 3 — Fix the cause, not the symptom

Do not suppress compile errors or warnings by disabling broad compiler checks unless genuinely justified.

Do not hard-code paths to make one local run pass.

## Rule 4 — No fake hardware success

Do not claim BlueZ or PipeWire runtime integration works in Phase 1.

The status screen reports service-skeleton readiness only.

## Rule 5 — Preserve testability

Any future-facing service should be replaceable by a fake without requiring OS services.

## Rule 6 — Do not advance phases

Once every Phase 1 acceptance criterion passes, stop implementation.

Do not start Phase 2.

---

# 27. Expected CMake Target Relationships

A reasonable target graph is:

```text
auralis-core
   ^
   |
   +----------------+----------------+----------------+
   |                |                |                |
auralis-bluetooth auralis-audio auralis-devices auralis-session
   \                |                |               /
    \_______________|________________|______________/
                         |
                         v
                  auralis-desktop
                         |
                         +--> auralis-ui / QML resources
```

If `DeviceManager` or `SessionManager` genuinely needs another interface target in Phase 1, add the narrow dependency. Do not link modules together merely for convenience.

The QML UI must not become a dependency of backend libraries.

---

# 28. Suggested File-Level Responsibilities

## `include/auralis/core/ServiceStatus.h`

- status enum;
- string conversion declaration.

## `src/core/ServiceStatus.cpp`

- conversion implementation.

## `include/auralis/core/Logger.h`

- logging initialization API;
- options.

## `src/core/Logger.cpp`

- Qt message handler/category setup;
- optional file sink handling;
- thread-safe enough for Qt application startup/runtime logging.

## `include/auralis/core/ConfigurationManager.h`

- stable config access API.

## `src/core/ConfigurationManager.cpp`

- QSettings/default/env merge logic.

## `include/auralis/core/ApplicationCore.h`

- QObject lifecycle/status API;
- injectable service dependencies.

## `src/core/ApplicationCore.cpp`

- orchestration only.

## `include/auralis/bluetooth/IBluetoothManager.h`

- lifecycle contract only.

## `include/auralis/bluetooth/BluetoothManager.h`

- Phase 1 stub declaration.

## `src/bluetooth/BluetoothManager.cpp`

- Phase 1 stub lifecycle and logs only.

## `include/auralis/audio/IPipeWireManager.h`

- lifecycle contract only.

## `src/audio/PipeWireManager.cpp`

- Phase 1 stub only.

## `include/auralis/devices/IDeviceManager.h`

- higher-level device service lifecycle seam.

## `src/devices/DeviceManager.cpp`

- Phase 1 skeleton only.

## `include/auralis/session/ISessionManager.h`

- session service lifecycle seam.

## `src/session/SessionManager.cpp`

- Phase 1 skeleton only.

## `apps/desktop/main.cpp`

- thin process bootstrap.

## `ui/qml/Main.qml`

- Phase 1 status shell only.

## `ui/components/StatusRow.qml`

- reusable label/status row.

---

# 29. Recommended QML Shape

A minimal conceptual QML layout is:

```qml
ApplicationWindow {
    visible: true
    width: 720
    height: 480
    title: "Auralis"

    Column {
        Text { text: "AURALIS" }
        Text { text: "System Status" }

        StatusRow {
            label: "Bluetooth"
            status: AppCore.bluetoothStatus
        }

        StatusRow {
            label: "Audio"
            status: AppCore.audioStatus
        }

        StatusRow {
            label: "PipeWire"
            status: AppCore.pipeWireStatus
        }

        Text {
            text: AppCore.ready ? "Auralis Core Ready" : "Auralis Core Not Ready"
        }
    }
}
```

This is illustrative, not a requirement to copy verbatim.

Use modern Qt Quick controls/layouts if needed, but keep dependencies minimal.

---

# 30. Suggested Test Fake Pattern

Create small fake implementations inside test code rather than production code where possible.

Example:

```cpp
class FakeBluetoothManager final : public IBluetoothManager {
public:
    explicit FakeBluetoothManager(bool initializeResult)
        : initializeResult_(initializeResult) {}

    bool initialize() override {
        status_ = initializeResult_
            ? ServiceStatus::Ready
            : ServiceStatus::Error;
        return initializeResult_;
    }

    void shutdown() override {
        status_ = ServiceStatus::Uninitialized;
        shutdownCalled_ = true;
    }

    ServiceStatus status() const noexcept override {
        return status_;
    }

    bool shutdownCalled() const noexcept {
        return shutdownCalled_;
    }

private:
    bool initializeResult_;
    bool shutdownCalled_ = false;
    ServiceStatus status_ = ServiceStatus::Uninitialized;
};
```

Use equivalent fakes for failure-order testing.

Do not introduce mocking-framework dependencies unless necessary.

---

# 31. Phase 1 Definition of “Ready”

This point is critical.

In Phase 1:

```text
Bluetooth Ready
```

means:

```text
The BluetoothManager foundation object was initialized successfully.
```

It does **not** mean:

```text
A Bluetooth adapter was found or BlueZ was queried.
```

Likewise:

```text
PipeWire Ready
```

means:

```text
The PipeWireManager foundation object was initialized successfully.
```

It does **not** mean:

```text
A live PipeWire connection or graph was inspected.
```

Document this in code comments, README, architecture docs, and/or UI developer note.

This prevents Phase 1 placeholder semantics from becoming a misleading product contract.

---

# 32. Git Strategy for Phase 1

Do not create a giant unreviewable commit if incremental commits are possible.

Suggested commits:

```text
phase-1: initialize Auralis project foundation
phase-1: add core status logging and configuration
phase-1: add service layer skeletons
phase-1: add application core lifecycle
phase-1: add Qt QML desktop shell
phase-1: add unit and integration test infrastructure
phase-1: document architecture and validation
```

At the final gate, the roadmap suggests the baseline commit message:

```text
phase-1: initialize Auralis project foundation
```

If the work is split into several commits, use an equivalent final consolidation/release marker only if the user wants it.

Suggested completion tag:

```text
v0.1-phase1
```

Do not make Git commits or tags automatically unless the environment/user has authorized the AI IDE to do so.

Always show the exact commands before destructive Git operations.

---

# 33. Final Validation Gate

Before declaring success, perform all of the following.

## 33.1 Clean build

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
```

Must succeed.

## 33.2 Test suite

```bash
ctest --test-dir build --output-on-failure
```

Must pass 100% of Phase 1 tests.

## 33.3 Application launch

```bash
./build/apps/desktop/auralis-desktop
```

Must launch the QML window.

## 33.4 Status binding

Verify UI status is sourced from C++ backend values.

## 33.5 Log output

Verify startup logs identify major subsystems.

## 33.6 Scope audit

Search the project for accidental Phase 2+ implementation or shell command calls.

Suggested audit searches:

```bash
grep -R "StartDiscovery\|StopDiscovery\|org.bluez.Device1\|bluetoothctl\|wpctl\|pactl\|pw-cli" -n . \
    --exclude-dir=build \
    --exclude='*.md'
```

Expected for production code in Phase 1: no forbidden implementation matches.

Also inspect dependencies to ensure no accidental circular linkage.

---

# 34. Required Final Report From the AI IDE

When Phase 1 is complete, do **not** simply say “done.”

Return a concise but concrete engineering report with these sections.

## 34.1 Implemented

List:

- repository/module structure;
- CMake targets;
- core lifecycle;
- logging;
- configuration;
- service skeletons;
- QML shell;
- tests;
- docs.

## 34.2 Files created/modified

Provide the final important tree.

## 34.3 Validation commands run

Include exact commands.

## 34.4 Validation results

Report:

```text
CMake configure: PASS/FAIL
Build: PASS/FAIL
CTest: PASS/FAIL
Desktop launch: PASS/FAIL
QML binding: PASS/FAIL
Scope audit: PASS/FAIL
```

Do not claim PASS if the command was not actually run.

## 34.5 Known limitations

Expected Phase 1 limitations should include:

- no real Bluetooth discovery;
- no pairing/connection lifecycle;
- no real PipeWire registry;
- no audio route control;
- no session behavior;
- no LE Audio.

## 34.6 Owner decisions still needed

Flag any unresolved decision, especially the software license if none was specified.

## 34.7 Stop statement

End with a clear statement equivalent to:

```text
Phase 1 foundation gate is satisfied. No Phase 2 Bluetooth discovery code was implemented.
```

Only make that statement if all Phase 1 gate requirements actually pass.

---

# 35. Definition of Done

Phase 1 is DONE only when this statement is true:

> From a clean checkout/build directory, Auralis configures with CMake, builds with Ninja, passes its hardware-independent tests, launches a Qt/QML desktop shell, initializes logging/configuration and all service skeletons through a testable `ApplicationCore`, exposes backend status to QML, and contains no real Bluetooth discovery, PipeWire graph control, audio routing, or session implementation.

Anything less is not Phase 1 complete.

Anything materially beyond this definition belongs to a later phase and should not be implemented now.

---

# 36. Start Command to the AI IDE

Begin implementation now.

1. Inspect the repository and Git status first if files already exist.
2. Establish the directory and CMake target structure.
3. Implement the Phase 1 core services and tests incrementally.
4. Build after each meaningful stage.
5. Wire the minimal Qt/QML UI to the C++ backend.
6. Run the clean-room validation gate.
7. Fix all Phase 1 failures.
8. Produce the required final engineering report.
9. Stop. Do not begin Phase 2.

**Primary success target:**

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/desktop/auralis-desktop
```

All four actions must succeed for the Phase 1 gate to pass.

---

# 37. Phase 1 Source Scope Summary

The intended Auralis Phase 1 outcome is a production-ready **project foundation**, not a hardware prototype.

Build the architecture now so that later phases can add:

```text
Phase 2 -> BlueZ discovery
Phase 3 -> pairing and device lifecycle
Phase 4 -> PipeWire endpoint integration
Phase 5 -> audio routing
Phase 6 -> multi-device sessions
Phase 7 -> complete GUI
Phase 8 -> reliability and packaging
```

But implement **none of those later behaviors in this phase**.

The best result is a small, clean, testable foundation that the next phase can extend without architectural rework.
