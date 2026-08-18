#include <auralis/bluetooth/BlueZDbusClient.h>

#include <auralis/bluetooth/BlueZConstants.h>

#include <auralis/core/LoggingCategories.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QMetaType>

using BlueZInterfaceMap = QMap<QString, QVariantMap>;
using BlueZManagedObjectMap = QMap<QDBusObjectPath, BlueZInterfaceMap>;
Q_DECLARE_METATYPE(BlueZInterfaceMap)
Q_DECLARE_METATYPE(BlueZManagedObjectMap)

namespace auralis::bluetooth {
namespace {

using InterfacePropertyMap = QMap<QString, QVariantMap>;
using ManagedObjectMap = QMap<QDBusObjectPath, InterfacePropertyMap>;

QVariant unwrapVariant(const QVariant& value);

QByteArray variantToByteArray(const QVariant& value)
{
    const QVariant unwrapped = unwrapVariant(value);
    if (unwrapped.typeId() == QMetaType::QByteArray) {
        return unwrapped.toByteArray();
    }
    if (unwrapped.userType() == qMetaTypeId<QDBusArgument>()) {
        const auto argument = unwrapped.value<QDBusArgument>();
        if (argument.currentSignature() == QLatin1String("ay")) {
            QByteArray bytes;
            argument >> bytes;
            return bytes;
        }
    }
    return unwrapped.toByteArray();
}

QVariant unwrapDbusArgument(const QDBusArgument& argument)
{
    const QString signature = argument.currentSignature();
    switch (argument.currentType()) {
    case QDBusArgument::ArrayType: {
        if (signature == QLatin1String("ay")) {
            QByteArray bytes;
            argument >> bytes;
            return bytes;
        }
        if (signature == QLatin1String("as")) {
            QStringList values;
            argument >> values;
            return values;
        }
        if (signature == QLatin1String("ao")) {
            QStringList paths;
            argument.beginArray();
            while (!argument.atEnd()) {
                QDBusObjectPath path;
                argument >> path;
                paths.push_back(path.path());
            }
            argument.endArray();
            return paths;
        }
        qCDebug(auralisBluetooth) << "Skipping D-Bus array" << signature;
        return {};
    }
    case QDBusArgument::MapType: {
        if (signature == QLatin1String("a{qv}")) {
            QHash<quint16, QByteArray> manufacturer;
            argument.beginMap();
            while (!argument.atEnd()) {
                argument.beginMapEntry();
                quint16 key = 0;
                QDBusVariant dbusValue;
                argument >> key >> dbusValue;
                argument.endMapEntry();
                manufacturer.insert(key, variantToByteArray(dbusValue.variant()));
            }
            argument.endMap();
            return QVariant::fromValue(manufacturer);
        }
        if (signature == QLatin1String("a{sv}")) {
            QVariantMap map;
            argument.beginMap();
            while (!argument.atEnd()) {
                argument.beginMapEntry();
                QString key;
                QDBusVariant dbusValue;
                argument >> key >> dbusValue;
                argument.endMapEntry();
                map.insert(key, unwrapVariant(dbusValue.variant()));
            }
            argument.endMap();
            return map;
        }
        if (signature == QLatin1String("a{yv}")) {
            QVariantMap map;
            argument.beginMap();
            while (!argument.atEnd()) {
                argument.beginMapEntry();
                uchar key = 0;
                QDBusVariant dbusValue;
                argument >> key >> dbusValue;
                argument.endMapEntry();
                map.insert(QString::number(key), unwrapVariant(dbusValue.variant()));
            }
            argument.endMap();
            return map;
        }
        qCDebug(auralisBluetooth) << "Skipping D-Bus map" << signature;
        return {};
    }
    case QDBusArgument::BasicType: {
        if (signature == QLatin1String("o")) {
            QDBusObjectPath path;
            argument >> path;
            return path.path();
        }
        if (signature == QLatin1String("s")) {
            QString text;
            argument >> text;
            return text;
        }
        if (signature == QLatin1String("b")) {
            bool flag = false;
            argument >> flag;
            return flag;
        }
        if (signature == QLatin1String("n")) {
            short value = 0;
            argument >> value;
            return static_cast<int>(value);
        }
        if (signature == QLatin1String("q")) {
            ushort value = 0;
            argument >> value;
            return static_cast<uint>(value);
        }
        if (signature == QLatin1String("i")) {
            int value = 0;
            argument >> value;
            return value;
        }
        if (signature == QLatin1String("u")) {
            uint value = 0;
            argument >> value;
            return value;
        }
        qCDebug(auralisBluetooth) << "Skipping D-Bus basic value" << signature;
        return {};
    }
    case QDBusArgument::VariantType: {
        QDBusVariant variant;
        argument >> variant;
        return unwrapVariant(variant.variant());
    }
    default:
        qCDebug(auralisBluetooth) << "Skipping D-Bus value" << signature << argument.currentType();
        return {};
    }
}

QVariant unwrapVariant(const QVariant& value)
{
    if (value.userType() == qMetaTypeId<QDBusVariant>()) {
        return unwrapVariant(value.value<QDBusVariant>().variant());
    }
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return unwrapDbusArgument(value.value<QDBusArgument>());
    }
    if (value.userType() == qMetaTypeId<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value;
}

QVariantMap normalizeProperties(const QVariantMap& properties)
{
    QVariantMap normalized;
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        normalized.insert(it.key(), unwrapVariant(it.value()));
    }
    return normalized;
}

QVariantMap interfaceMapToVariant(const InterfacePropertyMap& interfaces)
{
    QVariantMap result;
    const QString adapterInterface = bluez::kAdapterInterface.toString();
    const QString deviceInterface = bluez::kDeviceInterface.toString();
    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if (it.key() != adapterInterface && it.key() != deviceInterface) {
            continue;
        }
        result.insert(it.key(), normalizeProperties(it.value()));
    }
    return result;
}

QVariantMap objectsFromManagedMap(const ManagedObjectMap& managed)
{
    QVariantMap objects;
    for (auto it = managed.constBegin(); it != managed.constEnd(); ++it) {
        objects.insert(it.key().path(), interfaceMapToVariant(it.value()));
    }
    return objects;
}

QVariantMap decodeInterfaceMap(const QVariant& value)
{
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
        return interfaceMapToVariant(qdbus_cast<InterfacePropertyMap>(argument));
    }
    if (value.canConvert<InterfacePropertyMap>()) {
        return interfaceMapToVariant(qvariant_cast<InterfacePropertyMap>(value));
    }
    if (value.typeId() == QMetaType::QVariantMap) {
        QVariantMap normalized;
        const QVariantMap raw = value.toMap();
        for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
            if (it.value().typeId() == QMetaType::QVariantMap) {
                normalized.insert(it.key(), normalizeProperties(it.value().toMap()));
            } else {
                normalized.insert(it.key(), unwrapVariant(it.value()));
            }
        }
        return normalized;
    }
    return {};
}

QVariantMap decodeManagedObjects(const QDBusMessage& reply)
{
    if (reply.arguments().isEmpty()) {
        return {};
    }

    const QVariant first = reply.arguments().constFirst();
    if (first.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(first);
        return objectsFromManagedMap(qdbus_cast<ManagedObjectMap>(argument));
    }
    if (first.canConvert<ManagedObjectMap>()) {
        return objectsFromManagedMap(qvariant_cast<ManagedObjectMap>(first));
    }
    if (first.typeId() == QMetaType::QVariantMap) {
        QVariantMap objects;
        const QVariantMap raw = first.toMap();
        for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
            objects.insert(it.key(), decodeInterfaceMap(it.value()));
        }
        return objects;
    }
    return {};
}

} // namespace

BlueZDbusClient::BlueZDbusClient(QObject* parent)
    : IBlueZClient(parent)
    , connection_(QDBusConnection::systemBus())
{
    qDBusRegisterMetaType<InterfacePropertyMap>();
    qDBusRegisterMetaType<ManagedObjectMap>();
}

BlueZDbusClient::~BlueZDbusClient()
{
    shutdown();
}

void BlueZDbusClient::setBlueZAvailable(bool available)
{
    if (blueZAvailable_ == available) {
        return;
    }
    blueZAvailable_ = available;
    emit blueZAvailableChanged(available);
}

void BlueZDbusClient::subscribeToSignals()
{
    if (signalsSubscribed_ || !connection_.isConnected()) {
        return;
    }

    const bool added = connection_.connect(
        bluez::kService.toString(),
        bluez::kRootPath.toString(),
        bluez::kObjectManagerInterface.toString(),
        bluez::kSignalInterfacesAdded.toString(),
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,QDBusMessage)));
    const bool removed = connection_.connect(
        bluez::kService.toString(),
        bluez::kRootPath.toString(),
        bluez::kObjectManagerInterface.toString(),
        bluez::kSignalInterfacesRemoved.toString(),
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    const bool properties = connection_.connect(
        bluez::kService.toString(),
        QString(),
        bluez::kPropertiesInterface.toString(),
        bluez::kSignalPropertiesChanged.toString(),
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    if (!added) {
        qCWarning(auralisBluetooth) << "Failed to subscribe to InterfacesAdded";
    }
    if (!removed) {
        qCWarning(auralisBluetooth) << "Failed to subscribe to InterfacesRemoved";
    }
    if (!properties) {
        qCWarning(auralisBluetooth) << "Failed to subscribe to PropertiesChanged";
    }
    signalsSubscribed_ = true;
}

void BlueZDbusClient::unsubscribeFromSignals()
{
    if (!signalsSubscribed_ || !connection_.isConnected()) {
        signalsSubscribed_ = false;
        return;
    }

    connection_.disconnect(
        bluez::kService.toString(),
        bluez::kRootPath.toString(),
        bluez::kObjectManagerInterface.toString(),
        bluez::kSignalInterfacesAdded.toString(),
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,QDBusMessage)));
    connection_.disconnect(
        bluez::kService.toString(),
        bluez::kRootPath.toString(),
        bluez::kObjectManagerInterface.toString(),
        bluez::kSignalInterfacesRemoved.toString(),
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    connection_.disconnect(
        bluez::kService.toString(),
        QString(),
        bluez::kPropertiesInterface.toString(),
        bluez::kSignalPropertiesChanged.toString(),
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    signalsSubscribed_ = false;
}

bool BlueZDbusClient::initialize()
{
    if (initialized_) {
        return true;
    }

    connection_ = QDBusConnection::systemBus();
    systemBusConnected_ = connection_.isConnected();
    emit systemBusStateChanged(systemBusConnected_);
    if (!systemBusConnected_) {
        qCWarning(auralisBluetooth) << "SystemBusUnavailable";
        initialized_ = true;
        setBlueZAvailable(false);
        return true;
    }

    qCInfo(auralisBluetooth) << "SystemBusConnected";
    serviceWatcher_ = new QDBusServiceWatcher(
        bluez::kService.toString(),
        connection_,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(serviceWatcher_, &QDBusServiceWatcher::serviceRegistered, this, &BlueZDbusClient::onBlueZRegistered);
    connect(serviceWatcher_, &QDBusServiceWatcher::serviceUnregistered, this, &BlueZDbusClient::onBlueZUnregistered);

    subscribeToSignals();

    const bool registered = connection_.interface() != nullptr
        && connection_.interface()->isServiceRegistered(bluez::kService.toString());
    if (registered) {
        onBlueZRegistered(bluez::kService.toString());
    } else {
        qCWarning(auralisBluetooth) << "BlueZUnavailable";
        setBlueZAvailable(false);
    }

    initialized_ = true;
    return true;
}

void BlueZDbusClient::shutdown()
{
    if (!initialized_) {
        return;
    }
    unsubscribeFromSignals();
    if (serviceWatcher_ != nullptr) {
        serviceWatcher_->deleteLater();
        serviceWatcher_ = nullptr;
    }
    setBlueZAvailable(false);
    initialized_ = false;
}

bool BlueZDbusClient::isSystemBusConnected() const noexcept
{
    return systemBusConnected_;
}

bool BlueZDbusClient::isBlueZAvailable() const noexcept
{
    return blueZAvailable_;
}

void BlueZDbusClient::onBlueZRegistered(const QString&)
{
    qCInfo(auralisBluetooth) << "BlueZAvailable";
    setBlueZAvailable(true);
    requestSnapshot();
}

void BlueZDbusClient::onBlueZUnregistered(const QString&)
{
    qCWarning(auralisBluetooth) << "BlueZUnavailable";
    setBlueZAvailable(false);
}

void BlueZDbusClient::requestSnapshot()
{
    if (!connection_.isConnected() || !blueZAvailable_) {
        return;
    }

    qCInfo(auralisBluetooth) << "BlueZSnapshotRequested";
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        bluez::kRootPath.toString(),
        bluez::kObjectManagerInterface.toString(),
        bluez::kMethodGetManagedObjects.toString());
    auto* watcher = new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &BlueZDbusClient::onGetManagedObjectsFinished);
}

void BlueZDbusClient::onGetManagedObjectsFinished(QDBusPendingCallWatcher* watcher)
{
    watcher->deleteLater();
    const QDBusPendingReply<ManagedObjectMap> typed = *watcher;
    if (typed.isError()) {
        qCWarning(auralisBluetooth) << "BlueZSnapshotFailed" << typed.error().name() << typed.error().message();
        emit snapshotFailed(typed.error().name(), typed.error().message());
        return;
    }

    QVariantMap objects = objectsFromManagedMap(typed.value());
    if (objects.isEmpty()) {
        objects = decodeManagedObjects(watcher->reply());
    }
    qCInfo(auralisBluetooth) << "BlueZSnapshotReceived" << objects.size() << "objects";
    emit snapshotReceived(objects);
}

void BlueZDbusClient::startDiscovery(const QString& adapterPath)
{
    if (!connection_.isConnected() || adapterPath.isEmpty()) {
        emit startDiscoveryFinished(
            adapterPath,
            false,
            QStringLiteral("org.freedesktop.DBus.Error.Disconnected"),
            QStringLiteral("No D-Bus connection"));
        return;
    }

    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        adapterPath,
        bluez::kAdapterInterface.toString(),
        bluez::kMethodStartDiscovery.toString());
    auto* watcher = new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, adapterPath](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (reply.isError()) {
            emit startDiscoveryFinished(adapterPath, false, reply.error().name(), reply.error().message());
            return;
        }
        emit startDiscoveryFinished(adapterPath, true, {}, {});
    });
}

void BlueZDbusClient::stopDiscovery(const QString& adapterPath)
{
    if (!connection_.isConnected() || adapterPath.isEmpty()) {
        emit stopDiscoveryFinished(
            adapterPath,
            false,
            QStringLiteral("org.freedesktop.DBus.Error.Disconnected"),
            QStringLiteral("No D-Bus connection"));
        return;
    }

    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        adapterPath,
        bluez::kAdapterInterface.toString(),
        bluez::kMethodStopDiscovery.toString());
    auto* watcher = new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, adapterPath](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (reply.isError()) {
            emit stopDiscoveryFinished(adapterPath, false, reply.error().name(), reply.error().message());
            return;
        }
        emit stopDiscoveryFinished(adapterPath, true, {}, {});
    });
}

void BlueZDbusClient::onInterfacesAdded(const QDBusObjectPath& objectPath, const QDBusMessage& message)
{
    if (message.arguments().size() < 2) {
        qCWarning(auralisBluetooth) << "MalformedDbusPayload" << "signal=InterfacesAdded"
                                    << "path=" << objectPath.path();
        return;
    }
    QVariantMap interfaces = decodeInterfaceMap(message.arguments().at(1));
    if (interfaces.isEmpty() && !message.arguments().at(1).isValid()) {
        qCWarning(auralisBluetooth) << "MalformedDbusPayload" << "signal=InterfacesAdded"
                                    << "path=" << objectPath.path();
        return;
    }
    emit interfacesAdded(objectPath.path(), interfaces);
}

void BlueZDbusClient::onInterfacesRemoved(const QDBusObjectPath& objectPath, const QStringList& interfaces)
{
    emit interfacesRemoved(objectPath.path(), interfaces);
}

void BlueZDbusClient::onPropertiesChanged(
    const QString& interfaceName,
    const QVariantMap& changed,
    const QStringList& invalidated,
    const QDBusMessage& message)
{
    if (interfaceName != bluez::kAdapterInterface.toString()
        && interfaceName != bluez::kDeviceInterface.toString()) {
        return;
    }
    emit propertiesChanged(message.path(), interfaceName, normalizeProperties(changed), invalidated);
}

} // namespace auralis::bluetooth
