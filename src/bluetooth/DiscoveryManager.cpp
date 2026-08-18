#include <auralis/bluetooth/DiscoveryManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/IBlueZClient.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::bluetooth {

QString toString(DiscoveryState state)
{
    switch (state) {
    case DiscoveryState::Unavailable:
        return QStringLiteral("Unavailable");
    case DiscoveryState::Idle:
        return QStringLiteral("Idle");
    case DiscoveryState::Starting:
        return QStringLiteral("Starting");
    case DiscoveryState::Discovering:
        return QStringLiteral("Discovering");
    case DiscoveryState::Stopping:
        return QStringLiteral("Stopping");
    case DiscoveryState::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

DiscoveryManager::DiscoveryManager(IBlueZClient* client, AdapterManager* adapters, QObject* parent)
    : QObject(parent)
    , client_(client)
    , adapters_(adapters)
{
}

void DiscoveryManager::setState(DiscoveryState state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    emit stateChanged();
}

void DiscoveryManager::setError(BluetoothError error, const QString& message)
{
    lastError_ = error;
    lastErrorMessage_ = message.isEmpty() ? defaultMessage(error) : message;
    emit errorChanged();
}

void DiscoveryManager::dropOwnership(DiscoveryState next)
{
    ownsDiscovery_ = false;
    pendingAdapterPath_.clear();
    setState(next);
}

bool DiscoveryManager::canStartScan() const
{
    if (client_ == nullptr || adapters_ == nullptr) {
        return false;
    }
    if (!client_->isBlueZAvailable() || !adapters_->hasAdapter()) {
        return false;
    }
    if (!adapters_->selected().powered) {
        return false;
    }
    return state_ != DiscoveryState::Starting && !ownsDiscovery_;
}

bool DiscoveryManager::canStopScan() const
{
    return ownsDiscovery_ || state_ == DiscoveryState::Starting || state_ == DiscoveryState::Stopping
        || state_ == DiscoveryState::Discovering;
}

DiscoveryState DiscoveryManager::state() const noexcept
{
    return state_;
}

bool DiscoveryManager::ownsDiscovery() const noexcept
{
    return ownsDiscovery_;
}

BluetoothError DiscoveryManager::lastError() const noexcept
{
    return lastError_;
}

QString DiscoveryManager::lastErrorMessage() const
{
    return lastErrorMessage_;
}

QString DiscoveryManager::statusText() const
{
    if (client_ != nullptr && !client_->isSystemBusConnected()) {
        return defaultMessage(BluetoothError::NoSystemBus);
    }
    if (client_ != nullptr && !client_->isBlueZAvailable()) {
        return defaultMessage(BluetoothError::BlueZUnavailable);
    }
    if (adapters_ == nullptr || !adapters_->hasAdapter()) {
        return defaultMessage(BluetoothError::NoAdapter);
    }
    if (!adapters_->selected().powered) {
        return defaultMessage(BluetoothError::AdapterPoweredOff);
    }
    switch (state_) {
    case DiscoveryState::Starting:
        return QStringLiteral("Starting discovery...");
    case DiscoveryState::Discovering:
        return QStringLiteral("Scanning...");
    case DiscoveryState::Stopping:
        return QStringLiteral("Stopping discovery...");
    case DiscoveryState::Error:
        return lastErrorMessage_.isEmpty() ? QStringLiteral("Discovery error") : lastErrorMessage_;
    case DiscoveryState::Idle:
        return QStringLiteral("Ready to scan");
    case DiscoveryState::Unavailable:
        return defaultMessage(BluetoothError::NoAdapter);
    }
    return {};
}

void DiscoveryManager::onBlueZAvailabilityChanged(bool available)
{
    if (!available) {
        dropOwnership(DiscoveryState::Unavailable);
        setError(BluetoothError::BlueZUnavailable);
        return;
    }
    if (adapters_ != nullptr && adapters_->hasAdapter() && adapters_->selected().powered) {
        setState(DiscoveryState::Idle);
        return;
    }
    onSelectedAdapterChanged();
}

void DiscoveryManager::onSelectedAdapterChanged()
{
    if (client_ == nullptr || !client_->isBlueZAvailable()) {
        dropOwnership(DiscoveryState::Unavailable);
        return;
    }
    if (adapters_ == nullptr || !adapters_->hasAdapter()) {
        dropOwnership(DiscoveryState::Unavailable);
        setError(BluetoothError::NoAdapter);
        return;
    }
    if (!adapters_->selected().powered) {
        dropOwnership(DiscoveryState::Idle);
        setError(BluetoothError::AdapterPoweredOff);
        return;
    }
    if (state_ == DiscoveryState::Unavailable || state_ == DiscoveryState::Error) {
        setState(DiscoveryState::Idle);
    }
}

void DiscoveryManager::startScan()
{
    if (!canStartScan() || client_ == nullptr || adapters_ == nullptr) {
        return;
    }

    pendingAdapterPath_ = adapters_->selectedObjectPath();
    setError(BluetoothError::None);
    setState(DiscoveryState::Starting);
    qCInfo(auralisBluetooth) << "DiscoveryStartRequested" << pendingAdapterPath_;
    client_->startDiscovery(pendingAdapterPath_);
}

void DiscoveryManager::stopScan()
{
    if (!ownsDiscovery_ && state_ != DiscoveryState::Starting && state_ != DiscoveryState::Discovering) {
        return;
    }
    if (client_ == nullptr || adapters_ == nullptr || pendingAdapterPath_.isEmpty()) {
        dropOwnership(DiscoveryState::Idle);
        return;
    }

    setState(DiscoveryState::Stopping);
    qCInfo(auralisBluetooth) << "DiscoveryStopRequested" << pendingAdapterPath_;
    client_->stopDiscovery(pendingAdapterPath_);
}

void DiscoveryManager::handleStartFinished(
    const QString& adapterPath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    if (state_ != DiscoveryState::Starting) {
        return;
    }
    if (adapterPath != pendingAdapterPath_) {
        return;
    }

    if (succeeded || errorName == bluez::kErrorInProgress.toString()) {
        ownsDiscovery_ = true;
        setError(BluetoothError::None);
        setState(DiscoveryState::Discovering);
        qCInfo(auralisBluetooth) << "DiscoveryStarted" << adapterPath;
        return;
    }

    ownsDiscovery_ = false;
    pendingAdapterPath_.clear();
    const QString message = QStringLiteral("%1 (%2)").arg(errorMessage, errorName);
    qCWarning(auralisBluetooth) << "DiscoveryStartFailed" << adapterPath << "dbusError=" << errorName
                                << "message=" << errorMessage;
    setError(BluetoothError::DiscoveryStartFailed, message);
    setState(DiscoveryState::Idle);
}

void DiscoveryManager::handleStopFinished(
    const QString& adapterPath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    if (state_ != DiscoveryState::Stopping) {
        return;
    }
    if (!adapterPath.isEmpty() && adapterPath != pendingAdapterPath_) {
        return;
    }

    if (succeeded) {
        qCInfo(auralisBluetooth) << "DiscoveryStopped" << adapterPath;
        dropOwnership(DiscoveryState::Idle);
        setError(BluetoothError::None);
        return;
    }

    qCWarning(auralisBluetooth) << "DiscoveryStopFailed" << adapterPath << "dbusError=" << errorName
                                << "message=" << errorMessage;
    setError(BluetoothError::DiscoveryStopFailed, QStringLiteral("%1 (%2)").arg(errorMessage, errorName));
    // Do not claim a clean stop; stay Stopping until adapter Discovering is reconciled by caller.
    if (!ownsDiscovery_) {
        setState(DiscoveryState::Idle);
        return;
    }
    setState(DiscoveryState::Discovering);
}

} // namespace auralis::bluetooth
