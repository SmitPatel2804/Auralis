#include <auralis/bluetooth/DiscoveryManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/IBlueZClient.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::bluetooth {
namespace {

bool isPrerequisiteError(BluetoothError error)
{
    return error == BluetoothError::NoSystemBus || error == BluetoothError::BlueZUnavailable
        || error == BluetoothError::NoAdapter || error == BluetoothError::AdapterPoweredOff;
}

} // namespace

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
    const QString resolved = message.isEmpty() ? defaultMessage(error) : message;
    if (lastError_ == error && lastErrorMessage_ == resolved) {
        return;
    }
    lastError_ = error;
    lastErrorMessage_ = resolved;
    emit errorChanged();
}

void DiscoveryManager::setDesiredScanning(bool desired)
{
    if (desiredScanning_ == desired) {
        return;
    }
    desiredScanning_ = desired;
    qCInfo(auralisBluetooth) << "DiscoveryIntentChanged" << (desired ? "scanning=true" : "scanning=false");
    emit intentChanged();
}

void DiscoveryManager::setPendingOperation(PendingOperation operation)
{
    if (pendingOperation_ == operation) {
        return;
    }
    pendingOperation_ = operation;
    emit intentChanged();
}

void DiscoveryManager::dropSession(DiscoveryState next)
{
    ownsDiscovery_ = false;
    confirmedAdapterDiscovering_ = false;
    setPendingOperation(PendingOperation::None);
    operationAdapterPath_.clear();
    setState(next);
}

bool DiscoveryManager::prerequisitesValid() const
{
    if (client_ == nullptr || adapters_ == nullptr) {
        return false;
    }
    if (!client_->isSystemBusConnected() || !client_->isBlueZAvailable()) {
        return false;
    }
    if (!adapters_->hasAdapter() || !adapters_->selected().powered) {
        return false;
    }
    return true;
}

bool DiscoveryManager::isStaleCallback(const QString& adapterPath) const
{
    if (shuttingDown_) {
        return true;
    }
    if (operationGeneration_ != bluezGeneration_) {
        return true;
    }
    if (!adapterPath.isEmpty() && adapterPath != operationAdapterPath_) {
        return true;
    }
    return false;
}

bool DiscoveryManager::canStartScan() const
{
    if (shuttingDown_ || !prerequisitesValid()) {
        return false;
    }
    return !desiredScanning_;
}

bool DiscoveryManager::canStopScan() const
{
    return desiredScanning_ || ownsDiscovery_ || pendingOperation_ == PendingOperation::Start;
}

DiscoveryState DiscoveryManager::state() const noexcept
{
    return state_;
}

bool DiscoveryManager::ownsDiscovery() const noexcept
{
    return ownsDiscovery_;
}

bool DiscoveryManager::desiredScanning() const noexcept
{
    return desiredScanning_;
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

void DiscoveryManager::clearRecoverablePrerequisiteError()
{
    if (isPrerequisiteError(lastError_)) {
        setError(BluetoothError::None);
    }
}

void DiscoveryManager::beginStart()
{
    if (client_ == nullptr || adapters_ == nullptr) {
        return;
    }
    operationAdapterPath_ = adapters_->selectedObjectPath();
    operationGeneration_ = bluezGeneration_;
    setPendingOperation(PendingOperation::Start);
    setState(DiscoveryState::Starting);
    qCInfo(auralisBluetooth) << "DiscoveryStartRequested" << operationAdapterPath_;
    client_->startDiscovery(operationAdapterPath_);
}

void DiscoveryManager::beginStop()
{
    if (client_ == nullptr) {
        dropSession(DiscoveryState::Idle);
        return;
    }
    if (operationAdapterPath_.isEmpty() && adapters_ != nullptr) {
        operationAdapterPath_ = adapters_->selectedObjectPath();
    }
    if (operationAdapterPath_.isEmpty()) {
        dropSession(DiscoveryState::Idle);
        return;
    }
    operationGeneration_ = bluezGeneration_;
    setPendingOperation(PendingOperation::Stop);
    setState(DiscoveryState::Stopping);
    qCInfo(auralisBluetooth) << "DiscoveryStopRequested" << operationAdapterPath_;
    client_->stopDiscovery(operationAdapterPath_);
}

void DiscoveryManager::reconcileDesiredState()
{
    if (shuttingDown_) {
        return;
    }
    if (pendingOperation_ != PendingOperation::None) {
        return;
    }

    if (deviceOperationHold_) {
        if (ownsDiscovery_) {
            qCInfo(auralisBluetooth) << "DiscoveryPausedForDeviceOperation" << operationAdapterPath_;
            beginStop();
        }
        return;
    }

    if (desiredScanning_ && !ownsDiscovery_ && prerequisitesValid()) {
        beginStart();
        qCInfo(auralisBluetooth) << "DiscoveryStateReconciled" << toString(state_)
                                 << "desired=" << desiredScanning_ << "owns=" << ownsDiscovery_;
        return;
    }

    if (!desiredScanning_ && ownsDiscovery_) {
        beginStop();
        qCInfo(auralisBluetooth) << "DiscoveryStateReconciled" << toString(state_)
                                 << "desired=" << desiredScanning_ << "owns=" << ownsDiscovery_;
        return;
    }

    if (!desiredScanning_ && !ownsDiscovery_) {
        if (prerequisitesValid()) {
            if (state_ != DiscoveryState::Idle) {
                setState(DiscoveryState::Idle);
            }
        } else if (state_ != DiscoveryState::Unavailable && state_ != DiscoveryState::Idle) {
            setState(client_ != nullptr && client_->isBlueZAvailable() && adapters_ != nullptr && adapters_->hasAdapter()
                         ? DiscoveryState::Idle
                         : DiscoveryState::Unavailable);
        }
    }
}

void DiscoveryManager::reconcilePrerequisites()
{
    if (shuttingDown_) {
        return;
    }

    if (client_ == nullptr || !client_->isSystemBusConnected()) {
        setDesiredScanning(false);
        dropSession(DiscoveryState::Unavailable);
        setError(BluetoothError::NoSystemBus);
        return;
    }
    if (!client_->isBlueZAvailable()) {
        setDesiredScanning(false);
        dropSession(DiscoveryState::Unavailable);
        setError(BluetoothError::BlueZUnavailable);
        return;
    }
    if (adapters_ == nullptr || !adapters_->hasAdapter()) {
        setDesiredScanning(false);
        dropSession(DiscoveryState::Unavailable);
        setError(BluetoothError::NoAdapter);
        return;
    }
    if (!adapters_->selected().powered) {
        qCInfo(auralisBluetooth) << "AdapterPoweredOff" << adapters_->selectedObjectPath();
        setDesiredScanning(false);
        dropSession(DiscoveryState::Idle);
        setError(BluetoothError::AdapterPoweredOff);
        return;
    }

    const QString selected = adapters_->selectedObjectPath();
    if (!operationAdapterPath_.isEmpty() && operationAdapterPath_ != selected) {
        const AdapterData previous = adapters_->adapterAt(operationAdapterPath_);
        if (previous.objectPath.isEmpty()) {
            qCInfo(auralisBluetooth) << "AdapterRemoved" << operationAdapterPath_;
            setDesiredScanning(false);
            dropSession(prerequisitesValid() ? DiscoveryState::Idle : DiscoveryState::Unavailable);
        }
    }

    if (lastError_ == BluetoothError::AdapterPoweredOff) {
        qCInfo(auralisBluetooth) << "AdapterPoweredOn" << selected;
    }
    clearRecoverablePrerequisiteError();

    if (!ownsDiscovery_ && pendingOperation_ == PendingOperation::None && !desiredScanning_) {
        if (state_ == DiscoveryState::Unavailable || state_ == DiscoveryState::Error) {
            setState(DiscoveryState::Idle);
        }
    }

    reconcileDesiredState();
}

void DiscoveryManager::applyUnexpectedDiscoveryStop()
{
    const bool wanted = desiredScanning_;
    setDesiredScanning(false);
    dropSession(DiscoveryState::Idle);
    if (wanted) {
        setError(
            BluetoothError::DiscoveryStopFailed,
            QStringLiteral("Bluetooth discovery stopped unexpectedly"));
    }
    qCInfo(auralisBluetooth) << "DiscoveryStateReconciled unexpected Adapter1 Discovering=false";
}

void DiscoveryManager::onBlueZAvailabilityChanged(bool available)
{
    if (available != lastBlueZAvailable_) {
        ++bluezGeneration_;
        lastBlueZAvailable_ = available;
    }
    if (!available) {
        qCInfo(auralisBluetooth) << "BlueZUnavailable";
        setDesiredScanning(false);
        dropSession(DiscoveryState::Unavailable);
    } else {
        qCInfo(auralisBluetooth) << "BlueZAvailable";
    }
    reconcilePrerequisites();
}

void DiscoveryManager::onSelectedAdapterChanged()
{
    reconcilePrerequisites();
}

void DiscoveryManager::onAdapterDiscoveringPropertyChanged(bool discovering)
{
    if (shuttingDown_) {
        return;
    }
    if (!discovering) {
        confirmedAdapterDiscovering_ = false;
        if (!ownsDiscovery_ || pendingOperation_ == PendingOperation::Stop || deviceOperationHold_) {
            return;
        }
        applyUnexpectedDiscoveryStop();
        return;
    }
    if (ownsDiscovery_) {
        confirmedAdapterDiscovering_ = true;
    }
}

void DiscoveryManager::startScan()
{
    if (shuttingDown_) {
        return;
    }
    if (!prerequisitesValid()) {
        reconcilePrerequisites();
        return;
    }
    if (desiredScanning_) {
        return;
    }
    setDesiredScanning(true);
    setError(BluetoothError::None);
    reconcileDesiredState();
}

void DiscoveryManager::stopScan()
{
    if (shuttingDown_) {
        return;
    }
    if (!desiredScanning_ && !ownsDiscovery_ && pendingOperation_ != PendingOperation::Start) {
        return;
    }
    setDesiredScanning(false);
    reconcileDesiredState();
}

void DiscoveryManager::setDeviceOperationHold(bool hold)
{
    if (deviceOperationHold_ == hold) {
        return;
    }
    deviceOperationHold_ = hold;
    reconcileDesiredState();
}

void DiscoveryManager::shutdown()
{
    shuttingDown_ = true;
    desiredScanning_ = false;
    if (ownsDiscovery_ && client_ != nullptr && !operationAdapterPath_.isEmpty()) {
        qCInfo(auralisBluetooth) << "DiscoveryStopRequested" << operationAdapterPath_ << "reason=shutdown";
        client_->stopDiscovery(operationAdapterPath_);
    }
    ownsDiscovery_ = false;
    confirmedAdapterDiscovering_ = false;
    pendingOperation_ = PendingOperation::None;
    operationAdapterPath_.clear();
    setState(DiscoveryState::Unavailable);
}

void DiscoveryManager::handleStartFinished(
    const QString& adapterPath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    if (pendingOperation_ != PendingOperation::Start) {
        qCInfo(auralisBluetooth) << "Ignored stale StartDiscovery callback" << adapterPath;
        return;
    }
    if (isStaleCallback(adapterPath)) {
        qCInfo(auralisBluetooth) << "Ignored stale StartDiscovery callback" << adapterPath;
        setPendingOperation(PendingOperation::None);
        return;
    }

    setPendingOperation(PendingOperation::None);

    if (succeeded) {
        ownsDiscovery_ = true;
        setError(BluetoothError::None);
        setState(DiscoveryState::Discovering);
        qCInfo(auralisBluetooth) << "DiscoveryStarted" << adapterPath;
        reconcileDesiredState();
        return;
    }

    ownsDiscovery_ = false;
    confirmedAdapterDiscovering_ = false;
    setDesiredScanning(false);
    const QString message = QStringLiteral("%1 (%2)").arg(errorMessage, errorName);
    qCWarning(auralisBluetooth) << "DiscoveryStartFailed" << adapterPath << "dbusError=" << errorName
                                << "message=" << errorMessage;
    setError(BluetoothError::DiscoveryStartFailed, message);
    setState(prerequisitesValid() ? DiscoveryState::Idle : DiscoveryState::Unavailable);
    reconcileDesiredState();
}

void DiscoveryManager::handleStopFinished(
    const QString& adapterPath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    if (pendingOperation_ != PendingOperation::Stop) {
        qCInfo(auralisBluetooth) << "Ignored stale StopDiscovery callback" << adapterPath;
        return;
    }
    if (isStaleCallback(adapterPath)) {
        qCInfo(auralisBluetooth) << "Ignored stale StopDiscovery callback" << adapterPath;
        setPendingOperation(PendingOperation::None);
        return;
    }

    setPendingOperation(PendingOperation::None);

    if (succeeded) {
        qCInfo(auralisBluetooth) << "DiscoveryStopped" << adapterPath;
        ownsDiscovery_ = false;
        confirmedAdapterDiscovering_ = false;
        operationAdapterPath_.clear();
        setError(BluetoothError::None);
        setState(prerequisitesValid() ? DiscoveryState::Idle : DiscoveryState::Unavailable);
        reconcileDesiredState();
        return;
    }

    qCWarning(auralisBluetooth) << "DiscoveryStopFailed" << adapterPath << "dbusError=" << errorName
                                << "message=" << errorMessage;
    setError(BluetoothError::DiscoveryStopFailed, QStringLiteral("%1 (%2)").arg(errorMessage, errorName));
    if (!ownsDiscovery_) {
        setState(prerequisitesValid() ? DiscoveryState::Idle : DiscoveryState::Unavailable);
        reconcileDesiredState();
        return;
    }
    setState(DiscoveryState::Discovering);
}

} // namespace auralis::bluetooth
