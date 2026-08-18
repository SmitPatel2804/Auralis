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

std::optional<QString> normalizeBluetoothAddress(const QString& raw)
{
    QString cleaned = raw.trimmed().toUpper();
    if (cleaned.isEmpty()) {
        return std::nullopt;
    }
    cleaned.replace(QLatin1Char('-'), QLatin1Char(':'));
    cleaned.replace(QLatin1Char('_'), QLatin1Char(':'));
    if (!cleaned.contains(QLatin1Char(':'))) {
        if (cleaned.size() != 12) {
            return std::nullopt;
        }
        QString grouped;
        grouped.reserve(17);
        for (int i = 0; i < 12; i += 2) {
            if (i > 0) {
                grouped.append(QLatin1Char(':'));
            }
            grouped.append(cleaned.mid(i, 2));
        }
        cleaned = grouped;
    }
    const QStringList parts = cleaned.split(QLatin1Char(':'));
    if (parts.size() != 6) {
        return std::nullopt;
    }
    for (const QString& part : parts) {
        if (part.size() != 2) {
            return std::nullopt;
        }
        bool ok = false;
        part.toUInt(&ok, 16);
        if (!ok) {
            return std::nullopt;
        }
    }
    return cleaned;
}

} // namespace auralis::bluetooth
