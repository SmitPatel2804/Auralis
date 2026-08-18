#include <auralis/bluetooth/BluetoothManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/BlueZDbusClient.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/DiscoveryManager.h>
#include <auralis/bluetooth/IBlueZClient.h>

#include <auralis/core/LoggingCategories.h>

#include <QSet>

namespace auralis::bluetooth {

BluetoothManager::BluetoothManager(QObject* parent)
    : BluetoothManager(nullptr, parent)
{
}

BluetoothManager::BluetoothManager(IBlueZClient* client, QObject* parent)
    : QObject(parent)
    , client_(client)
{
    if (client_ == nullptr) {
        client_ = new BlueZDbusClient(this);
    } else if (client_->parent() == nullptr) {
        client_->setParent(this);
    }

    adapters_ = new AdapterManager(this);
    discovery_ = new DiscoveryManager(client_, adapters_, this);
    registry_ = new DeviceRegistry(this);
    model_ = new BluetoothDeviceListModel(registry_, this);
    statusText_ = defaultMessage(BluetoothError::BlueZUnavailable);
}

BluetoothManager::~BluetoothManager()
{
    shutdown();
}

QObject* BluetoothManager::uiObject()
{
    return this;
}

void BluetoothManager::connectClientSignals()
{
    if (signalsWired_ || client_ == nullptr) {
        return;
    }
    signalsWired_ = true;
    connect(client_, &IBlueZClient::blueZAvailableChanged, this, &BluetoothManager::handleBlueZAvailable);
    connect(client_, &IBlueZClient::snapshotReceived, this, &BluetoothManager::handleSnapshot);
    connect(client_, &IBlueZClient::interfacesAdded, this, &BluetoothManager::handleInterfacesAdded);
    connect(client_, &IBlueZClient::interfacesRemoved, this, &BluetoothManager::handleInterfacesRemoved);
    connect(client_, &IBlueZClient::propertiesChanged, this, &BluetoothManager::handlePropertiesChanged);
    connect(client_, &IBlueZClient::startDiscoveryFinished, discovery_, &DiscoveryManager::handleStartFinished);
    connect(client_, &IBlueZClient::stopDiscoveryFinished, discovery_, &DiscoveryManager::handleStopFinished);

    connect(adapters_, &AdapterManager::selectedAdapterChanged, this, [this]() {
        discovery_->onSelectedAdapterChanged();
        emit adapterChanged();
        emit scanningChanged();
        updateStatusText();
    });
    connect(adapters_, &AdapterManager::selectedAdapterUpdated, this, [this]() {
        discovery_->onSelectedAdapterChanged();
        emit adapterChanged();
        emit scanningChanged();
        updateStatusText();
    });
    connect(discovery_, &DiscoveryManager::stateChanged, this, [this]() {
        emit scanningChanged();
        updateStatusText();
    });
    connect(discovery_, &DiscoveryManager::errorChanged, this, [this]() {
        errorText_ = discovery_->lastError() == BluetoothError::None ? QString() : discovery_->lastErrorMessage();
        emit errorTextChanged();
        updateStatusText();
    });
    connect(registry_, &DeviceRegistry::countChanged, this, &BluetoothManager::deviceCountChanged);
}

void BluetoothManager::updateStatusText()
{
    const QString next = discovery_->statusText();
    if (statusText_ == next) {
        return;
    }
    statusText_ = next;
    emit statusTextChanged();
}

QVariantMap BluetoothManager::adapterProperties(const QVariantMap& interfaces) const
{
    return interfaces.value(bluez::kAdapterInterface.toString()).toMap();
}

QVariantMap BluetoothManager::deviceProperties(const QVariantMap& interfaces) const
{
    return interfaces.value(bluez::kDeviceInterface.toString()).toMap();
}

bool BluetoothManager::initialize()
{
    if (status_ == auralis::core::ServiceStatus::Ready) {
        return true;
    }

    status_ = auralis::core::ServiceStatus::Initializing;
    qCInfo(auralisBluetooth) << "BluetoothSubsystemInitializing";
    connectClientSignals();
    client_->initialize();
    handleBlueZAvailable(client_->isBlueZAvailable());
    status_ = auralis::core::ServiceStatus::Ready;
    qCInfo(auralisBluetooth) << "Bluetooth manager ready (BlueZ available =" << available_ << ")";
    return true;
}

void BluetoothManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    if (discovery_ != nullptr && discovery_->ownsDiscovery()) {
        discovery_->stopScan();
    }
    if (registry_ != nullptr) {
        registry_->clear();
    }
    if (adapters_ != nullptr) {
        adapters_->clear();
    }
    if (client_ != nullptr) {
        client_->shutdown();
    }
    available_ = false;
    status_ = auralis::core::ServiceStatus::Uninitialized;
    qCInfo(auralisBluetooth) << "Bluetooth manager shut down";
}

auralis::core::ServiceStatus BluetoothManager::status() const noexcept
{
    return status_;
}

bool BluetoothManager::available() const noexcept
{
    return available_;
}

bool BluetoothManager::adapterPowered() const
{
    return adapters_ != nullptr && adapters_->selected().powered;
}

bool BluetoothManager::scanning() const noexcept
{
    return discovery_ != nullptr && discovery_->ownsDiscovery();
}

bool BluetoothManager::canStartScan() const
{
    return discovery_ != nullptr && discovery_->canStartScan();
}

bool BluetoothManager::canStopScan() const
{
    return discovery_ != nullptr && discovery_->canStopScan();
}

int BluetoothManager::deviceCount() const
{
    return registry_ != nullptr ? registry_->count() : 0;
}

QString BluetoothManager::statusText() const
{
    return statusText_;
}

QString BluetoothManager::errorText() const
{
    return errorText_;
}

QString BluetoothManager::adapterName() const
{
    return adapters_ != nullptr ? adapters_->selected().displayName() : QString();
}

QString BluetoothManager::adapterAddress() const
{
    return adapters_ != nullptr ? adapters_->selected().address : QString();
}

bool BluetoothManager::adapterDiscovering() const
{
    return adapters_ != nullptr && adapters_->selected().discovering;
}

QAbstractItemModel* BluetoothManager::devices() const
{
    return model_;
}

void BluetoothManager::startScan()
{
    if (discovery_ != nullptr) {
        discovery_->startScan();
    }
}

void BluetoothManager::stopScan()
{
    if (discovery_ != nullptr) {
        discovery_->stopScan();
    }
}

void BluetoothManager::refresh()
{
    if (client_ != nullptr) {
        client_->requestSnapshot();
    }
}

void BluetoothManager::handleBlueZAvailable(bool available)
{
    if (available_ != available) {
        available_ = available;
        emit availableChanged();
    }
    if (!available && registry_ != nullptr) {
        registry_->clear();
    }
    if (!available && adapters_ != nullptr) {
        adapters_->clear();
    }
    if (discovery_ != nullptr) {
        discovery_->onBlueZAvailabilityChanged(available);
    }
    emit adapterChanged();
    emit scanningChanged();
    updateStatusText();
}

void BluetoothManager::handleSnapshot(const QVariantMap& objectsByPath)
{
    QSet<QString> adapterPaths;
    QSet<QString> devicePaths;
    for (auto it = objectsByPath.constBegin(); it != objectsByPath.constEnd(); ++it) {
        const QVariantMap interfaces = it.value().toMap();
        if (interfaces.contains(bluez::kAdapterInterface.toString())) {
            AdapterParseResult parsed = parseAdapter(it.key(), adapterProperties(interfaces));
            adapters_->upsertAdapter(parsed.adapter);
            adapterPaths.insert(it.key());
        }
        if (interfaces.contains(bluez::kDeviceInterface.toString())) {
            DeviceParseResult parsed = parseDevice(it.key(), deviceProperties(interfaces));
            registry_->upsertDevice(parsed.device);
            devicePaths.insert(it.key());
        }
    }
    adapters_->reconcile(adapterPaths);
    registry_->reconcile(devicePaths);
    discovery_->onSelectedAdapterChanged();
    qCInfo(auralisBluetooth) << "BlueZSnapshotApplied adapters=" << adapterPaths.size()
                             << "devices=" << devicePaths.size();
    emit adapterChanged();
    updateStatusText();
}

void BluetoothManager::handleInterfacesAdded(const QString& objectPath, const QVariantMap& interfaces)
{
    if (interfaces.contains(bluez::kAdapterInterface.toString())) {
        AdapterParseResult parsed = parseAdapter(objectPath, adapterProperties(interfaces));
        adapters_->upsertAdapter(parsed.adapter);
        qCInfo(auralisBluetooth) << "AdapterAdded" << objectPath;
    }
    if (interfaces.contains(bluez::kDeviceInterface.toString())) {
        DeviceParseResult parsed = parseDevice(objectPath, deviceProperties(interfaces));
        registry_->upsertDevice(parsed.device);
        qCInfo(auralisBluetooth) << "DeviceAdded" << objectPath << parsed.device.displayName();
    }
}

void BluetoothManager::handleInterfacesRemoved(const QString& objectPath, const QStringList& interfaces)
{
    if (interfaces.contains(bluez::kAdapterInterface.toString()) || interfaces.isEmpty()) {
        qCInfo(auralisBluetooth) << "AdapterRemoved" << objectPath;
        adapters_->removeAdapter(objectPath);
        if (discovery_ != nullptr) {
            discovery_->onSelectedAdapterChanged();
        }
    }
    if (interfaces.contains(bluez::kDeviceInterface.toString()) || interfaces.isEmpty()) {
        qCInfo(auralisBluetooth) << "DeviceRemoved" << objectPath;
        registry_->removeDevice(objectPath);
    }
}

void BluetoothManager::handlePropertiesChanged(
    const QString& objectPath,
    const QString& interfaceName,
    const QVariantMap& changed,
    const QStringList& invalidated)
{
    if (interfaceName == bluez::kAdapterInterface.toString()) {
        adapters_->applyPropertyChanges(objectPath, changed, invalidated);
        return;
    }
    if (interfaceName == bluez::kDeviceInterface.toString()) {
        registry_->applyPropertyChanges(objectPath, changed, invalidated);
    }
}

} // namespace auralis::bluetooth
