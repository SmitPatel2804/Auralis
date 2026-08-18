#include "DesktopApplication.h"

#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/ApplicationCore.h>
#include <auralis/core/Logger.h>
#include <auralis/core/LoggingCategories.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/session/SessionManager.h>

#include <QGuiApplication>
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
    services.bluetooth = std::move(bluetooth);
    services.pipeWire = std::make_unique<auralis::audio::PipeWireManager>(registry);
    services.devices = std::make_unique<auralis::devices::DeviceManager>();
    services.sessions = std::make_unique<auralis::session::SessionManager>();
    return services;
}

} // namespace

int DesktopApplication::run(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Auralis"));
    QGuiApplication::setApplicationName(QStringLiteral("Auralis"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    auralis::core::Logger::initialize({.enableConsole = true, .enableFile = false, .filePath = {}});
    qCInfo(auralisCore) << "Auralis starting";

    auralis::core::ApplicationCore core(makeProductionServices());
    qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

    if (!core.initialize()) {
        qCCritical(auralisCore) << "Application core initialization failed";
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
