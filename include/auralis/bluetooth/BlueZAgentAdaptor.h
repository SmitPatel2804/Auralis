#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>

namespace auralis::bluetooth {

class BlueZAgent;

class BlueZAgentAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

public:
    explicit BlueZAgentAdaptor(BlueZAgent* agent);

public slots:
    void Release();
    QString RequestPinCode(const QDBusObjectPath& device, const QDBusMessage& message);
    void DisplayPinCode(const QDBusObjectPath& device, const QString& pincode, const QDBusMessage& message);
    uint RequestPasskey(const QDBusObjectPath& device, const QDBusMessage& message);
    void DisplayPasskey(
        const QDBusObjectPath& device,
        uint passkey,
        uint entered,
        const QDBusMessage& message);
    void RequestConfirmation(const QDBusObjectPath& device, uint passkey, const QDBusMessage& message);
    void RequestAuthorization(const QDBusObjectPath& device, const QDBusMessage& message);
    void AuthorizeService(const QDBusObjectPath& device, const QString& uuid, const QDBusMessage& message);
    void Cancel();

private:
    BlueZAgent* agent_ = nullptr;
};

} // namespace auralis::bluetooth
