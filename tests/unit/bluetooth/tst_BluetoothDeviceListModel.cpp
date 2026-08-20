#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BluetoothDeviceListModel;
using auralis::bluetooth::DeviceRegistry;
using auralis::bluetooth::parseDevice;

class TstBluetoothDeviceListModel : public QObject {
    Q_OBJECT

private:
    static auralis::bluetooth::BluetoothDeviceData device(
        const QString& path,
        const QString& address,
        const QString& name = {})
    {
        QVariantMap properties{{QStringLiteral("Address"), address}};
        if (!name.isEmpty()) {
            properties.insert(QStringLiteral("Name"), name);
        }
        return parseDevice(path, properties).device;
    }

private slots:
    void exposesRolesAndData()
    {
        DeviceRegistry registry;
        BluetoothDeviceListModel model(&registry);
        registry.upsertDevice(device(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.roleNames().contains(BluetoothDeviceListModel::DisplayNameRole));
        QCOMPARE(
            model.data(model.index(0, 0), BluetoothDeviceListModel::DisplayNameRole).toString(),
            QStringLiteral("One"));
        QCOMPARE(
            model.data(model.index(0, 0), BluetoothDeviceListModel::AddressRole).toString(),
            QStringLiteral("AA:BB:CC:DD:EE:01"));
        QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceListModel::HasRssiRole).toBool(), false);
        QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceListModel::RssiRole).toInt(), 0);
    }

    void connectedRoleUpdatesWhenDeviceConnects()
    {
        DeviceRegistry registry;
        BluetoothDeviceListModel model(&registry);
        auto data = device(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("Buds"));
        data.connected = false;
        registry.upsertDevice(data);
        QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceListModel::ConnectedRole).toBool(), false);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        registry.applyPropertyChanges(QStringLiteral("/a"), {{QStringLiteral("Connected"), true}}, {});
        QCoreApplication::processEvents();
        QVERIFY(changed.count() >= 1);
        QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceListModel::ConnectedRole).toBool(), true);
    }

    void displayNameFallback()
    {
        DeviceRegistry registry;
        BluetoothDeviceListModel model(&registry);
        registry.upsertDevice(device(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:99")));
        QCOMPARE(
            model.data(model.index(0, 0), BluetoothDeviceListModel::DisplayNameRole).toString(),
            QStringLiteral("AA:BB:CC:DD:EE:99"));
    }

    void insertUpdateRemoveNotifications()
    {
        DeviceRegistry registry;
        BluetoothDeviceListModel model(&registry);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

        registry.upsertDevice(device(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        QCOMPARE(inserted.count(), 1);
        registry.applyPropertyChanges(QStringLiteral("/a"), {{QStringLiteral("RSSI"), -40}}, {});
        QCoreApplication::processEvents();
        QVERIFY(changed.count() >= 1);
        registry.removeDevice(QStringLiteral("/a"));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(model.rowCount(), 0);
    }

    void insertRemoveKeepRowCountConsistentWithViewProtocol()
    {
        DeviceRegistry registry;
        BluetoothDeviceListModel model(&registry);
        int aboutToInsertCount = -1;
        int insertedCount = -1;
        int aboutToRemoveCount = -1;
        int removedCount = -1;

        QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model, [&]() {
            aboutToInsertCount = model.rowCount();
        });
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model, [&]() {
            insertedCount = model.rowCount();
        });
        QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model, [&]() {
            aboutToRemoveCount = model.rowCount();
        });
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &model, [&]() {
            removedCount = model.rowCount();
        });

        registry.upsertDevice(device(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        QCOMPARE(aboutToInsertCount, 0);
        QCOMPARE(insertedCount, 1);

        registry.removeDevice(QStringLiteral("/a"));
        QCOMPARE(aboutToRemoveCount, 1);
        QCOMPARE(removedCount, 0);
    }
};

QTEST_GUILESS_MAIN(TstBluetoothDeviceListModel)
#include "tst_BluetoothDeviceListModel.moc"
