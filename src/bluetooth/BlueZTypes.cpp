#include <auralis/bluetooth/BlueZTypes.h>

namespace auralis::bluetooth {

QString AdapterData::displayName() const
{
    if (!alias.isEmpty()) {
        return alias;
    }
    if (!name.isEmpty()) {
        return name;
    }
    if (!address.isEmpty()) {
        return address;
    }
    return QStringLiteral("Bluetooth adapter");
}

QString BluetoothDeviceData::displayName() const
{
    if (!alias.isEmpty()) {
        return alias;
    }
    if (!name.isEmpty()) {
        return name;
    }
    if (!address.isEmpty()) {
        return address;
    }
    return QStringLiteral("Unknown Bluetooth Device");
}

QString BluetoothDeviceData::transportHint() const
{
    const bool randomLike =
        addressType.compare(QStringLiteral("random"), Qt::CaseInsensitive) == 0
        || addressType.compare(QStringLiteral("anonymous"), Qt::CaseInsensitive) == 0;
    const bool classicLike = hasClassOfDevice;

    if (randomLike && classicLike) {
        return QStringLiteral("Classic / BLE");
    }
    if (randomLike) {
        return QStringLiteral("BLE");
    }
    if (classicLike) {
        return QStringLiteral("Classic");
    }
    return QStringLiteral("Unknown");
}

QString BluetoothDeviceData::addressIndexKey() const
{
    return auralis::bluetooth::addressIndexKey(adapterPath, address, addressType);
}

QString addressIndexKey(const QString& adapterPath, const QString& address, const QString& addressType)
{
    if (adapterPath.isEmpty() || address.isEmpty()) {
        return {};
    }
    return adapterPath + QLatin1Char('|') + address.toUpper() + QLatin1Char('|') + addressType.toLower();
}

} // namespace auralis::bluetooth
