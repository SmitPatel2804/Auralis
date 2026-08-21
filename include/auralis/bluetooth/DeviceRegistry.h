#pragma once

#include <auralis/bluetooth/BlueZTypes.h>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <functional>

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

    bool setOperation(const QString& objectPath, DeviceOperation operation);
    bool clearOperation(const QString& objectPath);
    bool setLastError(const QString& objectPath, BluetoothError error, const QString& errorName, const QString& message);
    bool clearLastError(const QString& objectPath);
    bool setUserDisconnectRequested(const QString& objectPath, bool requested);
    bool setAutoReconnectEnabled(const QString& objectPath, bool enabled);
    bool setReconnectAttempt(const QString& objectPath, int attempt);
    /// Platform-neutral state mutation used by native backends after an OS
    /// callback. Emits the same model update notifications as BlueZ changes.
    bool updateDevice(const QString& objectPath, const std::function<void(BluetoothDeviceData&)>& mutator);

signals:
    void deviceAboutToBeAdded(int index);
    void deviceAdded(int index);
    void deviceUpdated(int index, const QList<int>& roles);
    void deviceAboutToBeRemoved(int index, const QString& objectPath);
    void deviceRemoved(int index, const QString& objectPath);
    void countChanged();
    void parseWarning(const QString& objectPath, const QString& property, const QString& message);

private:
    void touchLastSeen(BluetoothDeviceData& device) const;
    bool mutateDevice(const QString& objectPath, const std::function<void(BluetoothDeviceData&)>& mutator);

    QStringList order_;
    QHash<QString, BluetoothDeviceData> byPath_;
    QHash<QString, QString> byAddressKey_;
};

} // namespace auralis::bluetooth
