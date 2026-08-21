#pragma once

#include <auralis/bluetooth/BluetoothError.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/bluetooth/ReconnectPolicy.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace auralis::bluetooth {

class AdapterManager;
class BlueZAgent;
class BluetoothButtonControlManager;
class BluetoothDeviceListModel;
class BluetoothTransportFilterModel;
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
    Q_PROPERTY(QString scanModeText READ scanModeText NOTIFY scanningChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(int classicDeviceCount READ classicDeviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(int lowEnergyDeviceCount READ lowEnergyDeviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(int connectedDeviceCount READ connectedDeviceCount NOTIFY connectedDeviceCountChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY adapterChanged)
    Q_PROPERTY(QString adapterAddress READ adapterAddress NOTIFY adapterChanged)
    Q_PROPERTY(bool adapterDiscovering READ adapterDiscovering NOTIFY adapterChanged)
    Q_PROPERTY(QAbstractItemModel* devices READ devices CONSTANT)
    Q_PROPERTY(QAbstractItemModel* classicDevices READ classicDevices CONSTANT)
    Q_PROPERTY(QAbstractItemModel* lowEnergyDevices READ lowEnergyDevices CONSTANT)
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

    QString backendName() const override;

    bool available() const noexcept override;
    bool transportConnected() const noexcept override;
    bool adapterPresent() const noexcept override;
    bool adapterPowered() const;
    bool scanning() const noexcept;
    bool canStartScan() const;
    bool canStopScan() const;
    QString scanModeText() const;
    int deviceCount() const;
    int classicDeviceCount() const;
    int lowEnergyDeviceCount() const;
    QString statusText() const;
    QString errorText() const;
    QString adapterName() const;
    QString adapterAddress() const;
    bool adapterDiscovering() const;
    QAbstractItemModel* devices() const;
    QAbstractItemModel* classicDevices() const;
    QAbstractItemModel* lowEnergyDevices() const;
    QObject* pendingPairingRequest() const;
    bool agentRegistered() const;

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void startLowEnergyScan();
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
    /// Schedule reconnect through ReconnectPolicy (backoff/attempts). Prefer this over
    /// reconnectDevice() for session recovery intent so Phase 3 owns retry timing.
    void requestManagedReconnect(const QString& deviceId) override;
    void cancelManagedReconnect(const QString& deviceId) override;
    void suppressAutoReconnect(const QString& deviceId) override;
    void unsuppressAutoReconnect(const QString& deviceId) override;
    bool isAutoReconnectSuppressed(const QString& objectPath) const;
    Q_INVOKABLE void cancelDeviceOperation(const QString& deviceId);
    Q_INVOKABLE void acceptPairingRequest(const QString& requestId);
    Q_INVOKABLE void rejectPairingRequest(const QString& requestId);
    Q_INVOKABLE void submitPinCode(const QString& requestId, const QString& pin);
    Q_INVOKABLE void submitPasskey(const QString& requestId, uint passkey);
    Q_INVOKABLE QString serviceFriendlyName(const QString& uuid) const;
    Q_INVOKABLE bool userDisconnectRequestedForDevice(const QString& deviceId) const;
    Q_INVOKABLE QString deviceDisplayName(const QString& deviceId) const;
    Q_INVOKABLE void setDeviceButtonPolicy(const QString& deviceId, bool disallow);
    Q_INVOKABLE QString deviceButtonPolicyText(const QString& deviceId) const;
    int connectedDeviceCount() const;
    Q_INVOKABLE bool hasDevice(const QString& objectPath) const;
    Q_INVOKABLE QVariantMap deviceDetails(const QString& objectPath) const;

    DeviceRegistry* deviceRegistry() const noexcept override;
    ReconnectPolicy* reconnectPolicy() const noexcept;
    void setReconnectPolicyConfig(const ReconnectPolicyConfig& config);

    /// Pause/resume managed device reconnect during service loss or suspend (Phase 8).
    void pauseManagedReconnect() override;
    void resumeManagedReconnect() override;
    bool systemBusConnected() const;

signals:
    /// Platform-neutral transport health signal. On Linux this mirrors the
    /// system D-Bus connection; native backends emit their equivalent.
    void transportConnectedChanged(bool connected);
    void systemBusConnectedChanged(bool connected);
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
    void syncButtonPolicyForAddress(const QString& address, bool connected);
    void syncButtonPoliciesFromRegistry();
    QVariantMap deviceProperties(const QVariantMap& interfaces) const;
    QVariantMap adapterProperties(const QVariantMap& interfaces) const;

    IBlueZClient* client_ = nullptr;
    AdapterManager* adapters_ = nullptr;
    DiscoveryManager* discovery_ = nullptr;
    DeviceRegistry* registry_ = nullptr;
    BluetoothDeviceListModel* model_ = nullptr;
    BluetoothTransportFilterModel* classicModel_ = nullptr;
    BluetoothTransportFilterModel* lowEnergyModel_ = nullptr;
    BluetoothButtonControlManager* buttonControls_ = nullptr;
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
