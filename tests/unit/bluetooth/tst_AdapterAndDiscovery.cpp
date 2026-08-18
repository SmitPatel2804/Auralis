#include "FakeBlueZClient.h"

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/DiscoveryManager.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::AdapterData;
using auralis::bluetooth::AdapterManager;
using auralis::bluetooth::DiscoveryManager;
using auralis::bluetooth::DiscoveryState;
using auralis::bluetooth::parseAdapter;
using auralis::test::FakeBlueZClient;

class TstAdapterAndDiscovery : public QObject {
    Q_OBJECT

private:
    static AdapterData poweredHci0()
    {
        return parseAdapter(
                   QStringLiteral("/org/bluez/hci0"),
                   {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
                    {QStringLiteral("Alias"), QStringLiteral("smit")},
                    {QStringLiteral("Powered"), true}})
            .adapter;
    }

private slots:
    void prefersHci0()
    {
        AdapterManager adapters;
        adapters.upsertAdapter(parseAdapter(
                                   QStringLiteral("/org/bluez/hci1"),
                                   {{QStringLiteral("Address"), QStringLiteral("00:11:22:33:44:55")},
                                    {QStringLiteral("Powered"), true}})
                                   .adapter);
        adapters.upsertAdapter(poweredHci0());
        QCOMPARE(adapters.selectedObjectPath(), QStringLiteral("/org/bluez/hci0"));
    }

    void fallsBackToFirstAdapter()
    {
        AdapterManager adapters;
        adapters.upsertAdapter(parseAdapter(
                                   QStringLiteral("/org/bluez/hci1"),
                                   {{QStringLiteral("Address"), QStringLiteral("00:11:22:33:44:55")},
                                    {QStringLiteral("Powered"), true}})
                                   .adapter);
        QCOMPARE(adapters.selectedObjectPath(), QStringLiteral("/org/bluez/hci1"));
    }

    void startStopDiscovery()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);

        QSignalSpy stateSpy(&discovery, &DiscoveryManager::stateChanged);
        discovery.startScan();
        QCOMPARE(client.startRequests(), 1);
        QVERIFY(discovery.state() == DiscoveryState::Starting);
        discovery.handleStartFinished(QStringLiteral("/org/bluez/hci0"), true, {}, {});
        QVERIFY(discovery.state() == DiscoveryState::Discovering);
        QVERIFY(discovery.ownsDiscovery());

        discovery.stopScan();
        QVERIFY(discovery.state() == DiscoveryState::Stopping);
        discovery.handleStopFinished(QStringLiteral("/org/bluez/hci0"), true, {}, {});
        QVERIFY(discovery.state() == DiscoveryState::Idle);
        QVERIFY(!discovery.ownsDiscovery());
        QVERIFY(stateSpy.count() >= 4);
    }

    void startFailureReturnsToIdle()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.startScan();
        discovery.handleStartFinished(
            QStringLiteral("/org/bluez/hci0"),
            false,
            QStringLiteral("org.bluez.Error.NotReady"),
            QStringLiteral("Resource Not Ready"));
        QVERIFY(discovery.state() == DiscoveryState::Idle);
        QVERIFY(!discovery.ownsDiscovery());
        QVERIFY(discovery.canStartScan());
    }

    void duplicateStartIgnored()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.startScan();
        discovery.handleStartFinished(QStringLiteral("/org/bluez/hci0"), true, {}, {});
        discovery.startScan();
        QCOMPARE(client.startRequests(), 1);
    }

    void duplicateStopSafe()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.stopScan();
        QCOMPARE(client.stopRequests(), 0);
    }

    void poweredOffDisablesScan()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        AdapterData adapter = poweredHci0();
        adapter.powered = false;
        adapters.upsertAdapter(adapter);
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.onSelectedAdapterChanged();
        QVERIFY(!discovery.canStartScan());
        QCOMPARE(discovery.statusText(), QStringLiteral("Bluetooth is powered off"));
    }

    void adapterRemoved()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.startScan();
        discovery.handleStartFinished(QStringLiteral("/org/bluez/hci0"), true, {}, {});
        adapters.removeAdapter(QStringLiteral("/org/bluez/hci0"));
        discovery.onSelectedAdapterChanged();
        QVERIFY(discovery.state() == DiscoveryState::Unavailable);
        QVERIFY(!discovery.ownsDiscovery());
    }

    void blueZDisappears()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.startScan();
        discovery.handleStartFinished(QStringLiteral("/org/bluez/hci0"), true, {}, {});
        client.setBlueZAvailable(false);
        discovery.onBlueZAvailabilityChanged(false);
        QVERIFY(discovery.state() == DiscoveryState::Unavailable);
        QVERIFY(!discovery.ownsDiscovery());
    }

    void inProgressDoesNotClaimOwnership()
    {
        FakeBlueZClient client;
        AdapterManager adapters;
        adapters.upsertAdapter(poweredHci0());
        DiscoveryManager discovery(&client, &adapters);
        discovery.onBlueZAvailabilityChanged(true);
        discovery.startScan();
        discovery.handleStartFinished(
            QStringLiteral("/org/bluez/hci0"),
            false,
            QStringLiteral("org.bluez.Error.InProgress"),
            QStringLiteral("Operation already in progress"));
        QVERIFY(discovery.state() == DiscoveryState::Idle);
        QVERIFY(!discovery.ownsDiscovery());
        QVERIFY(discovery.canStartScan());
        QVERIFY(!discovery.lastErrorMessage().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstAdapterAndDiscovery)
#include "tst_AdapterAndDiscovery.moc"
