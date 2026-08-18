#include "fakes/ServiceFakes.h"

#include <auralis/core/ApplicationCore.h>
#include <auralis/core/Logger.h>

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using auralis::core::ApplicationCore;
using auralis::core::ApplicationServices;
using auralis::core::ConfigurationManager;
using auralis::test::FakeBluetoothManager;
using auralis::test::FakeConfigurationManager;
using auralis::test::FakeDeviceManager;
using auralis::test::FakePipeWireManager;
using auralis::test::FakeSessionManager;

class TstApplicationCore : public QObject {
    Q_OBJECT

private:
    static std::unique_ptr<ConfigurationManager> makeIsolatedConfig()
    {
        QTemporaryDir dir;
        dir.setAutoRemove(false);
        const QString path = dir.filePath(QStringLiteral("auralis-core-test.ini"));
        auto settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
        return std::make_unique<ConfigurationManager>(std::move(settings));
    }

    static ApplicationServices makeServices(
        bool bluetoothOk = true,
        bool pipeWireOk = true,
        bool devicesOk = true,
        bool sessionsOk = true)
    {
        ApplicationServices services;
        services.configuration = makeIsolatedConfig();
        services.bluetooth = std::make_unique<FakeBluetoothManager>(bluetoothOk);
        services.pipeWire = std::make_unique<FakePipeWireManager>(pipeWireOk);
        services.devices = std::make_unique<FakeDeviceManager>(devicesOk);
        services.sessions = std::make_unique<FakeSessionManager>(sessionsOk);
        return services;
    }

private slots:
    void initTestCase()
    {
        auralis::core::Logger::initialize();
    }

    void cleanupTestCase()
    {
        auralis::core::Logger::shutdown();
    }

    void succeedsWhenAllServicesInitialize()
    {
        ApplicationCore core(makeServices());
        QSignalSpy spy(&core, &ApplicationCore::statusChanged);

        QVERIFY(core.initialize());
        QVERIFY(core.isReady());
        QCOMPARE(core.coreStatus(), QStringLiteral("Ready"));
        QCOMPARE(core.bluetoothStatus(), QStringLiteral("Ready"));
        QCOMPARE(core.audioStatus(), QStringLiteral("Ready"));
        QCOMPARE(core.pipeWireStatus(), QStringLiteral("Ready"));
        QVERIFY(spy.count() >= 1);
    }

    void configurationFailurePreventsLaterServices()
    {
        auto bluetooth = std::make_unique<FakeBluetoothManager>(true);
        auto pipeWire = std::make_unique<FakePipeWireManager>(true);
        auto devices = std::make_unique<FakeDeviceManager>(true);
        auto sessions = std::make_unique<FakeSessionManager>(true);
        auto* bluetoothPtr = bluetooth.get();
        auto* pipeWirePtr = pipeWire.get();

        ApplicationServices services;
        services.configuration = std::make_unique<FakeConfigurationManager>(false);
        services.bluetooth = std::move(bluetooth);
        services.pipeWire = std::move(pipeWire);
        services.devices = std::move(devices);
        services.sessions = std::move(sessions);

        ApplicationCore core(std::move(services));
        QVERIFY(!core.initialize());
        QVERIFY(!core.isReady());
        QCOMPARE(core.coreStatus(), QStringLiteral("Error"));
        QVERIFY(!bluetoothPtr->initializeCalled());
        QVERIFY(!pipeWirePtr->initializeCalled());
    }

    void bluetoothFailureSetsErrorAndDoesNotStartLaterServices()
    {
        auto bluetooth = std::make_unique<FakeBluetoothManager>(false);
        auto pipeWire = std::make_unique<FakePipeWireManager>(true);
        auto devices = std::make_unique<FakeDeviceManager>(true);
        auto sessions = std::make_unique<FakeSessionManager>(true);
        auto* bluetoothPtr = bluetooth.get();
        auto* pipeWirePtr = pipeWire.get();
        auto* devicesPtr = devices.get();
        auto* sessionsPtr = sessions.get();

        ApplicationServices services;
        services.configuration = makeIsolatedConfig();
        services.bluetooth = std::move(bluetooth);
        services.pipeWire = std::move(pipeWire);
        services.devices = std::move(devices);
        services.sessions = std::move(sessions);

        ApplicationCore core(std::move(services));
        QVERIFY(!core.initialize());
        QCOMPARE(core.coreStatus(), QStringLiteral("Error"));
        QVERIFY(bluetoothPtr->initializeCalled());
        QVERIFY(bluetoothPtr->shutdownCalled());
        QVERIFY(!pipeWirePtr->initializeCalled());
        QVERIFY(!devicesPtr->initializeCalled());
        QVERIFY(!sessionsPtr->initializeCalled());
    }

    void pipeWireFailureDoesNotAbortApplication()
    {
        auto bluetooth = std::make_unique<FakeBluetoothManager>(true);
        auto pipeWire = std::make_unique<FakePipeWireManager>(false);
        auto* bluetoothPtr = bluetooth.get();
        auto* pipeWirePtr = pipeWire.get();

        ApplicationServices services;
        services.configuration = makeIsolatedConfig();
        services.bluetooth = std::move(bluetooth);
        services.pipeWire = std::move(pipeWire);
        services.devices = std::make_unique<FakeDeviceManager>(true);
        services.sessions = std::make_unique<FakeSessionManager>(true);

        ApplicationCore core(std::move(services));
        QVERIFY(core.initialize());
        QCOMPARE(core.coreStatus(), QStringLiteral("Ready"));
        QVERIFY(bluetoothPtr->initializeCalled());
        QVERIFY(!bluetoothPtr->shutdownCalled());
        QVERIFY(pipeWirePtr->initializeCalled());
        QVERIFY(!pipeWirePtr->shutdownCalled());
        QCOMPARE(core.bluetoothStatus(), QStringLiteral("Ready"));
        QCOMPARE(core.pipeWireStatus(), QStringLiteral("Error"));
    }

    void deviceFailureSetsError()
    {
        ApplicationCore core(makeServices(true, true, false, true));
        QVERIFY(!core.initialize());
        QCOMPARE(core.coreStatus(), QStringLiteral("Error"));
        QVERIFY(!core.isReady());
    }

    void sessionFailureSetsError()
    {
        ApplicationCore core(makeServices(true, true, true, false));
        QVERIFY(!core.initialize());
        QCOMPARE(core.coreStatus(), QStringLiteral("Error"));
        QVERIFY(!core.isReady());
    }

    void repeatedShutdownIsSafe()
    {
        ApplicationCore core(makeServices());
        QVERIFY(core.initialize());
        core.shutdown();
        core.shutdown();
        QCOMPARE(core.coreStatus(), QStringLiteral("Uninitialized"));
        QVERIFY(!core.isReady());
    }
};

QTEST_GUILESS_MAIN(TstApplicationCore)
#include "tst_ApplicationCore.moc"
