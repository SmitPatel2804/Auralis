#include <auralis/bluetooth/BluetoothManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/AgentCapability.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/BlueZDbusClient.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/BluetoothButtonControlManager.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/BluetoothTransportFilterModel.h>
#include <auralis/bluetooth/DeviceLifecycleManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/DiscoveryManager.h>
#include <auralis/bluetooth/IBlueZClient.h>
#include <auralis/bluetooth/PairingRequest.h>
#include <auralis/bluetooth/ReconnectPolicy.h>
#include <auralis/bluetooth/UuidCatalog.h>

#include <auralis/core/LoggingCategories.h>

#include <QSet>
#include <QVariantMap>

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
    classicModel_ = new BluetoothTransportFilterModel(false, this);
    lowEnergyModel_ = new BluetoothTransportFilterModel(true, this);
    classicModel_->setSourceModel(model_);
    lowEnergyModel_->setSourceModel(model_);
    buttonControls_ = new BluetoothButtonControlManager(this);
    model_->setButtonControlManager(buttonControls_);
    reconnect_ = new ReconnectPolicy(this);
    agent_ = new BlueZAgent(client_, defaultAgentCapability(), this);
    lifecycle_ = new DeviceLifecycleManager(client_, registry_, adapters_, agent_, reconnect_, discovery_, this);
    connect(
        reconnect_,
        &ReconnectPolicy::reconnectExhausted,
        this,
        [this](const QString& devicePath, int attempts, const QString& reason) {
            emit managedReconnectExhausted(devicePath, attempts, reason);
        });
    connect(
        reconnect_,
        &ReconnectPolicy::reconnectTerminalFailure,
        this,
        [this](const QString& devicePath, int attempts, const QString& reason) {
            emit managedReconnectTerminalFailure(devicePath, attempts, reason);
        });
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

QString BluetoothManager::backendName() const
{
    return QStringLiteral("BlueZ");
}

DeviceRegistry* BluetoothManager::deviceRegistry() const noexcept
{
    return registry_;
}

QString BluetoothManager::deviceDisplayName(const QString& deviceId) const
{
    if (registry_ == nullptr) {
        return deviceId;
    }
    const BluetoothDeviceData* device = registry_->findByObjectPath(deviceId);
    if (device == nullptr) {
        return deviceId;
    }
    return device->displayName();
}

int BluetoothManager::connectedDeviceCount() const
{
    if (registry_ == nullptr) {
        return 0;
    }
    int count = 0;
    for (const BluetoothDeviceData& device : registry_->devices()) {
        if (device.connected) {
            ++count;
        }
    }
    return count;
}

bool BluetoothManager::hasDevice(const QString& objectPath) const
{
    return registry_ != nullptr && registry_->findByObjectPath(objectPath) != nullptr;
}

QVariantMap BluetoothManager::deviceDetails(const QString& objectPath) const
{
    QVariantMap details;
    if (registry_ == nullptr) {
        return details;
    }
    const BluetoothDeviceData* device = registry_->findByObjectPath(objectPath);
    if (device == nullptr) {
        return details;
    }
    details.insert(QStringLiteral("objectPath"), device->objectPath);
    details.insert(QStringLiteral("name"), device->name);
    details.insert(QStringLiteral("alias"), device->alias);
    details.insert(QStringLiteral("displayName"), device->displayName());
    details.insert(QStringLiteral("address"), device->address);
    details.insert(QStringLiteral("addressType"), device->addressType);
    details.insert(QStringLiteral("rssi"), device->rssi);
    details.insert(QStringLiteral("hasRssi"), device->hasRssi);
    details.insert(QStringLiteral("paired"), device->paired);
    details.insert(QStringLiteral("trusted"), device->trusted);
    details.insert(QStringLiteral("connected"), device->connected);
    details.insert(QStringLiteral("blocked"), device->blocked);
    details.insert(QStringLiteral("servicesResolved"), device->servicesResolved);
    details.insert(QStringLiteral("classOfDevice"), device->hasClassOfDevice ? QVariant(device->classOfDevice) : QVariant());
    details.insert(QStringLiteral("hasClass"), device->hasClassOfDevice);
    details.insert(QStringLiteral("appearance"), device->hasAppearance ? QVariant(device->appearance) : QVariant());
    details.insert(QStringLiteral("hasAppearance"), device->hasAppearance);
    details.insert(
        QStringLiteral("lastSeen"),
        device->lastSeen.isValid() ? device->lastSeen.toLocalTime().toString(Qt::ISODate) : QString());
    details.insert(QStringLiteral("transport"), device->transportHint());
    details.insert(QStringLiteral("icon"), device->icon);
    details.insert(QStringLiteral("uuids"), device->uuids);
    if (buttonControls_ != nullptr) {
        details.insert(
            QStringLiteral("buttonPolicy"),
            ::auralis::bluetooth::deviceButtonPolicyText(buttonControls_->policyForAddress(device->address)));
        details.insert(
            QStringLiteral("canControlButtons"),
            buttonControls_->canControlButtonsForAddress(device->address));
        details.insert(
            QStringLiteral("buttonEffectiveStatus"),
            buttonControls_->effectiveStatusTextForAddress(device->address));
    }
    return details;
}

ReconnectPolicy* BluetoothManager::reconnectPolicy() const noexcept
{
    return reconnect_;
}

void BluetoothManager::pauseManagedReconnect()
{
    if (reconnect_ != nullptr) {
        reconnect_->pauseAll();
    }
}

void BluetoothManager::resumeManagedReconnect()
{
    if (reconnect_ != nullptr) {
        reconnect_->resumeAll();
    }
}

bool BluetoothManager::systemBusConnected() const
{
    return client_ != nullptr && client_->isSystemBusConnected();
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
    connect(registry_, &DeviceRegistry::countChanged, this, [this]() {
        emit deviceCountChanged();
        emit connectedDeviceCountChanged();
    });
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
    if (buttonControls_ != nullptr) {
        buttonControls_->initialize();
    }
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
    if (buttonControls_ != nullptr) {
        buttonControls_->shutdown();
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

bool BluetoothManager::transportConnected() const noexcept
{
    return systemBusConnected();
}

bool BluetoothManager::adapterPresent() const noexcept
{
    return adapters_ != nullptr && adapters_->hasAdapter();
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

QString BluetoothManager::scanModeText() const
{
    return scanning() ? QStringLiteral("Bluetooth") : QString();
}

int BluetoothManager::deviceCount() const
{
    return registry_ != nullptr ? registry_->count() : 0;
}

int BluetoothManager::classicDeviceCount() const
{
    return classicModel_ != nullptr ? classicModel_->rowCount() : 0;
}

int BluetoothManager::lowEnergyDeviceCount() const
{
    return lowEnergyModel_ != nullptr ? lowEnergyModel_->rowCount() : 0;
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

QAbstractItemModel* BluetoothManager::classicDevices() const { return classicModel_; }
QAbstractItemModel* BluetoothManager::lowEnergyDevices() const { return lowEnergyModel_; }

void BluetoothManager::startScan()
{
    if (discovery_ != nullptr) {
        discovery_->startScan();
    }
}

void BluetoothManager::startLowEnergyScan()
{
    // BlueZ uses one discovery session. Results are still separated by their
    // transport capability in the UI.
    startScan();
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
    if (!available) {
        pauseManagedReconnect();
        if (buttonControls_ != nullptr) {
            buttonControls_->releaseAll();
        }
        if (registry_ != nullptr) {
            registry_->clear();
        }
        if (adapters_ != nullptr) {
            adapters_->clear();
        }
    } else {
        resumeManagedReconnect();
        // Snapshot is owned by BlueZDbusClient::onBlueZRegistered — avoid duplicate GetManagedObjects.
        if (buttonControls_ != nullptr) {
            buttonControls_->reapplyAll();
        }
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
    emit systemBusConnectedChanged(connected);
    emit transportConnectedChanged(connected);
    if (!connected) {
        qCWarning(auralisBluetooth) << "SystemBusUnavailable";
        pauseManagedReconnect();
    } else {
        resumeManagedReconnect();
        // Bus recovery recreates watchers; BlueZ registration path owns the authoritative snapshot.
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
    syncButtonPoliciesFromRegistry();
    emit adapterChanged();
    emit deviceCountChanged();
    emit connectedDeviceCountChanged();
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
        syncButtonPolicyForAddress(parsed.device.address, parsed.device.connected);
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
        if (changed.contains(bluez::kPropConnected.toString()) || invalidated.contains(bluez::kPropConnected.toString())) {
            if (const BluetoothDeviceData* device = registry_->findByObjectPath(objectPath)) {
                syncButtonPolicyForAddress(device->address, device->connected);
            }
            emit connectedDeviceCountChanged();
        }
        if (changed.contains(bluez::kPropAddressType.toString())
            || changed.contains(bluez::kPropClass.toString())
            || invalidated.contains(bluez::kPropAddressType.toString())
            || invalidated.contains(bluez::kPropClass.toString())) {
            emit deviceCountChanged();
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
    QString address;
    if (registry_ != nullptr) {
        if (const BluetoothDeviceData* device = registry_->findByObjectPath(deviceId)) {
            address = device->address;
        }
    }
    if (lifecycle_ != nullptr) {
        lifecycle_->forgetDevice(deviceId);
    }
    if (buttonControls_ != nullptr && !address.isEmpty()) {
        buttonControls_->clearPolicyForAddress(address);
    }
}

void BluetoothManager::setDeviceButtonPolicy(const QString& deviceId, bool disallow)
{
    if (buttonControls_ == nullptr || registry_ == nullptr) {
        return;
    }
    const BluetoothDeviceData* device = registry_->findByObjectPath(deviceId);
    if (device == nullptr || device->address.trimmed().isEmpty()) {
        return;
    }
    buttonControls_->setPolicyForAddress(
        device->address,
        disallow ? DeviceButtonPolicy::Disallow : DeviceButtonPolicy::Allow);
    syncButtonPolicyForAddress(device->address, device->connected);
}

QString BluetoothManager::deviceButtonPolicyText(const QString& deviceId) const
{
    if (buttonControls_ == nullptr || registry_ == nullptr) {
        return ::auralis::bluetooth::deviceButtonPolicyText(DeviceButtonPolicy::Allow);
    }
    const BluetoothDeviceData* device = registry_->findByObjectPath(deviceId);
    if (device == nullptr) {
        return ::auralis::bluetooth::deviceButtonPolicyText(DeviceButtonPolicy::Allow);
    }
    return ::auralis::bluetooth::deviceButtonPolicyText(buttonControls_->policyForAddress(device->address));
}

void BluetoothManager::syncButtonPolicyForAddress(const QString& address, bool connected)
{
    if (buttonControls_ == nullptr || address.trimmed().isEmpty()) {
        return;
    }
    buttonControls_->syncDevice(address, connected);
}

void BluetoothManager::syncButtonPoliciesFromRegistry()
{
    if (buttonControls_ == nullptr || registry_ == nullptr) {
        return;
    }
    for (int i = 0; i < registry_->count(); ++i) {
        const BluetoothDeviceData device = registry_->at(i);
        buttonControls_->syncDevice(device.address, device.connected);
    }
}

void BluetoothManager::reconnectDevice(const QString& deviceId)
{
    if (lifecycle_ != nullptr) {
        lifecycle_->reconnectDevice(deviceId);
    }
}

void BluetoothManager::requestManagedReconnect(const QString& deviceId)
{
    if (reconnect_ == nullptr || deviceId.trimmed().isEmpty()) {
        return;
    }
    if (reconnect_->isScheduled(deviceId) || reconnect_->isReconnectInProgress(deviceId)) {
        qCDebug(auralisBluetooth) << "ManagedReconnectAlreadyScheduled" << deviceId;
        return;
    }
    reconnect_->scheduleReconnect(deviceId);
}

void BluetoothManager::cancelManagedReconnect(const QString& deviceId)
{
    if (reconnect_ == nullptr || deviceId.trimmed().isEmpty()) {
        return;
    }
    reconnect_->cancelReconnect(deviceId);
}

void BluetoothManager::suppressAutoReconnect(const QString& deviceId)
{
    if (lifecycle_ != nullptr && !deviceId.trimmed().isEmpty()) {
        lifecycle_->suppressAutoReconnect(deviceId);
    }
}

void BluetoothManager::unsuppressAutoReconnect(const QString& deviceId)
{
    if (lifecycle_ != nullptr && !deviceId.trimmed().isEmpty()) {
        lifecycle_->unsuppressAutoReconnect(deviceId);
    }
}

bool BluetoothManager::isAutoReconnectSuppressed(const QString& objectPath) const
{
    if (lifecycle_ == nullptr) {
        return false;
    }
    return lifecycle_->isAutoReconnectSuppressed(objectPath);
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
