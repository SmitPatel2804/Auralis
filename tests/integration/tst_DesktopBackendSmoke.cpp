#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/core/ApplicationCore.h>
#include <auralis/core/Logger.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/session/SessionManager.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QtQml>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

class TstDesktopBackendSmoke : public QObject {
    Q_OBJECT

private slots:
    void qmlBindsToBackendStatus()
    {
        auralis::core::Logger::initialize();

        auralis::core::ApplicationServices services;
        services.bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>();
        services.pipeWire = std::make_unique<auralis::audio::PipeWireManager>();
        services.devices = std::make_unique<auralis::devices::DeviceManager>();
        services.sessions = std::make_unique<auralis::session::SessionManager>();

        auralis::core::ApplicationCore core(std::move(services));
        QVERIFY(core.initialize());
        QCOMPARE(core.bluetoothStatus(), QStringLiteral("Ready"));
        QCOMPARE(core.audioStatus(), QStringLiteral("Ready"));
        QCOMPARE(core.pipeWireStatus(), QStringLiteral("Ready"));
        QVERIFY(core.isReady());

        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QQmlApplicationEngine engine;
        engine.loadFromModule("Auralis.Ui", "Main");
        QVERIFY2(!engine.rootObjects().isEmpty(), "QML root failed to load under QT_QPA_PLATFORM=offscreen");

        auto* window = engine.rootObjects().constFirst();
        QVERIFY(window != nullptr);
        QCOMPARE(window->property("title").toString(), QStringLiteral("Auralis"));

        core.shutdown();
        auralis::core::Logger::shutdown();
    }
};

QTEST_MAIN(TstDesktopBackendSmoke)
#include "tst_DesktopBackendSmoke.moc"
