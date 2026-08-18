#pragma once

#include <auralis/bluetooth/AgentCapability.h>
#include <auralis/bluetooth/PairingRequest.h>

#include <QDBusMessage>
#include <QHash>
#include <QObject>
#include <QString>

namespace auralis::bluetooth {

class BlueZAgentAdaptor;
class IBlueZClient;

class BlueZAgent final : public QObject {
    Q_OBJECT

public:
    explicit BlueZAgent(
        IBlueZClient* client,
        AgentCapability capability = AgentCapability::KeyboardDisplay,
        QObject* parent = nullptr);
    ~BlueZAgent() override;

    bool initialize();
    void shutdown();
    bool isRegistered() const noexcept;

    PairingRequest* pendingRequest() const;
    AgentCapability capability() const noexcept;
    void setCapability(AgentCapability capability);

    void handleRelease();
    QString handleRequestPinCode(const QString& devicePath, const QDBusMessage& message);
    void handleDisplayPinCode(const QString& devicePath, const QString& pincode, const QDBusMessage& message);
    uint handleRequestPasskey(const QString& devicePath, const QDBusMessage& message);
    void handleDisplayPasskey(
        const QString& devicePath,
        uint passkey,
        uint entered,
        const QDBusMessage& message);
    void handleRequestConfirmation(const QString& devicePath, uint passkey, const QDBusMessage& message);
    void handleRequestAuthorization(const QString& devicePath, const QDBusMessage& message);
    void handleAuthorizeService(const QString& devicePath, const QString& uuid, const QDBusMessage& message);
    void handleCancel();

    Q_INVOKABLE void acceptPairingRequest(const QString& requestId);
    Q_INVOKABLE void rejectPairingRequest(const QString& requestId);
    Q_INVOKABLE void submitPinCode(const QString& requestId, const QString& pin);
    Q_INVOKABLE void submitPasskey(const QString& requestId, uint passkey);
    void invalidateRequestsForDevice(const QString& devicePath, const QString& reason = {});

signals:
    void pendingRequestChanged();
    void registeredChanged(bool registered);

private slots:
    void onBlueZAvailableChanged(bool available);
    void onRegisterAgentFinished(bool succeeded, const QString& errorName, const QString& errorMessage);

private:
    struct PendingCall {
        QDBusMessage message;
        PairingRequest* request = nullptr;
        bool replyPending = true;
    };

    void registerWithBlueZ();
    void unregisterFromBlueZ();
    void invalidateAllRequests(const QString& reason = {});
    PairingRequest* createRequest(
        const QString& devicePath,
        PairingRequestType type,
        const QDBusMessage& message,
        bool delayReply = true);
    void completeRequest(const QString& requestId, const QVariant& replyValue);
    void rejectRequest(const QString& requestId, const QString& errorName);
    PendingCall* callForRequest(const QString& requestId);
    void clearSecrets(PairingRequest* request);
    QDBusMessage delayedMessage(const QDBusMessage& message) const;

    IBlueZClient* client_ = nullptr;
    BlueZAgentAdaptor* adaptor_ = nullptr;
    bool exported_ = false;
    bool registered_ = false;
    AgentCapability capability_ = AgentCapability::KeyboardDisplay;
    PairingRequest* activeRequest_ = nullptr;
    QHash<QString, PendingCall> pendingCalls_;
};

} // namespace auralis::bluetooth
