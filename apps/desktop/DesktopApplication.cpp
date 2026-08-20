#include "DesktopApplication.h"

#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/ApplicationCore.h>
#include <auralis/core/QmlTypeRegistration.h>
#include <auralis/core/Logger.h>
#include <auralis/core/LoggingCategories.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/recovery/RecoveryManager.h>
#include <auralis/session/SessionManager.h>
#include <auralis/ui/NotificationController.h>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QTimer>
#include <QtQml>

#include <memory>

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

namespace auralis::desktop {
namespace {

auralis::core::ApplicationServices makeProductionServices()
{
    auralis::core::ApplicationServices services;
    services.configuration = std::make_unique<auralis::core::ConfigurationManager>();
    auto bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>();
    auralis::bluetooth::DeviceRegistry* registry = bluetooth->deviceRegistry();
    auto pipeWire = std::make_unique<auralis::audio::PipeWireManager>(registry);
    services.bluetooth = std::move(bluetooth);
    services.pipeWire = std::move(pipeWire);
    services.devices = std::make_unique<auralis::devices::DeviceManager>();
    services.sessions = std::make_unique<auralis::session::SessionManager>(
        static_cast<auralis::bluetooth::BluetoothManager*>(services.bluetooth.get()),
        static_cast<auralis::audio::PipeWireManager*>(services.pipeWire.get()),
        QString());
    return services;
}

void wireRecoveryOrchestration(auralis::core::ApplicationCore& core)
{
    auto* recovery = core.recoveryManager();
    if (recovery == nullptr) {
        return;
    }

    auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(core.bluetooth());
    auto* pipeWire = qobject_cast<auralis::audio::PipeWireManager*>(core.audio());
    auto* sessions = qobject_cast<auralis::session::SessionManager*>(core.sessions());
    auto* configuration = qobject_cast<auralis::core::ConfigurationManager*>(core.configuration());
    auto* power = core.powerMonitor();

    if (pipeWire != nullptr && configuration != nullptr) {
        pipeWire->setAutoReconnectEnabled(configuration->autoRecoverServices());
        QObject::connect(
            configuration,
            &auralis::core::ConfigurationManager::autoRecoverServicesChanged,
            pipeWire,
            [pipeWire, configuration]() {
                pipeWire->setAutoReconnectEnabled(configuration->autoRecoverServices());
            });
    }

    auralis::recovery::RecoveryManager::HostHooks hooks;
    hooks.pauseBluetoothReconnect = [bluetooth]() {
        if (bluetooth != nullptr) {
            bluetooth->pauseManagedReconnect();
        }
    };
    hooks.resumeBluetoothReconnect = [bluetooth]() {
        if (bluetooth != nullptr) {
            bluetooth->resumeManagedReconnect();
        }
    };
    hooks.requestBlueZRefresh = [bluetooth]() {
        if (bluetooth != nullptr) {
            bluetooth->refresh();
        }
    };
    hooks.requestPipeWireReconnect = [pipeWire]() {
        if (pipeWire != nullptr) {
            pipeWire->requestReconnect();
        }
    };
    hooks.refreshActiveSession = [sessions]() {
        if (sessions != nullptr) {
            sessions->refreshActiveSession();
        }
    };
    hooks.isBlueZAvailable = [bluetooth]() {
        return bluetooth != nullptr && bluetooth->available();
    };
    hooks.isPipeWireConnected = [pipeWire]() {
        return pipeWire != nullptr && pipeWire->connected();
    };
    hooks.isPipeWireGraphReady = [pipeWire]() {
        return pipeWire != nullptr && pipeWire->connected() && pipeWire->initialSyncComplete();
    };
    hooks.isSystemBusConnected = [bluetooth]() {
        return bluetooth != nullptr && bluetooth->systemBusConnected();
    };
    recovery->setHooks(std::move(hooks));

    if (bluetooth != nullptr) {
        QObject::connect(
            bluetooth,
            &auralis::bluetooth::BluetoothManager::availableChanged,
            recovery,
            [recovery, bluetooth]() { recovery->notifyBlueZAvailable(bluetooth->available()); });
        QObject::connect(
            bluetooth,
            &auralis::bluetooth::BluetoothManager::systemBusConnectedChanged,
            recovery,
            &auralis::recovery::RecoveryManager::notifySystemBusConnected);
        if (power != nullptr) {
            QObject::connect(
                bluetooth,
                &auralis::bluetooth::BluetoothManager::systemBusConnectedChanged,
                power,
                [power](bool connected) {
                    if (connected) {
                        power->notifySystemBusAvailable();
                    } else {
                        power->notifySystemBusUnavailable();
                    }
                });
            if (bluetooth->systemBusConnected()) {
                power->notifySystemBusAvailable();
            } else {
                power->notifySystemBusUnavailable();
            }
        }
        QObject::connect(
            bluetooth,
            &auralis::bluetooth::BluetoothManager::adapterChanged,
            recovery,
            [recovery, bluetooth]() {
                recovery->notifyAdapterPresent(
                    !bluetooth->adapterAddress().isEmpty() || bluetooth->adapterPowered());
            });
        recovery->notifyBlueZAvailable(bluetooth->available());
        recovery->notifySystemBusConnected(bluetooth->systemBusConnected());
    }

    if (pipeWire != nullptr) {
        const auto pushPw = [recovery, pipeWire]() {
            if (pipeWire->connected()) {
                recovery->notifyPipeWireConnected(true, pipeWire->initialSyncComplete());
            } else if (
                pipeWire->connectionState() == auralis::audio::PipeWireConnectionState::Error
                || pipeWire->connectionState() == auralis::audio::PipeWireConnectionState::Stopped) {
                recovery->notifyPipeWireError(pipeWire->lastError());
            } else {
                recovery->notifyPipeWireConnected(false, false);
            }
        };
        QObject::connect(pipeWire, &auralis::audio::PipeWireManager::connectionStateChanged, recovery, pushPw);
        QObject::connect(pipeWire, &auralis::audio::PipeWireManager::graphRevisionChanged, recovery, pushPw);
        QObject::connect(
            pipeWire,
            &auralis::audio::PipeWireManager::reconnectAttemptStarted,
            recovery,
            &auralis::recovery::RecoveryManager::notifyPipeWireReconnectAttempt);
        QObject::connect(
            pipeWire,
            &auralis::audio::PipeWireManager::reconnectExhausted,
            recovery,
            &auralis::recovery::RecoveryManager::notifyPipeWireReconnectExhausted);
        pushPw();
    }
}

} // namespace

int DesktopApplication::run(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Auralis"));
    QGuiApplication::setApplicationName(QStringLiteral("Auralis"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    auralis::core::Logger::initialize({.enableConsole = true, .enableFile = false, .filePath = {}});
    if (qEnvironmentVariableIntValue("AURALIS_VERBOSE") != 1) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "auralis.audio.debug=false\n"
            "auralis.bluetooth.debug=false\n"
            "auralis.session.debug=false\n"
            "auralis.ui.debug=false\n"
            "auralis.devices.debug=false\n"
            "auralis.config.debug=false\n"
            "auralis.core.debug=false"));
    }
    qCInfo(auralisCore) << "Auralis starting";

    auralis::core::ApplicationCore core(makeProductionServices());
    auralis::core::registerAuralisQmlTypes();
    qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

    if (!core.initialize()) {
        qCCritical(auralisCore) << "Application core initialization failed";
    }
    wireRecoveryOrchestration(core);

    if (auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(core.bluetooth())) {
        QObject::connect(bluetooth, &auralis::bluetooth::BluetoothManager::errorTextChanged, &core, [&core, bluetooth]() {
            const QString text = bluetooth->errorText();
            if (text.isEmpty()) {
                return;
            }
            if (auto* notes = qobject_cast<auralis::ui::NotificationController*>(core.notifications())) {
                notes->postError(QStringLiteral("Bluetooth"), text);
            }
        });
    }
    if (auto* sessions = qobject_cast<auralis::session::SessionManager*>(core.sessions())) {
        QObject::connect(
            sessions,
            &auralis::session::SessionManager::sessionError,
            &core,
            [&core](const QString& sessionId, auralis::session::SessionError, const QString& detail) {
                if (auto* notes = qobject_cast<auralis::ui::NotificationController*>(core.notifications())) {
                    notes->postError(QStringLiteral("Session"), detail.isEmpty() ? sessionId : detail);
                }
            });
        if (auto* configuration = qobject_cast<auralis::core::ConfigurationManager*>(core.configuration())) {
            if (configuration->restoreLastSession()) {
                sessions->restoreLastSession();
            }
        }
    }
    if (auto* audio = qobject_cast<auralis::audio::PipeWireManager*>(core.audio())) {
        if (auto* router = audio->audioRouter()) {
            QObject::connect(
                router,
                &auralis::audio::AudioRouter::routeError,
                &core,
                [&core](const QString& routeId, auralis::audio::RouteError, const QString& detail) {
                    if (auto* notes = qobject_cast<auralis::ui::NotificationController*>(core.notifications())) {
                        notes->postError(QStringLiteral("Routing"), detail.isEmpty() ? routeId : detail);
                    }
                });
        }
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            qCCritical(auralisUi) << "QML root object creation failed";
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("Auralis.Ui", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCCritical(auralisUi) << "QML root failed to load";
        core.shutdown();
        auralis::core::Logger::shutdown();
        return 1;
    }

    qCInfo(auralisUi) << "QML root loaded";

    if (qEnvironmentVariableIntValue("AURALIS_AUTO_SCAN") == 1) {
        if (auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(core.bluetooth())) {
            QTimer::singleShot(800, bluetooth, &auralis::bluetooth::BluetoothManager::startScan);
            QTimer::singleShot(4000, bluetooth, &auralis::bluetooth::BluetoothManager::stopScan);
            QTimer::singleShot(4500, &app, &QCoreApplication::quit);
        }
    }

    const int exitCode = app.exec();

    core.shutdown();
    auralis::core::Logger::shutdown();
    return exitCode;
}

} // namespace auralis::desktop
