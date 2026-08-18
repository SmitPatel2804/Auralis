#include <auralis/bluetooth/BlueZAgentAdaptor.h>

#include <auralis/bluetooth/BlueZAgent.h>

namespace auralis::bluetooth {

BlueZAgentAdaptor::BlueZAgentAdaptor(BlueZAgent* agent)
    : QDBusAbstractAdaptor(agent)
    , agent_(agent)
{
}

void BlueZAgentAdaptor::Release()
{
    agent_->handleRelease();
}

QString BlueZAgentAdaptor::RequestPinCode(const QDBusObjectPath& device, const QDBusMessage& message)
{
    message.setDelayedReply(true);
    return agent_->handleRequestPinCode(device.path(), message);
}

void BlueZAgentAdaptor::DisplayPinCode(
    const QDBusObjectPath& device,
    const QString& pincode,
    const QDBusMessage& message)
{
    message.setDelayedReply(true);
    agent_->handleDisplayPinCode(device.path(), pincode, message);
}

uint BlueZAgentAdaptor::RequestPasskey(const QDBusObjectPath& device, const QDBusMessage& message)
{
    message.setDelayedReply(true);
    return agent_->handleRequestPasskey(device.path(), message);
}

void BlueZAgentAdaptor::DisplayPasskey(
    const QDBusObjectPath& device,
    uint passkey,
    uint entered,
    const QDBusMessage& message)
{
    agent_->handleDisplayPasskey(device.path(), passkey, entered, message);
}

void BlueZAgentAdaptor::RequestConfirmation(
    const QDBusObjectPath& device,
    uint passkey,
    const QDBusMessage& message)
{
    message.setDelayedReply(true);
    agent_->handleRequestConfirmation(device.path(), passkey, message);
}

void BlueZAgentAdaptor::RequestAuthorization(const QDBusObjectPath& device, const QDBusMessage& message)
{
    message.setDelayedReply(true);
    agent_->handleRequestAuthorization(device.path(), message);
}

void BlueZAgentAdaptor::AuthorizeService(
    const QDBusObjectPath& device,
    const QString& uuid,
    const QDBusMessage& message)
{
    message.setDelayedReply(true);
    agent_->handleAuthorizeService(device.path(), uuid, message);
}

void BlueZAgentAdaptor::Cancel()
{
    agent_->handleCancel();
}

} // namespace auralis::bluetooth
