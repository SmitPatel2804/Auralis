#include <auralis/bluetooth/BluetoothManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/AgentCapability.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/BlueZDbusClient.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/DeviceLifecycleManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/DiscoveryManager.h>
#include <auralis/bluetooth/IBlueZClient.h>
#include <auralis/bluetooth/PairingRequest.h>
#include <auralis/bluetooth/ReconnectPolicy.h>
#include <auralis/bluetooth/UuidCatalog.h>

#include <auralis/core/LoggingCategories.h>

#include <QSet>

namespace auralis::bluetooth {
namespace {

constexpr auto kEnvAgentCapability = "AURALIS_BLUETOOTH_AGENT_CAPABILITY";

int errorPriority(BluetoothError error)
{
    switch (error) {
    case BluetoothError::NoSystemBus:
        return 1;
    case BluetoothError::BlueZUnavailable:
        return 2;
    case BluetoothError::NoAdapter:
        return 3;
    case BluetoothError::AdapterPoweredOff:
        return 4;
    case BluetoothError::DiscoveryStartFailed:
    case BluetoothError::DiscoveryStopFailed:
        return 5;
    case BluetoothError::DbusCallFailed:
        return 6;
    default:
        return 100;
    }
}

AgentCapability defaultAgentCapability()
{
    return parseAgentCapability(QString::fromLocal8Bit(qgetenv(kEnvAgentCapability)));
}

} // namespace

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
    reconnect_ = new ReconnectPolicy(this);
    agent_ = new BlueZAgent(client_, defaultAgentCapability(), this);
    lifecycle_ = new DeviceLifecycleManager(client_, registry_, adapters_, agent_, reconnect_, this);
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

DeviceRegistry* BluetoothManager::deviceRegistry() const noexcept
{
    return registry_;
}

void BluetoothManager::connectClientSignals()
{
    if (signalsWired_ || client_ == nullptr) {
        return;
    }
    signalsWired_ = true;
    connect(client_, &IBlueZClient::blueZAvailableChanged, this, &BluetoothManager::handleBlueZAvailable);
    connect(client_, &IBlueZClient::systemBusStateChanged, this, &BluetoothManager::handleSystemBusStateChanged);
    connect(client_, &IBlueZClient::snapshotReceived, this, &BluetoothManager::handleSnapshot);
    connect(client_, &IBlueZClient::snapshotFailed, this, &BluetoothManager::handleSnapshotFailed);
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
        refreshDisplayedError();
    });
    connect(adapters_, &AdapterManager::selectedAdapterUpdated, this, [this]() {
        discovery_->onSelectedAdapterChanged();
        emit adapterChanged();
        emit scanningChanged();
        updateStatusText();
        refreshDisplayedError();
    });
    connect(discovery_, &DiscoveryManager::stateChanged, this, [this]() {
        emit scanningChanged();
        updateStatusText();
    });
    connect(discovery_, &DiscoveryManager::intentChanged, this, [this]() {
        emit scanningChanged();
        updateStatusText();
    });
    connect(discovery_, &DiscoveryManager::errorChanged, this, [this]() {
        refreshDisplayedError();
        updateStatusText();
    });
    connect(registry_, &DeviceRegistry::countChanged, this, &BluetoothManager::deviceCountChanged);
    if (agent_ != nullptr) {
        connect(agent_, &BlueZAgent::pendingRequestChanged, this, [this]() {
            if (agent_ != nullptr && registry_ != nullptr) {
                if (PairingRequest* request = agent_->pendingRequest()) {
                    if (const BluetoothDeviceData* device = registry_->findByObjectPath(request->devicePath())) {
                        request->setDeviceName(device->displayName());
                    }
                }
            }
            emit pendingPairingRequestChanged();
        });
        connect(agent_, &BlueZAgent::registeredChanged, this, &BluetoothManager::agentRegisteredChanged);
    }
}

void BluetoothManager::refreshDisplayedError()
{
    BluetoothError error = discovery_ != nullptr ? discovery_->lastError() : BluetoothError::None;
    QString message = discovery_ != nullptr ? discovery_->lastErrorMessage() : QString();

    if (snapshotError_ != BluetoothError::None) {
        if (error == BluetoothError::None || errorPriority(snapshotError_) < errorPriority(error)) {
            error = snapshotError_;
            message = snapshotErrorMessage_;
        }
    }

    const QString text = error == BluetoothError::None ? QString() : message;
    if (errorText_ == text) {
        return;
    }
    errorText_ = text;
    emit errorTextChanged();
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
    if (agent_ != nullptr) {
        agent_->initialize();
    }
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

    if (discovery_ != nullptr) {
        discovery_->shutdown();
    }
    if (lifecycle_ != nullptr) {
        lifecycle_->shutdown();
    }
    if (agent_ != nullptr) {
        agent_->shutdown();
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
    if (lifecycle_ != nullptr) {
        lifecycle_->onBlueZAvailabilityChanged(available);
    }
    emit adapterChanged();
    emit scanningChanged();
    updateStatusText();
    refreshDisplayedError();
}

void BluetoothManager::handleSystemBusStateChanged(bool connected)
{
    if (discovery_ != nullptr) {
        discovery_->onSelectedAdapterChanged();
    }
    emit scanningChanged();
    updateStatusText();
    refreshDisplayedError();
    if (!connected) {
        qCWarning(auralisBluetooth) << "SystemBusUnavailable";
    }
}

void BluetoothManager::handleSnapshotFailed(const QString& errorName, const QString& errorMessage)
{
    snapshotError_ = BluetoothError::DbusCallFailed;
    snapshotErrorMessage_ = QStringLiteral("Failed to refresh BlueZ state: %1 (%2)").arg(errorMessage, errorName);
    qCWarning(auralisBluetooth) << "BlueZSnapshotFailed" << errorName << errorMessage;
    refreshDisplayedError();
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
            logParseWarnings(it.key(), bluez::kAdapterInterface.toString(), parsed.warnings);
            adapters_->upsertAdapter(parsed.adapter);
            adapterPaths.insert(it.key());
        }
        if (interfaces.contains(bluez::kDeviceInterface.toString())) {
            DeviceParseResult parsed = parseDevice(it.key(), deviceProperties(interfaces));
            logParseWarnings(it.key(), bluez::kDeviceInterface.toString(), parsed.warnings);
            registry_->upsertDevice(parsed.device);
            devicePaths.insert(it.key());
        }
    }
    adapters_->reconcile(adapterPaths);
    registry_->reconcile(devicePaths);
    snapshotError_ = BluetoothError::None;
    snapshotErrorMessage_.clear();
    discovery_->onSelectedAdapterChanged();
    if (adapters_->hasAdapter()) {
        discovery_->onAdapterDiscoveringPropertyChanged(adapters_->selected().discovering);
    }
    qCInfo(auralisBluetooth) << "BlueZSnapshotApplied adapters=" << adapterPaths.size()
                             << "devices=" << devicePaths.size();
    if (lifecycle_ != nullptr) {
        lifecycle_->applyStoredMetadataToRegistry();
        lifecycle_->onSnapshotApplied();
    }
    emit adapterChanged();
    updateStatusText();
    refreshDisplayedError();
}

void BluetoothManager::handleInterfacesAdded(const QString& objectPath, const QVariantMap& interfaces)
{
    if (interfaces.contains(bluez::kAdapterInterface.toString())) {
        AdapterParseResult parsed = parseAdapter(objectPath, adapterProperties(interfaces));
        logParseWarnings(objectPath, bluez::kAdapterInterface.toString(), parsed.warnings);
        adapters_->upsertAdapter(parsed.adapter);
        qCInfo(auralisBluetooth) << "AdapterAdded" << objectPath;
    }
    if (interfaces.contains(bluez::kDeviceInterface.toString())) {
        DeviceParseResult parsed = parseDevice(objectPath, deviceProperties(interfaces));
        logParseWarnings(objectPath, bluez::kDeviceInterface.toString(), parsed.warnings);
        registry_->upsertDevice(parsed.device);
        if (lifecycle_ != nullptr) {
            lifecycle_->applyStoredMetadataToRegistry();
        }
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
        if (lifecycle_ != nullptr) {
            lifecycle_->onDeviceRemoved(objectPath);
        }
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
        if (objectPath == adapters_->selectedObjectPath()
            && (changed.contains(bluez::kPropDiscovering.toString())
                || invalidated.contains(bluez::kPropDiscovering.toString()))) {
            discovery_->onAdapterDiscoveringPropertyChanged(adapters_->selected().discovering);
        }
        return;
    }
    if (interfaceName == bluez::kDeviceInterface.toString()) {
        registry_->applyPropertyChanges(objectPath, changed, invalidated);
        if (lifecycle_ != nullptr) {
            lifecycle_->onDevicePropertiesChanged(objectPath, changed, invalidated);
        }
    }
}

QObject* BluetoothManager::pendingPairingRequest() const
{
    return agent_ != nullptr ? agent_->pendingRequest() : nullptr;
}

bool BluetoothManager::agentRegistered() const
{
    return agent_ != nullptr && agent_->isRegistered();
}

void BluetoothManager::pairDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->pairDevice(deviceId);
    }
}

void BluetoothManager::cancelPairing(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->cancelPairing(deviceId);
    }
}

void BluetoothManager::trustDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->trustDevice(deviceId);
    }
}

void BluetoothManager::untrustDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->untrustDevice(deviceId);
    }
}

void BluetoothManager::connectDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->connectDevice(deviceId);
    }
}

void BluetoothManager::disconnectDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->disconnectDevice(deviceId);
    }
}

void BluetoothManager::forgetDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->forgetDevice(deviceId);
    }
}

void BluetoothManager::reconnectDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->reconnectDevice(deviceId);
    }
}

void BluetoothManager::cancelDeviceOperation(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->cancelDeviceOperation(deviceId);
    }
}

void BluetoothManager::acceptPairingRequest(const QString& requestId)
{
    if (agent_ != nullptr) {
        agent_->acceptPairingRequest(requestId);
    }
}

void BluetoothManager::rejectPairingRequest(const QString& requestId)
{
    if (agent_ != nullptr) {
        agent_->rejectPairingRequest(requestId);
    }
}

void BluetoothManager::submitPinCode(const QString& requestId, const QString& pin)
{
    if (agent_ != nullptr) {
        agent_->submitPinCode(requestId, pin);
    }
}

void BluetoothManager::submitPasskey(const QString& requestId, uint passkey)
{
    if (agent_ != nullptr) {
        agent_->submitPasskey(requestId, passkey);
    }
}

QString BluetoothManager::serviceFriendlyName(const QString& uuid) const
{
    return UuidCatalog::friendlyName(uuid);
}

bool BluetoothManager::userDisconnectRequestedForDevice(const QString& deviceId) const
{
    if (registry_ == nullptr) {
        return false;
    }
    const BluetoothDeviceData* device = registry_->findByObjectPath(deviceId);
    if (device != nullptr) {
        return device->userDisconnectRequested;
    }
    return false;
}

void BluetoothManager::setReconnectPolicyConfig(const ReconnectPolicyConfig& config)
{
    if (reconnect_ != nullptr) {
        reconnect_->setConfig(config);
    }
}

} // namespace auralis::bluetooth
