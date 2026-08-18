#pragma once

#include <auralis/bluetooth/BlueZTypes.h>

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>

namespace auralis::bluetooth {

struct ParseWarning {
    QString property;
    QString message;
};

struct DeviceParseResult {
    BluetoothDeviceData device;
    QList<ParseWarning> warnings;
};

struct AdapterParseResult {
    AdapterData adapter;
    QList<ParseWarning> warnings;
};

DeviceParseResult parseDevice(const QString& objectPath, const QVariantMap& properties);
AdapterParseResult parseAdapter(const QString& objectPath, const QVariantMap& properties);

// Apply BlueZ PropertiesChanged (changed + invalidated) onto an existing device record.
DeviceParseResult applyDevicePropertyChanges(
    BluetoothDeviceData device,
    const QVariantMap& changed,
    const QStringList& invalidated);

AdapterParseResult applyAdapterPropertyChanges(
    AdapterData adapter,
    const QVariantMap& changed,
    const QStringList& invalidated);

} // namespace auralis::bluetooth
