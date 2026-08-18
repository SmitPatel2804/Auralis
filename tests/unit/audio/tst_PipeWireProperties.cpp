#include <auralis/audio/PipeWireProperties.h>
#include <auralis/bluetooth/BlueZTypes.h>

#include <QtTest>

using auralis::audio::PipeWireProperties;
using auralis::bluetooth::normalizeBluetoothAddress;

class TstPipeWireProperties : public QObject {
    Q_OBJECT

private slots:
    void missingKey()
    {
        const PipeWireProperties props = PipeWireProperties::fromHash({});
        QVERIFY(!props.contains(QStringLiteral("media.class")));
        QCOMPARE(props.value(QStringLiteral("media.class"), QStringLiteral("fallback")), QStringLiteral("fallback"));
        QVERIFY(!props.optionalValue(QStringLiteral("media.class")).has_value());
        QVERIFY(!props.uintValue(QStringLiteral("object.serial")).has_value());
    }

    void emptyValueIsMissing()
    {
        const PipeWireProperties props = PipeWireProperties::fromHash({{QStringLiteral("node.name"), QString()}});
        QVERIFY(!props.optionalValue(QStringLiteral("node.name")).has_value());
        QCOMPARE(props.value(QStringLiteral("node.name"), QStringLiteral("x")), QStringLiteral("x"));
    }

    void validIntegerParsing()
    {
        const PipeWireProperties props = PipeWireProperties::fromHash({{QStringLiteral("audio.rate"), QStringLiteral("48000")}});
        QCOMPARE(props.uintValue(QStringLiteral("audio.rate")).value_or(0), static_cast<quint64>(48000));
    }

    void invalidIntegerParsing()
    {
        const PipeWireProperties props = PipeWireProperties::fromHash({{QStringLiteral("audio.rate"), QStringLiteral("not-a-number")}});
        QVERIFY(!props.uintValue(QStringLiteral("audio.rate")).has_value());
    }

    void snapshotIndependentOfSourceHash()
    {
        QHash<QString, QString> source{{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}};
        const PipeWireProperties props = PipeWireProperties::fromHash(source);
        source.insert(QStringLiteral("media.class"), QStringLiteral("mutated"));
        QCOMPARE(props.value(QStringLiteral("media.class")), QStringLiteral("Audio/Sink"));
    }

    void bluetoothAddressNormalization()
    {
        QCOMPARE(normalizeBluetoothAddress(QStringLiteral("aa:bb:cc:dd:ee:ff")).value_or(QString()), QStringLiteral("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(normalizeBluetoothAddress(QStringLiteral("AA_BB_CC_DD_EE_FF")).value_or(QString()), QStringLiteral("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(normalizeBluetoothAddress(QStringLiteral("AA-BB-CC-DD-EE-FF")).value_or(QString()), QStringLiteral("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(normalizeBluetoothAddress(QStringLiteral("AABBCCDDEEFF")).value_or(QString()), QStringLiteral("AA:BB:CC:DD:EE:FF"));
    }

    void invalidBluetoothAddressRejected()
    {
        QVERIFY(!normalizeBluetoothAddress(QStringLiteral("not-an-address")).has_value());
        QVERIFY(!normalizeBluetoothAddress(QStringLiteral("AA:BB")).has_value());
        QVERIFY(!normalizeBluetoothAddress(QStringLiteral("")).has_value());
        QVERIFY(!normalizeBluetoothAddress(QStringLiteral("GG:BB:CC:DD:EE:FF")).has_value());
    }

    void exactBlueZPathComparison()
    {
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
        QCOMPARE(path, QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"));
        QVERIFY(path != QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66"));
    }
};

QTEST_GUILESS_MAIN(TstPipeWireProperties)
#include "tst_PipeWireProperties.moc"
