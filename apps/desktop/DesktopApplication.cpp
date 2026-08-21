#include "DesktopApplication.h"

#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/IAudioManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>
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
#include <QtGlobal>

#include <memory>
#include <type_traits>

#if defined(Q_OS_LINUX)
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#else
#include <auralis/audio/NativeAudioManager.h>
#include <auralis/bluetooth/NativeBluetoothManager.h>
#endif

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

namespace auralis::desktop {
namespace {

auralis::core::ApplicationServices makeProductionServices()
{
    auralis::core::ApplicationServices services;
    services.configuration = std::make_unique<auralis::core::ConfigurationManager>();
#if defined(Q_OS_LINUX)
    auto bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>();
#else
    auto bluetooth = std::make_unique<auralis::bluetooth::NativeBluetoothManager>();
#endif
    auralis::bluetooth::DeviceRegistry* registry = bluetooth->deviceRegistry();
#if defined(Q_OS_LINUX)
    auto pipeWire = std::make_unique<auralis::audio::PipeWireManager>(registry);
#else
    auto pipeWire = std::make_unique<auralis::audio::NativeAudioManager>(registry);
#endif
    services.bluetooth = std::move(bluetooth);
    services.pipeWire = std::move(pipeWire);
    services.devices = std::make_unique<auralis::devices::DeviceManager>();
    services.sessions = std::make_unique<auralis::session::SessionManager>(
        services.bluetooth.get(),
        services.pipeWire.get(),
        QString());
    return services;
}

void wireRecoveryOrchestration(auralis::core::ApplicationCore& core)
{
    auto* recovery = core.recoveryManager();
    if (recovery == nullptr) {
        return;
    }

    auto* bluetooth = core.bluetoothService();
    auto* audio = core.audioService();
    QObject* bluetoothUi = bluetooth != nullptr ? bluetooth->uiObject() : nullptr;
    QObject* audioUi = audio != nullptr ? audio->uiObject() : nullptr;
    auto* sessions = qobject_cast<auralis::session::SessionManager*>(core.sessions());
    auto* configuration = qobject_cast<auralis::core::ConfigurationManager*>(core.configuration());
    auto* power = core.powerMonitor();

    if (audio != nullptr && configuration != nullptr) {
        audio->setAutoReconnectEnabled(configuration->autoRecoverServices());
        QObject::connect(
            configuration,
            &auralis::core::ConfigurationManager::autoRecoverServicesChanged,
            audioUi != nullptr ? audioUi : &core,
            [audio, configuration]() {
                audio->setAutoReconnectEnabled(configuration->autoRecoverServices());
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
    hooks.requestPipeWireReconnect = [audio]() {
        if (audio != nullptr) {
            audio->requestReconnect();
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
    hooks.isPipeWireConnected = [audio]() {
        return audio != nullptr && audio->connected();
    };
    hooks.isPipeWireGraphReady = [audio]() {
        return audio != nullptr && audio->graphReady();
    };
    hooks.isSystemBusConnected = [bluetooth]() {
        return bluetooth != nullptr && bluetooth->transportConnected();
    };
    hooks.isAdapterPresent = [bluetooth]() {
        return bluetooth != nullptr && bluetooth->adapterPresent();
    };
    hooks.audioLastError = [audio]() {
        return audio != nullptr ? audio->lastError() : QString();
    };
    recovery->setHooks(std::move(hooks));

    if (bluetooth != nullptr && bluetoothUi != nullptr) {
        QObject::connect(bluetoothUi, SIGNAL(availableChanged()), recovery, SLOT(synchronizeBluetoothHealth()));
        QObject::connect(bluetoothUi, SIGNAL(adapterChanged()), recovery, SLOT(synchronizeBluetoothHealth()));
        QObject::connect(
            bluetoothUi,
            SIGNAL(transportConnectedChanged(bool)),
            recovery,
            SLOT(notifySystemBusConnected(bool)));
        if (power != nullptr) {
            QObject::connect(
                bluetoothUi,
                SIGNAL(transportConnectedChanged(bool)),
                power,
                SLOT(injectTransportAvailability(bool)));
            if (bluetooth->transportConnected()) {
                power->notifySystemBusAvailable();
            } else {
                power->notifySystemBusUnavailable();
            }
        }
        recovery->synchronizeBluetoothHealth();
    }

    if (audio != nullptr && audioUi != nullptr) {
        QObject::connect(audioUi, SIGNAL(connectionStateChanged()), recovery, SLOT(synchronizeAudioHealth()));
        QObject::connect(audioUi, SIGNAL(graphRevisionChanged()), recovery, SLOT(synchronizeAudioHealth()));
        QObject::connect(
            audioUi,
            SIGNAL(reconnectAttemptStarted(int)),
            recovery,
            SLOT(notifyPipeWireReconnectAttempt(int)));
        QObject::connect(
            audioUi,
            SIGNAL(reconnectExhausted(QString)),
            recovery,
            SLOT(notifyPipeWireReconnectExhausted(QString)));
        recovery->synchronizeAudioHealth();
    }
}

} // namespace

int DesktopApplication::run(int argc, char* argv[])
{
    // Auralis supplies its own visual treatment for Qt Quick Controls. Native
    // styles intentionally reject background/content overrides on platforms
    // such as Windows, so use the cross-platform customizable style unless a
    // developer explicitly selected another one before launch.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));
    }

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

    if (auto* bluetoothService = core.bluetoothService()) {
#if defined(Q_OS_LINUX)
        auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(bluetoothService->uiObject());
#else
        auto* bluetooth = qobject_cast<auralis::bluetooth::NativeBluetoothManager*>(bluetoothService->uiObject());
#endif
        if (bluetooth != nullptr) {
            using BluetoothType = std::remove_pointer_t<decltype(bluetooth)>;
            QObject::connect(bluetooth, &BluetoothType::errorTextChanged, &core, [&core, bluetooth]() {
                const QString text = bluetooth->errorText();
                if (text.isEmpty()) {
                    return;
                }
                if (auto* notes = qobject_cast<auralis::ui::NotificationController*>(core.notifications())) {
                    notes->postError(QStringLiteral("Bluetooth"), text);
                }
            });
        }
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
    if (auto* audio = core.audioService()) {
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
        if (auto* bluetooth = core.bluetoothService(); bluetooth != nullptr && bluetooth->uiObject() != nullptr) {
            QObject* bluetoothUi = bluetooth->uiObject();
            QTimer::singleShot(800, bluetoothUi, [bluetoothUi]() {
                QMetaObject::invokeMethod(bluetoothUi, "startScan", Qt::QueuedConnection);
            });
            QTimer::singleShot(4000, bluetoothUi, [bluetoothUi]() {
                QMetaObject::invokeMethod(bluetoothUi, "stopScan", Qt::QueuedConnection);
            });
            QTimer::singleShot(4500, &app, &QCoreApplication::quit);
        }
    }

    const int exitCode = app.exec();

    core.shutdown();
    auralis::core::Logger::shutdown();
    return exitCode;
}

} // namespace auralis::desktop
