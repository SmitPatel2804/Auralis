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
#include <QTimer>

using BlueZInterfaceMap = QMap<QString, QVariantMap>;
using BlueZManagedObjectMap = QMap<QDBusObjectPath, BlueZInterfaceMap>;
Q_DECLARE_METATYPE(BlueZInterfaceMap)
Q_DECLARE_METATYPE(BlueZManagedObjectMap)

namespace auralis::bluetooth {
namespace {

constexpr int kPairMethodTimeoutMs = 120000;
constexpr int kConnectMethodTimeoutMs = 60000;
constexpr int kStandardMethodTimeoutMs = 30000;

QDBusPendingCallWatcher* watchCall(
    const QDBusConnection& connection,
    const QDBusMessage& message,
    int timeoutMs,
    QObject* parent)
{
    return new QDBusPendingCallWatcher(connection.asyncCall(message, timeoutMs), parent);
}

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
    busHealthTimer_.setInterval(2000);
    connect(&busHealthTimer_, &QTimer::timeout, this, &BlueZDbusClient::pollSystemBusHealth);
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

void BlueZDbusClient::setSystemBusConnected(bool connected)
{
    if (systemBusConnected_ == connected) {
        return;
    }
    systemBusConnected_ = connected;
    emit systemBusStateChanged(connected);
}

void BlueZDbusClient::startBusHealthTimer()
{
    if (initialized_ && !busHealthTimer_.isActive()) {
        busHealthTimer_.start();
    }
}

void BlueZDbusClient::stopBusHealthTimer()
{
    busHealthTimer_.stop();
}

bool BlueZDbusClient::probeSystemBusConnected() const
{
    if (systemBusConnectedOverride_.has_value()) {
        return *systemBusConnectedOverride_;
    }
    return QDBusConnection::systemBus().isConnected();
}

bool BlueZDbusClient::isolateFromHostBus() const noexcept
{
    // Override mode must never create live watchers/subscriptions against host BlueZ.
    return systemBusConnectedOverride_.has_value();
}

bool BlueZDbusClient::acceptGeneration(quint64 generation) const noexcept
{
    return initialized_ && generation == busAttachGeneration_;
}

bool BlueZDbusClient::isolateFromHostBusForTesting() const noexcept
{
    return isolateFromHostBus();
}

void BlueZDbusClient::detachSystemBusInfrastructure()
{
    snapshotInFlight_ = false;
    pendingSnapshotRefresh_ = false;
    unsubscribeFromSignals();
    if (serviceWatcher_ != nullptr) {
        serviceWatcher_->deleteLater();
        serviceWatcher_ = nullptr;
    }
    setBlueZAvailable(false);
}

bool BlueZDbusClient::attachSystemBusInfrastructure()
{
    if (systemBusConnectedOverride_.has_value() && !*systemBusConnectedOverride_) {
        return false;
    }

    ++busAttachGeneration_;
    detachSystemBusInfrastructure();

    if (isolateFromHostBus()) {
        // Isolated FSM advance only — no host QDBusServiceWatcher / BlueZ subscriptions.
        // Still advance the signal epoch so generation fencing can be tested without host I/O.
        signalSubscriptionGeneration_ = busAttachGeneration_;
        return *systemBusConnectedOverride_;
    }

    connection_ = QDBusConnection::systemBus();
    if (!connection_.isConnected()) {
        return false;
    }

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
    return true;
}

void BlueZDbusClient::handleSystemBusLost()
{
    qCWarning(auralisBluetooth) << "SystemBusLost";
    detachSystemBusInfrastructure();
    setSystemBusConnected(false);
}

void BlueZDbusClient::attemptSystemBusReattach()
{
    qCInfo(auralisBluetooth) << "SystemBusReconnected";
    if (attachSystemBusInfrastructure()) {
        setSystemBusConnected(true);
    }
}

void BlueZDbusClient::pollSystemBusHealth()
{
    if (!initialized_) {
        return;
    }
    const bool connected = probeSystemBusConnected();
    if (connected && !systemBusConnected_) {
        attemptSystemBusReattach();
        return;
    }
    if (!connected && systemBusConnected_) {
        handleSystemBusLost();
    }
}

void BlueZDbusClient::injectSystemBusConnectedForTesting(bool connected)
{
    setSystemBusConnectedOverrideForTesting(connected);
    pollSystemBusHealth();
}

void BlueZDbusClient::setSystemBusConnectedOverrideForTesting(std::optional<bool> connected)
{
    systemBusConnectedOverride_ = connected;
}

void BlueZDbusClient::pollSystemBusHealthForTesting()
{
    pollSystemBusHealth();
}

void BlueZDbusClient::injectSnapshotFinishedForTesting(quint64 generation, const QVariantMap& objects, bool error)
{
    finishSnapshot(
        generation,
        objects,
        error,
        error ? QStringLiteral("org.auralis.Test.SnapshotError") : QString(),
        error ? QStringLiteral("injected") : QString());
}

void BlueZDbusClient::injectPairDeviceFinishedForTesting(
    quint64 generation,
    const QString& devicePath,
    bool ok,
    const QString& errorName,
    const QString& errorMessage)
{
    if (!acceptGeneration(generation)) {
        return;
    }
    emit pairDeviceFinished(devicePath, ok, errorName, errorMessage);
}

void BlueZDbusClient::injectInterfacesAddedForTesting(
    quint64 generation,
    const QString& objectPath,
    const QVariantMap& interfaces)
{
    if (!acceptGeneration(generation) || generation != signalSubscriptionGeneration_) {
        return;
    }
    emit interfacesAdded(objectPath, interfaces);
}

void BlueZDbusClient::setBlueZAvailableForTesting(bool available)
{
    setBlueZAvailable(available);
}

int BlueZDbusClient::busAttachGenerationForTesting() const noexcept
{
    return static_cast<int>(busAttachGeneration_);
}

bool BlueZDbusClient::busHealthTimerActiveForTesting() const noexcept
{
    return busHealthTimer_.isActive();
}

bool BlueZDbusClient::snapshotInFlightForTesting() const noexcept
{
    return snapshotInFlight_;
}

void BlueZDbusClient::subscribeToSignals()
{
    if (signalsSubscribed_ || isolateFromHostBus() || !connection_.isConnected()) {
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
    signalSubscriptionGeneration_ = busAttachGeneration_;
}

void BlueZDbusClient::unsubscribeFromSignals()
{
    if (!signalsSubscribed_) {
        signalSubscriptionGeneration_ = 0;
        return;
    }
    if (!connection_.isConnected()) {
        signalsSubscribed_ = false;
        signalSubscriptionGeneration_ = 0;
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
    signalSubscriptionGeneration_ = 0;
}

bool BlueZDbusClient::initialize()
{
    if (initialized_) {
        return true;
    }

    initialized_ = true;
    connection_ = QDBusConnection::systemBus();
    // Always-on health monitor while initialized (detect UP→DOWN at runtime).
    startBusHealthTimer();

    if (!probeSystemBusConnected()) {
        qCWarning(auralisBluetooth) << "SystemBusUnavailable";
        setSystemBusConnected(false);
        setBlueZAvailable(false);
        return true;
    }

    qCInfo(auralisBluetooth) << "SystemBusConnected";
    if (!attachSystemBusInfrastructure()) {
        setSystemBusConnected(false);
        setBlueZAvailable(false);
        return true;
    }
    setSystemBusConnected(true);
    return true;
}

void BlueZDbusClient::shutdown()
{
    if (!initialized_) {
        return;
    }
    stopBusHealthTimer();
    ++busAttachGeneration_;
    detachSystemBusInfrastructure();
    setSystemBusConnected(false);
    systemBusConnectedOverride_.reset();
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
    if (!initialized_ || !blueZAvailable_) {
        return;
    }
    if (systemBusConnectedOverride_.has_value()) {
        if (!*systemBusConnectedOverride_) {
            return;
        }
    } else if (!connection_.isConnected()) {
        return;
    }

    if (snapshotInFlight_) {
        pendingSnapshotRefresh_ = true;
        return;
    }
    issueSnapshotCall();
}

void BlueZDbusClient::issueSnapshotCall()
{
    snapshotInFlight_ = true;
    snapshotInFlightGeneration_ = busAttachGeneration_;
    pendingSnapshotRefresh_ = false;

    // Isolated / override environments exercise coalesce/generation without live GetManagedObjects.
    if (isolateFromHostBus()) {
        qCInfo(auralisBluetooth) << "BlueZSnapshotRequested generation=" << snapshotInFlightGeneration_;
        return;
    }

    qCInfo(auralisBluetooth) << "BlueZSnapshotRequested generation=" << snapshotInFlightGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        bluez::kRootPath.toString(),
        bluez::kObjectManagerInterface.toString(),
        bluez::kMethodGetManagedObjects.toString());
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    watcher->setProperty("busAttachGeneration", QVariant::fromValue(snapshotInFlightGeneration_));
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &BlueZDbusClient::onGetManagedObjectsFinished);
}

void BlueZDbusClient::finishSnapshot(
    quint64 generation,
    const QVariantMap& objects,
    bool error,
    const QString& name,
    const QString& message)
{
    if (!acceptGeneration(generation)) {
        return;
    }

    if (snapshotInFlight_ && generation == snapshotInFlightGeneration_) {
        snapshotInFlight_ = false;
    }
    if (error) {
        qCWarning(auralisBluetooth) << "BlueZSnapshotFailed" << name << message;
        emit snapshotFailed(name, message);
    } else {
        qCInfo(auralisBluetooth) << "BlueZSnapshotReceived" << objects.size() << "objects";
        emit snapshotReceived(objects);
    }

    if (pendingSnapshotRefresh_ && acceptGeneration(generation) && blueZAvailable_) {
        pendingSnapshotRefresh_ = false;
        issueSnapshotCall();
    }
}

void BlueZDbusClient::onGetManagedObjectsFinished(QDBusPendingCallWatcher* watcher)
{
    watcher->deleteLater();
    const quint64 generation = watcher->property("busAttachGeneration").toULongLong();
    if (!acceptGeneration(generation)) {
        // Do not clear a newer in-flight snapshot's bookkeeping.
        if (snapshotInFlight_ && generation == snapshotInFlightGeneration_) {
            snapshotInFlight_ = false;
        }
        return;
    }

    const QDBusPendingReply<ManagedObjectMap> typed = *watcher;
    if (typed.isError()) {
        finishSnapshot(generation, {}, true, typed.error().name(), typed.error().message());
        return;
    }

    QVariantMap objects = objectsFromManagedMap(typed.value());
    if (objects.isEmpty()) {
        objects = decodeManagedObjects(watcher->reply());
    }
    finishSnapshot(generation, objects, false, {}, {});
}

void BlueZDbusClient::startDiscovery(const QString& adapterPath)
{
    if (!connection_.isConnected() || adapterPath.isEmpty() || isolateFromHostBus()) {
        emit startDiscoveryFinished(
            adapterPath,
            false,
            QStringLiteral("org.freedesktop.DBus.Error.Disconnected"),
            QStringLiteral("No D-Bus connection"));
        return;
    }

    const quint64 generation = busAttachGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        adapterPath,
        bluez::kAdapterInterface.toString(),
        bluez::kMethodStartDiscovery.toString());
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, adapterPath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        if (reply.isError()) {
            emit startDiscoveryFinished(adapterPath, false, reply.error().name(), reply.error().message());
            return;
        }
        emit startDiscoveryFinished(adapterPath, true, {}, {});
    });
}

void BlueZDbusClient::stopDiscovery(const QString& adapterPath)
{
    if (!connection_.isConnected() || adapterPath.isEmpty() || isolateFromHostBus()) {
        emit stopDiscoveryFinished(
            adapterPath,
            false,
            QStringLiteral("org.freedesktop.DBus.Error.Disconnected"),
            QStringLiteral("No D-Bus connection"));
        return;
    }

    const quint64 generation = busAttachGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        adapterPath,
        bluez::kAdapterInterface.toString(),
        bluez::kMethodStopDiscovery.toString());
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, adapterPath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        if (reply.isError()) {
            emit stopDiscoveryFinished(adapterPath, false, reply.error().name(), reply.error().message());
            return;
        }
        emit stopDiscoveryFinished(adapterPath, true, {}, {});
    });
}

void BlueZDbusClient::onInterfacesAdded(const QDBusObjectPath& objectPath, const QDBusMessage& message)
{
    if (!acceptGeneration(signalSubscriptionGeneration_) || !signalsSubscribed_) {
        return;
    }
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
    if (!acceptGeneration(signalSubscriptionGeneration_) || !signalsSubscribed_) {
        return;
    }
    emit interfacesRemoved(objectPath.path(), interfaces);
}

void BlueZDbusClient::onPropertiesChanged(
    const QString& interfaceName,
    const QVariantMap& changed,
    const QStringList& invalidated,
    const QDBusMessage& message)
{
    if (!acceptGeneration(signalSubscriptionGeneration_) || !signalsSubscribed_) {
        return;
    }
    if (interfaceName != bluez::kAdapterInterface.toString()
        && interfaceName != bluez::kDeviceInterface.toString()) {
        return;
    }
    emit propertiesChanged(message.path(), interfaceName, normalizeProperties(changed), invalidated);
}

void BlueZDbusClient::pairDevice(const QString& devicePath)
{
    if (!connection_.isConnected() || devicePath.isEmpty() || isolateFromHostBus()) {
        emit pairDeviceFinished(devicePath, false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(), devicePath, bluez::kDeviceInterface.toString(), bluez::kMethodPair.toString());
    auto* watcher = watchCall(connection_, message, kPairMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, devicePath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit pairDeviceFinished(devicePath, !reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::cancelPairing(const QString& devicePath)
{
    if (!connection_.isConnected() || devicePath.isEmpty() || isolateFromHostBus()) {
        emit cancelPairingFinished(devicePath, false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(), devicePath, bluez::kDeviceInterface.toString(), bluez::kMethodCancelPairing.toString());
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, devicePath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit cancelPairingFinished(devicePath, !reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::connectDevice(const QString& devicePath)
{
    if (!connection_.isConnected() || devicePath.isEmpty() || isolateFromHostBus()) {
        emit connectDeviceFinished(devicePath, false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(), devicePath, bluez::kDeviceInterface.toString(), bluez::kMethodConnect.toString());
    auto* watcher = watchCall(connection_, message, kConnectMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, devicePath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit connectDeviceFinished(devicePath, !reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::disconnectDevice(const QString& devicePath)
{
    if (!connection_.isConnected() || devicePath.isEmpty() || isolateFromHostBus()) {
        emit disconnectDeviceFinished(devicePath, false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(), devicePath, bluez::kDeviceInterface.toString(), bluez::kMethodDisconnect.toString());
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, devicePath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit disconnectDeviceFinished(devicePath, !reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::setDeviceTrusted(const QString& devicePath, bool trusted)
{
    if (!connection_.isConnected() || devicePath.isEmpty() || isolateFromHostBus()) {
        emit setDeviceTrustedFinished(devicePath, trusted, false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        devicePath,
        bluez::kPropertiesInterface.toString(),
        bluez::kMethodSet.toString());
    message << bluez::kDeviceInterface.toString() << bluez::kPropTrusted.toString() << QVariant::fromValue(QDBusVariant(trusted));
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, devicePath, trusted, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit setDeviceTrustedFinished(devicePath, trusted, !reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::removeDevice(const QString& adapterPath, const QString& devicePath)
{
    if (!connection_.isConnected() || adapterPath.isEmpty() || devicePath.isEmpty() || isolateFromHostBus()) {
        emit removeDeviceFinished(adapterPath, devicePath, false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        adapterPath,
        bluez::kAdapterInterface.toString(),
        bluez::kMethodRemoveDevice.toString());
    message << QVariant::fromValue(QDBusObjectPath(devicePath));
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, adapterPath, devicePath, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit removeDeviceFinished(adapterPath, devicePath, !reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::registerAgent(const QString& agentPath, const QString& capability)
{
    if (!connection_.isConnected() || isolateFromHostBus()) {
        emit registerAgentFinished(false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        bluez::kAgentManagerPath.toString(),
        bluez::kAgentManagerInterface.toString(),
        bluez::kMethodRegisterAgent.toString());
    message << QVariant::fromValue(QDBusObjectPath(agentPath)) << capability;
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        agentRegistered_ = !reply.isError();
        emit registerAgentFinished(!reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::requestDefaultAgent(const QString& agentPath)
{
    if (!connection_.isConnected() || isolateFromHostBus()) {
        emit requestDefaultAgentFinished(false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        bluez::kAgentManagerPath.toString(),
        bluez::kAgentManagerInterface.toString(),
        bluez::kMethodRequestDefaultAgent.toString());
    message << QVariant::fromValue(QDBusObjectPath(agentPath));
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        emit requestDefaultAgentFinished(
            !reply.isError(),
            reply.isError() ? reply.error().name() : QString(),
            reply.isError() ? reply.error().message() : QString());
    });
}

void BlueZDbusClient::unregisterAgent(const QString& agentPath)
{
    if (!connection_.isConnected() || isolateFromHostBus()) {
        agentRegistered_ = false;
        emit unregisterAgentFinished(false, QStringLiteral("org.freedesktop.DBus.Error.Disconnected"), {});
        return;
    }
    const quint64 generation = busAttachGeneration_;
    QDBusMessage message = QDBusMessage::createMethodCall(
        bluez::kService.toString(),
        bluez::kAgentManagerPath.toString(),
        bluez::kAgentManagerInterface.toString(),
        bluez::kMethodUnregisterAgent.toString());
    message << QVariant::fromValue(QDBusObjectPath(agentPath));
    auto* watcher = watchCall(connection_, message, kStandardMethodTimeoutMs, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<> reply = *call;
        call->deleteLater();
        if (!acceptGeneration(generation)) {
            return;
        }
        agentRegistered_ = false;
        emit unregisterAgentFinished(!reply.isError(), reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

bool BlueZDbusClient::isAgentRegistered() const noexcept
{
    return agentRegistered_;
}

} // namespace auralis::bluetooth
