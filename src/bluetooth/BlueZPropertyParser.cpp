#include <auralis/bluetooth/BlueZPropertyParser.h>

#include <auralis/bluetooth/BlueZConstants.h>

#include <auralis/core/LoggingCategories.h>

#include <QVariant>

#include <optional>

namespace auralis::bluetooth {
namespace {

using bluez::kPropAdapter;
using bluez::kPropAddress;
using bluez::kPropAddressType;
using bluez::kPropAlias;
using bluez::kPropAppearance;
using bluez::kPropBlocked;
using bluez::kPropBonded;
using bluez::kPropClass;
using bluez::kPropConnected;
using bluez::kPropDiscoverable;
using bluez::kPropDiscovering;
using bluez::kPropIcon;
using bluez::kPropLegacyPairing;
using bluez::kPropManufacturerData;
using bluez::kPropModalias;
using bluez::kPropName;
using bluez::kPropPaired;
using bluez::kPropPairable;
using bluez::kPropPowered;
using bluez::kPropRssi;
using bluez::kPropServiceData;
using bluez::kPropServicesResolved;
using bluez::kPropTrusted;
using bluez::kPropTxPower;
using bluez::kPropUuids;

void addWarning(QList<ParseWarning>& warnings, QStringView property, const QString& message)
{
    warnings.push_back(ParseWarning{property.toString(), message});
}

std::optional<QString> asString(const QVariant& value, QList<ParseWarning>& warnings, QStringView property)
{
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    if (value.typeId() == QMetaType::QString) {
        return value.toString();
    }
    if (value.canConvert<QString>()) {
        return value.toString();
    }
    addWarning(warnings, property, QStringLiteral("expected string"));
    return std::nullopt;
}

std::optional<bool> asBool(const QVariant& value, QList<ParseWarning>& warnings, QStringView property)
{
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    if (value.typeId() == QMetaType::Bool) {
        return value.toBool();
    }
    if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::UInt) {
        return value.toInt() != 0;
    }
    addWarning(warnings, property, QStringLiteral("expected boolean"));
    return std::nullopt;
}

std::optional<qint16> asInt16(const QVariant& value, QList<ParseWarning>& warnings, QStringView property)
{
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }

    bool ok = false;
    const qlonglong number = value.toLongLong(&ok);
    if (!ok || number < -32768 || number > 32767) {
        addWarning(warnings, property, QStringLiteral("expected int16"));
        return std::nullopt;
    }
    return static_cast<qint16>(number);
}

std::optional<quint16> asUInt16(const QVariant& value, QList<ParseWarning>& warnings, QStringView property)
{
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    bool ok = false;
    const qulonglong number = value.toULongLong(&ok);
    if (!ok || number > 65535U) {
        addWarning(warnings, property, QStringLiteral("expected uint16"));
        return std::nullopt;
    }
    return static_cast<quint16>(number);
}

std::optional<quint32> asUInt32(const QVariant& value, QList<ParseWarning>& warnings, QStringView property)
{
    if (!value.isValid() || value.isNull()) {
        return std::nullopt;
    }
    bool ok = false;
    const qulonglong number = value.toULongLong(&ok);
    if (!ok || number > 0xFFFFFFFFULL) {
        addWarning(warnings, property, QStringLiteral("expected uint32"));
        return std::nullopt;
    }
    return static_cast<quint32>(number);
}

QStringList asStringList(const QVariant& value, QList<ParseWarning>& warnings, QStringView property)
{
    if (!value.isValid() || value.isNull()) {
        return {};
    }
    if (value.typeId() == QMetaType::QStringList) {
        return value.toStringList();
    }
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }
    addWarning(warnings, property, QStringLiteral("expected string list"));
    return {};
}

QByteArray asByteArray(const QVariant& value)
{
    if (value.typeId() == QMetaType::QByteArray) {
        return value.toByteArray();
    }
    if (value.canConvert<QByteArray>()) {
        return value.toByteArray();
    }
    return {};
}

QHash<quint16, QByteArray> asManufacturerData(
    const QVariant& value,
    QList<ParseWarning>& warnings,
    QStringView property)
{
    QHash<quint16, QByteArray> result;
    if (!value.isValid() || value.isNull()) {
        return result;
    }

    if (value.canConvert<QHash<quint16, QByteArray>>()) {
        return qvariant_cast<QHash<quint16, QByteArray>>(value);
    }

    if (value.typeId() == QMetaType::QVariantMap) {
        const QVariantMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            bool ok = false;
            const uint key = it.key().toUInt(&ok, 0);
            if (!ok || key > 65535U) {
                addWarning(warnings, property, QStringLiteral("invalid manufacturer key"));
                continue;
            }
            result.insert(static_cast<quint16>(key), asByteArray(it.value()));
        }
        return result;
    }

    addWarning(warnings, property, QStringLiteral("expected manufacturer data map"));
    return result;
}

QHash<QString, QByteArray> asServiceData(
    const QVariant& value,
    QList<ParseWarning>& warnings,
    QStringView property)
{
    QHash<QString, QByteArray> result;
    if (!value.isValid() || value.isNull()) {
        return result;
    }

    if (value.canConvert<QHash<QString, QByteArray>>()) {
        return qvariant_cast<QHash<QString, QByteArray>>(value);
    }

    if (value.typeId() == QMetaType::QVariantMap) {
        const QVariantMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            result.insert(it.key(), asByteArray(it.value()));
        }
        return result;
    }

    addWarning(warnings, property, QStringLiteral("expected service data map"));
    return result;
}

void applyString(
    QString& target,
    const QVariantMap& properties,
    QStringView key,
    QList<ParseWarning>& warnings)
{
    if (!properties.contains(key.toString())) {
        return;
    }
    if (const auto parsed = asString(properties.value(key.toString()), warnings, key)) {
        target = *parsed;
    }
}

void applyBool(
    bool& target,
    const QVariantMap& properties,
    QStringView key,
    QList<ParseWarning>& warnings)
{
    if (!properties.contains(key.toString())) {
        return;
    }
    if (const auto parsed = asBool(properties.value(key.toString()), warnings, key)) {
        target = *parsed;
    }
}

void applyDeviceProperties(BluetoothDeviceData& device, const QVariantMap& properties, QList<ParseWarning>& warnings)
{
    applyString(device.address, properties, kPropAddress, warnings);
    applyString(device.addressType, properties, kPropAddressType, warnings);
    applyString(device.name, properties, kPropName, warnings);
    applyString(device.alias, properties, kPropAlias, warnings);
    applyString(device.icon, properties, kPropIcon, warnings);
    applyString(device.adapterPath, properties, kPropAdapter, warnings);
    applyString(device.modalias, properties, kPropModalias, warnings);
    applyBool(device.paired, properties, kPropPaired, warnings);
    applyBool(device.bonded, properties, kPropBonded, warnings);
    applyBool(device.connected, properties, kPropConnected, warnings);
    applyBool(device.trusted, properties, kPropTrusted, warnings);
    applyBool(device.blocked, properties, kPropBlocked, warnings);
    applyBool(device.servicesResolved, properties, kPropServicesResolved, warnings);
    applyBool(device.legacyPairing, properties, kPropLegacyPairing, warnings);

    if (properties.contains(kPropClass.toString())) {
        if (const auto parsed = asUInt32(properties.value(kPropClass.toString()), warnings, kPropClass)) {
            device.classOfDevice = *parsed;
            device.hasClassOfDevice = true;
        }
    }
    if (properties.contains(kPropAppearance.toString())) {
        if (const auto parsed = asUInt16(properties.value(kPropAppearance.toString()), warnings, kPropAppearance)) {
            device.appearance = *parsed;
            device.hasAppearance = true;
        }
    }
    if (properties.contains(kPropRssi.toString())) {
        if (const auto parsed = asInt16(properties.value(kPropRssi.toString()), warnings, kPropRssi)) {
            device.rssi = *parsed;
            device.hasRssi = true;
        }
    }
    if (properties.contains(kPropTxPower.toString())) {
        if (const auto parsed = asInt16(properties.value(kPropTxPower.toString()), warnings, kPropTxPower)) {
            device.txPower = *parsed;
            device.hasTxPower = true;
        }
    }
    if (properties.contains(kPropUuids.toString())) {
        device.uuids = asStringList(properties.value(kPropUuids.toString()), warnings, kPropUuids);
    }
    if (properties.contains(kPropManufacturerData.toString())) {
        device.manufacturerData =
            asManufacturerData(properties.value(kPropManufacturerData.toString()), warnings, kPropManufacturerData);
    }
    if (properties.contains(kPropServiceData.toString())) {
        device.serviceData = asServiceData(properties.value(kPropServiceData.toString()), warnings, kPropServiceData);
    }
}

void invalidateDeviceProperty(BluetoothDeviceData& device, const QString& property)
{
    if (property == kPropRssi.toString()) {
        device.hasRssi = false;
        device.rssi = 0;
    } else if (property == kPropTxPower.toString()) {
        device.hasTxPower = false;
        device.txPower = 0;
    } else if (property == kPropName.toString()) {
        device.name.clear();
    } else if (property == kPropAlias.toString()) {
        device.alias.clear();
    } else if (property == kPropClass.toString()) {
        device.hasClassOfDevice = false;
        device.classOfDevice = 0;
    } else if (property == kPropAppearance.toString()) {
        device.hasAppearance = false;
        device.appearance = 0;
    } else if (property == kPropManufacturerData.toString()) {
        device.manufacturerData.clear();
    } else if (property == kPropServiceData.toString()) {
        device.serviceData.clear();
    } else if (property == kPropUuids.toString()) {
        device.uuids.clear();
    }
}

void applyAdapterProperties(AdapterData& adapter, const QVariantMap& properties, QList<ParseWarning>& warnings)
{
    applyString(adapter.address, properties, kPropAddress, warnings);
    applyString(adapter.name, properties, kPropName, warnings);
    applyString(adapter.alias, properties, kPropAlias, warnings);
    applyString(adapter.modalias, properties, kPropModalias, warnings);
    applyBool(adapter.powered, properties, kPropPowered, warnings);
    applyBool(adapter.discoverable, properties, kPropDiscoverable, warnings);
    applyBool(adapter.pairable, properties, kPropPairable, warnings);
    applyBool(adapter.discovering, properties, kPropDiscovering, warnings);
}

} // namespace

DeviceParseResult parseDevice(const QString& objectPath, const QVariantMap& properties)
{
    DeviceParseResult result;
    result.device.objectPath = objectPath;
    applyDeviceProperties(result.device, properties, result.warnings);
    return result;
}

AdapterParseResult parseAdapter(const QString& objectPath, const QVariantMap& properties)
{
    AdapterParseResult result;
    result.adapter.objectPath = objectPath;
    result.adapter.available = true;
    applyAdapterProperties(result.adapter, properties, result.warnings);
    return result;
}

DeviceParseResult applyDevicePropertyChanges(
    BluetoothDeviceData device,
    const QVariantMap& changed,
    const QStringList& invalidated)
{
    DeviceParseResult result;
    result.device = std::move(device);
    for (const QString& property : invalidated) {
        invalidateDeviceProperty(result.device, property);
    }
    applyDeviceProperties(result.device, changed, result.warnings);
    return result;
}

AdapterParseResult applyAdapterPropertyChanges(
    AdapterData adapter,
    const QVariantMap& changed,
    const QStringList& invalidated)
{
    AdapterParseResult result;
    result.adapter = std::move(adapter);
    for (const QString& property : invalidated) {
        if (property == kPropPowered.toString()) {
            result.adapter.powered = false;
        } else if (property == kPropDiscovering.toString()) {
            result.adapter.discovering = false;
        } else if (property == kPropName.toString()) {
            result.adapter.name.clear();
        } else if (property == kPropAlias.toString()) {
            result.adapter.alias.clear();
        }
    }
    applyAdapterProperties(result.adapter, changed, result.warnings);
    return result;
}

void logParseWarnings(
    const QString& objectPath,
    const QString& interfaceName,
    const QList<ParseWarning>& warnings)
{
    const bool adapter = interfaceName == bluez::kAdapterInterface.toString();
    const QLatin1String token = adapter ? QLatin1String("MalformedAdapterProperty")
                                        : QLatin1String("MalformedDeviceProperty");
    for (const ParseWarning& warning : warnings) {
        qCWarning(auralisBluetooth) << token << "path=" << objectPath << "property=" << warning.property
                                    << "message=" << warning.message;
    }
}

} // namespace auralis::bluetooth
