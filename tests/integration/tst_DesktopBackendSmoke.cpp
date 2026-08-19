#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/core/ApplicationCore.h>
#include <auralis/core/Logger.h>
#include <auralis/core/QmlTypeRegistration.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/session/SessionManager.h>

#include "FakeBlueZClient.h"

#include <auralis/bluetooth/DeviceRegistry.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QtQml>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

class TstDesktopBackendSmoke : public QObject {
    Q_OBJECT

private:
    static auralis::core::ApplicationServices makeServices(auralis::bluetooth::IBlueZClient* client = nullptr)
    {
        auralis::core::ApplicationServices services;
        if (client == nullptr) {
            services.bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>();
        } else {
            services.bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>(client);
        }
        auto* bluetooth = static_cast<auralis::bluetooth::BluetoothManager*>(services.bluetooth.get());
        services.pipeWire = std::make_unique<auralis::audio::PipeWireManager>(bluetooth->deviceRegistry());
        services.devices = std::make_unique<auralis::devices::DeviceManager>();
        services.sessions = std::make_unique<auralis::session::SessionManager>();
        return services;
    }

private slots:
    void qmlBindsToBackendStatus()
    {
        auralis::core::Logger::initialize();

        auralis::core::ApplicationCore core(makeServices());
        QVERIFY(core.initialize());
        QCOMPARE(core.bluetoothStatus(), QStringLiteral("Ready"));
        QVERIFY(core.audio() != nullptr);
        QVERIFY(core.pipeWireStatus() != QStringLiteral("Uninitialized"));
        QVERIFY(core.isReady());

        auralis::core::registerAuralisQmlTypes();
        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QQmlApplicationEngine engine;
        engine.loadFromModule("Auralis.Ui", "Main");
        QVERIFY2(!engine.rootObjects().isEmpty(), "QML root failed to load under QT_QPA_PLATFORM=offscreen");

        auto* window = engine.rootObjects().constFirst();
        QVERIFY(window != nullptr);
        QCOMPARE(window->property("title").toString(), QStringLiteral("Auralis"));
        core.navigateTo(3);
        QCOMPARE(core.currentPage(), 3);

        core.shutdown();
        auralis::core::Logger::shutdown();
    }

    void qmlSurvivesLiveDeviceInsert()
    {
        auralis::core::Logger::initialize();

        auto* client = new auralis::test::FakeBlueZClient;
        client->setAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Alias"), QStringLiteral("smit")},
             {QStringLiteral("Powered"), true}});

        auralis::core::ApplicationCore core(makeServices(client));
        QVERIFY(core.initialize());

        auralis::core::registerAuralisQmlTypes();
        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QQmlApplicationEngine engine;
        engine.loadFromModule("Auralis.Ui", "Main");
        QVERIFY2(!engine.rootObjects().isEmpty(), "QML root failed to load under QT_QPA_PLATFORM=offscreen");

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->show();
        window->resize(760, 720);
        QTest::qWait(100);

        auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(core.bluetooth());
        QVERIFY(bluetooth != nullptr);
        QCOMPARE(bluetooth->deviceCount(), 0);

        client->setDevice(
            QStringLiteral("/org/bluez/hci0/dev_88_42_D0_59_CC_71"),
            {{QStringLiteral("Address"), QStringLiteral("88:42:D0:59:CC:71")},
             {QStringLiteral("Alias"), QStringLiteral("JioSTB14305JioSTB")},
             {QStringLiteral("Name"), QStringLiteral("JioSTB14305JioSTB")},
             {QStringLiteral("AddressType"), QStringLiteral("public")},
             {QStringLiteral("Class"), 0x240418u},
             {QStringLiteral("Paired"), false},
             {QStringLiteral("Connected"), false},
             {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")}},
             {QStringLiteral("RSSI"), -62}});
        QTRY_COMPARE(bluetooth->deviceCount(), 1);
        client->updateDevice(QStringLiteral("/org/bluez/hci0/dev_88_42_D0_59_CC_71"), {{QStringLiteral("RSSI"), -55}});
        QTest::qWait(150);
        QCOMPARE(bluetooth->deviceCount(), 1);
        QVERIFY(engine.rootObjects().constFirst() != nullptr);

        core.shutdown();
        auralis::core::Logger::shutdown();
    }
};

QTEST_MAIN(TstDesktopBackendSmoke)
#include "tst_DesktopBackendSmoke.moc"
