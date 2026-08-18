#include <auralis/bluetooth/BlueZPropertyParser.h>

#include <QtTest>

#include <QHash>

using auralis::bluetooth::applyDevicePropertyChanges;
using auralis::bluetooth::parseAdapter;
using auralis::bluetooth::parseDevice;

class TstBlueZPropertyParser : public QObject {
    Q_OBJECT

private:
    static QVariantMap classicHeadphones()
    {
        return {
            {QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:03")},
            {QStringLiteral("AddressType"), QStringLiteral("public")},
            {QStringLiteral("Name"), QStringLiteral("Headphones")},
            {QStringLiteral("Class"), 0x240404u},
            {QStringLiteral("RSSI"), -67},
            {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")}},
            {QStringLiteral("Paired"), false},
            {QStringLiteral("Connected"), false},
        };
    }

    static QVariantMap bleHearingAid()
    {
        QHash<quint16, QByteArray> manufacturer;
        manufacturer.insert(0x004c, QByteArray::fromHex("0215"));
        QHash<QString, QByteArray> service;
        service.insert(QStringLiteral("0000fe59-0000-1000-8000-00805f9b34fb"), QByteArray::fromHex("01"));
        return {
            {QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:01")},
            {QStringLiteral("AddressType"), QStringLiteral("random")},
            {QStringLiteral("Alias"), QStringLiteral("Hearing Aid L")},
            {QStringLiteral("RSSI"), -51},
            {QStringLiteral("Appearance"), 1346},
            {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000180f-0000-1000-8000-00805f9b34fb")}},
            {QStringLiteral("ManufacturerData"), QVariant::fromValue(manufacturer)},
            {QStringLiteral("ServiceData"), QVariant::fromValue(service)},
        };
    }

private slots:
    void parsesClassicDevice()
    {
        const auto result = parseDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_03"), classicHeadphones());
        QCOMPARE(result.device.address, QStringLiteral("AA:BB:CC:DD:EE:03"));
        QCOMPARE(result.device.displayName(), QStringLiteral("Headphones"));
        QCOMPARE(result.device.transportHint(), QStringLiteral("Classic"));
        QVERIFY(result.device.hasRssi);
        QCOMPARE(result.device.rssi, static_cast<qint16>(-67));
        QVERIFY(result.device.hasClassOfDevice);
        QCOMPARE(result.device.uuids.size(), 1);
    }

    void parsesBleDevice()
    {
        const auto result = parseDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"), bleHearingAid());
        QCOMPARE(result.device.alias, QStringLiteral("Hearing Aid L"));
        QCOMPARE(result.device.displayName(), QStringLiteral("Hearing Aid L"));
        QCOMPARE(result.device.addressType, QStringLiteral("random"));
        QCOMPARE(result.device.transportHint(), QStringLiteral("BLE"));
        QVERIFY(result.device.hasAppearance);
        QCOMPARE(result.device.manufacturerData.size(), 1);
        QCOMPARE(result.device.serviceData.size(), 1);
    }

    void missingNameFallsBackToAddress()
    {
        QVariantMap properties{{QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:99")},
                               {QStringLiteral("AddressType"), QStringLiteral("random")}};
        const auto result = parseDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_99"), properties);
        QCOMPARE(result.device.displayName(), QStringLiteral("AA:BB:CC:DD:EE:99"));
        QVERIFY(!result.device.hasRssi);
    }

    void missingAliasUsesName()
    {
        QVariantMap properties{{QStringLiteral("Name"), QStringLiteral("Just a name")}};
        const auto result = parseDevice(QStringLiteral("/dev"), properties);
        QCOMPARE(result.device.displayName(), QStringLiteral("Just a name"));
    }

    void malformedRssiIsIgnored()
    {
        QVariantMap properties = classicHeadphones();
        properties.insert(QStringLiteral("RSSI"), QStringLiteral("not-a-number"));
        const auto result = parseDevice(QStringLiteral("/dev"), properties);
        QVERIFY(!result.device.hasRssi);
        QVERIFY(!result.warnings.isEmpty());
    }

    void invalidatedRssiClearsPresence()
    {
        auto parsed = parseDevice(QStringLiteral("/dev"), classicHeadphones());
        QVERIFY(parsed.device.hasRssi);
        const auto updated = applyDevicePropertyChanges(parsed.device, {}, {QStringLiteral("RSSI")});
        QVERIFY(!updated.device.hasRssi);
        QCOMPARE(updated.device.address, QStringLiteral("AA:BB:CC:DD:EE:03"));
    }

    void parsesAdapter()
    {
        const auto result = parseAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Alias"), QStringLiteral("smit")},
             {QStringLiteral("Powered"), true},
             {QStringLiteral("Discovering"), false}});
        QCOMPARE(result.adapter.address, QStringLiteral("E8:9E:B4:13:4C:CC"));
        QVERIFY(result.adapter.powered);
        QCOMPARE(result.adapter.displayName(), QStringLiteral("smit"));
    }
};

QTEST_GUILESS_MAIN(TstBlueZPropertyParser)
#include "tst_BlueZPropertyParser.moc"
