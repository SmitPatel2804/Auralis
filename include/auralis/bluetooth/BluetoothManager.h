#pragma once

#include <auralis/bluetooth/BluetoothError.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/bluetooth/ReconnectPolicy.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>

namespace auralis::bluetooth {

class AdapterManager;
class BlueZAgent;
class BluetoothDeviceListModel;
class DeviceLifecycleManager;
class DeviceRegistry;
class DiscoveryManager;
class IBlueZClient;
class PairingRequest;
class ReconnectPolicy;

class BluetoothManager final : public QObject, public IBluetoothManager {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY adapterChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool canStartScan READ canStartScan NOTIFY scanningChanged)
    Q_PROPERTY(bool canStopScan READ canStopScan NOTIFY scanningChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY adapterChanged)
    Q_PROPERTY(QString adapterAddress READ adapterAddress NOTIFY adapterChanged)
    Q_PROPERTY(bool adapterDiscovering READ adapterDiscovering NOTIFY adapterChanged)
    Q_PROPERTY(QAbstractItemModel* devices READ devices CONSTANT)
    Q_PROPERTY(QObject* pendingPairingRequest READ pendingPairingRequest NOTIFY pendingPairingRequestChanged)
    Q_PROPERTY(bool agentRegistered READ agentRegistered NOTIFY agentRegisteredChanged)

public:
    explicit BluetoothManager(QObject* parent = nullptr);
    explicit BluetoothManager(IBlueZClient* client, QObject* parent = nullptr);
    ~BluetoothManager() override;

    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;
    QObject* uiObject() override;

    bool available() const noexcept;
    bool adapterPowered() const;
    bool scanning() const noexcept;
    bool canStartScan() const;
    bool canStopScan() const;
    int deviceCount() const;
    QString statusText() const;
    QString errorText() const;
    QString adapterName() const;
    QString adapterAddress() const;
    bool adapterDiscovering() const;
    QAbstractItemModel* devices() const;
    QObject* pendingPairingRequest() const;
    bool agentRegistered() const;

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void pairDevice(const QString& deviceId);
    Q_INVOKABLE void cancelPairing(const QString& deviceId);
    Q_INVOKABLE void trustDevice(const QString& deviceId);
    Q_INVOKABLE void untrustDevice(const QString& deviceId);
    Q_INVOKABLE void connectDevice(const QString& deviceId);
    Q_INVOKABLE void disconnectDevice(const QString& deviceId);
    Q_INVOKABLE void forgetDevice(const QString& deviceId);
    Q_INVOKABLE void reconnectDevice(const QString& deviceId);
    /// Schedule reconnect through ReconnectPolicy (backoff/attempts). Prefer this over
    /// reconnectDevice() for session recovery intent so Phase 3 owns retry timing.
    void requestManagedReconnect(const QString& deviceId);
    void cancelManagedReconnect(const QString& deviceId);
    void suppressAutoReconnect(const QString& deviceId);
    void unsuppressAutoReconnect(const QString& deviceId);
    bool isAutoReconnectSuppressed(const QString& objectPath) const;
    Q_INVOKABLE void cancelDeviceOperation(const QString& deviceId);
    Q_INVOKABLE void acceptPairingRequest(const QString& requestId);
    Q_INVOKABLE void rejectPairingRequest(const QString& requestId);
    Q_INVOKABLE void submitPinCode(const QString& requestId, const QString& pin);
    Q_INVOKABLE void submitPasskey(const QString& requestId, uint passkey);
    Q_INVOKABLE QString serviceFriendlyName(const QString& uuid) const;
    Q_INVOKABLE bool userDisconnectRequestedForDevice(const QString& deviceId) const;
    Q_INVOKABLE QString deviceDisplayName(const QString& deviceId) const;
    Q_INVOKABLE int connectedDeviceCount() const;

    DeviceRegistry* deviceRegistry() const noexcept;
    ReconnectPolicy* reconnectPolicy() const noexcept;
    void setReconnectPolicyConfig(const ReconnectPolicyConfig& config);

signals:
    void availableChanged();
    void adapterChanged();
    void scanningChanged();
    void deviceCountChanged();
    void statusTextChanged();
    void errorTextChanged();
    void pendingPairingRequestChanged();
    void agentRegisteredChanged();
    void managedReconnectExhausted(const QString& devicePath, int attempts, const QString& reason);
    void managedReconnectTerminalFailure(const QString& devicePath, int attempts, const QString& reason);

private:
    void connectClientSignals();
    void handleSnapshot(const QVariantMap& objectsByPath);
    void handleInterfacesAdded(const QString& objectPath, const QVariantMap& interfaces);
    void handleInterfacesRemoved(const QString& objectPath, const QStringList& interfaces);
    void handlePropertiesChanged(
        const QString& objectPath,
        const QString& interfaceName,
        const QVariantMap& changed,
        const QStringList& invalidated);
    void handleBlueZAvailable(bool available);
    void handleSnapshotFailed(const QString& errorName, const QString& errorMessage);
    void handleSystemBusStateChanged(bool connected);
    void updateStatusText();
    void refreshDisplayedError();
    QVariantMap deviceProperties(const QVariantMap& interfaces) const;
    QVariantMap adapterProperties(const QVariantMap& interfaces) const;

    IBlueZClient* client_ = nullptr;
    AdapterManager* adapters_ = nullptr;
    DiscoveryManager* discovery_ = nullptr;
    DeviceRegistry* registry_ = nullptr;
    BluetoothDeviceListModel* model_ = nullptr;
    DeviceLifecycleManager* lifecycle_ = nullptr;
    BlueZAgent* agent_ = nullptr;
    ReconnectPolicy* reconnect_ = nullptr;
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
    bool available_ = false;
    bool signalsWired_ = false;
    QString statusText_;
    QString errorText_;
    BluetoothError snapshotError_ = BluetoothError::None;
    QString snapshotErrorMessage_;
};

} // namespace auralis::bluetooth
