#include <auralis/bluetooth/BluetoothButtonControlManager.h>
#include <auralis/bluetooth/DeviceButtonPolicy.h>

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#if defined(__linux__)
#include <linux/input.h>
#endif

using auralis::bluetooth::BluetoothButtonControlManager;
using auralis::bluetooth::BluetoothInputEndpoint;
using auralis::bluetooth::DeviceButtonEffectiveState;
using auralis::bluetooth::DeviceButtonPolicy;

class TstBluetoothButtonControlManager : public QObject {
    Q_OBJECT

private slots:
    void policyDefaultsToAllowAndPersistsDisallow()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        {
            auto settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
            BluetoothButtonControlManager manager(std::move(settings));
            QVERIFY(manager.initialize());
            QCOMPARE(manager.policyForAddress(QStringLiteral("AA:BB:CC:DD:EE:01")), DeviceButtonPolicy::Allow);
            manager.setPolicyForAddress(QStringLiteral("aa-bb-cc-dd-ee-01"), DeviceButtonPolicy::Disallow);
            QCOMPARE(manager.policyForAddress(QStringLiteral("AA:BB:CC:DD:EE:01")), DeviceButtonPolicy::Disallow);
            manager.shutdown();
        }
        {
            auto settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
            BluetoothButtonControlManager manager(std::move(settings));
            QVERIFY(manager.initialize());
            QCOMPARE(manager.policyForAddress(QStringLiteral("AA:BB:CC:DD:EE:01")), DeviceButtonPolicy::Disallow);
        }
    }

    void matchingEndpointAllowsSuppression()
    {
        auto settings = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("AuralisTest"), QStringLiteral("Buttons"));
        BluetoothButtonControlManager manager(std::move(settings));
        QStringList grabbed;
        QStringList released;
        manager.setInputProbeForTesting([]() {
            BluetoothInputEndpoint endpoint;
            endpoint.address = QStringLiteral("AA:BB:CC:DD:EE:02");
            endpoint.eventNode = QStringLiteral("/dev/input/event99");
            endpoint.name = QStringLiteral("BT Consumer Control");
            return QVector<BluetoothInputEndpoint>{endpoint};
        });
        manager.setGrabHooksForTesting(
            [&](const QString& node) {
                grabbed.append(node);
                return true;
            },
            [&](const QString& node) { released.append(node); });
        QVERIFY(manager.initialize());
        QVERIFY(manager.canControlButtonsForAddress(QStringLiteral("AA:BB:CC:DD:EE:02")));
        manager.setPolicyForAddress(QStringLiteral("AA:BB:CC:DD:EE:02"), DeviceButtonPolicy::Disallow);
        manager.syncDevice(QStringLiteral("AA:BB:CC:DD:EE:02"), true);
        QCOMPARE(manager.effectiveStateForAddress(QStringLiteral("AA:BB:CC:DD:EE:02")), DeviceButtonEffectiveState::Suppressed);
        QCOMPARE(grabbed.size(), 1);
        manager.syncDevice(QStringLiteral("AA:BB:CC:DD:EE:02"), false);
        QCOMPARE(released.size(), 1);
        manager.shutdown();
    }

    void disallowWithoutEndpointIsUnsupported()
    {
        auto settings = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("AuralisTest"), QStringLiteral("Buttons2"));
        BluetoothButtonControlManager manager(std::move(settings));
        manager.setInputProbeForTesting([]() { return QVector<BluetoothInputEndpoint>{}; });
        QVERIFY(manager.initialize());
        manager.setPolicyForAddress(QStringLiteral("11:22:33:44:55:66"), DeviceButtonPolicy::Disallow);
        manager.syncDevice(QStringLiteral("11:22:33:44:55:66"), true);
        QCOMPARE(
            manager.effectiveStateForAddress(QStringLiteral("11:22:33:44:55:66")),
            DeviceButtonEffectiveState::Unsupported);
        QVERIFY(manager.effectiveStatusTextForAddress(QStringLiteral("11:22:33:44:55:66"))
                    .contains(QStringLiteral("Unsupported")));
        manager.shutdown();
    }

    void repeatedCapabilityReadsUseCachedInputProbe()
    {
        auto settings = std::make_unique<QSettings>(
            QSettings::IniFormat,
            QSettings::UserScope,
            QStringLiteral("AuralisTest"),
            QStringLiteral("ButtonsCache"));
        BluetoothButtonControlManager manager(std::move(settings));
        int probes = 0;
        manager.setInputProbeForTesting([&]() {
            ++probes;
            return QVector<BluetoothInputEndpoint>{
                {QStringLiteral("AA:BB:CC:DD:EE:0C"), QStringLiteral("/dev/input/event12"), QStringLiteral("C")},
            };
        });
        QVERIFY(manager.initialize());
        QVERIFY(manager.canControlButtonsForAddress(QStringLiteral("AA:BB:CC:DD:EE:0C")));
        QVERIFY(manager.canControlButtonsForAddress(QStringLiteral("AA:BB:CC:DD:EE:0C")));
        QCOMPARE(probes, 1);

        manager.syncDevice(QStringLiteral("AA:BB:CC:DD:EE:0C"), true);
        QVERIFY(manager.canControlButtonsForAddress(QStringLiteral("AA:BB:CC:DD:EE:0C")));
        QCOMPARE(probes, 2);
    }

    void twoDevicesHaveIndependentPolicies()
    {
        auto settings = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("AuralisTest"), QStringLiteral("Buttons3"));
        BluetoothButtonControlManager manager(std::move(settings));
        QHash<QString, int> grabs;
        manager.setInputProbeForTesting([]() {
            return QVector<BluetoothInputEndpoint>{
                {QStringLiteral("AA:BB:CC:DD:EE:0A"), QStringLiteral("/dev/input/event10"), QStringLiteral("A")},
                {QStringLiteral("AA:BB:CC:DD:EE:0B"), QStringLiteral("/dev/input/event11"), QStringLiteral("B")},
            };
        });
        manager.setGrabHooksForTesting(
            [&](const QString& node) {
                grabs[node] += 1;
                return true;
            },
            [&](const QString& node) { grabs[node] -= 1; });
        QVERIFY(manager.initialize());
        manager.setPolicyForAddress(QStringLiteral("AA:BB:CC:DD:EE:0A"), DeviceButtonPolicy::Disallow);
        manager.setPolicyForAddress(QStringLiteral("AA:BB:CC:DD:EE:0B"), DeviceButtonPolicy::Allow);
        manager.syncDevice(QStringLiteral("AA:BB:CC:DD:EE:0A"), true);
        manager.syncDevice(QStringLiteral("AA:BB:CC:DD:EE:0B"), true);
        QCOMPARE(manager.effectiveStateForAddress(QStringLiteral("AA:BB:CC:DD:EE:0A")), DeviceButtonEffectiveState::Suppressed);
        QCOMPARE(manager.effectiveStateForAddress(QStringLiteral("AA:BB:CC:DD:EE:0B")), DeviceButtonEffectiveState::Allowed);
        QCOMPARE(grabs.value(QStringLiteral("/dev/input/event10")), 1);
        QCOMPARE(grabs.value(QStringLiteral("/dev/input/event11")), 0);
        manager.clearPolicyForAddress(QStringLiteral("AA:BB:CC:DD:EE:0A"));
        QCOMPARE(manager.policyForAddress(QStringLiteral("AA:BB:CC:DD:EE:0A")), DeviceButtonPolicy::Allow);
        manager.shutdown();
    }

    void mediaKeyCodesIncludePlayPause()
    {
#if defined(__linux__)
        QVERIFY(BluetoothButtonControlManager::isMediaKeyCode(KEY_PLAYPAUSE));
        QVERIFY(BluetoothButtonControlManager::isMediaKeyCode(KEY_NEXTSONG));
#else
        QSKIP("Linux media key codes only");
#endif
    }

    void permissionDeniedWhenGrabFails()
    {
        auto settings = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("AuralisTest"), QStringLiteral("Buttons4"));
        BluetoothButtonControlManager manager(std::move(settings));
        manager.setInputProbeForTesting([]() {
            return QVector<BluetoothInputEndpoint>{
                {QStringLiteral("DE:AD:BE:EF:00:01"), QStringLiteral("/dev/input/event42"), QStringLiteral("X")},
            };
        });
        manager.setGrabHooksForTesting([](const QString&) { return false; }, [](const QString&) {});
        QVERIFY(manager.initialize());
        manager.setPolicyForAddress(QStringLiteral("DE:AD:BE:EF:00:01"), DeviceButtonPolicy::Disallow);
        manager.syncDevice(QStringLiteral("DE:AD:BE:EF:00:01"), true);
        QCOMPARE(
            manager.effectiveStateForAddress(QStringLiteral("DE:AD:BE:EF:00:01")),
            DeviceButtonEffectiveState::PermissionDenied);
        manager.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstBluetoothButtonControlManager)
#include "tst_BluetoothButtonControlManager.moc"
