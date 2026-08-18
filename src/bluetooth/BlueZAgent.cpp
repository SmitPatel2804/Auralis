#include <auralis/bluetooth/BlueZAgent.h>

#include <auralis/bluetooth/BlueZAgentAdaptor.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/IBlueZClient.h>

#include <auralis/core/LoggingCategories.h>

#include <QDBusConnection>
#include <QMetaType>

namespace auralis::bluetooth {

BlueZAgent::BlueZAgent(IBlueZClient* client, QObject* parent)
    : QObject(parent)
    , client_(client)
{
    if (client_ != nullptr) {
        connect(client_, &IBlueZClient::blueZAvailableChanged, this, &BlueZAgent::onBlueZAvailableChanged);
        connect(client_, &IBlueZClient::registerAgentFinished, this, &BlueZAgent::onRegisterAgentFinished);
    }
}

BlueZAgent::~BlueZAgent()
{
    shutdown();
}

bool BlueZAgent::initialize()
{
    if (exported_) {
        return true;
    }
    adaptor_ = new BlueZAgentAdaptor(this);
    if (!QDBusConnection::systemBus().registerObject(bluez::kAuralisAgentPath.toString(), this)) {
        qCWarning(auralisBluetooth) << "Failed to export Agent1 at" << bluez::kAuralisAgentPath;
        adaptor_ = nullptr;
        return false;
    }
    exported_ = true;
    if (client_ != nullptr && client_->isBlueZAvailable()) {
        registerWithBlueZ();
    }
    return true;
}

void BlueZAgent::shutdown()
{
    invalidateAllRequests(QStringLiteral("Application shutting down"));
    unregisterFromBlueZ();
    if (exported_) {
        QDBusConnection::systemBus().unregisterObject(bluez::kAuralisAgentPath.toString());
        exported_ = false;
    }
    adaptor_ = nullptr;
}

bool BlueZAgent::isRegistered() const noexcept
{
    return registered_;
}

PairingRequest* BlueZAgent::pendingRequest() const
{
    return activeRequest_;
}

void BlueZAgent::registerWithBlueZ()
{
    if (!exported_ || client_ == nullptr || !client_->isBlueZAvailable() || registered_) {
        return;
    }
    qCInfo(auralisBluetooth) << "AgentRegisterRequested" << bluez::kAuralisAgentPath;
    client_->registerAgent(
        bluez::kAuralisAgentPath.toString(),
        bluez::kAgentCapabilityKeyboardDisplay.toString());
}

void BlueZAgent::unregisterFromBlueZ()
{
    if (client_ == nullptr || !registered_) {
        registered_ = false;
        emit registeredChanged(false);
        return;
    }
    client_->unregisterAgent(bluez::kAuralisAgentPath.toString());
    registered_ = false;
    emit registeredChanged(false);
}

void BlueZAgent::onBlueZAvailableChanged(bool available)
{
    if (available) {
        registerWithBlueZ();
        return;
    }
    registered_ = false;
    emit registeredChanged(false);
    invalidateAllRequests(QStringLiteral("BlueZ became unavailable"));
}

void BlueZAgent::onRegisterAgentFinished(bool succeeded, const QString& errorName, const QString& errorMessage)
{
    if (!succeeded) {
        qCWarning(auralisBluetooth) << "AgentRegisterFailed" << errorName << errorMessage;
        registered_ = false;
        emit registeredChanged(false);
        return;
    }
    registered_ = true;
    qCInfo(auralisBluetooth) << "AgentRegistered" << bluez::kAuralisAgentPath;
    emit registeredChanged(true);
}

QDBusMessage BlueZAgent::delayedMessage(const QDBusMessage& message) const
{
    // setDelayedReply is const and must run on the Qt-injected original, not a copy.
    if (message.isReplyRequired()) {
        message.setDelayedReply(true);
    }
    return message;
}

void BlueZAgent::handleRelease()
{
    qCInfo(auralisBluetooth) << "AgentRelease";
    invalidateAllRequests(QStringLiteral("Agent released by BlueZ"));
    registered_ = false;
    emit registeredChanged(false);
}

QString BlueZAgent::handleRequestPinCode(const QString& devicePath, const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentRequestPinCode" << devicePath;
    createRequest(devicePath, PairingRequestType::EnterPin, delayedMessage(message));
    return {};
}

void BlueZAgent::handleDisplayPinCode(
    const QString& devicePath,
    const QString& pincode,
    const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentDisplayPinCode" << devicePath;
    auto* request = createRequest(devicePath, PairingRequestType::DisplayPin, delayedMessage(message));
    request->setPinCode(pincode);
    request->setMessage(QStringLiteral("Enter PIN %1 on the remote device").arg(pincode));
}

uint BlueZAgent::handleRequestPasskey(const QString& devicePath, const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentRequestPasskey" << devicePath;
    createRequest(devicePath, PairingRequestType::EnterPasskey, delayedMessage(message));
    return 0;
}

void BlueZAgent::handleDisplayPasskey(
    const QString& devicePath,
    uint passkey,
    uint entered,
    const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentDisplayPasskey" << devicePath;
    auto* request = createRequest(devicePath, PairingRequestType::DisplayPasskey, message, false);
    request->setPasskey(passkey, entered);
    request->setMessage(QStringLiteral("Passkey %1").arg(passkey, 6, 10, QChar('0')));
}

void BlueZAgent::handleRequestConfirmation(
    const QString& devicePath,
    uint passkey,
    const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentRequestConfirmation" << devicePath;
    auto* request = createRequest(devicePath, PairingRequestType::ConfirmPasskey, delayedMessage(message));
    request->setPasskey(passkey);
    request->setMessage(QStringLiteral("Confirm pairing passkey %1").arg(passkey, 6, 10, QChar('0')));
}

void BlueZAgent::handleRequestAuthorization(const QString& devicePath, const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentRequestAuthorization" << devicePath;
    auto* request = createRequest(devicePath, PairingRequestType::AuthorizePairing, delayedMessage(message));
    request->setMessage(QStringLiteral("Authorize pairing with this device?"));
}

void BlueZAgent::handleAuthorizeService(
    const QString& devicePath,
    const QString& uuid,
    const QDBusMessage& message)
{
    qCInfo(auralisBluetooth) << "AgentAuthorizeService" << devicePath << uuid;
    auto* request = createRequest(devicePath, PairingRequestType::AuthorizeService, delayedMessage(message));
    request->setServiceUuid(uuid);
    request->setMessage(QStringLiteral("Authorize service %1?").arg(uuid));
}

void BlueZAgent::handleCancel()
{
    qCInfo(auralisBluetooth) << "AgentCancel";
    invalidateAllRequests(QStringLiteral("Pairing canceled"));
}

PairingRequest* BlueZAgent::createRequest(
    const QString& devicePath,
    PairingRequestType type,
    const QDBusMessage& message,
    bool delayReply)
{
    invalidateAllRequests();
    auto* request = new PairingRequest(this);
    request->setDevicePath(devicePath);
    request->setRequestType(type);
    activeRequest_ = request;
    pendingCalls_.insert(
        request->requestId(),
        PendingCall{delayReply ? delayedMessage(message) : message, request, delayReply});
    emit pendingRequestChanged();
    return request;
}

void BlueZAgent::invalidateAllRequests(const QString&)
{
    for (auto it = pendingCalls_.begin(); it != pendingCalls_.end(); ++it) {
        if (it->replyPending && it->message.isReplyRequired()) {
            QDBusConnection::systemBus().send(it->message.createErrorReply(
                bluez::kAgentErrorCanceled.toString(),
                QStringLiteral("Pairing request invalidated")));
        }
        clearSecrets(it->request);
    }
    pendingCalls_.clear();
    if (activeRequest_ != nullptr) {
        activeRequest_->deleteLater();
        activeRequest_ = nullptr;
        emit pendingRequestChanged();
    }
}

void BlueZAgent::acceptPairingRequest(const QString& requestId)
{
    auto* call = callForRequest(requestId);
    if (call == nullptr) {
        return;
    }
    if (call->request->requestType() == PairingRequestType::EnterPin
        || call->request->requestType() == PairingRequestType::EnterPasskey) {
        return;
    }
    completeRequest(requestId, QVariant());
}

void BlueZAgent::rejectPairingRequest(const QString& requestId)
{
    rejectRequest(requestId, bluez::kAgentErrorRejected.toString());
}

void BlueZAgent::submitPinCode(const QString& requestId, const QString& pin)
{
    auto* call = callForRequest(requestId);
    if (call == nullptr || call->request->requestType() != PairingRequestType::EnterPin) {
        return;
    }
    completeRequest(requestId, pin);
}

void BlueZAgent::submitPasskey(const QString& requestId, uint passkey)
{
    auto* call = callForRequest(requestId);
    if (call == nullptr || call->request->requestType() != PairingRequestType::EnterPasskey) {
        return;
    }
    completeRequest(requestId, passkey);
}

void BlueZAgent::completeRequest(const QString& requestId, const QVariant& replyValue)
{
    auto it = pendingCalls_.find(requestId);
    if (it == pendingCalls_.end()) {
        return;
    }
    PendingCall call = it.value();
    pendingCalls_.erase(it);
    qCInfo(auralisBluetooth) << "AgentReplyAccepted" << requestId;
    if (call.replyPending && call.message.isReplyRequired()) {
        if (replyValue.typeId() == QMetaType::QString) {
            QDBusConnection::systemBus().send(call.message.createReply(replyValue.toString()));
        } else if (replyValue.typeId() == QMetaType::UInt || replyValue.typeId() == QMetaType::Int) {
            QDBusConnection::systemBus().send(call.message.createReply(replyValue.toUInt()));
        } else {
            QDBusConnection::systemBus().send(call.message.createReply());
        }
    }
    clearSecrets(call.request);
    if (activeRequest_ == call.request) {
        activeRequest_->deleteLater();
        activeRequest_ = nullptr;
        emit pendingRequestChanged();
    }
}

void BlueZAgent::rejectRequest(const QString& requestId, const QString& errorName)
{
    auto it = pendingCalls_.find(requestId);
    if (it == pendingCalls_.end()) {
        return;
    }
    PendingCall call = it.value();
    pendingCalls_.erase(it);
    qCInfo(auralisBluetooth) << "AgentReplyRejected" << requestId << errorName;
    if (call.replyPending && call.message.isReplyRequired()) {
        QDBusConnection::systemBus().send(call.message.createErrorReply(errorName, QStringLiteral("Rejected by user")));
    }
    clearSecrets(call.request);
    if (activeRequest_ == call.request) {
        activeRequest_->deleteLater();
        activeRequest_ = nullptr;
        emit pendingRequestChanged();
    }
}

BlueZAgent::PendingCall* BlueZAgent::callForRequest(const QString& requestId)
{
    auto it = pendingCalls_.find(requestId);
    if (it == pendingCalls_.end()) {
        return nullptr;
    }
    return &it.value();
}

void BlueZAgent::clearSecrets(PairingRequest* request)
{
    if (request == nullptr) {
        return;
    }
    request->setPinCode({});
    request->setPasskey(0);
}

} // namespace auralis::bluetooth
