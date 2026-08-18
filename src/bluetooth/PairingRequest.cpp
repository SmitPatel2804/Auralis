#include <auralis/bluetooth/PairingRequest.h>

#include <QUuid>

namespace auralis::bluetooth {

QString toString(PairingRequestType type)
{
    switch (type) {
    case PairingRequestType::DisplayPin:
        return QStringLiteral("DisplayPin");
    case PairingRequestType::EnterPin:
        return QStringLiteral("EnterPin");
    case PairingRequestType::DisplayPasskey:
        return QStringLiteral("DisplayPasskey");
    case PairingRequestType::EnterPasskey:
        return QStringLiteral("EnterPasskey");
    case PairingRequestType::ConfirmPasskey:
        return QStringLiteral("ConfirmPasskey");
    case PairingRequestType::AuthorizePairing:
        return QStringLiteral("AuthorizePairing");
    case PairingRequestType::AuthorizeService:
        return QStringLiteral("AuthorizeService");
    }
    return QStringLiteral("Unknown");
}

PairingRequest::PairingRequest(QObject* parent)
    : QObject(parent)
{
}

bool PairingRequest::needsInput() const
{
    return requestType_ == PairingRequestType::EnterPin || requestType_ == PairingRequestType::EnterPasskey;
}

bool PairingRequest::needsConfirmation() const
{
    return requestType_ == PairingRequestType::ConfirmPasskey
        || requestType_ == PairingRequestType::AuthorizePairing
        || requestType_ == PairingRequestType::AuthorizeService
        || requestType_ == PairingRequestType::DisplayPin
        || requestType_ == PairingRequestType::DisplayPasskey;
}

void PairingRequest::setDevicePath(const QString& path)
{
    devicePath_ = path;
}

void PairingRequest::setDeviceName(const QString& name)
{
    if (deviceName_ == name) {
        return;
    }
    deviceName_ = name;
    emit deviceNameChanged();
}

void PairingRequest::setRequestType(PairingRequestType type)
{
    if (requestType_ == type) {
        return;
    }
    requestType_ = type;
    emit requestTypeChanged();
    emit needsInputChanged();
    emit needsConfirmationChanged();
}

void PairingRequest::setMessage(const QString& message)
{
    if (message_ == message) {
        return;
    }
    message_ = message;
    emit messageChanged();
}

void PairingRequest::setPinCode(const QString& pin)
{
    if (pinCode_ == pin) {
        return;
    }
    pinCode_ = pin;
    emit pinCodeChanged();
}

void PairingRequest::setPasskey(uint passkey, uint entered)
{
    if (passkey_ == passkey && enteredDigits_ == entered) {
        return;
    }
    passkey_ = passkey;
    enteredDigits_ = entered;
    emit passkeyChanged();
    emit enteredDigitsChanged();
}

void PairingRequest::setServiceUuid(const QString& uuid)
{
    if (serviceUuid_ == uuid) {
        return;
    }
    serviceUuid_ = uuid;
    emit serviceUuidChanged();
}

QString PairingRequest::createRequestId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace auralis::bluetooth
