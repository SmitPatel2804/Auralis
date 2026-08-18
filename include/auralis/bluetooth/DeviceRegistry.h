#pragma once

#include <auralis/bluetooth/BlueZTypes.h>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace auralis::bluetooth {

class DeviceRegistry final : public QObject {
    Q_OBJECT

public:
    explicit DeviceRegistry(QObject* parent = nullptr);

    // Returns true if a new row was inserted.
    bool upsertDevice(BluetoothDeviceData device);
    bool applyPropertyChanges(
        const QString& objectPath,
        const QVariantMap& changed,
        const QStringList& invalidated);
    void removeDevice(const QString& objectPath);
    void reconcile(const QSet<QString>& liveObjectPaths);
    void clear();

    int count() const noexcept;
    int indexOf(const QString& objectPath) const;
    const BluetoothDeviceData* findByObjectPath(const QString& objectPath) const;
    const BluetoothDeviceData* findByAddress(
        const QString& adapterPath,
        const QString& address,
        const QString& addressType) const;
    BluetoothDeviceData at(int index) const;
    QVector<BluetoothDeviceData> devices() const;

signals:
    void deviceAboutToBeAdded(int index);
    void deviceAdded(int index);
    void deviceUpdated(int index, const QList<int>& roles);
    void deviceAboutToBeRemoved(int index, const QString& objectPath);
    void deviceRemoved(int index, const QString& objectPath);
    void countChanged();
    void parseWarning(const QString& objectPath, const QString& property, const QString& message);

private:
    void reindex();
    void touchLastSeen(BluetoothDeviceData& device) const;

    QStringList order_;
    QHash<QString, BluetoothDeviceData> byPath_;
    QHash<QString, QString> byAddressKey_;
};

} // namespace auralis::bluetooth
