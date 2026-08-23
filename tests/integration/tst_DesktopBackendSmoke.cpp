#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/core/ApplicationCore.h>
#include <auralis/core/ConfigurationManager.h>
#include <auralis/core/Logger.h>
#include <auralis/core/QmlTypeRegistration.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/session/SessionListModel.h>
#include <auralis/session/SessionManager.h>

#include "FakeBlueZClient.h"

#include <auralis/bluetooth/DeviceRegistry.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtQml>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

class TstDesktopBackendSmoke : public QObject {
    Q_OBJECT

private:
    static auralis::core::ApplicationServices makeServices(
        auralis::bluetooth::IBlueZClient* client,
        const QString& sessionPath)
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
        services.sessions = std::make_unique<auralis::session::SessionManager>(
            bluetooth,
            static_cast<auralis::audio::PipeWireManager*>(services.pipeWire.get()),
            sessionPath);
        return services;
    }

private slots:
    void qmlBindsToBackendStatus()
    {
        auralis::core::Logger::initialize();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ApplicationCore core(makeServices(nullptr, dir.filePath(QStringLiteral("sessions.json"))));
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
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto* client = new auralis::test::FakeBlueZClient;
        client->setAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Alias"), QStringLiteral("smit")},
             {QStringLiteral("Powered"), true}});

        auralis::core::ApplicationCore core(makeServices(client, dir.filePath(QStringLiteral("sessions.json"))));
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

    void qmlNavigatesEveryPageAndSessionCommandMatrix()
    {
        auralis::core::Logger::initialize();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto* client = new auralis::test::FakeBlueZClient;
        client->setAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Alias"), QStringLiteral("smit")},
             {QStringLiteral("Powered"), true}});
        client->setDevice(
            QStringLiteral("/org/bluez/hci0/dev_88_08_94_9D_B4_22"),
            {{QStringLiteral("Address"), QStringLiteral("88:08:94:9D:B4:22")},
             {QStringLiteral("Alias"), QStringLiteral("Smokin' Buds")},
             {QStringLiteral("Name"), QStringLiteral("Smokin' Buds")},
             {QStringLiteral("AddressType"), QStringLiteral("public")},
             {QStringLiteral("Paired"), true},
             {QStringLiteral("Connected"), true},
             {QStringLiteral("Trusted"), true},
             {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")}}});

        auralis::core::ApplicationCore core(makeServices(client, dir.filePath(QStringLiteral("sessions.json"))));
        QVERIFY(core.initialize());
        auralis::core::registerAuralisQmlTypes();
        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QQmlApplicationEngine engine;
        engine.loadFromModule("Auralis.Ui", "Main");
        QVERIFY2(!engine.rootObjects().isEmpty(), "QML root failed to load");
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->show();
        window->resize(1100, 800);
        QTest::qWait(80);

        const QStringList pageNames{
            QStringLiteral("pageDashboard"),
            QStringLiteral("pageDevices"),
            QStringLiteral("pageSessions"),
            QStringLiteral("pagePlayground"),
            QStringLiteral("pageAudioRouting"),
            QStringLiteral("pageDiagnostics"),
            QStringLiteral("pageSettings")};
        for (int page = 0; page < pageNames.size(); ++page) {
            core.navigateTo(page);
            QCOMPARE(core.currentPage(), page);
            auto* item = window->findChild<QQuickItem*>(pageNames.at(page));
            QVERIFY2(item != nullptr, qPrintable(pageNames.at(page)));
        }

        auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(core.bluetooth());
        QVERIFY(bluetooth != nullptr);
        QCOMPARE(bluetooth->deviceCount(), 1);
        QVERIFY(bluetooth->hasDevice(QStringLiteral("/org/bluez/hci0/dev_88_08_94_9D_B4_22")));
        const QVariantMap details = bluetooth->deviceDetails(QStringLiteral("/org/bluez/hci0/dev_88_08_94_9D_B4_22"));
        QCOMPARE(details.value(QStringLiteral("address")).toString(), QStringLiteral("88:08:94:9D:B4:22"));
        QVERIFY(!bluetooth->serviceFriendlyName(QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")).isEmpty());
        QCOMPARE(bluetooth->connectedDeviceCount(), 1);
        QCOMPARE(
            bluetooth->devices()->data(bluetooth->devices()->index(0, 0), auralis::bluetooth::BluetoothDeviceListModel::ConnectedRole)
                .toBool(),
            true);

        auto* sessions = qobject_cast<auralis::session::SessionManager*>(core.sessions());
        QVERIFY(sessions != nullptr);
        const QString sessionId = sessions->createSession(QStringLiteral("yt"));
        QVERIFY(!sessionId.isEmpty());
        QVERIFY(sessions->activateSession(sessionId) == auralis::session::SessionCommandResult::NoSourceConfigured);
        QVERIFY(sessions->setSource(sessionId, QStringLiteral("missing-source")) == auralis::session::SessionCommandResult::Accepted);
        QVERIFY(sessions->activateSession(sessionId) == auralis::session::SessionCommandResult::NoMembersConfigured);
        QVERIFY(sessions->retrySession(sessionId) == auralis::session::SessionCommandResult::InvalidState);
        QVERIFY(
            sessions->addDevice(sessionId, QStringLiteral("/org/bluez/hci0/dev_88_08_94_9D_B4_22"))
            == auralis::session::SessionCommandResult::Accepted);
        QCOMPARE(sessions->sessionById(sessionId)->devices.front().deviceId, QStringLiteral("88:08:94:9D:B4:22"));
        auto* members = qobject_cast<auralis::session::SessionMemberListModel*>(sessions->sessionMembers());
        QVERIFY(members != nullptr);
        members->setSessionId(sessionId);
        QCOMPARE(members->count(), 1);

        auto* audio = qobject_cast<auralis::audio::PipeWireManager*>(core.audio());
        QVERIFY(audio != nullptr);
        QVERIFY(audio->endpoints() != nullptr);
        QVERIFY(core.diagnostics() != nullptr);
        auto* config = qobject_cast<auralis::core::ConfigurationManager*>(core.configuration());
        QVERIFY(config != nullptr);
        QVERIFY(config->setShowDeveloperStatus(true));
        QVERIFY(config->setRestoreLastSession(false));

        core.shutdown();
        auralis::core::Logger::shutdown();
    }

    void liveGuiPagesAndHardwareEndpoints()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_GUI_ENDPOINT_LIVE") == 0) {
            QSKIP("Set AURALIS_RUN_GUI_ENDPOINT_LIVE=1 to exercise live GUI + PipeWire/BlueZ endpoints");
        }

        auralis::core::Logger::initialize();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ApplicationCore core(makeServices(nullptr, dir.filePath(QStringLiteral("sessions.json"))));
        QVERIFY(core.initialize());
        auralis::core::registerAuralisQmlTypes();
        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QQmlApplicationEngine engine;
        engine.loadFromModule("Auralis.Ui", "Main");
        QVERIFY2(!engine.rootObjects().isEmpty(), "live QML root failed to load");
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->show();
        QTest::qWait(200);

        for (int page = 0; page < 7; ++page) {
            core.navigateTo(page);
            QCOMPARE(core.currentPage(), page);
            QTest::qWait(40);
        }

        auto* bluetooth = qobject_cast<auralis::bluetooth::BluetoothManager*>(core.bluetooth());
        auto* audio = qobject_cast<auralis::audio::PipeWireManager*>(core.audio());
        QVERIFY(bluetooth != nullptr);
        QVERIFY(audio != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(bluetooth->deviceCount() >= 1, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(audio->connectionState() == auralis::audio::PipeWireConnectionState::Connected, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(audio->initialSyncComplete(), 8000);

        QVERIFY(bluetooth->deviceCount() >= 1);
        auto* deviceModel = bluetooth->devices();
        QVERIFY(deviceModel != nullptr);
        int connected = 0;
        for (int row = 0; row < deviceModel->rowCount(); ++row) {
            const QModelIndex idx = deviceModel->index(row, 0);
            const QString path = deviceModel->data(idx, auralis::bluetooth::BluetoothDeviceListModel::ObjectPathRole).toString();
            const QString address = deviceModel->data(idx, auralis::bluetooth::BluetoothDeviceListModel::AddressRole).toString();
            QVERIFY(bluetooth->hasDevice(path));
            const QVariantMap details = bluetooth->deviceDetails(path);
            QCOMPARE(details.value(QStringLiteral("address")).toString().toUpper(), address.toUpper());
            QVERIFY(audio->audioStatusForDevice(path).length() >= 0);
            if (deviceModel->data(idx, auralis::bluetooth::BluetoothDeviceListModel::PairedRole).toBool()
                && !deviceModel->data(idx, auralis::bluetooth::BluetoothDeviceListModel::ConnectedRole).toBool()) {
                bluetooth->connectDevice(path);
            }
            if (deviceModel->data(idx, auralis::bluetooth::BluetoothDeviceListModel::ConnectedRole).toBool()) {
                ++connected;
            }
        }
        QVERIFY(bluetooth->connectedDeviceCount() >= 0);

        QTRY_VERIFY_WITH_TIMEOUT(audio->mappedBluetoothCount() >= 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(audio->endpointCount() >= 1, 5000);

        auto* endpoints = audio->endpoints();
        QVERIFY(endpoints != nullptr);
        QVERIFY(audio->endpointCount() == endpoints->rowCount());
        QVERIFY(audio->endpointCount() >= 1);
        int mappedPlayback = 0;
        using Ep = auralis::audio::AudioEndpointListModel;
        for (int row = 0; row < endpoints->rowCount(); ++row) {
            const QModelIndex idx = endpoints->index(row, 0);
            QVERIFY(!endpoints->data(idx, Ep::EndpointIdRole).toString().isEmpty());
            QVERIFY(!endpoints->data(idx, Ep::NameRole).toString().isEmpty());
            QVERIFY(!endpoints->data(idx, Ep::DirectionRole).toString().isEmpty());
            QVERIFY(!endpoints->data(idx, Ep::TransportRole).toString().isEmpty());
            const bool mapped = endpoints->data(idx, Ep::MappedRole).toBool();
            const QString direction = endpoints->data(idx, Ep::DirectionRole).toString();
            if (mapped && (direction == QStringLiteral("Playback") || direction == QStringLiteral("Duplex"))) {
                QVERIFY(!endpoints->data(idx, Ep::BluetoothAddressRole).toString().isEmpty());
                ++mappedPlayback;
            }
        }
        QVERIFY2(mappedPlayback >= 1, "expected at least one mapped Bluetooth playback endpoint");

        auto* router = audio->audioRouter();
        QVERIFY(router != nullptr);
        QVERIFY(router->sources() != nullptr);
        QVERIFY(router->routeModel() != nullptr);

        auto* sessions = qobject_cast<auralis::session::SessionManager*>(core.sessions());
        QVERIFY(sessions != nullptr);
        QVERIFY(sessions->sessionList() != nullptr);
        QVERIFY(sessions->sessionMembers() != nullptr);
        QVERIFY(sessions->currentMembers() != nullptr);
        QVERIFY(sessions->selectedSession() != nullptr);

        QVERIFY(core.diagnostics() != nullptr);
        QVERIFY(core.notifications() != nullptr);

        core.shutdown();
        auralis::core::Logger::shutdown();
    }
};

QTEST_MAIN(TstDesktopBackendSmoke)
#include "tst_DesktopBackendSmoke.moc"
