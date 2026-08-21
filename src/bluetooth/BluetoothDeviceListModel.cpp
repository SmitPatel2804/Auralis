#include <auralis/bluetooth/BluetoothDeviceListModel.h>

#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/DeviceButtonPolicy.h>
#include <auralis/bluetooth/DeviceOperation.h>

#if defined(Q_OS_LINUX)
#include <auralis/bluetooth/BluetoothButtonControlManager.h>
#endif

namespace auralis::bluetooth {

BluetoothDeviceListModel::BluetoothDeviceListModel(DeviceRegistry* registry, QObject* parent)
    : QAbstractListModel(parent)
    , registry_(registry)
{
    if (registry_ == nullptr) {
        return;
    }

    connect(registry_, &DeviceRegistry::deviceAboutToBeAdded, this, &BluetoothDeviceListModel::onDeviceAboutToBeAdded);
    connect(registry_, &DeviceRegistry::deviceAdded, this, &BluetoothDeviceListModel::onDeviceAdded);
    connect(
        registry_,
        &DeviceRegistry::deviceUpdated,
        this,
        &BluetoothDeviceListModel::onDeviceUpdated,
        Qt::QueuedConnection);
    connect(
        registry_,
        &DeviceRegistry::deviceAboutToBeRemoved,
        this,
        &BluetoothDeviceListModel::onDeviceAboutToBeRemoved);
    connect(registry_, &DeviceRegistry::deviceRemoved, this, &BluetoothDeviceListModel::onDeviceRemoved);
}

void BluetoothDeviceListModel::setButtonControlManager(BluetoothButtonControlManager* manager)
{
#if defined(Q_OS_LINUX)
    if (buttonControls_ == manager) {
        return;
    }
    if (buttonControls_ != nullptr) {
        disconnect(buttonControls_, nullptr, this, nullptr);
    }
    buttonControls_ = manager;
    if (buttonControls_ != nullptr) {
        connect(
            buttonControls_,
            &BluetoothButtonControlManager::policyChanged,
            this,
            &BluetoothDeviceListModel::onButtonPolicyChanged);
        connect(
            buttonControls_,
            &BluetoothButtonControlManager::effectiveStateChanged,
            this,
            &BluetoothDeviceListModel::onButtonPolicyChanged);
    }
    if (rowCount() > 0) {
        emit dataChanged(index(0), index(rowCount() - 1));
    }
#else
    Q_UNUSED(manager);
    buttonControls_ = nullptr;
#endif
}

int BluetoothDeviceListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || registry_ == nullptr) {
        return 0;
    }
    return registry_->count();
}

QVariant BluetoothDeviceListModel::data(const QModelIndex& index, int role) const
{
    if (registry_ == nullptr || !index.isValid() || index.row() < 0 || index.row() >= registry_->count()) {
        return {};
    }

    const BluetoothDeviceData device = registry_->at(index.row());
    switch (role) {
    case InternalIdRole:
    case ObjectPathRole:
        return device.objectPath;
    case AddressRole:
        return device.address;
    case AddressTypeRole:
        return device.addressType;
    case DisplayNameRole:
    case Qt::DisplayRole:
        return device.displayName();
    case NameRole:
        return device.name;
    case AliasRole:
        return device.alias;
    case RssiRole:
        return static_cast<int>(device.rssi);
    case HasRssiRole:
        return device.hasRssi;
    case PairedRole:
        return device.paired;
    case ConnectedRole:
        return device.connected;
    case TrustedRole:
        return device.trusted;
    case BlockedRole:
        return device.blocked;
    case ServicesResolvedRole:
        return device.servicesResolved;
    case IconRole:
        return device.icon;
    case ClassRole:
        return device.hasClassOfDevice ? QVariant(device.classOfDevice) : QVariant();
    case HasClassRole:
        return device.hasClassOfDevice;
    case AppearanceRole:
        return device.hasAppearance ? QVariant(device.appearance) : QVariant();
    case HasAppearanceRole:
        return device.hasAppearance;
    case UuidsRole:
        return device.uuids;
    case LastSeenRole:
        return device.lastSeen;
    case TransportHintRole:
        return device.transportHint();
    case OperationRole:
        return static_cast<int>(device.operation);
    case LogicalStateRole:
        return static_cast<int>(deriveLogicalState(device));
    case OperationTextRole:
        return operationStatusText(device.operation, device.reconnectAttempt, 5);
    case LastErrorMessageRole:
        return device.lastErrorMessage;
    case CanPairRole:
        return canPair(device);
    case CanCancelPairingRole:
        return canCancelPairing(device);
    case CanTrustRole:
        return canTrust(device);
    case CanUntrustRole:
        return canUntrust(device);
    case CanConnectRole:
#if defined(Q_OS_LINUX)
        return canConnect(device);
#else
        // Qt's native desktop Bluetooth API can pair devices but does not
        // expose profile connect/disconnect operations.
        return false;
#endif
    case CanDisconnectRole:
#if defined(Q_OS_LINUX)
        return canDisconnect(device);
#else
        return false;
#endif
    case CanForgetRole:
        return canForget(device);
    case CanReconnectRole:
#if defined(Q_OS_LINUX)
        return canReconnect(device);
#else
        return false;
#endif
    case CanCancelOperationRole:
        return canCancelOperation(device);
    case ButtonPolicyRole:
#if defined(Q_OS_LINUX)
        return buttonControls_ != nullptr
            ? static_cast<int>(buttonControls_->policyForAddress(device.address))
            : static_cast<int>(DeviceButtonPolicy::Allow);
#else
        return static_cast<int>(DeviceButtonPolicy::Allow);
#endif
    case ButtonPolicyTextRole:
#if defined(Q_OS_LINUX)
        return buttonControls_ != nullptr
            ? deviceButtonPolicyText(buttonControls_->policyForAddress(device.address))
            : deviceButtonPolicyText(DeviceButtonPolicy::Allow);
#else
        return deviceButtonPolicyText(DeviceButtonPolicy::Allow);
#endif
    case CanControlButtonsRole:
#if defined(Q_OS_LINUX)
        return buttonControls_ != nullptr && buttonControls_->canControlButtonsForAddress(device.address);
#else
        return false;
#endif
    case ButtonEffectiveStateRole:
#if defined(Q_OS_LINUX)
        return buttonControls_ != nullptr
            ? static_cast<int>(buttonControls_->effectiveStateForAddress(device.address))
            : static_cast<int>(DeviceButtonEffectiveState::Allowed);
#else
        return static_cast<int>(DeviceButtonEffectiveState::Unsupported);
#endif
    case ButtonEffectiveStatusRole:
#if defined(Q_OS_LINUX)
        return buttonControls_ != nullptr
            ? buttonControls_->effectiveStatusTextForAddress(device.address)
            : deviceButtonEffectiveStateText(DeviceButtonEffectiveState::Allowed);
#else
#if defined(Q_OS_WIN)
        return QStringLiteral("Not enforced on Windows: a signed per-device media-control component is required");
#elif defined(Q_OS_MACOS)
        return QStringLiteral("Not enforced on macOS: per-device media-button suppression is unavailable");
#else
        return deviceButtonEffectiveStateText(DeviceButtonEffectiveState::Unsupported);
#endif
#endif
    default:
        return {};
    }
}

QHash<int, QByteArray> BluetoothDeviceListModel::roleNames() const
{
    return {
        {InternalIdRole, "internalId"},
        {ObjectPathRole, "objectPath"},
        {AddressRole, "address"},
        {AddressTypeRole, "addressType"},
        {DisplayNameRole, "displayName"},
        {NameRole, "name"},
        {AliasRole, "alias"},
        {RssiRole, "rssi"},
        {HasRssiRole, "hasRssi"},
        {PairedRole, "paired"},
        {ConnectedRole, "connected"},
        {TrustedRole, "trusted"},
        {BlockedRole, "blocked"},
        {ServicesResolvedRole, "servicesResolved"},
        {IconRole, "icon"},
        {ClassRole, "classOfDevice"},
        {HasClassRole, "hasClass"},
        {AppearanceRole, "appearance"},
        {HasAppearanceRole, "hasAppearance"},
        {UuidsRole, "uuids"},
        {LastSeenRole, "lastSeen"},
        {TransportHintRole, "transportHint"},
        {OperationRole, "operation"},
        {LogicalStateRole, "logicalState"},
        {OperationTextRole, "operationText"},
        {LastErrorMessageRole, "lastErrorMessage"},
        {CanPairRole, "canPair"},
        {CanCancelPairingRole, "canCancelPairing"},
        {CanTrustRole, "canTrust"},
        {CanUntrustRole, "canUntrust"},
        {CanConnectRole, "canConnect"},
        {CanDisconnectRole, "canDisconnect"},
        {CanForgetRole, "canForget"},
        {CanReconnectRole, "canReconnect"},
        {CanCancelOperationRole, "canCancelOperation"},
        {ButtonPolicyRole, "buttonPolicy"},
        {ButtonPolicyTextRole, "buttonPolicyText"},
        {CanControlButtonsRole, "canControlButtons"},
        {ButtonEffectiveStateRole, "buttonEffectiveState"},
        {ButtonEffectiveStatusRole, "buttonEffectiveStatus"},
    };
}

void BluetoothDeviceListModel::onDeviceAboutToBeAdded(int index)
{
    beginInsertRows(QModelIndex(), index, index);
}

void BluetoothDeviceListModel::onDeviceAdded(int)
{
    endInsertRows();
}

void BluetoothDeviceListModel::onDeviceUpdated(int index, const QList<int>& roles)
{
    if (index < 0 || registry_ == nullptr || index >= registry_->count()) {
        return;
    }
    const QModelIndex modelIndex = this->index(index);
    if (roles.isEmpty()) {
        emit dataChanged(modelIndex, modelIndex);
        return;
    }
    emit dataChanged(modelIndex, modelIndex, roles);
}

void BluetoothDeviceListModel::onDeviceAboutToBeRemoved(int index, const QString&)
{
    beginRemoveRows(QModelIndex(), index, index);
}

void BluetoothDeviceListModel::onDeviceRemoved(int, const QString&)
{
    endRemoveRows();
}

void BluetoothDeviceListModel::onButtonPolicyChanged(const QString& address)
{
#if defined(Q_OS_LINUX)
    if (registry_ == nullptr || address.isEmpty()) {
        return;
    }
    for (int row = 0; row < registry_->count(); ++row) {
        if (!BluetoothButtonControlManager::addressesMatch(registry_->at(row).address, address)) {
            continue;
        }
        const QModelIndex modelIndex = index(row);
        emit dataChanged(
            modelIndex,
            modelIndex,
            {ButtonPolicyRole,
             ButtonPolicyTextRole,
             CanControlButtonsRole,
             ButtonEffectiveStateRole,
             ButtonEffectiveStatusRole});
    }
#else
    Q_UNUSED(address);
#endif
}

} // namespace auralis::bluetooth
