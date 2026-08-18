#pragma once

#include <auralis/bluetooth/BluetoothError.h>
#include <auralis/bluetooth/DeviceOperation.h>

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace auralis::bluetooth {

struct AdapterData {
    QString objectPath;
    QString address;
    QString name;
    QString alias;
    bool powered = false;
    bool discoverable = false;
    bool pairable = false;
    bool discovering = false;
    bool available = false;
    QString modalias;

    QString displayName() const;
};

struct BluetoothDeviceData {
    QString objectPath;
    QString adapterPath;
    QString address;
    QString addressType;
    QString name;
    QString alias;
    qint16 rssi = 0;
    bool hasRssi = false;
    bool paired = false;
    bool connected = false;
    bool trusted = false;
    bool blocked = false;
    bool servicesResolved = false;
    quint32 classOfDevice = 0;
    bool hasClassOfDevice = false;
    QString icon;
    quint16 appearance = 0;
    bool hasAppearance = false;
    QStringList uuids;
    QHash<quint16, QByteArray> manufacturerData;
    QHash<QString, QByteArray> serviceData;
    QDateTime lastSeen;
    qint16 txPower = 0;
    bool hasTxPower = false;
    bool legacyPairing = false;
    QString modalias;
    bool bonded = false;

    DeviceOperation operation = DeviceOperation::Idle;
    BluetoothError lastError = BluetoothError::None;
    QString lastErrorName;
    QString lastErrorMessage;
    QDateTime lastErrorTimestamp;
    int reconnectAttempt = 0;
    bool userDisconnectRequested = false;
    bool autoReconnectEnabled = true;

    QString displayName() const;
    QString transportHint() const;
    QString addressIndexKey() const;
};

QString addressIndexKey(const QString& adapterPath, const QString& address, const QString& addressType);
std::optional<QString> normalizeBluetoothAddress(const QString& raw);

} // namespace auralis::bluetooth
