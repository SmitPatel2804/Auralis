#pragma once

#include <QObject>
#include <QString>

namespace auralis::bluetooth {

Q_NAMESPACE

enum class PairingRequestType {
    DisplayPin,
    EnterPin,
    DisplayPasskey,
    EnterPasskey,
    ConfirmPasskey,
    AuthorizePairing,
    AuthorizeService
};
Q_ENUM_NS(PairingRequestType)

QString toString(PairingRequestType type);

class PairingRequest final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString requestId READ requestId CONSTANT)
    Q_PROPERTY(QString devicePath READ devicePath CONSTANT)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(int requestType READ requestTypeInt NOTIFY requestTypeChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString pinCode READ pinCode NOTIFY pinCodeChanged)
    Q_PROPERTY(uint passkey READ passkey NOTIFY passkeyChanged)
    Q_PROPERTY(uint enteredDigits READ enteredDigits NOTIFY enteredDigitsChanged)
    Q_PROPERTY(QString serviceUuid READ serviceUuid NOTIFY serviceUuidChanged)
    Q_PROPERTY(bool needsInput READ needsInput NOTIFY needsInputChanged)
    Q_PROPERTY(bool needsConfirmation READ needsConfirmation NOTIFY needsConfirmationChanged)

public:
    explicit PairingRequest(QObject* parent = nullptr);

    QString requestId() const { return requestId_; }
    QString devicePath() const { return devicePath_; }
    QString deviceName() const { return deviceName_; }
    PairingRequestType requestType() const { return requestType_; }
    int requestTypeInt() const { return static_cast<int>(requestType_); }
    QString message() const { return message_; }
    QString pinCode() const { return pinCode_; }
    uint passkey() const { return passkey_; }
    uint enteredDigits() const { return enteredDigits_; }
    QString serviceUuid() const { return serviceUuid_; }
    bool needsInput() const;
    bool needsConfirmation() const;

    void setDevicePath(const QString& path);
    void setDeviceName(const QString& name);
    void setRequestType(PairingRequestType type);
    void setMessage(const QString& message);
    void setPinCode(const QString& pin);
    void setPasskey(uint passkey, uint entered = 0);
    void setServiceUuid(const QString& uuid);

    static QString createRequestId();

signals:
    void deviceNameChanged();
    void requestTypeChanged();
    void messageChanged();
    void pinCodeChanged();
    void passkeyChanged();
    void enteredDigitsChanged();
    void serviceUuidChanged();
    void needsInputChanged();
    void needsConfirmationChanged();

private:
    QString requestId_ = createRequestId();
    QString devicePath_;
    QString deviceName_;
    PairingRequestType requestType_ = PairingRequestType::AuthorizePairing;
    QString message_;
    QString pinCode_;
    uint passkey_ = 0;
    uint enteredDigits_ = 0;
    QString serviceUuid_;
};

} // namespace auralis::bluetooth
