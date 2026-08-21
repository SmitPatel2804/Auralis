#pragma once

#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/bluetooth/ReconnectPolicy.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

namespace auralis::bluetooth {

class BluetoothDeviceListModel;
class DeviceRegistry;

/// Windows/macOS Bluetooth facade backed by the host Qt/native Bluetooth
/// integration. It deliberately exposes the same QML contract as the BlueZ
/// manager so the UI and session engine remain platform-neutral.
class NativeBluetoothManager final : public QObject, public IBluetoothManager {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY adapterChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool canStartScan READ canStartScan NOTIFY scanningChanged)
    Q_PROPERTY(bool canStopScan READ canStopScan NOTIFY scanningChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(int connectedDeviceCount READ connectedDeviceCount NOTIFY connectedDeviceCountChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY adapterChanged)
    Q_PROPERTY(QString adapterAddress READ adapterAddress NOTIFY adapterChanged)
    Q_PROPERTY(bool adapterDiscovering READ scanning NOTIFY adapterChanged)
    Q_PROPERTY(QAbstractItemModel* devices READ devices CONSTANT)
    Q_PROPERTY(QObject* pendingPairingRequest READ pendingPairingRequest CONSTANT)
    Q_PROPERTY(bool agentRegistered READ agentRegistered CONSTANT)

public:
    explicit NativeBluetoothManager(QObject* parent = nullptr);
    ~NativeBluetoothManager() override;

    bool initialize() override;
    void shutdown() override;
    core::ServiceStatus status() const noexcept override;
    QObject* uiObject() override;
    QString backendName() const override;
    bool available() const noexcept override;
    bool transportConnected() const noexcept override;
    bool adapterPresent() const noexcept override;
    DeviceRegistry* deviceRegistry() const noexcept override;

    bool adapterPowered() const;
    bool scanning() const noexcept;
    bool canStartScan() const;
    bool canStopScan() const;
    int deviceCount() const;
    int connectedDeviceCount() const;
    QString statusText() const;
    QString errorText() const;
    QString adapterName() const;
    QString adapterAddress() const;
    QAbstractItemModel* devices() const;
    QObject* pendingPairingRequest() const;
    bool agentRegistered() const;

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void refresh() override;
    Q_INVOKABLE void pairDevice(const QString& deviceId);
    Q_INVOKABLE void cancelPairing(const QString& deviceId);
    Q_INVOKABLE void trustDevice(const QString& deviceId);
    Q_INVOKABLE void untrustDevice(const QString& deviceId);
    Q_INVOKABLE void connectDevice(const QString& deviceId);
    Q_INVOKABLE void disconnectDevice(const QString& deviceId);
    Q_INVOKABLE void forgetDevice(const QString& deviceId);
    Q_INVOKABLE void reconnectDevice(const QString& deviceId);
    Q_INVOKABLE void cancelDeviceOperation(const QString& deviceId);
    Q_INVOKABLE void acceptPairingRequest(const QString&) {}
    Q_INVOKABLE void rejectPairingRequest(const QString&) {}
    Q_INVOKABLE void submitPinCode(const QString&, const QString&) {}
    Q_INVOKABLE void submitPasskey(const QString&, uint) {}
    Q_INVOKABLE QString serviceFriendlyName(const QString& uuid) const;
    Q_INVOKABLE bool userDisconnectRequestedForDevice(const QString& deviceId) const;
    Q_INVOKABLE QString deviceDisplayName(const QString& deviceId) const;
    Q_INVOKABLE void setDeviceButtonPolicy(const QString&, bool) {}
    Q_INVOKABLE QString deviceButtonPolicyText(const QString&) const { return QStringLiteral("ALLOW"); }
    Q_INVOKABLE bool hasDevice(const QString& objectPath) const;
    Q_INVOKABLE QVariantMap deviceDetails(const QString& objectPath) const;

    void requestManagedReconnect(const QString& deviceId) override;
    void cancelManagedReconnect(const QString& deviceId) override;
    void suppressAutoReconnect(const QString& deviceId) override;
    void unsuppressAutoReconnect(const QString& deviceId) override;
    void pauseManagedReconnect() override;
    void resumeManagedReconnect() override;

signals:
    void transportConnectedChanged(bool connected);
    void availableChanged();
    void adapterChanged();
    void scanningChanged();
    void deviceCountChanged();
    void connectedDeviceCountChanged();
    void statusTextChanged();
    void errorTextChanged();
    void pendingPairingRequestChanged();
    void agentRegisteredChanged();
    void managedReconnectExhausted(const QString& devicePath, int attempts, const QString& reason);
    void managedReconnectTerminalFailure(const QString& devicePath, int attempts, const QString& reason);

private:
    struct State;
    void updateDeviceConnectionState();
    void setError(const QString& text);
    void clearError();

    std::unique_ptr<State> stateImpl_;
    DeviceRegistry* registry_ = nullptr;
    BluetoothDeviceListModel* model_ = nullptr;
    ReconnectPolicy* reconnect_ = nullptr;
    core::ServiceStatus status_ = core::ServiceStatus::Uninitialized;
    QString errorText_;
};

} // namespace auralis::bluetooth
